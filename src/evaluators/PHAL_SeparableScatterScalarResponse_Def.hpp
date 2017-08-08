//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

//IK, 9/13/14: this has Epetra but does not get compiled if ALBANY_EPETRA_EXE is turned off.

#include "Albany_Utils.hpp"
#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"
#if defined(ALBANY_EPETRA)
#include "Epetra_Export.h"
#include "Petra_Converters.hpp"
#endif
#include "PHAL_Utilities.hpp"

// **********************************************************************
// Base Class Generic Implemtation
// **********************************************************************
namespace PHAL {

template<typename EvalT, typename Traits>
SeparableScatterScalarResponseBase<EvalT, Traits>::
SeparableScatterScalarResponseBase(const Teuchos::ParameterList& p,
                                   const Teuchos::RCP<Albany::Layouts>& dl)
{
  setup(p, dl);
}

template<typename EvalT, typename Traits>
void SeparableScatterScalarResponseBase<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(local_response,fm);
  if (!this->stand_alone)
    this->utils.setFieldData(local_response_eval,fm);
}

template<typename EvalT, typename Traits>
void
SeparableScatterScalarResponseBase<EvalT, Traits>::
setup(const Teuchos::ParameterList& p, const Teuchos::RCP<Albany::Layouts>& dl)
{
  this->stand_alone = p.get<bool>("Stand-alone Evaluator");

  // Setup fields we require
  auto local_response_tag =
    p.get<PHX::Tag<ScalarT> >("Local Response Field Tag");
  local_response = decltype(local_response)(local_response_tag);
  if (this->stand_alone) {
    this->addDependentField(local_response);
  } else {
    local_response_eval = decltype(local_response_eval)(local_response_tag);
    this->addEvaluatedField(local_response_eval);
  }
}

// **********************************************************************
// Specialization: Jacobian
// **********************************************************************

template<typename Traits>
SeparableScatterScalarResponse<PHAL::AlbanyTraits::Jacobian, Traits>::
SeparableScatterScalarResponse(const Teuchos::ParameterList& p,
                const Teuchos::RCP<Albany::Layouts>& dl)
{
  this->setup(p,dl);
}

template<typename Traits>
void SeparableScatterScalarResponse<PHAL::AlbanyTraits::Jacobian, Traits>::
preEvaluate(typename Traits::PreEvalData workset)
{
  // Initialize derivatives
  Teuchos::RCP<Tpetra_MultiVector> dgdxT = workset.dgdxT;
  Teuchos::RCP<Tpetra_MultiVector> overlapped_dgdxT = workset.overlapped_dgdxT;
  if (dgdxT != Teuchos::null) {
    dgdxT->putScalar(0.0);
    overlapped_dgdxT->putScalar(0.0);
  }

  Teuchos::RCP<Tpetra_MultiVector> dgdxdotT = workset.dgdxdotT;
  Teuchos::RCP<Tpetra_MultiVector> overlapped_dgdxdotT =
    workset.overlapped_dgdxdotT;
  if (dgdxdotT != Teuchos::null) {
    dgdxdotT->putScalar(0.0);
    overlapped_dgdxdotT->putScalar(0.0);
  }
}

template<typename Traits>
void SeparableScatterScalarResponse<PHAL::AlbanyTraits::Jacobian, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  // Here we scatter the *local* response derivative
  auto nodeID = workset.wsElNodeEqID;
  Teuchos::RCP<Tpetra_MultiVector> dgdxT = workset.overlapped_dgdxT;
  Teuchos::RCP<Tpetra_MultiVector> dgdxdotT = workset.overlapped_dgdxdotT;
  Teuchos::RCP<Tpetra_MultiVector> dgT;
  if (dgdxT != Teuchos::null)
    dgT = dgdxT;
  else
    dgT = dgdxdotT;

  // Loop over cells in workset
  for (std::size_t cell=0; cell < workset.numCells; ++cell) {
    // Loop over responses

    for (std::size_t res = 0; res < this->global_response.size(); res++) {
      auto val = this->local_response(cell, res);

      // Loop over nodes in cell
      for (unsigned int node_dof=0; node_dof<numNodes; node_dof++) {
        int neq = nodeID.dimension(2);

        // Loop over equations per node
        for (unsigned int eq_dof=0; eq_dof<neq; eq_dof++) {

          // local derivative component
          int deriv = neq * node_dof + eq_dof;

          // local DOF
          int dof = nodeID(cell,node_dof,eq_dof);

          // Set dg/dx
          dgT->sumIntoLocalValue(dof, res, val.dx(deriv));

        } // column equations
      } // column nodes
    } // response

  } // cell
}

template<typename Traits>
void SeparableScatterScalarResponse<PHAL::AlbanyTraits::Jacobian, Traits>::
evaluate2DFieldsDerivativesDueToExtrudedSolution(typename Traits::EvalData workset, std::string& sideset, Teuchos::RCP<const CellTopologyData> cellTopo)
{
  // Here we scatter the *local* response derivative
  Teuchos::RCP<Tpetra_MultiVector> dgdxT = workset.overlapped_dgdxT;
  Teuchos::RCP<Tpetra_MultiVector> dgdxdotT = workset.overlapped_dgdxdotT;
  Teuchos::RCP<Tpetra_MultiVector> dgT;
  if (dgdxT != Teuchos::null)
    dgT = dgdxT;
  else
    dgT = dgdxdotT;

  const int neq = workset.wsElNodeEqID.dimension(2);
  const Albany::NodalDOFManager& solDOFManager = workset.disc->getOverlapDOFManager("ordinary_solution");
  const Albany::LayeredMeshNumbering<LO>& layeredMeshNumbering = *workset.disc->getLayeredMeshNumbering();
  int numLayers = layeredMeshNumbering.numLayers;
  const Teuchos::ArrayRCP<Teuchos::ArrayRCP<GO> >& wsElNodeID  = workset.disc->getWsElNodeID()[workset.wsIndex];

  if (workset.sideSets == Teuchos::null)
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error, "Side sets not properly specified on the mesh" << std::endl);

  const Albany::SideSetList& ssList = *(workset.sideSets);
  Albany::SideSetList::const_iterator it = ssList.find(sideset);

  if (it != ssList.end()) {
    const std::vector<Albany::SideStruct>& sideSet = it->second;

    for (std::size_t iSide = 0; iSide < sideSet.size(); ++iSide) { // loop over the sides on this ws and name
      // Get the data that corresponds to the side
      const int elem_GID = sideSet[iSide].elem_GID;
      const int elem_LID = sideSet[iSide].elem_LID;
      const int elem_side = sideSet[iSide].side_local_id;
      const CellTopologyData_Subcell& side =  cellTopo->side[elem_side];
      int numSideNodes = side.topology->node_count;

      const Teuchos::ArrayRCP<GO>& elNodeID = wsElNodeID[elem_LID];
      for (std::size_t res = 0; res < this->global_response.size(); res++) {
        auto val = this->local_response(elem_LID, res);
        LO base_id, ilayer;
        for (int i = 0; i < numSideNodes; ++i) {
          std::size_t node = side.node[i];
          LO lnodeId = workset.disc->getOverlapNodeMapT()->getLocalElement(elNodeID[node]);
          layeredMeshNumbering.getIndices(lnodeId, base_id, ilayer);
          for (unsigned int il_col=0; il_col<numLayers+1; il_col++) {
            LO inode = layeredMeshNumbering.getId(base_id, il_col);
            for (unsigned int eq_col=0; eq_col<neq; eq_col++) {
              LO dof = solDOFManager.getLocalDOF(inode, eq_col);
              int deriv = neq *this->numNodes+il_col*neq*numSideNodes + neq*i + eq_col;
              dgT->sumIntoLocalValue(dof, res, val.dx(deriv));
            }
          }
        }
      }
    }
  }
}

template<typename Traits>
void SeparableScatterScalarResponse<PHAL::AlbanyTraits::Jacobian, Traits>::
postEvaluate(typename Traits::PostEvalData workset)
{
  // Here we scatter the *global* response
  Teuchos::RCP<Tpetra_Vector> gT = workset.gT;
  if (gT != Teuchos::null) {
    Teuchos::ArrayRCP<ST> gT_nonconstView = gT->get1dViewNonConst();
    for (PHAL::MDFieldIterator<const ScalarT> gr(this->global_response);
         ! gr.done(); ++gr)
      gT_nonconstView[gr.idx()] = gr.ref().val();
  }

  // Here we scatter the *global* response derivatives
  Teuchos::RCP<Tpetra_MultiVector> dgdxT = workset.dgdxT;
  Teuchos::RCP<Tpetra_MultiVector> overlapped_dgdxT = workset.overlapped_dgdxT;
  if (dgdxT != Teuchos::null)
    dgdxT->doExport(*overlapped_dgdxT, *workset.x_importerT, Tpetra::ADD);

  Teuchos::RCP<Tpetra_MultiVector> dgdxdotT = workset.dgdxdotT;
  Teuchos::RCP<Tpetra_MultiVector> overlapped_dgdxdotT =
    workset.overlapped_dgdxdotT;
  if (dgdxdotT != Teuchos::null)
    dgdxdotT->doExport(*overlapped_dgdxdotT, *workset.x_importerT, Tpetra::ADD);
}

// **********************************************************************
// Specialization: Distributed Parameter Derivative
// **********************************************************************
template<typename Traits>
SeparableScatterScalarResponse<PHAL::AlbanyTraits::DistParamDeriv, Traits>::
SeparableScatterScalarResponse(const Teuchos::ParameterList& p,
                               const Teuchos::RCP<Albany::Layouts>& dl)
{
  this->setup(p,dl);
}

template<typename Traits>
void SeparableScatterScalarResponse<PHAL::AlbanyTraits::DistParamDeriv, Traits>::
preEvaluate(typename Traits::PreEvalData workset)
{
  //IKT, FIXME, 1/24/17: replace workset.dgdp below with workset.dgdpT 
  //once ATO:Constraint_2D_adj test passes with this change.  Remove ifdef guards 
  //when this is done. 
  Teuchos::RCP<Tpetra_MultiVector> dgdpT = workset.dgdpT;
  Teuchos::RCP<Tpetra_MultiVector> overlapped_dgdpT = workset.overlapped_dgdpT;
  if (dgdpT != Teuchos::null)
    dgdpT->putScalar(0.0);
  if (overlapped_dgdpT != Teuchos::null)
    overlapped_dgdpT->putScalar(0.0);
}

template<typename Traits>
void SeparableScatterScalarResponse<PHAL::AlbanyTraits::DistParamDeriv, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  // Here we scatter the *local* response derivative
  Teuchos::RCP<Tpetra_MultiVector> dgdpT = workset.overlapped_dgdpT;

  if (dgdpT == Teuchos::null)
    return;

  int num_deriv = numNodes;

  // Loop over cells in workset

  const Albany::IDArray&  wsElDofs = workset.distParamLib->get(workset.dist_param_deriv_name)->workset_elem_dofs()[workset.wsIndex];
  for (std::size_t cell=0; cell < workset.numCells; ++cell) {

    // Loop over responses
    for (std::size_t res = 0; res < this->global_response.size(); res++) {
     // ScalarT& val = this->local_response(cell, res);

      // Loop over nodes in cell
      for (int deriv=0; deriv<num_deriv; ++deriv) {
        const int row = wsElDofs((int)cell,deriv,0);

          // Set dg/dp
        if(row >=0){
          dgdpT->sumIntoLocalValue(row, res, (this->local_response(cell, res)).dx(deriv));
          }

      } // deriv
    } // response
  } // cell
}

template<typename Traits>
void SeparableScatterScalarResponse<PHAL::AlbanyTraits::DistParamDeriv, Traits>::
postEvaluate(typename Traits::PostEvalData workset)
{
  Teuchos::RCP<Tpetra_Vector> gT = workset.gT;
  if (gT != Teuchos::null) {
    Teuchos::ArrayRCP<double> gT_nonconstView = gT->get1dViewNonConst(); 
    for (std::size_t res = 0; res < this->global_response.size(); res++) {
      gT_nonconstView[res] = this->global_response[res].val();
    }
  }

  Teuchos::RCP<Tpetra_MultiVector> dgdpT = workset.dgdpT;
  Teuchos::RCP<Tpetra_MultiVector> overlapped_dgdpT = workset.overlapped_dgdpT;
  if ((dgdpT != Teuchos::null)&&(overlapped_dgdpT != Teuchos::null)) {
    Tpetra_Export exporterT(overlapped_dgdpT->getMap(), dgdpT->getMap());
    dgdpT->doExport(*overlapped_dgdpT, exporterT, Tpetra::ADD);
  }
}

// **********************************************************************
// Specialization: Stochastic Galerkin Jacobian
// **********************************************************************
#ifdef ALBANY_SG
template<typename Traits>
SeparableScatterScalarResponse<PHAL::AlbanyTraits::SGJacobian, Traits>::
SeparableScatterScalarResponse(const Teuchos::ParameterList& p,
                const Teuchos::RCP<Albany::Layouts>& dl)
{
  this->setup(p,dl);
}

template<typename Traits>
void SeparableScatterScalarResponse<PHAL::AlbanyTraits::SGJacobian, Traits>::
preEvaluate(typename Traits::PreEvalData workset)
{
  // Initialize derivatives
  Teuchos::RCP<Stokhos::EpetraMultiVectorOrthogPoly> dgdx_sg =
    workset.sg_dgdx;
  Teuchos::RCP<Stokhos::EpetraMultiVectorOrthogPoly> overlapped_dgdx_sg =
    workset.overlapped_sg_dgdx;
  if (dgdx_sg != Teuchos::null) {
    dgdx_sg->init(0.0);
    overlapped_dgdx_sg->init(0.0);
  }

  Teuchos::RCP<Stokhos::EpetraMultiVectorOrthogPoly> dgdxdot_sg =
    workset.sg_dgdxdot;
  Teuchos::RCP<Stokhos::EpetraMultiVectorOrthogPoly> overlapped_dgdxdot_sg =
    workset.overlapped_sg_dgdxdot;
  if (dgdxdot_sg != Teuchos::null) {
    dgdxdot_sg->init(0.0);
    overlapped_dgdxdot_sg->init(0.0);
  }
}

template<typename Traits>
void SeparableScatterScalarResponse<PHAL::AlbanyTraits::SGJacobian, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  // Here we scatter the *local* SG response derivative
  auto nodeID = workset.wsElNodeEqID;
  Teuchos::RCP<Stokhos::EpetraMultiVectorOrthogPoly> dgdx_sg =
    workset.overlapped_sg_dgdx;
  Teuchos::RCP<Stokhos::EpetraMultiVectorOrthogPoly> dgdxdot_sg =
    workset.overlapped_sg_dgdxdot;
  Teuchos::RCP<Stokhos::EpetraMultiVectorOrthogPoly> dg_sg;
  if (dgdx_sg != Teuchos::null)
    dg_sg = dgdx_sg;
  else
    dg_sg = dgdxdot_sg;

  // Loop over cells in workset
  for (std::size_t cell=0; cell < workset.numCells; ++cell) {

    // Loop over responses
    for (std::size_t res = 0; res < this->global_response.size(); res++) {
      typename PHAL::Ref<ScalarT>::type val = this->local_response(cell, res);

      // Loop over nodes in cell
      for (unsigned int node_dof=0; node_dof<numNodes; node_dof++) {
        int neq = nodeID.dimension(2);

        // Loop over equations per node
        for (unsigned int eq_dof=0; eq_dof<neq; eq_dof++) {

          // local derivative component
          int deriv = neq * node_dof + eq_dof;

          // local DOF
          int dof = nodeID(cell,node_dof,eq_dof);

          // Set dg/dx
          for (int block=0; block<dg_sg->size(); block++)
            (*dg_sg)[block].SumIntoMyValue(dof, res,
                                           val.dx(deriv).coeff(block));

        } // column equations
      } // response
    } // node
  } // cell
}

template<typename Traits>
void SeparableScatterScalarResponse<PHAL::AlbanyTraits::SGJacobian, Traits>::
postEvaluate(typename Traits::PostEvalData workset)
{
  // Here we scatter the *global* SG response
  Teuchos::RCP< Stokhos::EpetraVectorOrthogPoly > g_sg = workset.sg_g;
  if (g_sg != Teuchos::null) {
    for (std::size_t res = 0; res < this->global_response.size(); res++) {
      typename PHAL::Ref<ScalarT>::type val = this->global_response[res];
      for (int block=0; block<g_sg->size(); block++)
        (*g_sg)[block][res] = val.val().coeff(block);
    }
  }

  // Here we scatter the *global* SG response derivatives
  Teuchos::RCP<Stokhos::EpetraMultiVectorOrthogPoly> dgdx_sg =
    workset.sg_dgdx;
  Teuchos::RCP<Stokhos::EpetraMultiVectorOrthogPoly> overlapped_dgdx_sg =
    workset.overlapped_sg_dgdx;
  if (dgdx_sg != Teuchos::null)
    for (int block=0; block<dgdx_sg->size(); block++)
      (*dgdx_sg)[block].Export((*overlapped_dgdx_sg)[block],
                               *workset.x_importer, Add);

  Teuchos::RCP<Stokhos::EpetraMultiVectorOrthogPoly> dgdxdot_sg =
    workset.sg_dgdxdot;
  Teuchos::RCP<Stokhos::EpetraMultiVectorOrthogPoly> overlapped_dgdxdot_sg =
    workset.overlapped_sg_dgdxdot;
  if (dgdxdot_sg != Teuchos::null)
    for (int block=0; block<dgdxdot_sg->size(); block++)
      (*dgdxdot_sg)[block].Export((*overlapped_dgdxdot_sg)[block],
                                  *workset.x_importer, Add);
}
#endif 
#ifdef ALBANY_ENSEMBLE 

// **********************************************************************
// Specialization: Multi-point Jacobian
// **********************************************************************

template<typename Traits>
SeparableScatterScalarResponse<PHAL::AlbanyTraits::MPJacobian, Traits>::
SeparableScatterScalarResponse(const Teuchos::ParameterList& p,
                         const Teuchos::RCP<Albany::Layouts>& dl)
{
  this->setup(p,dl);
}

template<typename Traits>
void SeparableScatterScalarResponse<PHAL::AlbanyTraits::MPJacobian, Traits>::
preEvaluate(typename Traits::PreEvalData workset)
{
  // Initialize derivatives
  Teuchos::RCP<Stokhos::ProductEpetraMultiVector> dgdx_mp =
    workset.mp_dgdx;
  Teuchos::RCP<Stokhos::ProductEpetraMultiVector> overlapped_dgdx_mp =
    workset.overlapped_mp_dgdx;
  if (dgdx_mp != Teuchos::null) {
    dgdx_mp->init(0.0);
    overlapped_dgdx_mp->init(0.0);
  }

  Teuchos::RCP<Stokhos::ProductEpetraMultiVector> dgdxdot_mp =
    workset.mp_dgdxdot;
  Teuchos::RCP<Stokhos::ProductEpetraMultiVector> overlapped_dgdxdot_mp =
    workset.overlapped_mp_dgdxdot;
  if (dgdxdot_mp != Teuchos::null) {
    dgdxdot_mp->init(0.0);
    overlapped_dgdxdot_mp->init(0.0);
  }
}

template<typename Traits>
void SeparableScatterScalarResponse<PHAL::AlbanyTraits::MPJacobian, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  // Here we scatter the *local* MP response derivative
  auto nodeID = workset.wsElNodeEqID;
  Teuchos::RCP<Stokhos::ProductEpetraMultiVector> dgdx_mp =
    workset.overlapped_mp_dgdx;
  Teuchos::RCP<Stokhos::ProductEpetraMultiVector> dgdxdot_mp =
    workset.overlapped_mp_dgdxdot;
  Teuchos::RCP<Stokhos::ProductEpetraMultiVector> dg_mp;
  if (dgdx_mp != Teuchos::null)
    dg_mp = dgdx_mp;
  else
    dg_mp = dgdxdot_mp;

  // Loop over cells in workset
  for (std::size_t cell=0; cell < workset.numCells; ++cell) {

    // Loop over responses
    for (std::size_t res = 0; res < this->global_response.size(); res++) {
      typename PHAL::Ref<ScalarT>::type val = this->local_response(cell, res);

      // Loop over nodes in cell
      for (unsigned int node_dof=0; node_dof<numNodes; node_dof++) {
        int neq = nodeID.dimension(2);

        // Loop over equations per node
        for (unsigned int eq_dof=0; eq_dof<neq; eq_dof++) {

          // local derivative component
          int deriv = neq * node_dof + eq_dof;

          // local DOF
          int dof = nodeID(cell,node_dof,eq_dof);

          // Set dg/dx
          for (int block=0; block<dg_mp->size(); block++)
            (*dg_mp)[block].SumIntoMyValue(dof, res,
                                           val.dx(deriv).coeff(block));

        } // column equations
      } // response
    } // node
  } // cell
}

template<typename Traits>
void SeparableScatterScalarResponse<PHAL::AlbanyTraits::MPJacobian, Traits>::
postEvaluate(typename Traits::PostEvalData workset)
{
  // Here we scatter the *global* MP response
  Teuchos::RCP<Stokhos::ProductEpetraVector> g_mp = workset.mp_g;
  if (g_mp != Teuchos::null) {
    for (std::size_t res = 0; res < this->global_response.size(); res++) {
      typename PHAL::Ref<ScalarT>::type val = this->global_response[res];
      for (int block=0; block<g_mp->size(); block++)
        (*g_mp)[block][res] = val.val().coeff(block);
    }
  }

  // Here we scatter the *global* MP response derivatives
  Teuchos::RCP<Stokhos::ProductEpetraMultiVector> dgdx_mp =
    workset.mp_dgdx;
  Teuchos::RCP<Stokhos::ProductEpetraMultiVector> overlapped_dgdx_mp =
    workset.overlapped_mp_dgdx;
  if (dgdx_mp != Teuchos::null)
    for (int block=0; block<dgdx_mp->size(); block++)
      (*dgdx_mp)[block].Export((*overlapped_dgdx_mp)[block],
                               *workset.x_importer, Add);

  Teuchos::RCP<Stokhos::ProductEpetraMultiVector> dgdxdot_mp =
    workset.mp_dgdxdot;
  Teuchos::RCP<Stokhos::ProductEpetraMultiVector> overlapped_dgdxdot_mp =
    workset.overlapped_mp_dgdxdot;
  if (dgdxdot_mp != Teuchos::null)
    for (int block=0; block<dgdxdot_mp->size(); block++)
      (*dgdxdot_mp)[block].Export((*overlapped_dgdxdot_mp)[block],
                                  *workset.x_importer, Add);
}
#endif

}

