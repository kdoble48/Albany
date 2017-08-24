//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

// ===================================================
//! Includes
// ===================================================

#include "Interface.hpp"
#include "Albany_MpasSTKMeshStruct.hpp"
#include "Teuchos_ParameterList.hpp"
#include "Teuchos_RCP.hpp"
#include "Albany_Utils.hpp"
#include "Albany_SolverFactory.hpp"
#include "Teuchos_XMLParameterListHelpers.hpp"
#include "Teuchos_StandardCatchMacros.hpp"
#include <stk_mesh/base/FieldBase.hpp>
#include <stk_mesh/base/GetEntities.hpp>
#include "Piro_PerformSolve.hpp"
#include "Albany_OrdinarySTKFieldContainer.hpp"
#include "Albany_STKDiscretization.hpp"

#ifdef ALBANY_SEACAS
#include <stk_io/IossBridge.hpp>
#include <stk_io/StkMeshIoBroker.hpp>
#include <Ionit_Initializer.h>
#endif

Teuchos::RCP<Albany::MpasSTKMeshStruct> meshStruct;
Teuchos::RCP<Albany::Application> albanyApp;
Teuchos::RCP<Teuchos::ParameterList> paramList;
Teuchos::RCP<const Teuchos_Comm> mpiCommT;
Teuchos::RCP<Teuchos::ParameterList> discParams;
Teuchos::RCP<Albany::SolverFactory> slvrfctry;
Teuchos::RCP<double> MPAS_dt;

double MPAS_gravity(9.8), MPAS_rho_ice(910.0), MPAS_rho_seawater(1028.0), MPAS_sea_level(0),
       MPAS_flowParamA(1e-4), MPAS_enhancementFactor(1.0), MPAS_flowLawExponent(3), MPAS_dynamic_thickness(1e-2),
       MPAS_ClausiusClapeyoronCoeff(9.7546e-8);
bool MPAS_useGLP(true);

#ifdef MPAS_USE_EPETRA
  Teuchos::RCP<Thyra::ModelEvaluator<double> > solver;
  bool TpetraBuild = false;
#else
  Teuchos::RCP<Thyra::ResponseOnlyModelEvaluatorBase<double> > solver;
  bool TpetraBuild = true;
#endif
bool keptMesh =false;

typedef struct TET_ {
  int verts[4];
  int neighbours[4];
  char bound_type[4];
} TET;

/***********************************************************/


void velocity_solver_solve_fo(int nLayers, int nGlobalVertices,
    int nGlobalTriangles, bool ordering, bool first_time_step,
    const std::vector<int>& indexToVertexID,
    const std::vector<int>& indexToTriangleID, double minBeta,
    const std::vector<double>& regulThk,
    const std::vector<double>& levelsNormalizedThickness,
    const std::vector<double>& elevationData,
    const std::vector<double>& thicknessData,
    const std::vector<double>& betaData,
    const std::vector<double>& bedTopographyData,
    const std::vector<double>& smbData,
    const std::vector<double>& temperatureOnTetra,
    std::vector<double>& dissipationHeatOnTetra,
    std::vector<double>& velocityOnVertices,
    int& error,
    const double& deltat) {

  int numVertices3D = (nLayers + 1) * indexToVertexID.size();
  int numPrisms = nLayers * indexToTriangleID.size();
  int vertexColumnShift = (ordering == 1) ? 1 : nGlobalVertices;
  int lVertexColumnShift = (ordering == 1) ? 1 : indexToVertexID.size();
  int vertexLayerShift = (ordering == 0) ? 1 : nLayers + 1;

  int elemColumnShift = (ordering == 1) ? 3 : 3 * nGlobalTriangles;
  int lElemColumnShift = (ordering == 1) ? 3 : 3 * indexToTriangleID.size();
  int elemLayerShift = (ordering == 0) ? 3 : 3 * nLayers;

  int neq = meshStruct->neq;

  const bool interleavedOrdering = meshStruct->getInterleavedOrdering();

  *MPAS_dt =  deltat;

  Teuchos::ArrayRCP<double>& layerThicknessRatio = meshStruct->layered_mesh_numbering->layers_ratio;
  for (int i = 0; i < nLayers; i++) {
    layerThicknessRatio[i] = levelsNormalizedThickness[i+1]-levelsNormalizedThickness[i];
  }

  typedef Albany::AbstractSTKFieldContainer::VectorFieldType VectorFieldType;
  typedef Albany::AbstractSTKFieldContainer::ScalarFieldType ScalarFieldType;

  VectorFieldType* solutionField;

  if (interleavedOrdering)
    solutionField = Teuchos::rcp_dynamic_cast<
        Albany::OrdinarySTKFieldContainer<true> >(
        meshStruct->getFieldContainer())->getSolutionField();
  else
    solutionField = Teuchos::rcp_dynamic_cast<
        Albany::OrdinarySTKFieldContainer<false> >(
        meshStruct->getFieldContainer())->getSolutionField();

  ScalarFieldType* surfaceHeightField = meshStruct->metaData->get_field <ScalarFieldType> (stk::topology::NODE_RANK, "surface_height");
  ScalarFieldType* thicknessField = meshStruct->metaData->get_field <ScalarFieldType> (stk::topology::NODE_RANK, "ice_thickness");
  ScalarFieldType* bedTopographyField = meshStruct->metaData->get_field <ScalarFieldType> (stk::topology::NODE_RANK, "bed_topography");
  ScalarFieldType* smbField = meshStruct->metaData->get_field <ScalarFieldType> (stk::topology::NODE_RANK, "surface_mass_balance");
  VectorFieldType* dirichletField = meshStruct->metaData->get_field <VectorFieldType> (stk::topology::NODE_RANK, "dirichlet_field");
  ScalarFieldType* basalFrictionField = meshStruct->metaData->get_field <ScalarFieldType> (stk::topology::NODE_RANK, "basal_friction");

  for (UInt j = 0; j < numVertices3D; ++j) {
    int ib = (ordering == 0) * (j % lVertexColumnShift)
        + (ordering == 1) * (j / vertexLayerShift);
    int il = (ordering == 0) * (j / lVertexColumnShift)
        + (ordering == 1) * (j % vertexLayerShift);
    int gId = il * vertexColumnShift + vertexLayerShift * indexToVertexID[ib];
    stk::mesh::Entity node = meshStruct->bulkData->get_entity(stk::topology::NODE_RANK, gId + 1);
    double* coord = stk::mesh::field_data(*meshStruct->getCoordinatesField(), node);
    coord[2] = elevationData[ib] - levelsNormalizedThickness[nLayers - il] * thicknessData[ib];


    double* thickness = stk::mesh::field_data(*thicknessField, node);
    thickness[0] = thicknessData[ib];
    double* sHeight = stk::mesh::field_data(*surfaceHeightField, node);
    sHeight[0] = elevationData[ib];
    double* bedTopography = stk::mesh::field_data(*bedTopographyField, node);
    bedTopography[0] = bedTopographyData[ib];
    if(smbField != NULL) {
      double* smb = stk::mesh::field_data(*smbField, node);
      smb[0] = smbData[ib];
    }
    double* sol = stk::mesh::field_data(*solutionField, node);
    sol[0] = velocityOnVertices[j];
    sol[1] = velocityOnVertices[j + numVertices3D];
    if(neq==3) {
      sol[2] = thicknessData[ib];
    }
    double* dirichletVel = stk::mesh::field_data(*dirichletField, node);
    dirichletVel[0]=velocityOnVertices[j]; //velocityOnVertices stores initial guess and dirichlet velocities.
    dirichletVel[1]=velocityOnVertices[j + numVertices3D];
    if (il == 0) {
      double* beta = stk::mesh::field_data(*basalFrictionField, node);
      beta[0] = std::max(betaData[ib], minBeta);
    }
  }

  ScalarFieldType* temperature_field = meshStruct->metaData->get_field<ScalarFieldType>(stk::topology::ELEMENT_RANK, "temperature");

  for (UInt j = 0; j < numPrisms; ++j) {
    int ib = (ordering == 0) * (j % (lElemColumnShift / 3))
        + (ordering == 1) * (j / (elemLayerShift / 3));
    int il = (ordering == 0) * (j / (lElemColumnShift / 3))
        + (ordering == 1) * (j % (elemLayerShift / 3));
    int gId = il * elemColumnShift + elemLayerShift * indexToTriangleID[ib];
    int lId = il * lElemColumnShift + elemLayerShift * ib;
    for (int iTetra = 0; iTetra < 3; iTetra++) {
      stk::mesh::Entity elem = meshStruct->bulkData->get_entity(stk::topology::ELEMENT_RANK, ++gId);
      double* temperature = stk::mesh::field_data(*temperature_field, elem);
      temperature[0] = temperatureOnTetra[lId++];
    }
  }

  meshStruct->setHasRestartSolution(true);//!first_time_step);

  if (!first_time_step) {
    meshStruct->setRestartDataTime(
        paramList->sublist("Problem").get("Homotopy Restart Step", 1.));
    double homotopy =
        paramList->sublist("Problem").sublist("FELIX Viscosity").get(
            "Glen's Law Homotopy Parameter", 1.0);
    if (meshStruct->restartDataTime() == homotopy) {
      paramList->sublist("Problem").set("Solution Method", "Steady");
      paramList->sublist("Piro").set("Solver Type", "NOX");
    }
  }



  if(!keptMesh) {
    albanyApp->createDiscretization();
  } else {
    auto abs_disc = albanyApp->getDiscretization();
    auto stk_disc = Teuchos::rcp_dynamic_cast<Albany::STKDiscretization>(abs_disc);
    stk_disc->updateMesh();
  }
  albanyApp->finalSetUp(paramList);

  bool success = true;
  Teuchos::ArrayRCP<const ST> solution_constView;
  Teuchos::RCP<const Tpetra_Map> overlapMap;
  try {
#ifdef MPAS_USE_EPETRA
  solver = slvrfctry->createThyraSolverAndGetAlbanyApp(albanyApp, mpiCommT, mpiCommT, Teuchos::null, false);
#else
   solver = slvrfctry->createAndGetAlbanyAppT(albanyApp, mpiCommT, mpiCommT, Teuchos::null, false);
#endif

  Teuchos::ParameterList solveParams;
  solveParams.set("Compute Sensitivities", false);

  Teuchos::Array<Teuchos::RCP<const Thyra::VectorBase<double> > > thyraResponses;
  Teuchos::Array<
      Teuchos::Array<Teuchos::RCP<const Thyra::MultiVectorBase<double> > > > thyraSensitivities;
  Piro::PerformSolveBase(*solver, solveParams, thyraResponses,
      thyraSensitivities);

  overlapMap = albanyApp->getDiscretization()->getOverlapMapT();
  Teuchos::RCP<Tpetra_Import> import = Teuchos::rcp(new Tpetra_Import(albanyApp->getDiscretization()->getMapT(), overlapMap));
  Teuchos::RCP<Tpetra_Vector> solution = Teuchos::rcp(new Tpetra_Vector(overlapMap));
  solution->doImport(*albanyApp->getDiscretization()->getSolutionFieldT(), *import, Tpetra::INSERT);
  solution_constView = solution->get1dView();
  }
  TEUCHOS_STANDARD_CATCH_STATEMENTS(true, std::cerr, success);

  error = !success;


  for (UInt j = 0; j < numVertices3D; ++j) {
    int ib = (ordering == 0) * (j % lVertexColumnShift)
        + (ordering == 1) * (j / vertexLayerShift);
    int il = (ordering == 0) * (j / lVertexColumnShift)
        + (ordering == 1) * (j % vertexLayerShift);
    int gId = il * vertexColumnShift + vertexLayerShift * indexToVertexID[ib];

    int lId0, lId1;

    if (interleavedOrdering) {
      lId0 = overlapMap->getLocalElement(neq * gId);
      lId1 = lId0 + 1;
    } else {
      lId0 = overlapMap->getLocalElement(gId);
      lId1 = lId0 + numVertices3D;
    }
    velocityOnVertices[j] = solution_constView[lId0];
    velocityOnVertices[j + numVertices3D] = solution_constView[lId1];
  }


  ScalarFieldType* dissipationHeatField = meshStruct->metaData->get_field <ScalarFieldType> (stk::topology::ELEMENT_RANK, "dissipation_heat");
  for (UInt j = 0; j < numPrisms; ++j) {
    int ib = (ordering == 0) * (j % (lElemColumnShift / 3))
        + (ordering == 1) * (j / (elemLayerShift / 3));
    int il = (ordering == 0) * (j / (lElemColumnShift / 3))
        + (ordering == 1) * (j % (elemLayerShift / 3));
    int gId = il * elemColumnShift + elemLayerShift * indexToTriangleID[ib];
    int lId = il * lElemColumnShift + elemLayerShift * ib;
    for (int iTetra = 0; iTetra < 3; iTetra++) {
      stk::mesh::Entity elem = meshStruct->bulkData->get_entity(stk::topology::ELEMENT_RANK, ++gId);
      double* dissipationHeat = stk::mesh::field_data(*dissipationHeatField, elem);
      dissipationHeatOnTetra[lId++] = dissipationHeat[0];
    }
  }

  keptMesh = true;

  //UInt componentGlobalLength = (nLayers+1)*nGlobalVertices; //mesh3DPtr->numGlobalVertices();
}

void velocity_solver_export_fo_velocity(MPI_Comm reducedComm) {
#ifdef ALBANY_SEACAS
  Teuchos::RCP<stk::io::StkMeshIoBroker> mesh_data = Teuchos::rcp(new stk::io::StkMeshIoBroker(reducedComm));
    mesh_data->set_bulk_data(*meshStruct->bulkData);
    size_t idx = mesh_data->create_output_mesh("IceSheet.exo", stk::io::WRITE_RESULTS);
    mesh_data->process_output_request(idx, 0.0);
#endif
}

void velocity_solver_finalize() {
}

/*duality:
 *
 *   mpas(F) |  lifev
 *  ---------|---------
 *   cell    |  vertex
 *   vertex  |  triangle
 *   edge    |  edge
 *
 */

void velocity_solver_compute_2d_grid(MPI_Comm reducedComm) {
  keptMesh = false;
  mpiCommT = Albany::createTeuchosCommFromMpiComm(reducedComm);
}

void velocity_solver_set_physical_parameters(double const& gravity, double const& ice_density, double const& ocean_density, double const& sea_level, double const& flowParamA,
                                             double const& enhancementFactor, double const& flowLawExponent, double const& dynamic_thickness, bool const& use_GLP, double const& clausiusClapeyoronCoeff) {
  MPAS_gravity=gravity;
  MPAS_rho_ice = ice_density;
  MPAS_rho_seawater = ocean_density;
  MPAS_sea_level = sea_level;
  MPAS_flowParamA = flowParamA;
  MPAS_enhancementFactor = enhancementFactor;
  MPAS_flowLawExponent = flowLawExponent;
  MPAS_dynamic_thickness = dynamic_thickness;
  MPAS_useGLP = use_GLP;
  MPAS_ClausiusClapeyoronCoeff = clausiusClapeyoronCoeff;
}

void velocity_solver_extrude_3d_grid(int nLayers, int nGlobalTriangles,
    int nGlobalVertices, int nGlobalEdges, int Ordering, MPI_Comm reducedComm,
    const std::vector<int>& indexToVertexID,
    const std::vector<int>& mpasIndexToVertexID,
    const std::vector<double>& verticesCoords,
    const std::vector<bool>& isVertexBoundary,
    const std::vector<int>& verticesOnTria,
    const std::vector<bool>& isBoundaryEdge,
    const std::vector<int>& trianglesOnEdge,
    const std::vector<int>& trianglesPositionsOnEdge,
    const std::vector<int>& verticesOnEdge,
    const std::vector<int>& indexToEdgeID,
    const std::vector<GO>& indexToTriangleID,
    const std::vector<int>& dirichletNodesIds,
    const std::vector<int>& floating2dEdgesIds) {

  slvrfctry = Teuchos::rcp(
      new Albany::SolverFactory("albany_input.xml", mpiCommT));
  paramList = Teuchos::rcp(&slvrfctry->getParameters(), false);

  Teuchos::Array<std::string> arrayRequiredFields(7); 
  arrayRequiredFields[0]="temperature";  arrayRequiredFields[1]="ice_thickness"; arrayRequiredFields[2]="surface_height"; arrayRequiredFields[3]="bed_topography";
  arrayRequiredFields[4]="basal_friction";  arrayRequiredFields[5]="surface_mass_balance"; arrayRequiredFields[6]="dirichlet_field";
  paramList->sublist("Problem").set("Required Fields", arrayRequiredFields);

  //Physical Parameters
  if(paramList->sublist("Problem").isSublist("FELIX Physical Parameters")) {
    std::cout<<"\nWARNING: Using Physical Parameters (gravity, ice/ocean densities) provided in Albany input file. In order to use those provided by MPAS, remove \"FELIX Physical Parameters\" sublist from Albany input file.\n"<<std::endl;
  }
 
  Teuchos::ParameterList& physParamList = paramList->sublist("Problem").sublist("FELIX Physical Parameters");
 
  double rho_ice, rho_seawater; 
  physParamList.set("Gravity Acceleration", physParamList.get("Gravity Acceleration", MPAS_gravity));
  physParamList.set("Ice Density", rho_ice = physParamList.get("Ice Density", MPAS_rho_ice));
  physParamList.set("Water Density", rho_seawater = physParamList.get("Water Density", MPAS_rho_seawater));
  physParamList.set("Clausius-Clapeyron coefficient", physParamList.get("Clausius-Clapeyron coefficient", MPAS_ClausiusClapeyoronCoeff));
  physParamList.set<bool>("Use GLP", physParamList.get("Use GLP", MPAS_useGLP)); //use GLP (Grounding line parametrization) unless actively disabled
  
  paramList->sublist("Problem").set("Name", paramList->sublist("Problem").get("Name", "FELIX Stokes First Order 3D"));

  MPAS_dt = Teuchos::rcp(new double(0.0));
  if (paramList->sublist("Problem").get<std::string>("Name") == "FELIX Coupled FO H 3D") {
    paramList->sublist("Problem").sublist("Parameter Fields").set("Register Surface Mass Balance", 1);
    *MPAS_dt = paramList->sublist("Problem").get("Time Step", 0.0);
    paramList->sublist("Problem").set("Time Step Ptr", MPAS_dt); //if it is not there set it to zero.
  }

  Teuchos::ParameterList& neumannBcList = paramList->sublist("Problem").sublist("Neumann BCs");
  std::string neumannStr = neumannBcList.currentParametersString();
  int cub_degree = physParamList.get<bool>("Use GLP") ? 8 : 3;
  if(neumannBcList.isParameter("Cubature Degree"))
    std::cout<<"\nWARNING: Using Cubature Degeree of Neumann BCs provided in Albany input file. In order to use boundary conditions provided by MPAS, remove \"Neumann BCs\" sublist from Albany input file.\n"<<std::endl;

  neumannBcList.set("Cubature Degree", neumannBcList.get("Cubature Degree", cub_degree));

  if (neumannStr.find("NBC on SS") == std::string::npos) {
    //Teuchos::RCP<Teuchos::Array<double> >inputArrayBasal = Teuchos::rcp(new Teuchos::Array<double> (5, 0.0));
    //neumannBcList.set("NBC on SS basalside for DOF all set basal", neumannBcList.get("NBC on SS basalside for DOF all set basal", *inputArrayBasal));
    //neumannBcList.set("BetaXY", neumannBcList.get("BetaXY", "Scalar Field"));

    //Lateral floating ice BCs
    Teuchos::RCP<Teuchos::Array<double> >inputArrayLateral = Teuchos::rcp(new Teuchos::Array<double> (1, rho_ice/rho_seawater));
    neumannBcList.set("NBC on SS floatinglateralside for DOF all set lateral", neumannBcList.get("NBC on SS floatinglateralside for DOF all set lateral", *inputArrayLateral));
  }
  else {
    std::cout<<"\nWARNING: Using basal and floating Neumann BCs options provided in Albany input file. In order to use boundary conditions provided by MPAS, remove \"Neumann BCs\" sublist from Albany input file.\n"<<std::endl;
  }

  //! temporary fix: basal friction needs to be a distributed parameter
    paramList->sublist("Problem").sublist("Distributed Parameters").set("Number of Parameter Vectors",1);
    paramList->sublist("Problem").sublist("Distributed Parameters").sublist("Distributed Parameter 0").set("Name", "basal_friction");


    paramList->sublist("Problem").set("Basal Side Name", "basalside");
    if (!paramList->sublist("Problem").isSublist("FELIX Basal Friction Coefficient")) {
      paramList->sublist("Problem").sublist("FELIX Basal Friction Coefficient").set("Type","Given Field");
    } else {
      std::cout<<"\nWARNING: Using Basal Friction Coefficient provided in Albany input file. In order to use boundary conditions provided by MPAS, remove \"Neumann BCs\" sublist from Albany input file.\n"<<std::endl;
    }

  //Dirichlet BCs
  if(!paramList->sublist("Problem").isSublist("Dirichlet BCs")) {
    paramList->sublist("Problem").sublist("Dirichlet BCs").set("DBC on NS dirichlet for DOF U0 prescribe Field", "dirichlet_field");
    paramList->sublist("Problem").sublist("Dirichlet BCs").set("DBC on NS dirichlet for DOF U1 prescribe Field", "dirichlet_field");
  }
  else {
    std::cout<<"\nWARNING: Using Dirichlet BCs options provided in Albany input file. In order to use those provided by MPAS, remove \"Dirichlet BCs\" sublist from Albany input file.\n"<<std::endl;
  }


  if(paramList->sublist("Problem").isSublist("FELIX Viscosity"))
    std::cout<<"\nWARNING: Using Viscosity options provided in Albany input file. In order to use those provided by MPAS, remove \"FELIX Viscosity\" sublist from Albany input file.\n"<<std::endl;

  Teuchos::ParameterList& viscosityList =  paramList->sublist("Problem").sublist("FELIX Viscosity"); //empty list if FELIXViscosity not in input file.

  viscosityList.set("Type", viscosityList.get("Type", "Glen's Law"));
  viscosityList.set("Glen's Law Homotopy Parameter", viscosityList.get("Glen's Law Homotopy Parameter", 1e-6));
  viscosityList.set("Glen's Law A", viscosityList.get("Glen's Law A", MPAS_flowParamA));
  viscosityList.set("Glen's Law n", viscosityList.get("Glen's Law n",  MPAS_flowLawExponent));
  viscosityList.set("Flow Rate Type", viscosityList.get("Flow Rate Type", "Temperature Based"));
  viscosityList.set("Extract Strain Rate Sq", viscosityList.get("Extract Strain Rate Sq", true)); //set true if not defined


  paramList->sublist("Problem").sublist("Body Force").set("Type", "FO INTERP SURF GRAD");
  

  Teuchos::ParameterList& discretizationList = paramList->sublist("Discretization");
  
  discretizationList.set("Method", discretizationList.get("Method", "Extruded")); //set to Extruded is not defined
  discretizationList.set("Cubature Degree", discretizationList.get("Cubature Degree", 1));  //set 1 if not defined
  discretizationList.set("Interleaved Ordering", discretizationList.get("Interleaved Ordering", true));  //set true if not define
  
  discretizationList.sublist("Required Fields Info").set<int>("Number Of Fields",7);
  Teuchos::ParameterList& field0 = discretizationList.sublist("Required Fields Info").sublist("Field 0");
  Teuchos::ParameterList& field1 = discretizationList.sublist("Required Fields Info").sublist("Field 1");
  Teuchos::ParameterList& field2 = discretizationList.sublist("Required Fields Info").sublist("Field 2");
  Teuchos::ParameterList& field3 = discretizationList.sublist("Required Fields Info").sublist("Field 3");
  Teuchos::ParameterList& field4 = discretizationList.sublist("Required Fields Info").sublist("Field 4");
  Teuchos::ParameterList& field5 = discretizationList.sublist("Required Fields Info").sublist("Field 5");
  Teuchos::ParameterList& field6 = discretizationList.sublist("Required Fields Info").sublist("Field 6");

  //set temperature
  field0.set<std::string>("Field Name", "temperature");
  field0.set<std::string>("Field Type", "Elem Scalar");
  field0.set<std::string>("Field Origin", "Mesh");

  //set ice thickness
  field1.set<std::string>("Field Name", "ice_thickness");
  field1.set<std::string>("Field Type", "Node Scalar");
  field1.set<std::string>("Field Origin", "Mesh");

  //set surface_height
  field2.set<std::string>("Field Name", "surface_height");
  field2.set<std::string>("Field Type", "Node Scalar");
  field2.set<std::string>("Field Origin", "Mesh");

  //set bed topography
  field3.set<std::string>("Field Name", "bed_topography");
  field3.set<std::string>("Field Type", "Node Scalar");
  field3.set<std::string>("Field Origin", "Mesh");

  //set basal friction
  field4.set<std::string>("Field Name", "basal_friction");
  field4.set<std::string>("Field Type", "Node Scalar");
  field4.set<std::string>("Field Origin", "Mesh");

  //set surface mass balance
  field5.set<std::string>("Field Name", "surface_mass_balance");
  field5.set<std::string>("Field Type", "Node Scalar");
  field5.set<std::string>("Field Origin", "Mesh");

  //set dirichlet field
  field6.set<std::string>("Field Name", "dirichlet_field");
  field6.set<std::string>("Field Type", "Node Vector");
  field6.set<std::string>("Field Origin", "Mesh");

  discParams = Teuchos::sublist(paramList, "Discretization", true);


  //discParams>setSublist("Side Set Discretizations");
  Teuchos::RCP<Teuchos::Array<std::string> > sideSetsArray = Teuchos::rcp(new Teuchos::Array<std::string> (1, "basalside"));
  discParams->sublist("Side Set Discretizations").set("Side Sets", *sideSetsArray);
  discParams->sublist("Side Set Discretizations").sublist("basalside").set("Method", "SideSetSTK");


  Albany::AbstractFieldContainer::FieldContainerRequirements req;
  albanyApp = Teuchos::rcp(new Albany::Application(mpiCommT));
  albanyApp->initialSetUp(paramList);

  int neq = (paramList->sublist("Problem").get<std::string>("Name") == "FELIX Coupled FO H 3D") ? 3 : 2;

  meshStruct = Teuchos::rcp(
      new Albany::MpasSTKMeshStruct(discParams, mpiCommT, indexToTriangleID,
          nGlobalTriangles, nLayers, Ordering));
  albanyApp->createMeshSpecs(meshStruct);

  albanyApp->buildProblem();

  meshStruct->constructMesh(mpiCommT, discParams, neq, req,
      albanyApp->getStateMgr().getStateInfoStruct(), indexToVertexID,
      mpasIndexToVertexID, verticesCoords, isVertexBoundary, nGlobalVertices,
      verticesOnTria, isBoundaryEdge, trianglesOnEdge, trianglesPositionsOnEdge,
      verticesOnEdge, indexToEdgeID, nGlobalEdges, indexToTriangleID,
      dirichletNodesIds, floating2dEdgesIds,
      meshStruct->getMeshSpecs()[0]->worksetSize, nLayers, Ordering);
}
//}

