//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include <iostream>
#include <string>


#include "Albany_Utils.hpp"
#include "Albany_SolverFactory.hpp"
#include "Albany_Memory.hpp"

#include "Piro_PerformSolve.hpp"
#include "Teuchos_ParameterList.hpp"

#include "Teuchos_GlobalMPISession.hpp"
#include "Teuchos_TimeMonitor.hpp"
#include "Teuchos_VerboseObject.hpp"
#include "Teuchos_StandardCatchMacros.hpp"
#include "Teuchos_FancyOStream.hpp"
#include "Thyra_DefaultProductVector.hpp"
#include "Thyra_DefaultProductVectorSpace.hpp"

#include "Tempus_IntegratorBasic.hpp" 
#include "Piro_ObserverToTempusIntegrationObserverAdapter.hpp"

// Uncomment for run time nan checking
// This is set in the toplevel CMakeLists.txt file
//#define ALBANY_CHECK_FPE

#ifdef ALBANY_CHECK_FPE
#include <math.h>
//#include <Accelerate/Accelerate.h>
#include <xmmintrin.h>
#endif
//#define ALBANY_FLUSH_DENORMALS
#ifdef ALBANY_FLUSH_DENORMALS
#include <xmmintrin.h>
#include <pmmintrin.h>
#endif

#include "Albany_DataTypes.hpp"

#include "Phalanx_config.hpp"

#include "Kokkos_Core.hpp"

#ifdef ALBANY_APF
#include "Albany_APFMeshStruct.hpp"
#endif

// Global variable that denotes this is the Tpetra executable
bool TpetraBuild = true;
const Tpetra::global_size_t INVALID = Teuchos::OrdinalTraits<Tpetra::global_size_t>::invalid ();

Teuchos::RCP<Tpetra_Vector>
createCombinedTpetraVector(
    Teuchos::Array<Teuchos::RCP<const Tpetra_Vector> > vecs)
{
  int  n_vecs = vecs.size();

  // Figure out how many local and global elements are in the
  // combined map by summing these quantities over each vector.
  LO local_num_elements = 0;
  GO global_num_elements = 0;

  for (int m = 0; m < n_vecs; ++m) {
    local_num_elements += vecs[m]->getMap()->getNodeNumElements();
    global_num_elements += vecs[m]->getMap()->getGlobalNumElements();
  }
  //Create global element indices array for combined map for this processor,
  //to be used to create the combined map.
  std::vector<GO> my_global_elements(local_num_elements);

  LO counter_local = 0;
  GO counter_global = 0;

  
  for (int m = 0; m < n_vecs; ++m) {
    LO local_num_elements_n = vecs[m]->getMap()->getNodeNumElements();
    GO global_num_elements_n = vecs[m]->getMap()->getGlobalNumElements();
    Teuchos::ArrayView<GO const> disc_global_elements = vecs[m]->getMap()->getNodeElementList();
 
    for (int l = 0; l < local_num_elements_n; ++l) {
      my_global_elements[counter_local + l] = counter_global + disc_global_elements[l];
    }
    counter_local += local_num_elements_n;
    counter_global += global_num_elements_n;
  }

  Teuchos::ArrayView<GO> const
  my_global_elements_AV =
      Teuchos::arrayView(&my_global_elements[0], local_num_elements);

  Teuchos::RCP<const Tpetra_Map>  combined_map = Teuchos::rcp(
      new Tpetra_Map(global_num_elements, my_global_elements_AV, 0, vecs[0]->getMap()->getComm()));
 
  Teuchos::RCP<Tpetra_Vector> combined_vec = Teuchos::rcp(new Tpetra_Vector(combined_map)); 
  
  counter_local = 0; 
  for (int m = 0; m < n_vecs; ++m) {
    int disc_local_elements_m = vecs[m]->getMap()->getNodeNumElements(); 
    Teuchos::RCP<Tpetra_Vector> temp = combined_vec->offsetViewNonConst(vecs[m]->getMap(), counter_local);
    Teuchos::Array<ST> array(vecs[m]->getMap()->getNodeNumElements());
    vecs[m]->get1dCopy(array);
    for (std::size_t i=0; i<vecs[m]->getMap()->getNodeNumElements(); ++i)
     temp->replaceLocalValue(i, array[i]);
    counter_local += disc_local_elements_m; 
  }

  return combined_vec;
}

// Overridden from Thyra::ModelEvaluator<ST>

//IKT: Similar to tpetraFromThyra but for thyra product vectors (currently only used in Coupled Schwarz problems). 
void tpetraFromThyraProdVec(
  const Teuchos::Array<Teuchos::RCP<const Thyra::VectorBase<ST> > > &thyraResponses,
  const Teuchos::Array<Teuchos::Array<Teuchos::RCP<const Thyra::MultiVectorBase<ST> > > > &thyraSensitivities,
  Teuchos::Array<Teuchos::RCP<const Tpetra_Vector> > &responses,
  Teuchos::Array<Teuchos::Array<Teuchos::RCP<const Tpetra_MultiVector> > > &sensitivities)
{
  Teuchos::RCP<Teuchos::FancyOStream> out(Teuchos::VerboseObjectBase::getDefaultOStream());
  responses.clear();
  //FIXME: right now only setting # of responses to 1 (solution vector) as printing of other 
  //responses does not work. 
  //responses.reserve(thyraResponses.size());
  responses.reserve(1); 
  typedef Teuchos::Array<Teuchos::RCP<const Thyra::VectorBase<ST> > > ThyraResponseArray;
  for (ThyraResponseArray::const_iterator it_begin = thyraResponses.begin(),
    it_end = thyraResponses.end(),
    it = it_begin;
    it != it_end;
    ++it) 
    {
    if (it == it_end-1) {
      Teuchos::RCP<const Thyra::ProductVectorBase<ST> > r_prod =
           Teuchos::nonnull(*it) ?
           Teuchos::rcp_dynamic_cast<const Thyra::ProductVectorBase<ST> >(*it,true) :
           Teuchos::null;
      if (r_prod != Teuchos::null) {
        //FIXME: productSpace()->numBlocks() when we have responses and >1 model does not work for some reason.  
        //Need to figure out why!
        const int nProdVecs = r_prod->productSpace()->numBlocks(); 
        //create Teuchos array of spaces / vectors, to be populated from the product vector
        Teuchos::Array<Teuchos::RCP<const Tpetra_Vector> > rs(nProdVecs); 
        for (int i=0; i<nProdVecs; i++) {
          rs[i] =  ConverterT::getConstTpetraVector(r_prod->getVectorBlock(i)); 
        }
        Teuchos::RCP<Tpetra_Vector> r_vec = createCombinedTpetraVector(rs); 
        responses.push_back(r_vec);
       }
     }
   }
  sensitivities.clear();
  sensitivities.reserve(thyraSensitivities.size());
  if (thyraSensitivities.size() > 0) 
    *out << "WARNING: For Thyra::ProductVectorBase, sensitivities are not yet supported! \n"; 
  typedef Teuchos::Array<Teuchos::Array<Teuchos::RCP<const Thyra::MultiVectorBase<ST> > > > ThyraSensitivityArray;
  for (ThyraSensitivityArray::const_iterator it_begin = thyraSensitivities.begin(),
    it_end = thyraSensitivities.end(),
    it = it_begin;
    it != it_end;
    ++it) {
      ThyraSensitivityArray::const_reference sens_thyra = *it;
      Teuchos::Array<Teuchos::RCP<const Tpetra_MultiVector> > sens;
      sens.reserve(sens_thyra.size());
      for (ThyraSensitivityArray::value_type::const_iterator jt = sens_thyra.begin(),
        jt_end = sens_thyra.end();
        jt != jt_end;
        ++jt) {
          Teuchos::RCP<const Thyra::ProductVectorBase<ST> > s_prod =
          Teuchos::nonnull(*jt) ?
          Teuchos::rcp_dynamic_cast<const Thyra::ProductVectorBase<ST> >(*jt,true) :
          Teuchos::null;
          //FIXME: ultimately we'll need to change this to set the sensitivity vector
          Teuchos::RCP<const Tpetra_Vector> s_vec = Teuchos::null; 
          sens.push_back(s_vec);
        }
     sensitivities.push_back(sens);
   }
}



void tpetraFromThyra(
  const Teuchos::Array<Teuchos::RCP<const Thyra::VectorBase<ST> > > &thyraResponses,
  const Teuchos::Array<Teuchos::Array<Teuchos::RCP<const Thyra::MultiVectorBase<ST> > > > &thyraSensitivities,
  Teuchos::Array<Teuchos::RCP<const Tpetra_Vector> > &responses,
  Teuchos::Array<Teuchos::Array<Teuchos::RCP<const Tpetra_MultiVector> > > &sensitivities)
{
  responses.clear();
  responses.reserve(thyraResponses.size());
  typedef Teuchos::Array<Teuchos::RCP<const Thyra::VectorBase<ST> > > ThyraResponseArray;
  for (ThyraResponseArray::const_iterator it_begin = thyraResponses.begin(),
      it_end = thyraResponses.end(),
      it = it_begin;
      it != it_end;
      ++it) {
    responses.push_back(Teuchos::nonnull(*it) ? ConverterT::getConstTpetraVector(*it) : Teuchos::null);
    }

  sensitivities.clear();
  sensitivities.reserve(thyraSensitivities.size());
  typedef Teuchos::Array<Teuchos::Array<Teuchos::RCP<const Thyra::MultiVectorBase<ST> > > > ThyraSensitivityArray;
  for (ThyraSensitivityArray::const_iterator it_begin = thyraSensitivities.begin(),
      it_end = thyraSensitivities.end(),
      it = it_begin;
      it != it_end;
      ++it) {
    ThyraSensitivityArray::const_reference sens_thyra = *it;
    Teuchos::Array<Teuchos::RCP<const Tpetra_MultiVector> > sens;
    sens.reserve(sens_thyra.size());
    for (ThyraSensitivityArray::value_type::const_iterator jt = sens_thyra.begin(),
        jt_end = sens_thyra.end();
        jt != jt_end;
        ++jt) {
        sens.push_back(Teuchos::nonnull(*jt) ? ConverterT::getConstTpetraMultiVector(*jt) : Teuchos::null);
    }
    sensitivities.push_back(sens);
  }
}

int main(int argc, char *argv[]) {

  int status=0; // 0 = pass, failures are incremented
  bool success = true;

  Teuchos::GlobalMPISession mpiSession(&argc,&argv);
  Kokkos::initialize(argc, argv);

#ifdef ALBANY_FLUSH_DENORMALS
  _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
  _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif

#ifdef ALBANY_CHECK_FPE
//	_mm_setcsr(_MM_MASK_MASK &~
//		(_MM_MASK_OVERFLOW | _MM_MASK_INVALID | _MM_MASK_DIV_ZERO) );
    _MM_SET_EXCEPTION_MASK(_MM_GET_EXCEPTION_MASK() & ~_MM_MASK_INVALID);
#endif

#ifdef ALBANY_64BIT_INT
  static_assert(sizeof(long) == 8,
      "Error: The 64 bit build of Albany assumes that sizeof(long) == 64 bits.");
#endif

#ifdef ALBANY_APF
  Albany::APFMeshStruct::initialize_libraries(&argc, &argv);
#endif

  using Teuchos::RCP;
  using Teuchos::rcp;

  RCP<Teuchos::FancyOStream> out(Teuchos::VerboseObjectBase::getDefaultOStream());

  // Command-line argument for input file
  Albany::CmdLineArgs cmd;
  cmd.parse_cmdline(argc, argv, *out);
  bool computeSensitivities = true; 

  try {
    RCP<Teuchos::Time> totalTime =
      Teuchos::TimeMonitor::getNewTimer("Albany: ***Total Time***");

    RCP<Teuchos::Time> setupTime =
      Teuchos::TimeMonitor::getNewTimer("Albany: Setup Time");
    Teuchos::TimeMonitor totalTimer(*totalTime); //start timer
    Teuchos::TimeMonitor setupTimer(*setupTime); //start timer

    RCP<const Teuchos_Comm> comm =
      Tpetra::DefaultPlatform::getDefaultPlatform().getComm();

    // Connect vtune for performance profiling
    if (cmd.vtune) {
      Albany::connect_vtune(comm->getRank());
    }

    Albany::SolverFactory slvrfctry(cmd.xml_filename, comm);
    RCP<Albany::Application> app;
    const RCP<Thyra::ResponseOnlyModelEvaluatorBase<ST> > solver =
      slvrfctry.createAndGetAlbanyAppT(app, comm, comm);
    
    setupTimer.~TimeMonitor();

    Teuchos::ParameterList &appPL = slvrfctry.getParameters();
    // Create debug output object
    Teuchos::ParameterList &debugParams =
      appPL.sublist("Debug Output", true);
    bool writeToMatrixMarketSoln = debugParams.get("Write Solution to MatrixMarket", false);
    bool writeToMatrixMarketDistrSolnMap = debugParams.get("Write Distributed Solution and Map to MatrixMarket", false);
    bool writeToCoutSoln = debugParams.get("Write Solution to Standard Output", false);

    std::string solnMethod = appPL.sublist("Problem").get<std::string>("Solution Method"); 
    if (solnMethod == "Transient Tempus No Piro") { 
      //Start of code to use Tempus to perform time-integration without going through Piro
      Teuchos::RCP<Thyra::ModelEvaluator<ST>> model = slvrfctry.returnModelT();
      Teuchos::RCP<Teuchos::ParameterList> tempusPL = Teuchos::null; 
      if (appPL.sublist("Piro").isSublist("Tempus")) {
        tempusPL = Teuchos::rcp(&(appPL.sublist("Piro").sublist("Tempus")), false); 
      }
      else {
        TEUCHOS_TEST_FOR_EXCEPTION(true,
          Teuchos::Exceptions::InvalidParameter,
          std::endl << "Error!  No Tempus sublist when attempting to run problem with Transient Tempus No Piro " <<
          "Solution Method. " << std::endl);
      }   
      auto integrator = Tempus::integratorBasic<double>(tempusPL, model);
      auto piro_observer = slvrfctry.returnObserverT(); 
      Teuchos::RCP<Tempus::IntegratorObserver<double> > tempus_observer = Teuchos::null;
      if (Teuchos::nonnull(piro_observer)) {
        auto solutionHistory = integrator->getSolutionHistory();
        auto timeStepControl = integrator->getTimeStepControl();
        tempus_observer = Teuchos::rcp(new Piro::ObserverToTempusIntegrationObserverAdapter<double>(solutionHistory, 
                                           timeStepControl, piro_observer));
      }
      if (Teuchos::nonnull(tempus_observer)) {
        integrator->setObserver(tempus_observer); 
        integrator->initialize(); 
      }
      bool integratorStatus = integrator->advanceTime(); 
      double time = integrator->getTime();
      *out << "\n Final time = " << time << "\n"; 
      Teuchos::RCP<Thyra::VectorBase<double> > x = integrator->getX();
      Teuchos::RCP<const Tpetra_Vector> x_tpetra = ConverterT::getConstTpetraVector(x);  
      if (writeToCoutSoln == true)  
        Albany::printTpetraVector(*out << "\nxfinal = \n", x_tpetra);
      if (writeToMatrixMarketSoln == true) { 
        //create serial map that puts the whole solution on processor 0
        int numMyElements = (x_tpetra->getMap()->getComm()->getRank() == 0) ? x_tpetra->getMap()->getGlobalNumElements() : 0;
        Teuchos::RCP<const Tpetra_Map> serial_map = Teuchos::rcp(new const Tpetra_Map(INVALID, numMyElements, 0, comm));
        //create importer from parallel map to serial map and populate serial solution x_tpetra_serial
        Teuchos::RCP<Tpetra_Import> importOperator = Teuchos::rcp(new Tpetra_Import(x_tpetra->getMap(), serial_map)); 
        Teuchos::RCP<Tpetra_Vector> x_tpetra_serial = Teuchos::rcp(new Tpetra_Vector(serial_map)); 
        x_tpetra_serial->doImport(*x_tpetra, *importOperator, Tpetra::INSERT);
        //writing to MatrixMarket file
         Tpetra_MatrixMarket_Writer::writeDenseFile("xfinal_tempus.mm", x_tpetra_serial);
      }
      if (writeToMatrixMarketDistrSolnMap == true) {
        //writing to MatrixMarket file
        Tpetra_MatrixMarket_Writer::writeDenseFile("xfinal_tempus_distributed.mm", *x_tpetra);
        Tpetra_MatrixMarket_Writer::writeMapFile("xfinal_tempus_distributed_map.mm", *x_tpetra->getMap());
      }
      *out << "\n Finish Transient Tempus No Piro time integration!\n";
      //End of code to use Tempus to perform time-integration without going through Piro
    }
    else {
        TEUCHOS_TEST_FOR_EXCEPTION(true, 
            Teuchos::Exceptions::InvalidParameter,
            "\n Error!  AlbanyTempus executable can only be run with 'Transient Tempus No Piro' Solution Method.  " <<
            "You have selected Solution Method = " <<  solnMethod << "\n");
    }
  }
  TEUCHOS_STANDARD_CATCH_STATEMENTS(true, std::cerr, success);
  if (!success) status+=10000;

  Teuchos::TimeMonitor::summarize(*out,false,true,false/*zero timers*/);

#ifdef ALBANY_APF
  Albany::APFMeshStruct::finalize_libraries();
#endif

  Kokkos::finalize_all();

  return status;
}
