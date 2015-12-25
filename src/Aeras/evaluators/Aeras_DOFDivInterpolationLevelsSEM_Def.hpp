//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Teuchos_TestForException.hpp"
#include "Phalanx_DataLayout.hpp"
#include "PHAL_Utilities.hpp"

#include "Intrepid2_FunctionSpaceTools.hpp"

namespace Aeras {

//**********************************************************************
template<typename EvalT, typename Traits>
DOFDivInterpolationLevelsSEM<EvalT, Traits>::
DOFDivInterpolationLevelsSEM(Teuchos::ParameterList& p,
                     const Teuchos::RCP<Aeras::Layouts>& dl) :
  val_node   (p.get<std::string>   ("Variable Name"),          dl->node_vector_level),
  GradBF     (p.get<std::string>   ("Gradient BF Name"),       dl->node_qp_gradient),
  jacobian_det  (p.get<std::string>  ("Jacobian Det Name"), dl->qp_scalar ),
  jacobian_inv  (p.get<std::string>  ("Jacobian Inv Name"), dl->qp_tensor ),
  //add grad_at_cub_points
  div_val_qp (p.get<std::string>   ("Divergence SEM Variable Name"), dl->qp_scalar_level),
  numNodes   (dl->node_scalar             ->dimension(1)),
  numDims    (dl->node_qp_gradient        ->dimension(3)),
  numQPs     (dl->node_qp_scalar          ->dimension(2)),
  numLevels  (dl->node_scalar_level       ->dimension(2))
{
  this->addDependentField(val_node);
  this->addDependentField(GradBF);
  this->addDependentField(jacobian_det);
  this->addDependentField(jacobian_inv);
  //add dependent field grad_at_cub_points
  this->addEvaluatedField(div_val_qp);

  this->setName("Aeras::DOFDivInterpolationLevelsSEM"+PHX::typeAsString<EvalT>());
  //std::cout<< "Aeras::DOFDivInterpolationLevels: " << numNodes << " " << numDims << " " << numQPs << " " << numLevels << std::endl;
}

//**********************************************************************
template<typename EvalT, typename Traits>
void DOFDivInterpolationLevelsSEM<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(val_node,fm);
  this->utils.setFieldData(GradBF,fm);
  this->utils.setFieldData(jacobian_det,fm);
  this->utils.setFieldData(jacobian_inv,fm);
  //add grad_at_cub_points
  this->utils.setFieldData(div_val_qp,fm);
}

//**********************************************************************
template<typename EvalT, typename Traits>
void DOFDivInterpolationLevelsSEM<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  PHAL::set(div_val_qp, 0.0);
  for (int cell=0; cell < workset.numCells; ++cell) 
    for (int qp=0; qp < numQPs; ++qp) 
      for (int node= 0 ; node < numNodes; ++node) 
        for (int level=0; level < numLevels; ++level) 
          for (int dim=0; dim<numDims; dim++) {
            div_val_qp(cell,qp,level) += val_node(cell,node,level,dim) * GradBF(cell,node,qp,dim);
            //std::cout << "gradbf: " << cell << " " << node << " " << qp << " " << dim << " " << GradBF(cell,node,qp,dim) << std::endl;
            //std::cout << "val_node " << val_node(cell,node,level,dim) << std::endl;

         }
}

}
