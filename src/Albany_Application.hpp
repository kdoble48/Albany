//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef ALBANY_APPLICATION_HPP
#define ALBANY_APPLICATION_HPP

#include "Teuchos_RCP.hpp"
#include "Teuchos_ArrayRCP.hpp"
#include "Teuchos_ParameterList.hpp"
#include "Teuchos_SerialDenseMatrix.hpp"
#include "Teuchos_VerboseObject.hpp"
#include "Teuchos_TimeMonitor.hpp"

#if defined(ALBANY_EPETRA)
#include "Epetra_Map.h"
#include "Epetra_Vector.h"
#include "Epetra_CrsMatrix.h"
#include "Epetra_Import.h"
#include "Epetra_Export.h"
#endif

#include "Albany_AbstractDiscretization.hpp"
#include "Albany_AbstractProblem.hpp"
#include "Albany_AbstractResponseFunction.hpp"
#include "Albany_StateManager.hpp"

#if defined(ALBANY_EPETRA)
#include "AAdapt_AdaptiveSolutionManager.hpp"
#endif

#include "AAdapt_AdaptiveSolutionManagerT.hpp"
#include "Albany_DiscretizationFactory.hpp"

#if defined(ALBANY_CUTR)
#include "CUTR_CubitMeshMover.hpp"
#include "STKMeshData.hpp"
#endif

#include "Sacado_ScalarParameterLibrary.hpp"
#include "Sacado_ScalarParameterVector.hpp"
#include "Sacado_ParameterAccessor.hpp"
#include "Sacado_ParameterRegistration.hpp"

#include "PHAL_AlbanyTraits.hpp"
#include "PHAL_Workset.hpp"

#if defined(ALBANY_STOKHOS)
#include "Stokhos_OrthogPolyExpansion.hpp"
#include "Stokhos_Quadrature.hpp"
#endif

#if defined(ALBANY_EPETRA)

#if defined(ALBANY_STOKHOS)
#include "Stokhos_EpetraVectorOrthogPoly.hpp"
#include "Stokhos_EpetraMultiVectorOrthogPoly.hpp"
#endif

#include "EpetraExt_MultiComm.h"
#include "LOCA_Epetra_Group.H"

#if defined(ALBANY_TEKO)
#include "Teko_InverseLibrary.hpp"
#endif

#endif

#if defined(ALBANY_MOR)
#if defined(ALBANY_EPETRA)
#include "MOR/Albany_MORFacade.hpp"
#endif
#endif

// Forward declarations.
namespace AAdapt { namespace rc { class Manager; } }

namespace Albany {

  class Application :
     public Sacado::ParameterAccessor<PHAL::AlbanyTraits::Residual, SPL_Traits> {
  public:

    enum SolutionMethod {Steady, Transient, TransientTempus, Continuation, Eigensolve};

    //! Constructor
    Application(const Teuchos::RCP<const Teuchos_Comm>& comm,
		const Teuchos::RCP<Teuchos::ParameterList>& params,
		const Teuchos::RCP<const Tpetra_Vector>& initial_guess =
		Teuchos::null);

    //! Constructor
    Application(const Teuchos::RCP<const Teuchos_Comm>& comm);
    
    //! Destructor
    ~Application();

    void initialSetUp(const Teuchos::RCP<Teuchos::ParameterList>& params);
    void createMeshSpecs();
    void createMeshSpecs(Teuchos::RCP<Albany::AbstractMeshStruct> mesh);
    void buildProblem();
    void createDiscretization();
    void finalSetUp(const Teuchos::RCP<Teuchos::ParameterList>& params, const Teuchos::RCP<const Tpetra_Vector>& initial_guess = Teuchos::null);

    //! Get underlying abstract discretization
    Teuchos::RCP<Albany::AbstractDiscretization> getDiscretization() const;

    //! Get problem object
    Teuchos::RCP<Albany::AbstractProblem> getProblem() const;

    //! Get communicator
    Teuchos::RCP<const Teuchos_Comm> getComm() const;

#if defined(ALBANY_EPETRA)
    //! Get Epetra communicator
    Teuchos::RCP<const Epetra_Comm> getEpetraComm() const;
    //! Get DOF map
    Teuchos::RCP<const Epetra_Map> getMap() const;
#endif

    //! Get Tpetra DOF map
    Teuchos::RCP<const Tpetra_Map> getMapT() const;

#if defined(ALBANY_EPETRA)
    //! Get Jacobian graph
    Teuchos::RCP<const Epetra_CrsGraph> getJacobianGraph() const;
#endif

    //! Get Tpetra Jacobian graph
    Teuchos::RCP<const Tpetra_CrsGraph> getJacobianGraphT() const;

#if defined(ALBANY_EPETRA)
    //! Get Preconditioner Operator
    Teuchos::RCP<Epetra_Operator> getPreconditioner();

    //! Get initial solution
    Teuchos::RCP<const Epetra_Vector> getInitialSolution() const;

    //! Get initial solution dot
    Teuchos::RCP<const Epetra_Vector> getInitialSolutionDot() const;
    Teuchos::RCP<const Epetra_Vector> getInitialSolutionDotDot() const;

#endif
    //! Get Preconditioner Operator
    Teuchos::RCP<Tpetra_Operator> getPreconditionerT();

    bool observeResponses() const {return observe_responses;} 
    
    int observeResponsesFreq() const {return response_observ_freq;} 

    Teuchos::Array<unsigned int> getMarkersForRelativeResponses() const {return relative_responses;}

#if defined(ALBANY_EPETRA)
    //! Get the solution memory manager
    Teuchos::RCP<AAdapt::AdaptiveSolutionManager> getAdaptSolMgr(){ return solMgr;}
#endif
    Teuchos::RCP<AAdapt::AdaptiveSolutionManagerT> getAdaptSolMgrT(){ return solMgrT;}

    //! Get parameter library
    Teuchos::RCP<ParamLib> getParamLib() const;

    //! Get distributed parameter library
    Teuchos::RCP<DistParamLib> getDistParamLib() const;

    //! Get solution method
    SolutionMethod getSolutionMethod() const {return solMethod; }

    //! Get number of responses
    int getNumResponses() const;

    int getNumEquations() const { return neq; }
    int getSpatialDimension() const { return spatial_dimension; }
    int getTangentDerivDimension() const { return tangent_deriv_dim; }

    //! Get response function
    Teuchos::RCP<AbstractResponseFunction> getResponse(int i) const;

    //! Return whether problem wants to use its own preconditioner
    bool suppliesPreconditioner() const;

#ifdef ALBANY_STOKHOS
    //! Get stochastic expansion
    Teuchos::RCP<Stokhos::OrthogPolyExpansion<int,double>>
    getStochasticExpansion();
#endif

    //! Intialize stochastic Galerkin method
#ifdef ALBANY_SG
    void init_sg(
      const Teuchos::RCP<const Stokhos::OrthogPolyBasis<int,double>>& basis,
      const Teuchos::RCP<const Stokhos::Quadrature<int,double>>& quad,
      const Teuchos::RCP<Stokhos::OrthogPolyExpansion<int,double>>& expansion,
      const Teuchos::RCP<const EpetraExt::MultiComm>& multiComm);
#endif

    //! Compute global residual
    /*!
     * Set xdot to NULL for steady-state problems
     */
#if defined(ALBANY_EPETRA)
    double 
    computeConditionNumber(Epetra_CrsMatrix& matrix); 
#endif 

#if defined(ALBANY_EPETRA)
    void computeGlobalResidual(const double current_time,
                               const Epetra_Vector* xdot,
                               const Epetra_Vector* xdotdot,
                               const Epetra_Vector& x,
                               const Teuchos::Array<ParamVec>& p,
                               Epetra_Vector& f);
#endif

     void computeGlobalResidualT(const double current_time,
                               const Tpetra_Vector* xdotT,
                               const Tpetra_Vector* xdotdotT,
                               const Tpetra_Vector& xT,
                               const Teuchos::Array<ParamVec>& p,
                               Tpetra_Vector& fT);

  private:

     void computeGlobalResidualImplT(const double current_time,
                                     const Teuchos::RCP<const Tpetra_Vector>& xdotT,
                                     const Teuchos::RCP<const Tpetra_Vector>& xdotdotT,
                                     const Teuchos::RCP<const Tpetra_Vector>& xT,
                                     const Teuchos::Array<ParamVec>& p,
                                     const Teuchos::RCP<Tpetra_Vector>& fT);
    
#if defined(ALBANY_LCM) 
     void computeGlobalResidualSDBCsImplT(const double current_time,
                                                const Teuchos::RCP<const Tpetra_Vector>& xdotT,
                                                const Teuchos::RCP<const Tpetra_Vector>& xdotdotT,
                                                const Teuchos::RCP<const Tpetra_Vector>& xT,
                                                const Teuchos::Array<ParamVec>& p,
                                                const Teuchos::RCP<Tpetra_Vector>& fT);
#endif

  public:

    //! Compute global Jacobian
    /*!
     * Set xdot to NULL for steady-state problems
     */
#if defined(ALBANY_EPETRA)
    void computeGlobalJacobian(const double alpha,
                               const double beta,
                               const double omega,
                               const double current_time,
                               const Epetra_Vector* xdot,
                               const Epetra_Vector* xdotdot,
                               const Epetra_Vector& x,
                               const Teuchos::Array<ParamVec>& p,
                               Epetra_Vector* f,
                               Epetra_CrsMatrix& jac);
#endif

     void computeGlobalJacobianT(const double alpha,
                                 const double beta,
			         const double omega,
                                 const double current_time,
                                 const Tpetra_Vector* xdotT,
                                 const Tpetra_Vector* xdotdotT,
                                 const Tpetra_Vector& xT,
                                 const Teuchos::Array<ParamVec>& p,
                                 Tpetra_Vector* fT,
                                 Tpetra_CrsMatrix& jacT);

  private:

     void computeGlobalJacobianImplT(const double alpha,
                                     const double beta,
                                     const double omega,
                                     const double current_time,
                                     const Teuchos::RCP<const Tpetra_Vector>& xdotT,
                                     const Teuchos::RCP<const Tpetra_Vector>& xdotdotT,
                                     const Teuchos::RCP<const Tpetra_Vector>& xT,
                                     const Teuchos::Array<ParamVec>& p,
                                     const Teuchos::RCP<Tpetra_Vector>& fT,
                                     const Teuchos::RCP<Tpetra_CrsMatrix>& jacT);

  public:

    //! Compute global Preconditioner
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void computeGlobalPreconditionerT(const Teuchos::RCP<Tpetra_CrsMatrix>& jac,
                                     const Teuchos::RCP<Tpetra_Operator>& prec);

#if defined(ALBANY_EPETRA)
    void computeGlobalPreconditioner(const Teuchos::RCP<Epetra_CrsMatrix>& jac,
                                     const Teuchos::RCP<Epetra_Operator>& prec);

    //! Compute global Tangent
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void computeGlobalTangent(const double alpha,
                              const double beta,
                               const double omega,
                              const double current_time,
                              bool sum_derivs,
                              const Epetra_Vector* xdot,
                              const Epetra_Vector* xdotdot,
                              const Epetra_Vector& x,
                              const Teuchos::Array<ParamVec>& p,
                              ParamVec* deriv_p,
                              const Epetra_MultiVector* Vx,
                              const Epetra_MultiVector* Vxdot,
                              const Epetra_MultiVector* Vxdotdot,
                              const Epetra_MultiVector* Vp,
                              Epetra_Vector* f,
                              Epetra_MultiVector* JV,
                              Epetra_MultiVector* fp);
#endif

     void computeGlobalTangentT(const double alpha,
                              const double beta,
			      const double omega,
                              const double current_time,
                              bool sum_derivs,
                              const Tpetra_Vector* xdotT,
                              const Tpetra_Vector* xdotdotT,
                              const Tpetra_Vector& xT,
                              const Teuchos::Array<ParamVec>& p,
                              ParamVec* deriv_p,
                              const Tpetra_MultiVector* VxT,
                              const Tpetra_MultiVector* VxdotT,
                              const Tpetra_MultiVector* VxdotdotT,
                              const Tpetra_MultiVector* VpT,
                              Tpetra_Vector* fT,
                              Tpetra_MultiVector* JVT,
                              Tpetra_MultiVector* fpT);

  private:

     void computeGlobalTangentImplT(const double alpha,
                                    const double beta,
                                    const double omega,
                                    const double current_time,
                                    bool sum_derivs,
                                    const Teuchos::RCP<const Tpetra_Vector>& xdotT,
                                    const Teuchos::RCP<const Tpetra_Vector>& xdotdotT,
                                    const Teuchos::RCP<const Tpetra_Vector>& xT,
                                    const Teuchos::Array<ParamVec>& par,
                                    ParamVec* deriv_par,
                                    const Teuchos::RCP<const Tpetra_MultiVector>& VxT,
                                    const Teuchos::RCP<const Tpetra_MultiVector>& VxdotT,
                                    const Teuchos::RCP<const Tpetra_MultiVector>& VxdotdotT,
                                    const Teuchos::RCP<const Tpetra_MultiVector>& VpT,
                                    const Teuchos::RCP<Tpetra_Vector>& fT,
                                    const Teuchos::RCP<Tpetra_MultiVector>& JVT,
                                    const Teuchos::RCP<Tpetra_MultiVector>& fpT);
    

  public:

     //! Compute df/dp*V or (df/dp)^T*V for distributed parameter p
     /*!
      * Set xdot to NULL for steady-state problems
      */
     void applyGlobalDistParamDerivImplT(const double current_time,
                                   const Teuchos::RCP<const Tpetra_Vector> &xdotT,
                                   const Teuchos::RCP<const Tpetra_Vector> &xdotdotT,
                                   const Teuchos::RCP<const Tpetra_Vector> &xT,
                                   const Teuchos::Array<ParamVec>& p,
                                   const std::string& dist_param_name,
                                   const bool trans,
                                   const Teuchos::RCP<const Tpetra_MultiVector>& VT,
                                   const Teuchos::RCP<Tpetra_MultiVector>& fpVT);
    


    //! Evaluate response functions
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void evaluateResponseT(
      int response_index,
      const double current_time,
      const Tpetra_Vector* xdotT,
      const Tpetra_Vector* xdotdotT,
      const Tpetra_Vector& xT,
      const Teuchos::Array<ParamVec>& p,
      Tpetra_Vector& gT);

    //! Evaluate tangent = alpha*dg/dx*Vx + beta*dg/dxdot*Vxdot + dg/dp*Vp
    /*!
     * Set xdot, dxdot_dp to NULL for steady-state problems
     */
    void evaluateResponseTangentT(
      int response_index,
      const double alpha,
      const double beta,
      const double omega,
      const double current_time,
      bool sum_derivs,
      const Tpetra_Vector* xdotT,
      const Tpetra_Vector* xdotdotT,
      const Tpetra_Vector& xT,
      const Teuchos::Array<ParamVec>& p,
      ParamVec* deriv_p,
      const Tpetra_MultiVector* VxdotT,
      const Tpetra_MultiVector* VxdotdotT,
      const Tpetra_MultiVector* VxT,
      const Tpetra_MultiVector* VpT,
      Tpetra_Vector* gT,
      Tpetra_MultiVector* gxT,
      Tpetra_MultiVector* gpT);

    //! Evaluate gradient = dg/dx, dg/dxdot, dg/dp
    /*!
     * Set xdot, dg_dxdot to NULL for steady-state problems
     */
#if defined(ALBANY_EPETRA)
    void evaluateResponseDerivative(
      int response_index,
      const double current_time,
      const Epetra_Vector* xdot,
      const Epetra_Vector* xdotdot,
      const Epetra_Vector& x,
      const Teuchos::Array<ParamVec>& p,
      ParamVec* deriv_p,
      Epetra_Vector* g,
      const EpetraExt::ModelEvaluator::Derivative& dg_dx,
      const EpetraExt::ModelEvaluator::Derivative& dg_dxdot,
      const EpetraExt::ModelEvaluator::Derivative& dg_dxdotdot,
      const EpetraExt::ModelEvaluator::Derivative& dg_dp);
#endif

    void evaluateResponseDerivativeT(
      int response_index,
      const double current_time,
      const Tpetra_Vector* xdotT,
      const Tpetra_Vector* xdotdotT,
      const Tpetra_Vector& xT,
      const Teuchos::Array<ParamVec>& p,
      ParamVec* deriv_p,
      Tpetra_Vector* gT,
      const Thyra::ModelEvaluatorBase::Derivative<ST>& dg_dxT,
      const Thyra::ModelEvaluatorBase::Derivative<ST>& dg_dxdotT,
      const Thyra::ModelEvaluatorBase::Derivative<ST>& dg_dxdotdotT,
      const Thyra::ModelEvaluatorBase::Derivative<ST>& dg_dpT);

    void evaluateResponseDistParamDerivT(  int response_index,
      const double current_time,
      const Tpetra_Vector* xdot,
      const Tpetra_Vector* xdotdot,
      const Tpetra_Vector& x,
      const Teuchos::Array<ParamVec>& param_array,
      const std::string& dist_param_name,
      Tpetra_MultiVector* dg_dp);

#ifdef ALBANY_SG
    //! Compute global residual for stochastic Galerkin problem
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void computeGlobalSGResidual(
      const double current_time,
      const Stokhos::EpetraVectorOrthogPoly* sg_xdot,
      const Stokhos::EpetraVectorOrthogPoly* sg_xdotdot,
      const Stokhos::EpetraVectorOrthogPoly& sg_x,
      const Teuchos::Array<ParamVec>& p,
      const Teuchos::Array<int>& sg_p_index,
      const Teuchos::Array< Teuchos::Array<SGType>>& sg_p_vals,
      Stokhos::EpetraVectorOrthogPoly& sg_f);

    //! Compute global Jacobian for stochastic Galerkin problem
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void computeGlobalSGJacobian(
      const double alpha,
      const double beta,
      const double omega,
      const double current_time,
      const Stokhos::EpetraVectorOrthogPoly* sg_xdot,
      const Stokhos::EpetraVectorOrthogPoly* sg_xdotdot,
      const Stokhos::EpetraVectorOrthogPoly& sg_x,
      const Teuchos::Array<ParamVec>& p,
      const Teuchos::Array<int>& sg_p_index,
      const Teuchos::Array< Teuchos::Array<SGType>>& sg_p_vals,
      Stokhos::EpetraVectorOrthogPoly* sg_f,
      Stokhos::VectorOrthogPoly<Epetra_CrsMatrix>& sg_jac);

    //! Compute global Tangent for stochastic Galerkin problem
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void computeGlobalSGTangent(
      const double alpha,
      const double beta,
      const double omega,
      const double current_time,
      bool sum_derivs,
      const Stokhos::EpetraVectorOrthogPoly* sg_xdot,
      const Stokhos::EpetraVectorOrthogPoly* sg_xdotdot,
      const Stokhos::EpetraVectorOrthogPoly& sg_x,
      const Teuchos::Array<ParamVec>& p,
      const Teuchos::Array<int>& sg_p_index,
      const Teuchos::Array< Teuchos::Array<SGType>>& sg_p_vals,
      ParamVec* deriv_p,
      const Epetra_MultiVector* Vx,
      const Epetra_MultiVector* Vxdot,
      const Epetra_MultiVector* Vxdotdot,
      const Epetra_MultiVector* Vp,
      Stokhos::EpetraVectorOrthogPoly* sg_f,
      Stokhos::EpetraMultiVectorOrthogPoly* sg_JVx,
      Stokhos::EpetraMultiVectorOrthogPoly* sg_fVp);

    //! Evaluate stochastic Galerkin response functions
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void evaluateSGResponse(
      int response_index,
      const double curr_time,
      const Stokhos::EpetraVectorOrthogPoly* sg_xdot,
      const Stokhos::EpetraVectorOrthogPoly* sg_xdotdot,
      const Stokhos::EpetraVectorOrthogPoly& sg_x,
      const Teuchos::Array<ParamVec>& p,
      const Teuchos::Array<int>& sg_p_index,
      const Teuchos::Array< Teuchos::Array<SGType>>& sg_p_vals,
      Stokhos::EpetraVectorOrthogPoly& sg_g);

    //! Evaluate tangent = dg/dx*dx/dp + dg/dxdot*dxdot/dp + dg/dp
    /*!
     * Set xdot, dxdot_dp to NULL for steady-state problems
     */
    void
    evaluateSGResponseTangent(
      int response_index,
      const double alpha,
      const double beta,
      const double omega,
      const double current_time,
      bool sum_derivs,
      const Stokhos::EpetraVectorOrthogPoly* sg_xdot,
      const Stokhos::EpetraVectorOrthogPoly* sg_xdotdot,
      const Stokhos::EpetraVectorOrthogPoly& sg_x,
      const Teuchos::Array<ParamVec>& p,
      const Teuchos::Array<int>& sg_p_index,
      const Teuchos::Array< Teuchos::Array<SGType>>& sg_p_vals,
      ParamVec* deriv_p,
      const Epetra_MultiVector* Vx,
      const Epetra_MultiVector* Vxdot,
      const Epetra_MultiVector* Vxdotdot,
      const Epetra_MultiVector* Vp,
      Stokhos::EpetraVectorOrthogPoly* sg_g,
      Stokhos::EpetraMultiVectorOrthogPoly* sg_JV,
      Stokhos::EpetraMultiVectorOrthogPoly* sg_gp);

    //! Evaluate gradient = dg/dx, dg/dxdot, dg/dp
    /*!
     * Set xdot, dg_dxdot to NULL for steady-state problems
     */
    void
    evaluateSGResponseDerivative(
      int response_index,
      const double current_time,
      const Stokhos::EpetraVectorOrthogPoly* sg_xdot,
      const Stokhos::EpetraVectorOrthogPoly* sg_xdotdot,
      const Stokhos::EpetraVectorOrthogPoly& sg_x,
      const Teuchos::Array<ParamVec>& p,
      const Teuchos::Array<int>& sg_p_index,
      const Teuchos::Array< Teuchos::Array<SGType>>& sg_p_vals,
      ParamVec* deriv_p,
      Stokhos::EpetraVectorOrthogPoly* sg_g,
      const EpetraExt::ModelEvaluator::SGDerivative& sg_dg_dx,
      const EpetraExt::ModelEvaluator::SGDerivative& sg_dg_dxdot,
      const EpetraExt::ModelEvaluator::SGDerivative& sg_dg_dxdotdot,
      const EpetraExt::ModelEvaluator::SGDerivative& sg_dg_dp);

#endif 
#ifdef ALBANY_ENSEMBLE 
    //! Compute global residual for stochastic Galerkin problem
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void computeGlobalMPResidual(
      const double current_time,
      const Stokhos::ProductEpetraVector* mp_xdot,
      const Stokhos::ProductEpetraVector* mp_xdotdot,
      const Stokhos::ProductEpetraVector& mp_x,
      const Teuchos::Array<ParamVec>& p,
      const Teuchos::Array<int>& mp_p_index,
      const Teuchos::Array< Teuchos::Array<MPType>>& mp_p_vals,
      Stokhos::ProductEpetraVector& mp_f);

    //! Compute global Jacobian for stochastic Galerkin problem
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void computeGlobalMPJacobian(
      const double alpha,
      const double beta,
      const double omega,
      const double current_time,
      const Stokhos::ProductEpetraVector* mp_xdot,
      const Stokhos::ProductEpetraVector* mp_xdotdot,
      const Stokhos::ProductEpetraVector& mp_x,
      const Teuchos::Array<ParamVec>& p,
      const Teuchos::Array<int>& mp_p_index,
      const Teuchos::Array< Teuchos::Array<MPType>>& mp_p_vals,
      Stokhos::ProductEpetraVector* mp_f,
      Stokhos::ProductContainer<Epetra_CrsMatrix>& mp_jac);

    //! Compute global Tangent for multi-point problem
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void computeGlobalMPTangent(
      const double alpha,
      const double beta,
      const double omega,
      const double current_time,
      bool sum_derivs,
      const Stokhos::ProductEpetraVector* mp_xdot,
      const Stokhos::ProductEpetraVector* mp_xdotdot,
      const Stokhos::ProductEpetraVector& mp_x,
      const Teuchos::Array<ParamVec>& p,
      const Teuchos::Array<int>& mp_p_index,
      const Teuchos::Array< Teuchos::Array<MPType>>& mp_p_vals,
      ParamVec* deriv_p,
      const Epetra_MultiVector* Vx,
      const Epetra_MultiVector* Vxdot,
      const Epetra_MultiVector* Vxdotdot,
      const Epetra_MultiVector* Vp,
      Stokhos::ProductEpetraVector* mp_f,
      Stokhos::ProductEpetraMultiVector* mp_JVx,
      Stokhos::ProductEpetraMultiVector* mp_fVp);

    //! Evaluate stochastic Galerkin response functions
    /*!
     * Set xdot to NULL for steady-state problems
     */
    void evaluateMPResponse(
      int response_index,
      const double curr_time,
      const Stokhos::ProductEpetraVector* mp_xdot,
      const Stokhos::ProductEpetraVector* mp_xdotdot,
      const Stokhos::ProductEpetraVector& mp_x,
      const Teuchos::Array<ParamVec>& p,
      const Teuchos::Array<int>& mp_p_index,
      const Teuchos::Array< Teuchos::Array<MPType>>& mp_p_vals,
      Stokhos::ProductEpetraVector& mp_g);

    //! Evaluate tangent = dg/dx*dx/dp + dg/dxdot*dxdot/dp + dg/dp
    /*!
     * Set xdot, dxdot_dp to NULL for steady-state problems
     */
    void
    evaluateMPResponseTangent(
      int response_index,
      const double alpha,
      const double beta,
      const double omega,
      const double current_time,
      bool sum_derivs,
      const Stokhos::ProductEpetraVector* mp_xdot,
      const Stokhos::ProductEpetraVector* mp_xdotdot,
      const Stokhos::ProductEpetraVector& mp_x,
      const Teuchos::Array<ParamVec>& p,
      const Teuchos::Array<int>& mp_p_index,
      const Teuchos::Array< Teuchos::Array<MPType>>& mp_p_vals,
      ParamVec* deriv_p,
      const Epetra_MultiVector* Vx,
      const Epetra_MultiVector* Vxdot,
      const Epetra_MultiVector* Vxdotdot,
      const Epetra_MultiVector* Vp,
      Stokhos::ProductEpetraVector* mp_g,
      Stokhos::ProductEpetraMultiVector* mp_JV,
      Stokhos::ProductEpetraMultiVector* mp_gp);

    //! Evaluate gradient = dg/dx, dg/dxdot, dg/dp
    /*!
     * Set xdot, dg_dxdot to NULL for steady-state problems
     */
    void
    evaluateMPResponseDerivative(
      int response_index,
      const double current_time,
      const Stokhos::ProductEpetraVector* mp_xdot,
      const Stokhos::ProductEpetraVector* mp_xdotdot,
      const Stokhos::ProductEpetraVector& mp_x,
      const Teuchos::Array<ParamVec>& p,
      const Teuchos::Array<int>& mp_p_index,
      const Teuchos::Array< Teuchos::Array<MPType>>& mp_p_vals,
      ParamVec* deriv_p,
      Stokhos::ProductEpetraVector* mp_g,
      const EpetraExt::ModelEvaluator::MPDerivative& mp_dg_dx,
      const EpetraExt::ModelEvaluator::MPDerivative& mp_dg_dxdot,
      const EpetraExt::ModelEvaluator::MPDerivative& mp_dg_dxdotdot,
      const EpetraExt::ModelEvaluator::MPDerivative& mp_dg_dp);
#endif

    //! Provide access to shapeParameters -- no AD
    PHAL::AlbanyTraits::Residual::ScalarT& getValue(const std::string &n);

    //! Class to manage state variables (a.k.a. history)
    StateManager& getStateMgr() {return stateMgr; }

#if defined(ALBANY_EPETRA)
    //! Evaluate state field manager
    void evaluateStateFieldManager(const double current_time,
                                   const Epetra_Vector* xdot,
                                   const Epetra_Vector* xdotdot,
                                   const Epetra_Vector& x);
#endif

    //! Evaluate state field manager
    void evaluateStateFieldManagerT(
        const double current_time,
        Teuchos::Ptr<const Tpetra_Vector> xdot,
        Teuchos::Ptr<const Tpetra_Vector> xdotdot,
        const Tpetra_Vector& x);

    void evaluateStateFieldManagerT(
        const double current_time,
        const Tpetra_MultiVector& x);

    //! Access to number of worksets - needed for working with StateManager
    int getNumWorksets() {
        return disc->getWsElNodeEqID().size();
    }

    //! Const access to problem parameter list
    Teuchos::RCP<const Teuchos::ParameterList> getProblemPL() const {
      return problemParams;
    }

    //! Access to problem parameter list
    Teuchos::RCP<Teuchos::ParameterList> getProblemPL() {
      return problemParams;
    }
    
   //! Const access to app parameter list
    Teuchos::RCP<const Teuchos::ParameterList> getAppPL() const {
      return params_;
    }

    //! Access to app parameter list
    Teuchos::RCP<Teuchos::ParameterList> getAppPL() {
      return params_;
    }

#if defined(ALBANY_EPETRA)
    //! Accessor function to Epetra_Import the solution from other PEs for output
    Epetra_Vector* getOverlapSolution(const Epetra_Vector& solution) {
      return solMgr->getOverlapSolution(solution);
    }
#endif

    bool is_adjoint;

  private:

    //! Private to prohibit copying
    Application(const Application&);

    //! Private to prohibit copying
    Application& operator=(const Application&);

#if defined(ALBANY_EPETRA) && defined(ALBANY_TEKO)
    //! Call to Teko to build strided block operator
    Teuchos::RCP<Epetra_Operator> buildWrappedOperator(
                           const Teuchos::RCP<Epetra_Operator>& Jac,
                           const Teuchos::RCP<Epetra_Operator>& wrapInput,
                           bool reorder=false) const;
#endif

    //! Utility function to set up ShapeParameters through Sacado
    void registerShapeParameters();

    void defineTimers();

    void removeEpetraRelatedPLs(const Teuchos::RCP<Teuchos::ParameterList>& params);

  public:


   
    //! Routine to get workset (bucket) size info needed by all Evaluation types
    template <typename EvalT>
    void loadWorksetBucketInfo(PHAL::Workset& workset, const int& ws);

#if defined(ALBANY_EPETRA)
    void loadBasicWorksetInfo(
            PHAL::Workset& workset,
            double current_time);
#endif

    void loadBasicWorksetInfoT(
            PHAL::Workset& workset,
            double current_time);

    void loadBasicWorksetInfoSDBCsT(
            PHAL::Workset& workset,
            Teuchos::RCP<const Tpetra_Vector> owned_sol,
            double current_time);

    void loadWorksetJacobianInfo(PHAL::Workset& workset,
                const double& alpha, const double& beta, const double& omega);

    Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct>>
    getEnrichedMeshSpecs() const {return meshSpecs; } 
 
    //! Routine to load common nodeset info into workset
    void loadWorksetNodesetInfo(PHAL::Workset& workset);

    //! Routine to load common sideset info into workset
    void loadWorksetSidesetInfo(PHAL::Workset& workset, const int ws);

    //! Routines for setting a scaling to be applied to the Jacobian/resdiual 
    void setScale(Teuchos::RCP<Tpetra_CrsMatrix> jacT = Teuchos::null); 
    void setScaleBCDofs(PHAL::Workset& workset);  

#if defined(ALBANY_EPETRA)
    void setupBasicWorksetInfo(
      PHAL::Workset& workset,
      double current_time,
      const Epetra_Vector* xdot,
      const Epetra_Vector* xdotdot,
      const Epetra_Vector* x,
      const Teuchos::Array<ParamVec>& p);
#endif

    void setupBasicWorksetInfoT(
      PHAL::Workset& workset,
      double current_time,
      Teuchos::RCP<const Tpetra_Vector> xdot,
      Teuchos::RCP<const Tpetra_Vector> xdotdot,
      Teuchos::RCP<const Tpetra_Vector> x,
      const Teuchos::Array<ParamVec>& p);

#ifdef ALBANY_SG
    void setupBasicWorksetInfo(
      PHAL::Workset& workset,
      double current_time,
      const Stokhos::EpetraVectorOrthogPoly* sg_xdot,
      const Stokhos::EpetraVectorOrthogPoly* sg_xdotdot,
      const Stokhos::EpetraVectorOrthogPoly* sg_x,
      const Teuchos::Array<ParamVec>& p,
      const Teuchos::Array<int>& sg_p_index,
      const Teuchos::Array< Teuchos::Array<SGType>>& sg_p_vals);
#endif 
#ifdef ALBANY_ENSEMBLE 

    void setupBasicWorksetInfo(
      PHAL::Workset& workset,
      double current_time,
      const Stokhos::ProductEpetraVector* mp_xdot,
      const Stokhos::ProductEpetraVector* mp_xdotdot,
      const Stokhos::ProductEpetraVector* mp_x,
      const Teuchos::Array<ParamVec>& p,
      const Teuchos::Array<int>& mp_p_index,
      const Teuchos::Array< Teuchos::Array<MPType>>& mp_p_vals);
#endif

#if defined(ALBANY_EPETRA)
    void setupTangentWorksetInfo(
      PHAL::Workset& workset,
      double current_time,
      bool sum_derivs,
      const Epetra_Vector* xdot,
      const Epetra_Vector* xdotdot,
      const Epetra_Vector* x,
      const Teuchos::Array<ParamVec>& p,
      ParamVec* deriv_p,
      const Epetra_MultiVector* Vxdot,
      const Epetra_MultiVector* Vxdotdot,
      const Epetra_MultiVector* Vx,
      const Epetra_MultiVector* Vp);
#endif

    void setupTangentWorksetInfoT(
      PHAL::Workset& workset,
      double current_time,
      bool sum_derivs,
      Teuchos::RCP<const Tpetra_Vector> xdotT,
      Teuchos::RCP<const Tpetra_Vector> xdotdotT,
      Teuchos::RCP<const Tpetra_Vector> xT,
      const Teuchos::Array<ParamVec>& p,
      ParamVec* deriv_p,
      Teuchos::RCP<const Tpetra_MultiVector> VxdotT,
      Teuchos::RCP<const Tpetra_MultiVector> VxdotdotT,
      Teuchos::RCP<const Tpetra_MultiVector> VxT,
      Teuchos::RCP<const Tpetra_MultiVector> VpT);

#ifdef ALBANY_SG
    void setupTangentWorksetInfo(
      PHAL::Workset& workset,
      double current_time,
      bool sum_derivs,
      const Stokhos::EpetraVectorOrthogPoly* sg_xdot,
      const Stokhos::EpetraVectorOrthogPoly* sg_xdotdot,
      const Stokhos::EpetraVectorOrthogPoly* sg_x,
      const Teuchos::Array<ParamVec>& p,
      ParamVec* deriv_p,
      const Teuchos::Array<int>& sg_p_index,
      const Teuchos::Array< Teuchos::Array<SGType>>& sg_p_vals,
      const Epetra_MultiVector* Vxdot,
      const Epetra_MultiVector* Vxdotdot,
      const Epetra_MultiVector* Vx,
      const Epetra_MultiVector* Vp);
#endif 
#ifdef ALBANY_ENSEMBLE 

    void setupTangentWorksetInfo(
      PHAL::Workset& workset,
      double current_time,
      bool sum_derivs,
      const Stokhos::ProductEpetraVector* mp_xdot,
      const Stokhos::ProductEpetraVector* mp_xdotdot,
      const Stokhos::ProductEpetraVector* mp_x,
      const Teuchos::Array<ParamVec>& p,
      ParamVec* deriv_p,
      const Teuchos::Array<int>& mp_p_index,
      const Teuchos::Array< Teuchos::Array<MPType>>& mp_p_vals,
      const Epetra_MultiVector* Vxdot,
      const Epetra_MultiVector* Vxdotdot,
      const Epetra_MultiVector* Vx,
      const Epetra_MultiVector* Vp);
#endif

    void postRegSetup(std::string eval);

#ifdef ALBANY_MOR
#if defined(ALBANY_EPETRA)
    Teuchos::RCP<MORFacade> getMorFacade();
#endif
#endif

#if defined(ALBANY_LCM)
  // Needed for coupled Schwarz
  public:
    void
    setApplications(
        Teuchos::ArrayRCP<Teuchos::RCP<Albany::Application>> ca)
    {
      apps_ = ca;
    }

    Teuchos::ArrayRCP<Teuchos::RCP<Albany::Application>>
    getApplications() const
    {
      return apps_;
    }

    void
    setAppIndex(int const i)
    {
      app_index_ = i;
    }

    int
    getAppIndex() const
    {
      return app_index_;
    }

    void
    setAppNameIndexMap(Teuchos::RCP<std::map<std::string, int>> & anim)
    {
      app_name_index_map_ = anim;
    }

    Teuchos::RCP<std::map<std::string, int>>
    getAppNameIndexMap() const
    {
      return app_name_index_map_;
    }

    void
    setCoupledAppBlockNodeset(
        std::string const & app_name,
        std::string const & block_name,
        std::string const & nodeset_name);

    std::string
    getCoupledBlockName(int const app_index) const
    {
      auto it = coupled_app_index_block_nodeset_names_map_.find(app_index);
      assert(it != coupled_app_index_block_nodeset_names_map_.end());
      return it->second.first;
    }

    std::string
    getNodesetName(int const app_index) const
    {
      auto it = coupled_app_index_block_nodeset_names_map_.find(app_index);
      assert(it != coupled_app_index_block_nodeset_names_map_.end());
      return it->second.second;
    }

    bool
    isCoupled(int const app_index) const
    {
      return
        coupled_app_index_block_nodeset_names_map_.find(app_index) !=
        coupled_app_index_block_nodeset_names_map_.end();
    }

    // Few coupled applications, so do this by brute force.
    std::string
    getAppName(int app_index = -1) const
    {
      if (app_index == -1) app_index = this->getAppIndex();

      std::string
      name;

      auto
      it = app_name_index_map_->begin();

      for ( ; it != app_name_index_map_->end(); ++it) {
        if (app_index == it->second) {
          name = it->first;
          break;
        }
      }

      assert(it != app_name_index_map_->end());

      return name;
    }

    Teuchos::RCP<Tpetra_Vector const> const &
    getX() const
    {
      return x_;
    }

    Teuchos::RCP<Tpetra_Vector const> const &
    getXdot() const
    {
      return xdot_;
    }

    Teuchos::RCP<Tpetra_Vector const> const &
    getXdotdot() const
    {
      return xdotdot_;
    }

    void
    setAlternatingSchwarz(bool isa = false)
    {
      is_alternating_schwarz_ = isa;
    }

    bool
    getAlternatingSchwarz() const
    {
      return is_alternating_schwarz_;
    }

    void
    setDBCTime(double t = 0.0)
    {
      dbc_time_ = t;
    }

    double
    getDBCTime() const
    {
      return dbc_time_;
    }

  private:
    Teuchos::ArrayRCP<Teuchos::RCP<Albany::Application>>
    apps_;

    int
    app_index_{-1};

    Teuchos::RCP<std::map<std::string, int>>
    app_name_index_map_{Teuchos::null};

    std::map<int, std::pair<std::string, std::string>>
    coupled_app_index_block_nodeset_names_map_;

    Teuchos::RCP<Tpetra_Vector const>
    x_{Teuchos::null};

    Teuchos::RCP<Tpetra_Vector const>
    xdot_{Teuchos::null};

    Teuchos::RCP<Tpetra_Vector const>
    xdotdot_{Teuchos::null};

    bool
    is_alternating_schwarz_{false};

    double
    dbc_time_{0.0};

#endif //ALBANY_LCM

  protected:

    bool no_dir_bcs_; 
 
    bool loca_sdbcs_valid_nonlin_solver_; 
 
    bool requires_sdbcs_;  
    
    bool requires_orig_dbcs_;  
 
#if defined(ALBANY_EPETRA)
    //! Communicator
    Teuchos::RCP<const Epetra_Comm> comm;
#endif

    //! Tpetra communicator and Kokkos node
    Teuchos::RCP<const Teuchos_Comm> commT;

    //! Output stream, defaults to pronting just Proc 0
    Teuchos::RCP<Teuchos::FancyOStream> out;

    //! Element discretization
    Teuchos::RCP<Albany::AbstractDiscretization> disc;

    //! discretization factory
    Teuchos::RCP<Albany::DiscretizationFactory> discFactory;

    //! mesh specs
    Teuchos::ArrayRCP<Teuchos::RCP<Albany::MeshSpecsStruct>> meshSpecs;

    //! Problem class
    Teuchos::RCP<Albany::AbstractProblem> problem;

    //! Problem Parameters
    Teuchos::RCP<Teuchos::ParameterList> problemParams;
    
    //! App Parameters
    Teuchos::RCP<Teuchos::ParameterList> params_;
    
    //! Parameter library
    Teuchos::RCP<ParamLib> paramLib;

    //! Distributed parameter library
    Teuchos::RCP<DistParamLib> distParamLib;

#if defined(ALBANY_EPETRA)
    //! Solution memory manager
    Teuchos::RCP<AAdapt::AdaptiveSolutionManager> solMgr;
#endif

    //! Solution memory manager
    Teuchos::RCP<AAdapt::AdaptiveSolutionManagerT> solMgrT;

    //! Reference configuration (update) manager
    Teuchos::RCP<AAdapt::rc::Manager> rc_mgr;

    //! Response functions
    Teuchos::Array< Teuchos::RCP<Albany::AbstractResponseFunction>> responses;

    //! Phalanx Field Manager for volumetric fills
    Teuchos::ArrayRCP<Teuchos::RCP<PHX::FieldManager<PHAL::AlbanyTraits>>> fm;

    //! Phalanx Field Manager for Dirichlet Conditions
    Teuchos::RCP<PHX::FieldManager<PHAL::AlbanyTraits>> dfm;

    //! Phalanx Field Manager for Neumann Conditions
    Teuchos::ArrayRCP<Teuchos::RCP<PHX::FieldManager<PHAL::AlbanyTraits>>> nfm;

    //! Phalanx Field Manager for states
    Teuchos::Array< Teuchos::RCP<PHX::FieldManager<PHAL::AlbanyTraits>>> sfm;

#ifdef ALBANY_STOKHOS
    //! Stochastic Galerkin basis
    Teuchos::RCP<const Stokhos::OrthogPolyBasis<int,double>> sg_basis;

    //! Stochastic Galerkin quadrature
    Teuchos::RCP<const Stokhos::Quadrature<int,double>> sg_quad;

    //! Stochastic Galerkin expansion
    Teuchos::RCP<Stokhos::OrthogPolyExpansion<int,double>> sg_expansion;
#endif

#if defined(ALBANY_EPETRA)
    //! Product multi-comm
    Teuchos::RCP<const EpetraExt::MultiComm> product_comm;

    //! Overlap stochastic map
    Teuchos::RCP<const Epetra_BlockMap> sg_overlap_map;

#ifdef ALBANY_STOKHOS
    //! SG overlapped solution vectors
    Teuchos::RCP< Stokhos::EpetraVectorOrthogPoly >  sg_overlapped_x;

    //! SG overlapped time derivative vectors
    Teuchos::RCP< Stokhos::EpetraVectorOrthogPoly > sg_overlapped_xdot;
    Teuchos::RCP< Stokhos::EpetraVectorOrthogPoly > sg_overlapped_xdotdot;

    //! SG overlapped residual vectors
    Teuchos::RCP< Stokhos::EpetraVectorOrthogPoly > sg_overlapped_f;

    //! Overlapped Jacobian matrixs
    Teuchos::RCP< Stokhos::VectorOrthogPoly<Epetra_CrsMatrix>> sg_overlapped_jac;

    //! MP overlapped solution vectors
    Teuchos::RCP< Stokhos::ProductEpetraVector >  mp_overlapped_x;

    //! MP overlapped time derivative vectors
    Teuchos::RCP< Stokhos::ProductEpetraVector > mp_overlapped_xdot;
    Teuchos::RCP< Stokhos::ProductEpetraVector > mp_overlapped_xdotdot;

    //! MP overlapped residual vectors
    Teuchos::RCP< Stokhos::ProductEpetraVector > mp_overlapped_f;

    //! Overlapped Jacobian matrixs
    Teuchos::RCP< Stokhos::ProductContainer<Epetra_CrsMatrix>> mp_overlapped_jac;
#endif
#endif

    bool explicit_scheme; 

    //! Data for Physics-Based Preconditioners
    bool physicsBasedPreconditioner;
    Teuchos::RCP<Teuchos::ParameterList> precParams;
    std::string precType;

    //! Type of solution method
    SolutionMethod solMethod;

    //! Integer specifying whether user wants to write Jacobian to MatrixMarket file
    // writeToMatrixMarketJac = 0: no writing to MatrixMarket (default)
    // writeToMatrixMarketJac =-1: write to MatrixMarket every time a Jacobian arises
    // writeToMatrixMarketJac = N: write N^th Jacobian to MatrixMarket
    // ...and similarly for writeToMatrixMarketRes (integer specifying whether user wants to write
    // residual to MatrixMarket file)
    int writeToMatrixMarketJac;
    int writeToMatrixMarketRes;
    int computeJacCondNum; 
    //! Integer specifying whether user wants to write Jacobian and residual to Standard output (cout)
    int writeToCoutJac;
    int writeToCoutRes;

    //Value to scale Jacobian/Residual by to possibly improve conditioning
    double scale; 
    double scaleBCdofs; 
    //Scaling types
    enum SCALETYPE {CONSTANT, DIAG, ABSROWSUM};
    SCALETYPE scale_type;

    //! Shape Optimization data
    bool shapeParamsHaveBeenReset;
    std::vector<RealType> shapeParams;
    std::vector<std::string> shapeParamNames;
#ifdef ALBANY_CUTR
    Teuchos::RCP<CUTR::CubitMeshMover> meshMover;
#endif

    unsigned int neq, spatial_dimension, tangent_deriv_dim;

#if defined(ALBANY_EPETRA) && defined(ALBANY_TEKO)
    //! Teko stuff
    Teuchos::RCP<Teko::InverseLibrary> inverseLib;
    Teuchos::RCP<Teko::InverseFactory> inverseFac;
    Teuchos::RCP<Epetra_Operator> wrappedJac;
#endif
    std::vector<int> blockDecomp;

    std::set<std::string> setupSet;
    mutable int phxGraphVisDetail;
    mutable int stateGraphVisDetail;

    StateManager stateMgr;

    bool morphFromInit;
    bool ignore_residual_in_jacobian;

    //! To prevent a singular mass matrix associated with Dirichlet
    //  conditions, optionally add a small perturbation to the diag
    double perturbBetaForDirichlets;

    void determinePiroSolver(const Teuchos::RCP<Teuchos::ParameterList>& topLevelParams);

#ifdef ALBANY_MOR
#if defined(ALBANY_EPETRA)
    Teuchos::RCP<MORFacade> morFacade;
#endif
#endif

    int derivatives_check_;

    int num_time_deriv;
    
    //The following are for Jacobian/residual scaling 
    Teuchos::Array<Teuchos::Array<int>> offsets_;
    Teuchos::RCP<Tpetra_Vector> scaleVec_;  

    //boolean read from input file telling code whether to compute/print responses every step 
    bool observe_responses; 
    
    //how often one wants the responses to be computed/printed
    int response_observ_freq; 

    //local responses
    Teuchos::Array<unsigned int> relative_responses;

  };
}

template <typename EvalT>
void Albany::Application::loadWorksetBucketInfo(PHAL::Workset& workset,
                                                const int& ws)
{
  const auto& wsElNodeEqID = disc->getWsElNodeEqID();
  const auto& wsElNodeID = disc->getWsElNodeID();
  const auto& coords = disc->getCoords();
  const auto& wsEBNames = disc->getWsEBNames();
  const auto& sphereVolume = disc->getSphereVolume();
  const auto& latticeOrientation = disc->getLatticeOrientation();

  workset.numCells = wsElNodeEqID[ws].dimension(0);
  workset.wsElNodeEqID = wsElNodeEqID[ws];
  workset.wsElNodeID = wsElNodeID[ws];
  workset.wsCoords = coords[ws];
  workset.wsSphereVolume = sphereVolume[ws];
  workset.wsLatticeOrientation = latticeOrientation[ws];
  workset.EBName = wsEBNames[ws];
  workset.wsIndex = ws;

  workset.local_Vp.resize(workset.numCells);

//  workset.print(*out);

  // Sidesets are integrated within the Cells
  loadWorksetSidesetInfo(workset, ws);

  workset.stateArrayPtr = &stateMgr.getStateArray(Albany::StateManager::ELEM, ws);
#if defined(ALBANY_EPETRA)
  workset.disc = disc;  // Needed by FELIX for sideset DOF save
  workset.eigenDataPtr = stateMgr.getEigenData();
  workset.auxDataPtr = stateMgr.getAuxData();
#endif
 //FIXME, 6/25: This line was causing link error.  Need to figure out why. 
 // workset.auxDataPtrT = stateMgr.getAuxDataT();

}

#endif // ALBANY_APPLICATION_HPP
