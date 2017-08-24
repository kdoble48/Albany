//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//
#include "MechanicsProblem.hpp"

#include "../utils/SolutionSniffer.hpp"
#include "Albany_BCUtils.hpp"
#include "Albany_ProblemUtils.hpp"
#include "Albany_Utils.hpp"
#include "Albany_MaterialDatabase.hpp"
#include "Albany_MaterialDatabase.hpp"
#include "PHAL_AlbanyTraits.hpp"

void
Albany::MechanicsProblem::getVariableType(
    Teuchos::ParameterList& param_list, const std::string& default_type,
    Albany::MechanicsProblem::MECH_VAR_TYPE& variable_type, bool& have_variable,
    bool& have_equation) {

  std::string
  type = param_list.get("Variable Type", default_type);

  if (type == "None") {
    variable_type = MECH_VAR_TYPE_NONE;
  }
  else if (type == "Constant") {
    variable_type = MECH_VAR_TYPE_CONSTANT;
  }
  else if (type == "DOF") {
    variable_type = MECH_VAR_TYPE_DOF;
  }
  else if (type == "Time Dependent") {
    variable_type = MECH_VAR_TYPE_TIMEDEP;
  }
  else {
    TEUCHOS_TEST_FOR_EXCEPTION(
        true, std::logic_error, "Unknown variable type " << type << '\n');
  }

  have_variable = (variable_type != MECH_VAR_TYPE_NONE);
  have_equation = (variable_type == MECH_VAR_TYPE_DOF);
}
//------------------------------------------------------------------------------
std::string
Albany::MechanicsProblem::variableTypeToString(
    Albany::MechanicsProblem::MECH_VAR_TYPE variable_type) {

  if (variable_type == MECH_VAR_TYPE_NONE) {
    return "None";
  }
  else if (variable_type == MECH_VAR_TYPE_CONSTANT) {
    return "Constant";
  }
  else if (variable_type == MECH_VAR_TYPE_TIMEDEP) {
    return "Time Dependent";
  }

  return "DOF";
}

//------------------------------------------------------------------------------
Albany::MechanicsProblem::MechanicsProblem(
    const Teuchos::RCP<Teuchos::ParameterList>& params,
    const Teuchos::RCP<ParamLib>& param_lib, const int num_dims,
    const Teuchos::RCP<AAdapt::rc::Manager>& rc_mgr,
    Teuchos::RCP<const Teuchos::Comm<int>>& commT)
    : Albany::AbstractProblem(params, param_lib),
      have_source_(false),
      thermal_source_(SOURCE_TYPE_NONE),
      thermal_source_evaluated_(false),
      have_contact_(false),
      num_dims_(num_dims),
      have_mech_eq_(false),
      have_temperature_eq_(false),
      have_dislocation_density_(false),
      have_dislocation_density_eq_(false),
      have_pore_pressure_eq_(false),
      have_transport_eq_(false),
      have_hydrostress_eq_(false),
      have_damage_eq_(false),
      have_stab_pressure_eq_(false),
      have_peridynamics_(false),
      have_topmod_adaptation_(false),
      have_sizefield_adaptation_(false),
      use_sdbcs_(false), 
      rc_mgr_(rc_mgr) {

  std::string &
  method = params->get("Name", "Mechanics ");

  *out << "Problem Name = " << method << '\n';

  std::string &
  sol_method = params->get("Solution Method", "Steady");

  *out << "Solution Method = " << sol_method << '\n';

  if (sol_method == "Transient Tempus") {
    dynamic_tempus_ = true;
  } else {
    dynamic_tempus_ = false;
  }

  // Are any source functions specified?
  have_source_ = params->isSublist("Source Functions");

  // Is contact specified?
  have_contact_ = params->isSublist("Contact");

  // Is adaptation specified?
  bool 
  adapt_sublist_exists = params->isSublist("Adaptation");

  if (adapt_sublist_exists) {
    Teuchos::ParameterList const &
    adapt_params = params->sublist("Adaptation");

    std::string const &
    adaptation_method_name = adapt_params.get<std::string>("Method");

    have_sizefield_adaptation_ = (adaptation_method_name == "RPI Albany Size");
  }

  getVariableType(
      params->sublist("Displacement"), "DOF", mech_type_, have_mech_,
      have_mech_eq_);

  getVariableType(
      params->sublist("Temperature"), "None", temperature_type_,
      have_temperature_, have_temperature_eq_);

  getVariableType(
      params->sublist("DislocationDensity"), "None", dislocation_density_type_,
      have_dislocation_density_, have_dislocation_density_eq_);

  getVariableType(
      params->sublist("Pore Pressure"), "None", pore_pressure_type_,
      have_pore_pressure_, have_pore_pressure_eq_);

  getVariableType(
      params->sublist("Transport"), "None", transport_type_, have_transport_,
      have_transport_eq_);

  getVariableType(
      params->sublist("HydroStress"), "None", hydrostress_type_,
      have_hydrostress_, have_hydrostress_eq_);

  getVariableType(
      params->sublist("Damage"), "None", damage_type_, have_damage_,
      have_damage_eq_);

  getVariableType(
      params->sublist("Stabilized Pressure"), "None", stab_pressure_type_,
      have_stab_pressure_, have_stab_pressure_eq_);

  // Compute number of equations
  int
  num_eq{0};

  if (have_mech_eq_) num_eq += num_dims_;
  if (have_temperature_eq_) num_eq++;
  if (have_dislocation_density_eq_) {
    num_eq += LCM::DislocationDensity::get_num_slip(num_dims_);
  }
  if (have_pore_pressure_eq_) num_eq++;
  if (have_transport_eq_) num_eq++;
  if (have_hydrostress_eq_) num_eq++;
  if (have_damage_eq_) num_eq++;
  if (have_stab_pressure_eq_) num_eq++;

  this->setNumEquations(num_eq);

  // Print out a summary of the problem
  *out << "Mechanics problem:" << '\n'
       << "\tSpatial dimension             : " << num_dims_ << '\n'
       << "\tMechanics variables           : "
       << variableTypeToString(mech_type_) << '\n'
       << "\tTemperature variables         : "
       << variableTypeToString(temperature_type_) << '\n'
       << "\tDislocation Density variables         : "
       << variableTypeToString(dislocation_density_type_) << '\n'
       << "\tPore Pressure variables       : "
       << variableTypeToString(pore_pressure_type_) << '\n'
       << "\tTransport variables           : "
       << variableTypeToString(transport_type_) << '\n'
       << "\tHydroStress variables         : "
       << variableTypeToString(hydrostress_type_) << '\n'
       << "\tDamage variables              : "
       << variableTypeToString(damage_type_) << '\n'
       << "\tStabilized Pressure variables : "
       << variableTypeToString(stab_pressure_type_) << '\n';

  material_db_ = Albany::createMaterialDatabase(params, commT);

  // Determine the Thermal source
  //   - the "Source Functions" list must be present in the input file,
  //   - we must have temperature and have included a temperature equation
  // FIXME: have_temperature_ and have_temperature_eq_ are mutually exclusive - CA
  if (have_source_ && have_temperature_ && have_temperature_eq_) {
    // If a thermal source is specified
    if (params->sublist("Source Functions").isSublist("Thermal Source")) {
      Teuchos::ParameterList& thSrcPL =
          params->sublist("Source Functions").sublist("Thermal Source");

      if (thSrcPL.get<std::string>("Thermal Source Type", "None") ==
          "Block Dependent") {
        if (Teuchos::nonnull(material_db_)) {
          thermal_source_ = SOURCE_TYPE_MATERIAL;
        }
      } else {
        thermal_source_ = SOURCE_TYPE_INPUT;
      }
    }
  }

  // the following function returns the problem information required for
  // setting the rigid body modes (RBMs) for elasticity problems (in
  // src/Albany_SolverFactory.cpp) written by IK, Feb. 2012

  // Need numPDEs should be num_dims_ + nDOF for other governing equations  -SS

  // FIXME: add rigid body modes for dislocation densities -CA
  int const
  num_PDEs = neq;

  int const
  num_elasticity_dims = have_mech_eq_ ? num_dims_ : 0;

  int const
  num_scalar = neq - num_elasticity_dims;

  int
  null_space_dim{0};

  if (have_mech_eq_) {
    switch(num_dims_) {
      case 1: {
        null_space_dim = 1;
        break;
      }
      case 2: {
        null_space_dim = 3;
        break;
      }
      case 3: {
        null_space_dim = 6;
        break;
      }
      default: {
        TEUCHOS_TEST_FOR_EXCEPTION(
            true, std::logic_error,
            '\n' << "Error: " << __FILE__ << " line " << __LINE__
                 << ": num_dims_ set incorrectly." << '\n');
        break;
      }
    }
  }

  rigidBodyModes->setParameters(
      num_PDEs, num_elasticity_dims, num_scalar, null_space_dim);

  // Check whether we are doing adaptive insertion with topology modification.
  bool const
  have_adaptation = params->isSublist("Adaptation");

  if (have_adaptation == true) {
    Teuchos::ParameterList const &
    adapt_params = params->sublist("Adaptation");

    std::string const &
    adaptation_method_name = adapt_params.get<std::string>("Method");

    have_topmod_adaptation_ = adaptation_method_name == "Topmod";
  }

  // User-defined NOX status test that can be passed to the ModelEvaluators
  // This allows a ModelEvaluator to indicate to NOX that something has failed,
  // which is useful for adaptive step size reduction
  if (params->isParameter("Constitutive Model NOX Status Test")) {
    nox_status_test_ = params->get<Teuchos::RCP<NOX::StatusTest::Generic>>(
        "Constitutive Model NOX Status Test");
  } else {
    nox_status_test_ = Teuchos::rcp(new NOX::StatusTest::ModelEvaluatorFlag);
  }

  bool
  requireLatticeOrientationOnMesh = false;

  if (Teuchos::nonnull(material_db_)) {
    std::vector<bool>
    readOrientationFromMesh = material_db_->getAllMatchingParams<bool>(
            "Read Lattice Orientation From Mesh");

    for (unsigned int i = 0; i < readOrientationFromMesh.size(); i++) {
      if (readOrientationFromMesh[i]) {
        requireLatticeOrientationOnMesh = true;
      }
    }
  }
  if (requireLatticeOrientationOnMesh) {
    requirements.push_back("Lattice_Orientation");
  }
}
//------------------------------------------------------------------------------
Albany::MechanicsProblem::~MechanicsProblem() {}
//------------------------------------------------------------------------------
void
Albany::MechanicsProblem::buildProblem(
    Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct>> meshSpecs,
    Albany::StateManager& stateMgr) {
  // Construct All Phalanx Evaluators
  int physSets = meshSpecs.size();
  *out << "Num MeshSpecs: " << physSets << '\n';
  fm.resize(physSets);
  bool haveSidesets = false;

  *out << "Calling MechanicsProblem::buildEvaluators" << '\n';
  for (int ps = 0; ps < physSets; ++ps) {
    fm[ps] = Teuchos::rcp(new PHX::FieldManager<PHAL::AlbanyTraits>);
    buildEvaluators(
        *fm[ps], *meshSpecs[ps], stateMgr, BUILD_RESID_FM, Teuchos::null);
    if (meshSpecs[ps]->ssNames.size() > 0) haveSidesets = true;
  }
  *out << "Calling MechanicsProblem::constructDirichletEvaluators" << '\n';
  constructDirichletEvaluators(*meshSpecs[0]);

  if (haveSidesets) {
    *out << "Calling MechanicsProblem::constructNeumannEvaluators" << '\n';
    constructNeumannEvaluators(meshSpecs[0]);
  }
}
//------------------------------------------------------------------------------
Teuchos::Array<Teuchos::RCP<const PHX::FieldTag>>
Albany::MechanicsProblem::buildEvaluators(
    PHX::FieldManager<PHAL::AlbanyTraits>& fm0,
    const Albany::MeshSpecsStruct& meshSpecs, Albany::StateManager& stateMgr,
    Albany::FieldManagerChoice fmchoice,
    const Teuchos::RCP<Teuchos::ParameterList>& responseList) {

  // Call constructeEvaluators<EvalT>(*rfm[0], *meshSpecs[0], stateMgr);
  // for each EvalT in PHAL::AlbanyTraits::BEvalTypes
  ConstructEvaluatorsOp<MechanicsProblem> op(
      *this, fm0, meshSpecs, stateMgr, fmchoice, responseList);

  Sacado::mpl::for_each<PHAL::AlbanyTraits::BEvalTypes>
  fe(op);

  return *op.tags;
}
//------------------------------------------------------------------------------
void
Albany::MechanicsProblem::constructDirichletEvaluators(
    const Albany::MeshSpecsStruct& meshSpecs) {
  // Construct Dirichlet evaluators for all nodesets and names

  std::vector<std::string>
  dirichletNames(neq);

  int
  index{0};

  if (have_mech_eq_) {
    dirichletNames[index++] = "X";
    if (num_dims_ > 1) dirichletNames[index++] = "Y";
    if (num_dims_ > 2) dirichletNames[index++] = "Z";
  }

  if (have_temperature_eq_) dirichletNames[index++] = "T";

  if (have_dislocation_density_eq_) {
    for (int i{0}; i < LCM::DislocationDensity::get_num_slip(num_dims_); ++i) {
      dirichletNames[index++] = Albany::strint("DD", i, '_');
    }
  }

  if (have_pore_pressure_eq_) dirichletNames[index++] = "P";
  if (have_transport_eq_) dirichletNames[index++] = "C";
  if (have_hydrostress_eq_) dirichletNames[index++] = "TAU";
  if (have_damage_eq_) dirichletNames[index++] = "D";
  if (have_stab_pressure_eq_) dirichletNames[index++] = "SP";

  // Pass on the Application as well that is needed for
  // the coupled Schwarz BC. It is just ignored otherwise.
  Teuchos::RCP<Albany::Application> const &
  application = getApplication();

  this->params->set<Teuchos::RCP<Albany::Application>>(
      "Application", application);

  Albany::BCUtils<Albany::DirichletTraits>
  dirUtils;

  dfm = dirUtils.constructBCEvaluators(
      meshSpecs.nsNames, dirichletNames, this->params, this->paramLib);

  use_sdbcs_ = dirUtils.useSDBCs(); 
  offsets_ = dirUtils.getOffsets();
}
//------------------------------------------------------------------------------
// Neumann BCs
// Traction BCs
void
Albany::MechanicsProblem::constructNeumannEvaluators(
    const Teuchos::RCP<Albany::MeshSpecsStruct>& meshSpecs) {
  // Note: we only enter this function if sidesets are defined in the mesh file
  // i.e. meshSpecs.ssNames.size() > 0

  Albany::BCUtils<Albany::NeumannTraits>
  neuUtils;

  // Check to make sure that Neumann BCs are given in the input file
  if (!neuUtils.haveBCSpecified(this->params)) { 
    return;
  }

  // Construct BC evaluators for all side sets and names
  // Note that the string index sets up the equation offset,
  // so ordering is important

  std::vector<std::string>
  neumannNames(neq + 1);

  // Last entry specifies behavior for setting NBC on "DOF all"
  // By "all", we mean components of the traction vector only
  // Other fields cannot use this specifier
  neumannNames[neq] = "all";

  Teuchos::Array<Teuchos::Array<int>>
  offsets;

  offsets.resize(neq + 1);

  int
  index{0};

  if (have_mech_eq_)
  {
    // There are num_dims_ components of the traction vector, so set accordingly
    offsets[neq].resize(num_dims_);
    for (int i{0}; i < num_dims_; ++i) {
      offsets[neq][i] = i;
    }

    // Components of the traction vector
    char
    components[] = {'x', 'y', 'z'};

    while (index < num_dims_) {
      std::ostringstream sstream;
      sstream << "sig_" << components[index];
      neumannNames[index].assign(sstream.str());
      offsets[index].resize(1);
      offsets[index][0] = index;
      index++;
    }
  }

  if (have_temperature_eq_) {
    neumannNames[index] = "T";
    offsets[index].resize(1);
    offsets[index++][0] = 1;
  }

  if (have_dislocation_density_eq_) {
    for (int i{0}; i < LCM::DislocationDensity::get_num_slip(num_dims_); ++i) { 
      neumannNames[index] = Albany::strint("DF", i, '_');
      offsets[index].resize(1);
      offsets[index++][0] = i;
    }
  }


  // Construct BC evaluators for all possible names of conditions
  // Should only specify flux vector components (dudx, dudy, dudz),
  // or dudn, not both

  Teuchos::ArrayRCP<std::string>
  dof_names(1);

  dof_names[0] = "Displacement";

  std::vector<std::string>
  condNames(3);  // dudx, dudy, dudz, dudn, P

  // Note that sidesets are only supported for two and 3D currently
  if (num_dims_ == 2) {
    condNames[0] = "(t_x, t_y)";
  }
  else if (num_dims_ == 3) {
    condNames[0] = "(t_x, t_y, t_z)";
  }
  else {
    TEUCHOS_TEST_FOR_EXCEPTION(
        true, Teuchos::Exceptions::InvalidParameter,
        '\n' << "Error: Sidesets only supported in 2 and 3D." << '\n');
  }

  condNames[1] = "dudn";
  condNames[2] = "P";

  // FIXME: The resize below assumes a single element block
  nfm.resize(1);

  nfm[0] = neuUtils.constructBCEvaluators(
      meshSpecs,
      neumannNames,
      dof_names,
      true,  // isVectorField
      0,     // offsetToFirstDOF
      condNames,
      offsets,
      dl_,
      this->params,
      this->paramLib);
}

//------------------------------------------------------------------------------
Teuchos::RCP<const Teuchos::ParameterList>
Albany::MechanicsProblem::getValidProblemParameters() const {

  Teuchos::RCP<Teuchos::ParameterList>
  validPL = this->getGenericProblemParams("ValidMechanicsProblemParams");

  validPL->set<bool>(
      "Register dirichlet_field", true, "Flag to register dirichlet_field");

  validPL->set<std::string>(
      "MaterialDB Filename", "materials.xml",
      "Filename of material database xml file");

  validPL->sublist("Displacement", false, "");
  validPL->sublist("Temperature", false, "");
  validPL->sublist("DislocationDensity", false, "");
  validPL->sublist("Pore Pressure", false, "");
  validPL->sublist("Transport", false, "");
  validPL->sublist("HydroStress", false, "");
  validPL->sublist("Damage", false, "");
  validPL->sublist("Stabilized Pressure", false, "");

  return validPL;
}

//------------------------------------------------------------------------------
void
Albany::MechanicsProblem::getAllocatedStates(
    Teuchos::ArrayRCP<Teuchos::ArrayRCP<
        Teuchos::RCP<Kokkos::DynRankView<RealType, PHX::Device>>>>
        old_state,
    Teuchos::ArrayRCP<Teuchos::ArrayRCP<
        Teuchos::RCP<Kokkos::DynRankView<RealType, PHX::Device>>>>
        new_state) const {
  old_state = old_state_;
  new_state = new_state_;
}

//------------------------------------------------------------------------------
void
Albany::MechanicsProblem::applyProblemSpecificSolverSettings(
    Teuchos::RCP<Teuchos::ParameterList> params)
{
  // Acquire the NOX "Solver Options" and "Status Tests" parameter lists
  Teuchos::RCP<Teuchos::ParameterList>
  solver_opts_params;

  Teuchos::RCP<Teuchos::ParameterList>
  status_tests_params;

  bool
  have_solver_opts{false};

  bool
  have_status_test{false};

  if (params->isSublist("Piro")) {
    if (params->sublist("Piro").isSublist("NOX")) {
      if (params->sublist("Piro").sublist("NOX").isSublist("Solver Options")) {
        have_solver_opts = true;
      }
      if (params->sublist("Piro").sublist("NOX").isSublist("Status Tests")) {
        have_status_test = true;
      }
    }
  }

  if (have_solver_opts && have_status_test) {
    // Add the model evaulator flag as a status test.
    Teuchos::ParameterList &
    solver_opts_params =
        params->sublist("Piro").sublist("NOX").sublist("Solver Options");

    Teuchos::ParameterList &
    status_tests_params =
        params->sublist("Piro").sublist("NOX").sublist("Status Tests");

    Teuchos::ParameterList
    old_params = status_tests_params;

    Teuchos::ParameterList
    new_params;

    new_params.set<std::string>("Test Type", "Combo");
    new_params.set<std::string>("Combo Type", "OR");
    new_params.set<int>("Number of Tests", 2);
    new_params.sublist("Test 0");
    new_params.sublist("Test 0").set("Test Type", "User Defined");
    new_params.sublist("Test 0").set("User Status Test", nox_status_test_);
    new_params.sublist("Test 1") = old_params;

    status_tests_params = new_params;

    // Create a NOX observer that will reset the status flag at the beginning of
    // a nonlinear solve if one does not exist already
    std::string const
    ppo_str{"User Defined Pre/Post Operator"};

    bool const
    have_ppo = solver_opts_params.isParameter(ppo_str);

    Teuchos::RCP<NOX::Abstract::PrePostOperator>
    ppo{Teuchos::null};

    if (have_ppo == true) {
      ppo = solver_opts_params.get<decltype(ppo)>(ppo_str);
    } else {
      ppo = Teuchos::rcp(new LCM::SolutionSniffer);
      solver_opts_params.set(ppo_str, ppo);
      ALBANY_ASSERT(solver_opts_params.isParameter(ppo_str) == true);
    }

    constexpr bool
    throw_on_fail{true};

    Teuchos::RCP<LCM::SolutionSniffer>
    status_test_op =Teuchos::rcp_dynamic_cast<LCM::SolutionSniffer>
    (ppo, throw_on_fail);

    Teuchos::RCP<NOX::StatusTest::ModelEvaluatorFlag>
    status_test =
        Teuchos::rcp_dynamic_cast<NOX::StatusTest::ModelEvaluatorFlag>(
            nox_status_test_);

    status_test_op->setStatusTest(status_test);
  }
}
