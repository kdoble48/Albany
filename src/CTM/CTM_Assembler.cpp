#include "CTM_Assembler.hpp"
#include "CTM_SolutionInfo.hpp"

namespace CTM {

using Teuchos::rcpFromRef;

Assembler::Assembler(
    RCP<SolutionInfo> s_info,
    RCP<Albany::AbstractProblem> prob,
    RCP<Albany::AbstractDiscretization> d,
    RCP<Albany::StateManager> sm) {
  neq = disc->getNumEq();
}

void Assembler::loadWorksetBucketInfo(PHAL::Workset& workset, const int ws) {

  // get discretization data
  auto wsElNodeEqID = disc->getWsElNodeEqID();
  auto wsElNodeID = disc->getWsElNodeID();
  auto coords = disc->getCoords();
  auto wsEBNames = disc->getWsEBNames();

  // populate workset info
  workset.numCells = wsElNodeEqID[ws].size();
  workset.wsElNodeEqID = wsElNodeEqID[ws];
  workset.wsElNodeID = wsElNodeID[ws];
  workset.wsCoords = coords[ws];
  workset.EBName = wsEBNames[ws];
  workset.wsIndex = ws;
  workset.local_Vp.resize(workset.numCells);
  loadWorksetSidesetInfo(workset, ws);
  workset.stateArrayPtr =
    &(state_mgr->getStateArray(Albany::StateManager::ELEM, ws));

  // kokkos views
  Kokkos::View<int***, PHX::Device> wsElNodeEqID_kokkos(
      "wsElNodeEqID_kokkos",
      workset.numCells, wsElNodeEqID[ws][0].size(),
      wsElNodeEqID[ws][0][0].size());
  workset.wsElNodeEqID_kokkos = wsElNodeEqID_kokkos;
  for (int i = 0; i < workset.numCells; ++i)
  for (int j = 0; j < wsElNodeEqID[ws][0].size(); ++j)
  for (int k = 0; k < wsElNodeEqID[ws][0][0].size(); ++k)
    workset.wsElNodeEqID_kokkos(i, j, k) = workset.wsElNodeEqID[i][j][k];
}

void Assembler::loadWorksetSidesetInfo(PHAL::Workset& workset, const int ws) {
  workset.sideSets = rcpFromRef(disc->getSideSets(ws));
}

void Assembler::loadBasicWorksetInfo(
    PHAL::Workset& workset, const double t_new, const double t_old) {
  workset.numEqs = neq;
  workset.xT = sol_info->ghost->x;
  workset.xdotT = sol_info->ghost->x_dot;
  workset.xdotdotT = Teuchos::null;
  workset.current_time = t_new;
  workset.previous_time = t_old;
  workset.distParamLib = Teuchos::null;
  workset.disc = disc;
  workset.transientTerms = Teuchos::nonnull(workset.xdotT);
  workset.accelerationTerms = false;
}

void Assembler::loadWorksetJacobianInfo(
    PHAL::Workset& workset, const double alpha, const double beta,
    const double omega) {
  workset.m_coeff = alpha;
  workset.n_coeff = omega;
  workset.j_coeff = beta;
  workset.ignore_residual = false;
  workset.is_adjoint = false;
}

void Assembler::loadWorksetNodesetInfo(PHAL::Workset& workset) {
  workset.nodeSets = rcpFromRef(disc->getNodeSets());
  workset.nodeSetCoords = rcpFromRef(disc->getNodeSetCoords());
}

void Assembler::postRegSetup() {
  using JAC = PHAL::AlbanyTraits::Jacobian;
  for (int ps = 0; ps < fm.size(); ++ps) {
    std::vector<PHX::index_size_type> dd;
    int nnodes = mesh_specs[ps].get()->ctd.node_count;
    int deriv_dims = neq * nnodes;
    dd.push_back(deriv_dims);
    fm[ps]->setKokkosExtendedDataTypeDimensions<JAC>(dd);
    fm[ps]->postRegistrationSetupForType<JAC>("Jacobian");
    if (nfm != Teuchos::null && ps < nfm.size()) {
      nfm[ps]->setKokkosExtendedDataTypeDimensions<JAC>(dd);
      nfm[ps]->postRegistrationSetupForType<JAC>("Jacobian");
    }
  }
  if (dfm != Teuchos::null) {
    std::vector<PHX::index_size_type> dd;
    int nnodes = mesh_specs[0].get()->ctd.node_count;
    int deriv_dims = neq * nnodes;
    dd.push_back(deriv_dims);
    dfm->setKokkosExtendedDataTypeDimensions<JAC>(dd);
    dfm->postRegistrationSetupForType<JAC>("Jacobian");
  }
}

} // namespace CTM
