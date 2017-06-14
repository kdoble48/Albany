//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include <iostream>
#include <string>

#include <matrix_container.hpp>
#include <communicator.hpp>

#include "Albany_Memory.hpp"
#include "Albany_SolverFactory.hpp"
#include "Albany_Utils.hpp"

#include "Piro_PerformSolve.hpp"
#include "Teuchos_ParameterList.hpp"

#include "Teuchos_FancyOStream.hpp"
#include "Teuchos_GlobalMPISession.hpp"
#include "Teuchos_XMLParameterListHelpers.hpp"
#include "Teuchos_StandardCatchMacros.hpp"
#include "Teuchos_TimeMonitor.hpp"
#include "Teuchos_VerboseObject.hpp"
#include "Thyra_DefaultProductVector.hpp"
#include "Thyra_DefaultProductVectorSpace.hpp"

#include "ATO_TopoTools.hpp"

// Uncomment for run time nan checking
// This is set in the toplevel CMakeLists.txt file
//#define ALBANY_CHECK_FPE

#if defined(ALBANY_CHECK_FPE)
#include <math.h>
//#include <Accelerate/Accelerate.h>
#include <xmmintrin.h>
#endif
//#define ALBANY_FLUSH_DENORMALS
#if defined(ALBANY_FLUSH_DENORMALS)
#include <pmmintrin.h>
#include <xmmintrin.h>
#endif

#include "Albany_DataTypes.hpp"

#include "Phalanx.hpp"
#include "Phalanx_config.hpp"

#include "Kokkos_Core.hpp"

#if defined(ALBANY_APF)
#include "Albany_APFMeshStruct.hpp"
#endif

#include <Plato_Application.hpp>
#include <Plato_Exceptions.hpp>
#include <Plato_Interface.hpp>
#include <Plato_PenaltyModel.hpp>
#include <Plato_SharedData.hpp>

/******************************************************************************/
class MPMD_App : public Plato::Application 
/******************************************************************************/
{
  public:
    MPMD_App(int argc, char **argv, MPI_Comm& localComm);
    virtual ~MPMD_App();
    void initialize(std::vector<Plato::SharedData*>);
    void compute(std::string);
    void finalize();
  
    void importData(std::string name, Plato::SharedData* sf);
    void exportData(std::string name, Plato::SharedData* sf);
  
  private:

    // functions
    void throwParsingException(
      std::string spec, std::string name, 
      const std::map<std::string,std::vector<double>*>& valueMap);

    void throwParsingException(
      std::string spec, std::string name, 
      const std::map<std::string,DistributedVector*>& fieldMap);

    void copyFieldIntoState(std::string name, Plato::SharedData* sf);
    void copyFieldFromState(std::string name, Plato::SharedData* sf);
    void copyFieldIntoDistParam(std::string name, Plato::SharedData* sf);
    void copyFieldFromDistParam(std::string name, Plato::SharedData* sf);

    void copyValueFromState(std::string name, Plato::SharedData* sf);

    bool isElemNodeState(std::string localFieldName);
    bool isDistParam(std::string localFieldName);

    bool addField(pugi::xml_node&, vector<Plato::SharedData*>& sharedData);
    bool addValue(pugi::xml_node&, vector<Plato::SharedData*>& sharedData);

    // data
    Teuchos::RCP<Albany::Application> m_app;
    Teuchos::RCP<Thyra::ResponseOnlyModelEvaluatorBase<ST>> m_solver;
    Teuchos::RCP<Albany::SolverFactory> m_solverFactory;
    Teuchos::RCP<const Teuchos_Comm> m_comm;
    Teuchos::Array<Teuchos::RCP<const Tpetra_Vector>> m_responses;
    Teuchos::Array<Teuchos::Array<Teuchos::RCP<const Tpetra_MultiVector>>> m_sensitivities;

    Teuchos::RCP<Tpetra_Vector> m_localVector, m_overlapVector;
    Teuchos::RCP<Tpetra_Import> m_importer;
    Teuchos::RCP<Tpetra_Export> m_exporter;

    pugi::xml_document m_inputTree;
  
    std::map<std::string,std::string> m_stateMap, m_distParamMap;
//    std::map<std::string,std::vector<double>*> m_valueMap;


};
/******************************************************************************/



//Communicator LocalComm;

/******************************************************************************/
int main(int argc, char **argv)
/******************************************************************************/
{
    MPI_Init(&argc, &argv);

    Plato::Interface* platoInterface = new Plato::Interface();

    MPI_Comm localComm;
    platoInterface->getLocalComm(localComm);
//    LocalComm.init(localComm);

    MPMD_App* myApp = new MPMD_App(argc, argv, localComm);
    platoInterface->registerPerformer(myApp);

    platoInterface->perform();

    delete myApp;

    MPI_Finalize();

}



/******************************************************************************/
MPMD_App::MPMD_App(int argc, char **argv, MPI_Comm& localComm)
/******************************************************************************/
{

  int status = 0;  // 0 = pass, failures are incremented
  bool success = true;

  Kokkos::initialize(argc, argv);

#ifdef ALBANY_FLUSH_DENORMALS
  _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
  _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif

#ifdef ALBANY_CHECK_FPE
  _MM_SET_EXCEPTION_MASK(_MM_GET_EXCEPTION_MASK() & ~_MM_MASK_INVALID);
#endif

#ifdef ALBANY_64BIT_INT
  static_assert(
      sizeof(long) == 8,
      "Error: The 64 bit build of Albany assumes that sizeof(long) == 64 "
      "bits.");
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

  try {
    RCP<Teuchos::Time> totalTime =
        Teuchos::TimeMonitor::getNewTimer("Albany: ***Total Time***");

    RCP<Teuchos::Time> setupTime =
        Teuchos::TimeMonitor::getNewTimer("Albany: Setup Time");
    Teuchos::TimeMonitor totalTimer(*totalTime);  // start timer
    Teuchos::TimeMonitor setupTimer(*setupTime);  // start timer

    Tpetra::MpiPlatform<Tpetra::Details::DefaultTypes::node_type> localPlatform(Teuchos::null, localComm);
    m_comm = localPlatform.getComm();

    // Connect vtune for performance profiling
    if (cmd.vtune) { Albany::connect_vtune(m_comm->getRank()); }

    // parse input into Teuchos::ParameterList
    Teuchos::RCP<Teuchos::ParameterList>
      appParams = Teuchos::createParameterList("Albany Parameters");
    Teuchos::updateParametersFromXmlFileAndBroadcast(cmd.xml_filename, appParams.ptr(), *m_comm);

    Teuchos::ParameterList& probParams = appParams->sublist("Problem",false);

    Teuchos::ParameterList& topoParams = probParams.get<Teuchos::ParameterList>("Topologies");
    int ntopos = topoParams.get<int>("Number of Topologies");

    Teuchos::RCP<ATO::TopologyArray>
    topologyArray = Teuchos::rcp( new Teuchos::Array<Teuchos::RCP<ATO::Topology> >(ntopos) );
    for(int itopo=0; itopo<ntopos; itopo++){
      Teuchos::ParameterList& tParams = topoParams.sublist(Albany::strint("Topology",itopo));
      (*topologyArray)[itopo] = Teuchos::rcp(new ATO::Topology(tParams, itopo));
    }
    
    // add topology objects
    probParams.set<Teuchos::RCP<ATO::TopologyArray> >("Topologies",topologyArray);
    
    // send in ParameterList instead of xml filename 
    m_solverFactory = rcp(new Albany::SolverFactory(appParams, m_comm));
    m_solver = m_solverFactory->createAndGetAlbanyAppT(m_app, m_comm, m_comm);

    setupTimer.~TimeMonitor();

    std::string solnMethod =
        m_solverFactory->getParameters().sublist("Problem").get<std::string>(
            "Solution Method");
    if (solnMethod == "Transient Tempus No Piro") {
      TEUCHOS_TEST_FOR_EXCEPTION(
          true, Teuchos::Exceptions::InvalidParameter,
          std::endl
              << "Error!  Please run AlbanyTempus executable with Solution "
                 "Method = Transient Tempus No Piro.\n");
    }
  }
  TEUCHOS_STANDARD_CATCH_STATEMENTS(true, std::cerr, success);
  if (!success) status += 10000;

  
  const char* input_char = std::getenv("PLATO_APP_FILE");
  pugi::xml_parse_result result = m_inputTree.load_file(input_char);
  if(!result){
    //throw
  }
}



/******************************************************************************/
void MPMD_App::initialize(vector<Plato::SharedData*> sharedData)
/******************************************************************************/
{

  Albany::StateManager& stateMgr = m_app->getStateMgr();
  Teuchos::RCP<Albany::AbstractDiscretization> disc = stateMgr.getDiscretization();
  Teuchos::RCP<const Tpetra_Map> localNodeMapT   = disc->getNodeMapT();
  Teuchos::RCP<const Tpetra_Map> overlapNodeMapT = disc->getOverlapNodeMapT();

  m_localVector    = Teuchos::rcp(new Tpetra_Vector(localNodeMapT));
  m_overlapVector  = Teuchos::rcp(new Tpetra_Vector(overlapNodeMapT));
  m_importer       = Teuchos::rcp(new Tpetra_Import(localNodeMapT, overlapNodeMapT));
  m_exporter       = Teuchos::rcp(new Tpetra_Export(overlapNodeMapT, localNodeMapT));



  // parse Operation definition
  //
  pugi::xml_node node = m_inputTree.child("Operation");
  if( m_inputTree.next_sibling("Operation") ){ 
    std::stringstream message;
    message << "Only one 'Operation' definition allowed per Albany performer.  Multiple provided." << std::endl;
    Plato::ParsingException pe(message.str());
    throw pe;
  }

  // parse InputFields
  //
  for(pugi::xml_node inputNode : node.children("InputField")){
    addField(inputNode, sharedData);
  }

  // parse InputValues
  //
  for(pugi::xml_node inputNode : node.children("OutputValue")){
    addValue(inputNode, sharedData);
  }
  
  // parse OutputFields
  //
  for(pugi::xml_node inputNode : node.children("OutputField")){
    addField(inputNode, sharedData);
  }
}

/******************************************************************************/
bool MPMD_App::addValue(pugi::xml_node& inputNode, vector<Plato::SharedData*>& sharedData)
/******************************************************************************/
{
  string sharedValueName = Plato::Parse::getString(inputNode,"SharedValueName");

  // is the requested SharedValue defined?
  bool found = false;
  for(Plato::SharedData* sd : sharedData){
    std::string name = sd->myName();
    if(sd->myLayout() == "Value" && name == sharedValueName){
      found = true; break;
    }
  }
  if(!found){
    std::stringstream message;
    message << "Cannot find specified SharedValueName: '" << sharedValueName << "'" << std::endl;
    Plato::ParsingException pe(message.str());
    throw pe;
  }

  // is the requested local field defined?
  string localValueName = Plato::Parse::getString(inputNode,"LocalValueName");
  if( isElemNodeState(localValueName) ){
    m_stateMap[sharedValueName] = localValueName;
  } else {
    std::stringstream message;
    message << "Cannot find specified LocalValueName: '" << localValueName << "'" << std::endl;
    Plato::ParsingException pe(message.str());
    throw pe;
  }
}

/******************************************************************************/
bool MPMD_App::addField(pugi::xml_node& inputNode, vector<Plato::SharedData*>& sharedData)
/******************************************************************************/
{
  string sharedFieldName = Plato::Parse::getString(inputNode,"SharedFieldName");

  // is the requested SharedField defined?
  bool found = false;
  for(Plato::SharedData* sd : sharedData){
    std::string name = sd->myName();
    if(sd->myLayout() == "Field" && name == sharedFieldName){
      found = true; break;
    }
  }
  if(!found){
    std::stringstream message;
    message << "Cannot find specified SharedFieldName: '" << sharedFieldName << "'" << std::endl;
    Plato::ParsingException pe(message.str());
    throw pe;
  }

  // is the requested local field defined?
  string localFieldName = Plato::Parse::getString(inputNode,"LocalFieldName");
  if( isElemNodeState(localFieldName) ){
    m_stateMap[sharedFieldName] = localFieldName;
    Teuchos::RCP<PHX::MDALayout<Cell,Node>> 
      node_scalar = Teuchos::rcp(new PHX::MDALayout<Cell,Node>(1,1));
//    m_app->getStateMgr().registerStateVariable(localFieldName, node_scalar, "all",
//                                               "scalar", 0.0, /*registerOldState=*/ false, /*writeOut=*/ true);
  } else
  if( isDistParam(localFieldName) ){
    m_distParamMap[sharedFieldName] = localFieldName;
  } else {
    std::stringstream message;
    message << "Cannot find specified LocalFieldName: '" << localFieldName << "'" << std::endl;
    Plato::ParsingException pe(message.str());
    throw pe;
  }
}

/******************************************************************************/
bool MPMD_App::isElemNodeState(std::string localFieldName)
/******************************************************************************/
{
  Albany::StateManager&  stateMgr     = m_app->getStateMgr();
  Albany::StateArrays&   stateArrays  = stateMgr.getStateArrays();
  Albany::StateArrayVec& elemNodeFlds = stateArrays.elemStateArrays;
  int numWorksets = elemNodeFlds.size();
  if(numWorksets){
    auto it = elemNodeFlds[0].find(localFieldName);
    if(it != elemNodeFlds[0].end()){
      return true;
    }
  }
  return false;
}
/******************************************************************************/
bool MPMD_App::isDistParam(std::string localFieldName)
/******************************************************************************/
{
  return false;
}

void
tpetraFromThyra( 
    const Teuchos::Array<Teuchos::RCP<const Thyra::VectorBase<ST>>> &thyraResponses,
    const Teuchos::Array<Teuchos::Array< Teuchos::RCP<const Thyra::MultiVectorBase<ST>>>> &thyraSensitivities,
    Teuchos::Array<Teuchos::RCP<const Tpetra_Vector>> &responses,
    Teuchos::Array<Teuchos::Array<Teuchos::RCP<const Tpetra_MultiVector>>> &sensitivities);

void
tpetraFromThyraProdVec(
    const Teuchos::Array<Teuchos::RCP<const Thyra::VectorBase<ST>>> &thyraResponses,
    const Teuchos::Array<Teuchos::Array< Teuchos::RCP<const Thyra::MultiVectorBase<ST>>>> &thyraSensitivities,
    Teuchos::Array<Teuchos::RCP<const Tpetra_Vector>> &responses,
    Teuchos::Array<Teuchos::Array<Teuchos::RCP<const Tpetra_MultiVector>>> &sensitivities);

// Global variable that denotes this is the Tpetra executable
bool TpetraBuild = true;
//const Tpetra::global_size_t INVALID = Teuchos::OrdinalTraits<Tpetra::global_size_t>::invalid();


/******************************************************************************/
void
MPMD_App::compute(std::string noOp)
/******************************************************************************/
{

    Teuchos::ParameterList &solveParams = m_solverFactory->getAnalysisParameters().sublist("Solve", false);

    Teuchos::Array<Teuchos::RCP<const Thyra::VectorBase<ST>>> thyraResponses;
    Teuchos::Array<Teuchos::Array<Teuchos::RCP<const Thyra::MultiVectorBase<ST>>>> thyraSensitivities;

    Piro::PerformSolve(*m_solver, solveParams, thyraResponses, thyraSensitivities);

    tpetraFromThyra(thyraResponses, thyraSensitivities, m_responses, m_sensitivities);

}



/******************************************************************************/
void MPMD_App::finalize()
/******************************************************************************/
{
  Kokkos::finalize_all();
}



/******************************************************************************/
void MPMD_App::importData(std::string name, Plato::SharedData* sf)
/******************************************************************************/
{
  if(sf->myLayout() == "Field"){
    {
      auto it = m_stateMap.find(name);
      if(it != m_stateMap.end()){
        copyFieldIntoState(it->second, sf);
        return;
      }
    }
    {
      auto it = m_distParamMap.find(name);
      if(it != m_distParamMap.end()){
        copyFieldIntoDistParam(it->second, sf);
        return;
      }
    }
  } else
  if(sf->myLayout() == "Value"){
  }
}


/******************************************************************************/
void MPMD_App::copyFieldIntoState(std::string name, Plato::SharedData* sf)
/******************************************************************************/
{
  Albany::StateManager& stateMgr = m_app->getStateMgr();

  Albany::StateArrays& stateArrays = stateMgr.getStateArrays();
  Albany::StateArrayVec& dest = stateArrays.elemStateArrays;
  int numWorksets = dest.size();

  Teuchos::RCP<Albany::AbstractDiscretization> disc = stateMgr.getDiscretization();
  const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<GO> > >::type&
    wsElNodeID = disc->getWsElNodeID();

  m_localVector->putScalar(0.0);
  Teuchos::ArrayRCP<double> ltopo = m_localVector->get1dViewNonConst(); 
  int numLocalVals = m_localVector->getLocalLength();
  Teuchos::RCP<const Tpetra_Map> localNodeMap = m_localVector->getMap();
  for(int lid=0; lid<numLocalVals; lid++){
    int gid = localNodeMap->getGlobalElement(lid);
    // HACK (figure out why DataLayer's GIDs start from 1 but Albany's start from 0?)
    sf->getData(ltopo[lid],gid+1);
  }
  m_overlapVector->doImport(*m_localVector, *m_importer, Tpetra::INSERT);
  Teuchos::RCP<Albany::NodeFieldContainer>
    nodeContainer = stateMgr.getNodalDataBase()->getNodeContainer();
  auto it = nodeContainer->find(name+"_node");
  if(it != nodeContainer->end())
    (*nodeContainer)[name+"_node"]->saveFieldVector(m_overlapVector,/*offset=*/0);

  // copy the field into the state manager
  Teuchos::RCP<const Tpetra_Map> overlapNodeMap = m_overlapVector->getMap();
  Teuchos::ArrayRCP<double> otopo = m_overlapVector->get1dViewNonConst(); 
  for(int ws=0; ws<numWorksets; ws++){
    Albany::MDArray& wsTopo = dest[ws][name];
    int numCells = wsTopo.dimension(0), numNodes = wsTopo.dimension(1);
    for(int cell=0; cell<numCells; cell++)
      for(int node=0; node<numNodes; node++){
        int gid = wsElNodeID[ws][cell][node];
        int lid = overlapNodeMap->getLocalElement(gid);
        wsTopo(cell,node) = otopo[lid];
      }
  }
}

/******************************************************************************/
void MPMD_App::copyValueFromState(std::string name, Plato::SharedData* sf)
/******************************************************************************/
{
  Albany::StateManager& stateMgr = m_app->getStateMgr();

  Albany::StateArrays& stateArrays = stateMgr.getStateArrays();
  Albany::StateArrayVec& src = stateArrays.elemStateArrays;

  // copy the field from the state manager
  Albany::MDArray& valSrc = src[/*worksetIndex=*/0][name];
  double globalVal, val = valSrc(0);
  if( m_comm != Teuchos::null ){
    Teuchos::reduceAll(*m_comm, Teuchos::REDUCE_SUM, /*numvals=*/ 1, &val, &globalVal);
  } else globalVal = val;
//  valSrc(0)=0.0;
  sf->setData(globalVal);
}

/******************************************************************************/
void MPMD_App::copyFieldFromState(std::string name, Plato::SharedData* sf)
/******************************************************************************/
{
  Albany::StateManager& stateMgr = m_app->getStateMgr();

  Albany::StateArrays& stateArrays = stateMgr.getStateArrays();
  Albany::StateArrayVec& src = stateArrays.elemStateArrays;
  int numWorksets = src.size();

  Teuchos::RCP<Albany::AbstractDiscretization> disc = stateMgr.getDiscretization();
  const Albany::WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<GO> > >::type&
    wsElNodeID = disc->getWsElNodeID();

  m_overlapVector->putScalar(0.0);

  // copy the field from the state manager
  for(int ws=0; ws<numWorksets; ws++){
    Albany::MDArray& wsSrc = src[ws][name];
    int numCells = wsSrc.dimension(0);
    int numNodes = wsSrc.dimension(1);
    for(int cell=0; cell<numCells; cell++)
      for(int node=0; node<numNodes; node++) {
        m_overlapVector->sumIntoGlobalValue(wsElNodeID[ws][cell][node],wsSrc(cell,node));
      }
  }
  m_localVector->putScalar(0.0);
  m_localVector->doExport(*m_overlapVector, *m_exporter, Tpetra::ADD);
  Teuchos::ArrayRCP<double> ltopo = m_localVector->get1dViewNonConst(); 
  int numLocalVals = m_localVector->getLocalLength();
  auto map = m_localVector->getMap();
  for(int lid=0; lid<numLocalVals; lid++){
    int gid = map->getGlobalElement(lid);
    // HACK (figure out why DataLayer's GIDs start from 1 but Albany's start from 0?)
    sf->setData(ltopo[lid],gid+1);
  }
//  sf->setData(ltopo.get());

  Teuchos::RCP<Albany::NodeFieldContainer>
    nodeContainer = stateMgr.getNodalDataBase()->getNodeContainer();
  auto it = nodeContainer->find(name+"_node");
  if(it != nodeContainer->end()){
    m_overlapVector->doImport(*m_localVector, *m_importer, Tpetra::INSERT);
    (*nodeContainer)[name+"_node"]->saveFieldVector(m_overlapVector,/*offset=*/0);
  }
}

/******************************************************************************/
void MPMD_App::copyFieldIntoDistParam(std::string name, Plato::SharedData* sf)
/******************************************************************************/
{
}
/******************************************************************************/
void MPMD_App::copyFieldFromDistParam(std::string name, Plato::SharedData* sf)
/******************************************************************************/
{
}





/******************************************************************************/
void MPMD_App::exportData(std::string name, Plato::SharedData* sf)
/******************************************************************************/
{
  if(sf->myLayout() == "Field"){
    {
      auto it = m_stateMap.find(name);
      if(it != m_stateMap.end()){
        copyFieldFromState(it->second, sf);
        return;
      }
    }
    {
      auto it = m_distParamMap.find(name);
      if(it != m_distParamMap.end()){
        copyFieldFromDistParam(it->second, sf);
        return;
      }
    }
  } else
  if(sf->myLayout() == "Value"){
    {
      auto it = m_stateMap.find(name);
      if(it != m_stateMap.end()){
        copyValueFromState(it->second, sf);
        return;
      }
    }
  }
}


/******************************************************************************/
void
MPMD_App::throwParsingException(
  std::string spec, std::string name, 
  const std::map<std::string,std::vector<double>*>& valueMap)
/******************************************************************************/
{
    std::stringstream message;
    message << "Cannot find specified '" << spec << "': " << name << std::endl;
    message << "Values available: " << std::endl;
    for(auto f : valueMap){
      message << f.first << std::endl;
    }
    Plato::ParsingException pe(message.str());
    throw pe;
}

/******************************************************************************/
void
MPMD_App::throwParsingException(
  std::string spec, std::string name, 
  const std::map<std::string,DistributedVector*>& fieldMap)
/******************************************************************************/
{
    std::stringstream message;
    message << "Cannot find specified '" << spec << "': " << name << std::endl;
    message << "Fields available: " << std::endl;
    for(auto f : fieldMap){
      message << f.first << std::endl;
    }
    Plato::ParsingException pe(message.str());
    throw pe;
}



/******************************************************************************/
MPMD_App::~MPMD_App()
/******************************************************************************/
{
#ifdef ALBANY_APF
  Albany::APFMeshStruct::finalize_libraries();
#endif

}

/******************************************************************************/
void
tpetraFromThyra(
    const Teuchos::Array<Teuchos::RCP<const Thyra::VectorBase<ST>>> &thyraResponses,
    const Teuchos::Array<Teuchos::Array<Teuchos::RCP<const Thyra::MultiVectorBase<ST>>>> &thyraSensitivities,
    Teuchos::Array<Teuchos::RCP<const Tpetra_Vector>> &responses,
    Teuchos::Array<Teuchos::Array<Teuchos::RCP<const Tpetra_MultiVector>>> &sensitivities) 
/******************************************************************************/
{
  responses.clear();
  responses.reserve(thyraResponses.size());
  typedef Teuchos::Array<Teuchos::RCP<const Thyra::VectorBase<ST>>>
      ThyraResponseArray;
  for (ThyraResponseArray::const_iterator it_begin = thyraResponses.begin(),
                                          it_end = thyraResponses.end(),
                                          it = it_begin;
       it != it_end; ++it) {
    responses.push_back(
        Teuchos::nonnull(*it) ? ConverterT::getConstTpetraVector(*it)
                              : Teuchos::null);
  }

  sensitivities.clear();
  sensitivities.reserve(thyraSensitivities.size());
  typedef Teuchos::Array<
      Teuchos::Array<Teuchos::RCP<const Thyra::MultiVectorBase<ST>>>>
      ThyraSensitivityArray;
  for (ThyraSensitivityArray::const_iterator
           it_begin = thyraSensitivities.begin(),
           it_end = thyraSensitivities.end(), it = it_begin;
       it != it_end; ++it) {
    ThyraSensitivityArray::const_reference sens_thyra = *it;
    Teuchos::Array<Teuchos::RCP<const Tpetra_MultiVector>> sens;
    sens.reserve(sens_thyra.size());
    for (ThyraSensitivityArray::value_type::const_iterator
             jt = sens_thyra.begin(),
             jt_end = sens_thyra.end();
         jt != jt_end; ++jt) {
      sens.push_back(
          Teuchos::nonnull(*jt) ? ConverterT::getConstTpetraMultiVector(*jt)
                                : Teuchos::null);
    }
    sensitivities.push_back(sens);
  }
}