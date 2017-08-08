//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef AERAS_COMPUTE_AND_SCATTER_JAC_HPP
#define AERAS_COMPUTE_AND_SCATTER_JAC_HPP

#include "Phalanx_config.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"

#include "Aeras_Layouts.hpp"

#include "Teuchos_ParameterList.hpp"

namespace Aeras {
/** \brief Gathers Coordinates values from the Newton coordinates vector into 
    the nodal fields of the field manager

    Currently makes an assumption that the stride is constant for dofs
    and that the nmber of dofs is equal to the size of the coordinates
    names vector.

*/

template<typename EvalT, typename Traits> 
class ComputeAndScatterJacBase
  : public PHX::EvaluatorWithBaseImpl<Traits>,
    public PHX::EvaluatorDerived<EvalT, Traits>  {
  
public:
  typedef typename EvalT::ScalarT ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;
  
  ComputeAndScatterJacBase(const Teuchos::ParameterList& p,
                              const Teuchos::RCP<Aeras::Layouts>& dl);
  
  void postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& vm);
  
  virtual void evaluateFields(typename Traits::EvalData d)=0;
  
protected:

  //OG not used anymore
  Teuchos::RCP<PHX::FieldTag> scatter_operation;
  //OG not used anymore
  const int numNodes;
  const int numDims;
  const int numLevels;
  const int worksetSize;
  int numFields; 
  int numNodeVar; 
  int numVectorLevelVar;
  int numScalarLevelVar;
  int numTracerVar;

protected:

  PHX::MDField<const RealType,Cell,Node,QuadPoint> BF;
  PHX::MDField<const MeshScalarT,Cell,Node,QuadPoint> wBF;
  PHX::MDField<const MeshScalarT,Cell,Node,QuadPoint,Dim> GradBF;
  PHX::MDField<const MeshScalarT,Cell,Node,QuadPoint,Dim> wGradBF;
  PHX::MDField<const MeshScalarT,Cell,Node> lambda_nodal;
  PHX::MDField<const MeshScalarT,Cell,Node> theta_nodal;

protected:
  double sqrtHVcoef;

};

template<typename EvalT, typename Traits> class ComputeAndScatterJac;

// **************************************************************
// **************************************************************
// * Specializations
// **************************************************************
// **************************************************************

// **************************************************************
// Jacobian
// **************************************************************
template<typename Traits>
class ComputeAndScatterJac<PHAL::AlbanyTraits::Jacobian,Traits>
  : public ComputeAndScatterJacBase<PHAL::AlbanyTraits::Jacobian, Traits>  {
public:
  typedef typename PHAL::AlbanyTraits::Jacobian::ScalarT ScalarT;
  ComputeAndScatterJac(const Teuchos::ParameterList& p,
                              const Teuchos::RCP<Aeras::Layouts>& dl);
  void evaluateFields(typename Traits::EvalData d); 

#ifdef ALBANY_KOKKOS_UNDER_DEVELOPMENT
  typedef typename Tpetra_CrsMatrix::local_matrix_type  LocalMatrixType;
  LocalMatrixType jacobian;
  RealType mc;
  int neq;

  Albany::AbstractDiscretization::WorksetConn nodeID;

  struct ComputeAndScatterJac_buildMass_Tag{};

  template<int numn>
  struct ComputeAndScatterJac_buildLaplace_Tag{};

  typedef Kokkos::View<int***, PHX::Device>::execution_space ExecutionSpace;

  typedef Kokkos::RangePolicy<ExecutionSpace, ComputeAndScatterJac_buildMass_Tag> ComputeAndScatterJac_buildMass_Policy;

  template<int numn>
  using ComputeAndScatterJac_buildLaplace_Policy = Kokkos::RangePolicy<ExecutionSpace, ComputeAndScatterJac_buildLaplace_Tag<numn>>;

  KOKKOS_INLINE_FUNCTION
  void operator() (const ComputeAndScatterJac_buildMass_Tag& tag, const int& i) const;

  template<int numn>
  KOKKOS_INLINE_FUNCTION
  void operator() (const ComputeAndScatterJac_buildLaplace_Tag<numn>& tag, const int& i) const;

private:
  Kokkos::DynRankView<LO, PHX::Device> colT;

#endif
};

// **************************************************************
// GENERIC: Specializations for SG and MP not yet implemented
// **************************************************************
template<typename Traits>
class ComputeAndScatterJac<PHAL::AlbanyTraits::DistParamDeriv,Traits>
  : public ComputeAndScatterJacBase<PHAL::AlbanyTraits::DistParamDeriv, Traits>  {
public:
  typedef typename PHAL::AlbanyTraits::DistParamDeriv::ScalarT ScalarT;
  ComputeAndScatterJac(const Teuchos::ParameterList& p,
                  const Teuchos::RCP<Aeras::Layouts>& dl) : 
    ComputeAndScatterJacBase<PHAL::AlbanyTraits::DistParamDeriv,Traits>(p,dl){}
  void evaluateFields(typename Traits::EvalData d)
    {throw "Aeras::GatherSolution not implemented for all tempate specializations";};
};


// **************************************************************
// GENERIC: Specializations for SG and MP not yet implemented
// **************************************************************
template<typename EvalT, typename Traits>
class ComputeAndScatterJac
  : public ComputeAndScatterJacBase<EvalT, Traits>  {
public:
  typedef typename EvalT::ScalarT ScalarT;
  ComputeAndScatterJac(const Teuchos::ParameterList& p,
                              const Teuchos::RCP<Aeras::Layouts>& dl)
      : ComputeAndScatterJacBase<EvalT,Traits>(p,dl)
    {
    };
  void evaluateFields(typename Traits::EvalData d)
    {throw "Aeras::GatherSolution not implemented for all tempate specializations";};
};

#ifdef ALBANY_ENSEMBLE 
// **************************************************************
// Multi-point Residual 
// **************************************************************
template<typename Traits>
class ComputeAndScatterJac<PHAL::AlbanyTraits::MPResidual,Traits>
  : public ComputeAndScatterJacBase<PHAL::AlbanyTraits::MPResidual, Traits>  {
public:
  typedef typename PHAL::AlbanyTraits::MPResidual::ScalarT ScalarT;
  ComputeAndScatterJac(const Teuchos::ParameterList& p,
                              const Teuchos::RCP<Aeras::Layouts>& dl);
  void evaluateFields(typename Traits::EvalData d); 
};

// **************************************************************
// Multi-point Jacobian
// **************************************************************
template<typename Traits>
class ComputeAndScatterJac<PHAL::AlbanyTraits::MPJacobian,Traits>
  : public ComputeAndScatterJacBase<PHAL::AlbanyTraits::MPJacobian, Traits>  {
public:
  typedef typename PHAL::AlbanyTraits::MPJacobian::ScalarT ScalarT;
  ComputeAndScatterJac(const Teuchos::ParameterList& p,
                              const Teuchos::RCP<Aeras::Layouts>& dl);
  void evaluateFields(typename Traits::EvalData d); 
};
#endif

}

#endif
