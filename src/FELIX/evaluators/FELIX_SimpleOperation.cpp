//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "PHAL_AlbanyTraits.hpp"

#include "FELIX_SimpleOperation.hpp"
#include "FELIX_SimpleOperation_Def.hpp"

PHAL_INSTANTIATE_TEMPLATE_CLASS_WITH_ONE_SCALAR_TYPE(FELIX::SimpleOperationLog)
PHAL_INSTANTIATE_TEMPLATE_CLASS_WITH_ONE_SCALAR_TYPE(FELIX::SimpleOperationExp)
PHAL_INSTANTIATE_TEMPLATE_CLASS_WITH_EXTRA_ARGS(FELIX::SimpleOperationBase,FadType,FELIX::SimpleOps::Log<FadType>)
PHAL_INSTANTIATE_TEMPLATE_CLASS_WITH_EXTRA_ARGS(FELIX::SimpleOperationBase,RealType,FELIX::SimpleOps::Log<RealType>)
PHAL_INSTANTIATE_TEMPLATE_CLASS_WITH_EXTRA_ARGS(FELIX::SimpleOperationBase,FadType,FELIX::SimpleOps::Exp<FadType>)
PHAL_INSTANTIATE_TEMPLATE_CLASS_WITH_EXTRA_ARGS(FELIX::SimpleOperationBase,RealType,FELIX::SimpleOps::Exp<RealType>)
