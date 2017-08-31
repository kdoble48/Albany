//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

//IK, 9/12/14: Epetra ifdef'ed out if ALBANY_EPETRA_EXE turned off

#include <iostream>
#include <string>

#include "Albany_MultiSTKFieldContainer.hpp"

#include "Albany_Utils.hpp"
#include <stk_mesh/base/GetBuckets.hpp>

#ifdef ALBANY_SEACAS
#include <stk_io/IossBridge.hpp>
#endif

#include "Teuchos_VerboseObject.hpp"

static const char *sol_tag_name[3] = {
      "Exodus Solution Name",
      "Exodus SolutionDot Name",
      "Exodus SolutionDotDot Name"
      };

static const char *sol_id_name[3] = {
      "solution",
      "solution_dot",
      "solution_dotdot"
      };

static const char *res_tag_name = {
      "Exodus Residual Name",
      };

static const char *res_id_name = {
      "residual",
      };

template<bool Interleaved>
Albany::MultiSTKFieldContainer<Interleaved>::MultiSTKFieldContainer(
  const Teuchos::RCP<Teuchos::ParameterList>& params_,
  const Teuchos::RCP<stk::mesh::MetaData>& metaData_,
  const Teuchos::RCP<stk::mesh::BulkData>& bulkData_,
  const int neq_,
  const AbstractFieldContainer::FieldContainerRequirements& req,
  const int numDim_,
  const Teuchos::RCP<Albany::StateInfoStruct>& sis,
  const Teuchos::Array<Teuchos::Array<std::string> >& solution_vector,
  const Teuchos::Array<std::string>& residual_vector)
  : GenericSTKFieldContainer<Interleaved>(params_, metaData_, bulkData_, neq_, numDim_),
    haveResidual(false), buildSphereVolume(false), buildLatticeOrientation(false) {

  typedef typename AbstractSTKFieldContainer::VectorFieldType VFT;
  typedef typename AbstractSTKFieldContainer::ScalarFieldType SFT;
  typedef typename AbstractSTKFieldContainer::SphereVolumeFieldType SVFT;

  sol_vector_name.resize(solution_vector.size());
  sol_index.resize(solution_vector.size());

  // Check the input

  for(int vec_num = 0; vec_num < solution_vector.size(); vec_num++){

    if(solution_vector[vec_num].size() == 0) { // Do the default solution vector

      std::string name = params_->get<std::string>(sol_tag_name[vec_num], sol_id_name[vec_num]);
      VFT* solution = & metaData_->declare_field< VFT >(stk::topology::NODE_RANK, name);
      stk::mesh::put_field(*solution, metaData_->universal_part(), neq_);
#ifdef ALBANY_SEACAS
      stk::io::set_field_role(*solution, Ioss::Field::TRANSIENT);
#endif

      sol_vector_name[vec_num].push_back(name);
      sol_index[vec_num].push_back(this->neq);

    }

    else if(solution_vector[vec_num].size() == 1) { // User is just renaming the entire solution vector

      VFT* solution = & metaData_->declare_field< VFT >(stk::topology::NODE_RANK, solution_vector[vec_num][0]);
      stk::mesh::put_field(*solution, metaData_->universal_part(), neq_);
#ifdef ALBANY_SEACAS
      stk::io::set_field_role(*solution, Ioss::Field::TRANSIENT);
#endif

      sol_vector_name[vec_num].push_back(solution_vector[vec_num][0]);
      sol_index[vec_num].push_back(neq_);

    }

    else { // user is breaking up the solution into multiple fields

      // make sure the number of entries is even

      TEUCHOS_TEST_FOR_EXCEPTION((solution_vector[vec_num].size() % 2), std::logic_error,
                               "Error in input file: specification of solution vector layout is incorrect." << std::endl);

      int len, accum = 0;

      for(int i = 0; i < solution_vector[vec_num].size(); i += 2) {

        if(solution_vector[vec_num][i + 1] == "V") {

          len = numDim_; // vector
          accum += len;
          VFT* solution = & metaData_->declare_field< VFT >(stk::topology::NODE_RANK, solution_vector[vec_num][i]);
          stk::mesh::put_field(*solution, metaData_->universal_part(), len);
#ifdef ALBANY_SEACAS
          stk::io::set_field_role(*solution, Ioss::Field::TRANSIENT);
#endif
          sol_vector_name[vec_num].push_back(solution_vector[vec_num][i]);
          sol_index[vec_num].push_back(len);

        }

        else if(solution_vector[vec_num][i + 1] == "S") {

          len = 1; // scalar
          accum += len;
          SFT* solution = & metaData_->declare_field< SFT >(stk::topology::NODE_RANK, solution_vector[vec_num][i]);
          stk::mesh::put_field(*solution, metaData_->universal_part());
#ifdef ALBANY_SEACAS
          stk::io::set_field_role(*solution, Ioss::Field::TRANSIENT);
#endif
          sol_vector_name[vec_num].push_back(solution_vector[vec_num][i]);
          sol_index[vec_num].push_back(len);

        }

        else

          TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
                                   "Error in input file: specification of solution vector layout is incorrect." << std::endl);

      }

      TEUCHOS_TEST_FOR_EXCEPTION(accum != neq_, std::logic_error,
                               "Error in input file: specification of solution vector layout is incorrect." << std::endl);

    }
  }

#if defined(ALBANY_LCM)
  // do the residual next

  if(residual_vector.size() == 0) { // Do the default residual vector

    std::string name = params_->get<std::string>(res_tag_name, res_id_name);
    VFT* residual = & metaData_->declare_field< VFT >(stk::topology::NODE_RANK, name);
    stk::mesh::put_field(*residual, metaData_->universal_part(), neq_);
#ifdef ALBANY_SEACAS
    stk::io::set_field_role(*residual, Ioss::Field::TRANSIENT);
#endif

    res_vector_name.push_back(name);
    res_index.push_back(neq_);

  }

  else if(residual_vector.size() == 1) { // User is just renaming the entire residual vector

    VFT* residual = & metaData_->declare_field< VFT >(stk::topology::NODE_RANK, residual_vector[0]);
    stk::mesh::put_field(*residual, metaData_->universal_part(), neq_);
#ifdef ALBANY_SEACAS
    stk::io::set_field_role(*residual, Ioss::Field::TRANSIENT);
#endif

    res_vector_name.push_back(residual_vector[0]);
    res_index.push_back(neq_);

  }

  else { // user is breaking up the residual into multiple fields

    // make sure the number of entries is even

    TEUCHOS_TEST_FOR_EXCEPTION((residual_vector.size() % 2), std::logic_error,
                               "Error in input file: specification of residual vector layout is incorrect." << std::endl);

    int len, accum = 0;

    for(int i = 0; i < residual_vector.size(); i += 2) {

      if(residual_vector[i + 1] == "V") {

        len = numDim_; // vector
        accum += len;
        VFT* residual = & metaData_->declare_field< VFT >(stk::topology::NODE_RANK, residual_vector[i]);
        stk::mesh::put_field(*residual, metaData_->universal_part(), len);
#ifdef ALBANY_SEACAS
        stk::io::set_field_role(*residual, Ioss::Field::TRANSIENT);
#endif
        res_vector_name.push_back(residual_vector[i]);
        res_index.push_back(len);

      }

      else if(residual_vector[i + 1] == "S") {

        len = 1; // scalar
        accum += len;
        SFT* residual = & metaData_->declare_field< SFT >(stk::topology::NODE_RANK, residual_vector[i]);
        stk::mesh::put_field(*residual, metaData_->universal_part());
#ifdef ALBANY_SEACAS
        stk::io::set_field_role(*residual, Ioss::Field::TRANSIENT);
#endif
        res_vector_name.push_back(residual_vector[i]);
        res_index.push_back(len);

      }

      else

        TEUCHOS_TEST_FOR_EXCEPTION(true, std::logic_error,
                                   "Error in input file: specification of residual vector layout is incorrect." << std::endl);

    }

    TEUCHOS_TEST_FOR_EXCEPTION(accum != neq_, std::logic_error,
                               "Error in input file: specification of residual vector layout is incorrect." << std::endl);

  }

  haveResidual = true;

#endif

  //Do the coordinates
  this->coordinates_field = & metaData_->declare_field< VFT >(stk::topology::NODE_RANK, "coordinates");
  stk::mesh::put_field(*this->coordinates_field , metaData_->universal_part(), numDim_);
#ifdef ALBANY_SEACAS
  stk::io::set_field_role(*this->coordinates_field, Ioss::Field::MESH);
#endif

  if (numDim_==3)
  {
    this->coordinates_field3d = this->coordinates_field;
  }
  else
  {
    this->coordinates_field3d = & metaData_->declare_field< VFT >(stk::topology::NODE_RANK, "coordinates3d");
    stk::mesh::put_field(*this->coordinates_field3d , metaData_->universal_part(), 3);
#ifdef ALBANY_SEACAS
    stk::io::set_field_role(*this->coordinates_field3d, Ioss::Field::MESH);
#endif
  }

#if defined(ALBANY_LCM) && defined(ALBANY_SEACAS)
  // sphere volume is a mesh attribute read from a genesis mesh file containing sphere element (used for peridynamics)
  this->sphereVolume_field = metaData_->template get_field< SVFT >(stk::topology::ELEMENT_RANK, "volume");
  if(this->sphereVolume_field != 0){
    buildSphereVolume = true;
    stk::io::set_field_role(*this->sphereVolume_field, Ioss::Field::ATTRIBUTE);
  }
  // lattice orientation is mesh attributes read from a genesis mesh file use with certain solid mechanics material models
  bool hasLatticeOrientationFieldContainerRequirement = (std::find(req.begin(), req.end(), "Lattice_Orientation") != req.end());
  if(hasLatticeOrientationFieldContainerRequirement){
    // STK says that attributes are of type Field<double,anonymous>[ name: "extra_attribute_3" , #states: 1 ]
    this->latticeOrientation_field = metaData_->template get_field<stk::mesh::FieldBase>(stk::topology::ELEMENT_RANK, "extra_attribute_9");
    if(this->latticeOrientation_field != 0){
      buildLatticeOrientation = true;
      stk::io::set_field_role(*this->latticeOrientation_field, Ioss::Field::ATTRIBUTE);
    }
  }
#endif

  this->addStateStructs(sis);

  initializeSTKAdaptation();

}

template<bool Interleaved>
Albany::MultiSTKFieldContainer<Interleaved>::~MultiSTKFieldContainer() {
}

template<bool Interleaved>
void Albany::MultiSTKFieldContainer<Interleaved>::initializeSTKAdaptation() {

  typedef typename AbstractSTKFieldContainer::IntScalarFieldType ISFT;

  this->proc_rank_field =
    & this->metaData->template declare_field< ISFT >(stk::topology::ELEMENT_RANK, "proc_rank");

  this->refine_field =
    & this->metaData->template declare_field< ISFT >(stk::topology::ELEMENT_RANK, "refine_field");

  // Processor rank field, a scalar
  stk::mesh::put_field(
      *this->proc_rank_field,
      this->metaData->universal_part());

  stk::mesh::put_field(
      *this->refine_field,
      this->metaData->universal_part());

#if defined(ALBANY_LCM)
  // Fracture state used for adaptive insertion.
  // It exists for all entities except cells (elements).

  for (stk::mesh::EntityRank rank = stk::topology::NODE_RANK; rank < stk::topology::ELEMENT_RANK; ++rank) {
    this->fracture_state[rank] = &this->metaData->template declare_field< ISFT >(rank, "fracture_state");
    stk::mesh::put_field(
        *this->fracture_state[rank],
        this->metaData->universal_part());

  }
#endif // ALBANY_LCM


#ifdef ALBANY_SEACAS
  stk::io::set_field_role(*this->proc_rank_field, Ioss::Field::MESH);
  stk::io::set_field_role(*this->refine_field, Ioss::Field::MESH);
#if defined(ALBANY_LCM)
  for (stk::mesh::EntityRank rank = stk::topology::NODE_RANK; rank < stk::topology::ELEMENT_RANK; ++rank) {
    stk::io::set_field_role(*this->fracture_state[rank], Ioss::Field::MESH);
  }
#endif // ALBANY_LCM
#endif

}

#if defined(ALBANY_EPETRA)
template<bool Interleaved>
void Albany::MultiSTKFieldContainer<Interleaved>::fillSolnVector(Epetra_Vector& soln,
    stk::mesh::Selector& sel, const Teuchos::RCP<const Epetra_Map>& node_map) {

  typedef typename AbstractSTKFieldContainer::VectorFieldType VFT;
  typedef typename AbstractSTKFieldContainer::ScalarFieldType SFT;

  // Iterate over the on-processor nodes by getting node buckets and iterating over each bucket.
  stk::mesh::BucketVector const& all_elements = this->bulkData->get_buckets(stk::topology::NODE_RANK, sel);
  this->numNodes = node_map->NumMyElements(); // Needed for the getDOF function to work correctly
  // This is either numOwnedNodes or numOverlapNodes, depending on
  // which map is passed in

  for(stk::mesh::BucketVector::const_iterator it = all_elements.begin() ; it != all_elements.end() ; ++it) {

    const stk::mesh::Bucket& bucket = **it;

    int offset = 0;

    for(int k = 0; k < sol_index[0].size(); k++) {

      if(sol_index[0][k] == 1) { // Scalar

        SFT* field = this->metaData->template get_field<SFT>(stk::topology::NODE_RANK, sol_vector_name[0][k]);
        this->fillVectorHelper(soln, field, node_map, bucket, offset);

      }

      else {

        VFT* field = this->metaData->template get_field<VFT>(stk::topology::NODE_RANK, sol_vector_name[0][k]);
        this->fillVectorHelper(soln, field, node_map, bucket, offset);

      }

      offset += sol_index[0][k];

    }

  }
}

template<bool Interleaved>
void Albany::MultiSTKFieldContainer<Interleaved>::fillVector(Epetra_Vector& field_vector, const std::string&  field_name,
    stk::mesh::Selector& field_selection, const Teuchos::RCP<const Epetra_Map>& field_node_map, const NodalDOFManager& nodalDofManager) {

  // Iterate over the on-processor nodes by getting node buckets and iterating over each bucket.

  stk::mesh::BucketVector const& all_elements = this->bulkData->get_buckets(stk::topology::NODE_RANK, field_selection);

  if(nodalDofManager.numComponents() > 1) {
    AbstractSTKFieldContainer::VectorFieldType* field  = this->metaData->template get_field<AbstractSTKFieldContainer::VectorFieldType>(stk::topology::NODE_RANK, field_name);
    for(stk::mesh::BucketVector::const_iterator it = all_elements.begin() ; it != all_elements.end() ; ++it) {
      const stk::mesh::Bucket& bucket = **it;
      this->fillVectorHelper(field_vector, field, field_node_map, bucket, nodalDofManager);
    }
  }
  else {
    AbstractSTKFieldContainer::ScalarFieldType* field  = this->metaData->template get_field<AbstractSTKFieldContainer::ScalarFieldType>(stk::topology::NODE_RANK, field_name);
    for(stk::mesh::BucketVector::const_iterator it = all_elements.begin() ; it != all_elements.end() ; ++it) {
      const stk::mesh::Bucket& bucket = **it;
      this->fillVectorHelper(field_vector, field, field_node_map, bucket, nodalDofManager);
    }
  }
}
#endif

template<bool Interleaved>
void Albany::MultiSTKFieldContainer<Interleaved>::fillVectorT(Tpetra_Vector& field_vector, const std::string&  field_name,
    stk::mesh::Selector& field_selection, const Teuchos::RCP<const Tpetra_Map>& field_node_map, const NodalDOFManager& nodalDofManager) {

  // Iterate over the on-processor nodes by getting node buckets and iterating over each bucket.

  stk::mesh::BucketVector const& all_elements = this->bulkData->get_buckets(stk::topology::NODE_RANK, field_selection);

  if(nodalDofManager.numComponents() > 1) {
    AbstractSTKFieldContainer::VectorFieldType* field  = this->metaData->template get_field<AbstractSTKFieldContainer::VectorFieldType>(stk::topology::NODE_RANK, field_name);
    for(stk::mesh::BucketVector::const_iterator it = all_elements.begin() ; it != all_elements.end() ; ++it) {
      const stk::mesh::Bucket& bucket = **it;
      this->fillVectorHelperT(field_vector, field, field_node_map, bucket, nodalDofManager);
    }
  }
  else {
    AbstractSTKFieldContainer::ScalarFieldType* field  = this->metaData->template get_field<AbstractSTKFieldContainer::ScalarFieldType>(stk::topology::NODE_RANK, field_name);
    for(stk::mesh::BucketVector::const_iterator it = all_elements.begin() ; it != all_elements.end() ; ++it) {
      const stk::mesh::Bucket& bucket = **it;
      this->fillVectorHelperT(field_vector, field, field_node_map, bucket, nodalDofManager);
    }
  }
}


template<bool Interleaved>
void Albany::MultiSTKFieldContainer<Interleaved>::fillSolnVectorT(Tpetra_Vector &solnT,
       stk::mesh::Selector &sel, const Teuchos::RCP<const Tpetra_Map>& node_mapT){

  typedef typename AbstractSTKFieldContainer::VectorFieldType VFT;
  typedef typename AbstractSTKFieldContainer::ScalarFieldType SFT;

  // Iterate over the on-processor nodes by getting node buckets and iterating over each bucket.
  stk::mesh::BucketVector const& all_elements = this->bulkData->get_buckets(stk::topology::NODE_RANK, sel);
  this->numNodes = node_mapT->getNodeNumElements(); // Needed for the getDOF function to work correctly
                                        // This is either numOwnedNodes or numOverlapNodes, depending on
                                        // which map is passed in

  for(stk::mesh::BucketVector::const_iterator it = all_elements.begin() ; it != all_elements.end() ; ++it) {

    const stk::mesh::Bucket& bucket = **it;

    int offset = 0;

    for(int k = 0; k < sol_index[0].size(); k++){

       if(sol_index[0][k] == 1){ // Scalar

          SFT* field = this->metaData->template get_field<SFT>(stk::topology::NODE_RANK, sol_vector_name[0][k]);
          this->fillVectorHelperT(solnT, field, node_mapT, bucket, offset);

       }
       else {

          VFT* field = this->metaData->template get_field<VFT>(stk::topology::NODE_RANK, sol_vector_name[0][k]);
          this->fillVectorHelperT(solnT, field, node_mapT, bucket, offset);

       }

       offset += sol_index[0][k];

    }

  }
}

template<bool Interleaved>
void Albany::MultiSTKFieldContainer<Interleaved>::fillSolnMultiVector(Tpetra_MultiVector &solnT,
       stk::mesh::Selector &sel, const Teuchos::RCP<const Tpetra_Map>& node_mapT){

  typedef typename AbstractSTKFieldContainer::VectorFieldType VFT;
  typedef typename AbstractSTKFieldContainer::ScalarFieldType SFT;

  // Iterate over the on-processor nodes by getting node buckets and iterating over each bucket.
  stk::mesh::BucketVector const& all_elements = this->bulkData->get_buckets(stk::topology::NODE_RANK, sel);
  this->numNodes = node_mapT->getNodeNumElements(); // Needed for the getDOF function to work correctly
                                        // This is either numOwnedNodes or numOverlapNodes, depending on
                                        // which map is passed in

  for(stk::mesh::BucketVector::const_iterator it = all_elements.begin() ; it != all_elements.end() ; ++it) {

    const stk::mesh::Bucket& bucket = **it;

    for(int vector_component = 0; vector_component < solnT.getNumVectors(); vector_component++){

      int offset = 0;

      for(int k = 0; k < sol_index[vector_component].size(); k++){

         if(sol_index[vector_component][k] == 1){ // Scalar

            SFT* field = this->metaData->template get_field<SFT>(
                              stk::topology::NODE_RANK, sol_vector_name[vector_component][k]);
            this->fillMultiVectorHelper(solnT, field, node_mapT, bucket, vector_component, offset);

         }
         else {

            VFT* field = this->metaData->template get_field<VFT>(
                              stk::topology::NODE_RANK, sol_vector_name[vector_component][k]);
            this->fillMultiVectorHelper(solnT, field, node_mapT, bucket, vector_component, offset);

         }

         offset += sol_index[vector_component][k];

      }

    }

  }
}

#if defined(ALBANY_EPETRA)
template<bool Interleaved>
void Albany::MultiSTKFieldContainer<Interleaved>::saveSolnVector(const Epetra_Vector& soln,
    stk::mesh::Selector& sel, const Teuchos::RCP<const Epetra_Map>& node_map) {

  typedef typename AbstractSTKFieldContainer::VectorFieldType VFT;
  typedef typename AbstractSTKFieldContainer::ScalarFieldType SFT;

  // Iterate over the on-processor nodes by getting node buckets and iterating over each bucket.
  stk::mesh::BucketVector const& all_elements = this->bulkData->get_buckets(stk::topology::NODE_RANK, sel);
  this->numNodes = node_map->NumMyElements(); // Needed for the getDOF function to work correctly
  // This is either numOwnedNodes or numOverlapNodes, depending on
  // which map is passed in

  for(stk::mesh::BucketVector::const_iterator it = all_elements.begin() ; it != all_elements.end() ; ++it) {

    const stk::mesh::Bucket& bucket = **it;

    int offset = 0;

    for(int k = 0; k < sol_index[0].size(); k++) {

      if(sol_index[0][k] == 1) { // Scalar

        SFT* field = this->metaData->template get_field<SFT>(stk::topology::NODE_RANK, sol_vector_name[0][k]);
        this->saveVectorHelper(soln, field, node_map, bucket, offset);

      }

      else {

        VFT* field = this->metaData->template get_field<VFT>(stk::topology::NODE_RANK, sol_vector_name[0][k]);
        this->saveVectorHelper(soln, field, node_map, bucket, offset);

      }

      offset += sol_index[0][k];

    }

  }
}

template<bool Interleaved>
void Albany::MultiSTKFieldContainer<Interleaved>::saveVector(const Epetra_Vector& field_vector, const std::string&  field_name,
    stk::mesh::Selector& field_selection, const Teuchos::RCP<const Epetra_Map>& field_node_map, const NodalDOFManager& nodalDofManager) {

  // Iterate over the on-processor nodes by getting node buckets and iterating over each bucket.
  stk::mesh::BucketVector const& all_elements = this->bulkData->get_buckets(stk::topology::NODE_RANK, field_selection);

  if(nodalDofManager.numComponents() > 1) {
    AbstractSTKFieldContainer::VectorFieldType* field  = this->metaData->template get_field<AbstractSTKFieldContainer::VectorFieldType>(stk::topology::NODE_RANK, field_name);
    for(stk::mesh::BucketVector::const_iterator it = all_elements.begin() ; it != all_elements.end() ; ++it) {
      const stk::mesh::Bucket& bucket = **it;
      this->saveVectorHelper(field_vector, field, field_node_map, bucket, nodalDofManager);
    }
  }
  else {
    AbstractSTKFieldContainer::ScalarFieldType* field  = this->metaData->template get_field<AbstractSTKFieldContainer::ScalarFieldType>(stk::topology::NODE_RANK, field_name);
    for(stk::mesh::BucketVector::const_iterator it = all_elements.begin() ; it != all_elements.end() ; ++it) {
      const stk::mesh::Bucket& bucket = **it;
      this->saveVectorHelper(field_vector, field, field_node_map, bucket, nodalDofManager);
    }
  }
}
#endif


//Tpetra version of above
template<bool Interleaved>
void Albany::MultiSTKFieldContainer<Interleaved>::saveSolnVectorT(const Tpetra_Vector& solnT,
    stk::mesh::Selector& sel, const Teuchos::RCP<const Tpetra_Map>& node_mapT)
{
  typedef typename AbstractSTKFieldContainer::VectorFieldType VFT;
  typedef typename AbstractSTKFieldContainer::ScalarFieldType SFT;

  // Iterate over the on-processor nodes by getting node buckets and iterating over each bucket.
  stk::mesh::BucketVector const& all_elements = this->bulkData->get_buckets(stk::topology::NODE_RANK, sel);


  this->numNodes = node_mapT->getNodeNumElements(); // Needed for the getDOF function to work correctly
  // This is either numOwnedNodes or numOverlapNodes, depending on
  // which map is passed in

  for (stk::mesh::BucketVector::const_iterator it = all_elements.begin() ; it != all_elements.end() ; ++it) {
    const stk::mesh::Bucket& bucket = **it;
    int offset = 0;
    for(int k = 0; k < sol_index[0].size(); k++) {
      if(sol_index[0][k] == 1) { // Scalar
        SFT* field = this->metaData->template get_field<SFT>(stk::topology::NODE_RANK, sol_vector_name[0][k]);
        this->saveVectorHelperT(solnT, field, node_mapT, bucket, offset);
      }
      else {
        VFT* field = this->metaData->template get_field<VFT>(stk::topology::NODE_RANK, sol_vector_name[0][k]);
        this->saveVectorHelperT(solnT, field, node_mapT, bucket, offset);
      }
      offset += sol_index[0][k];
    }
  }
}

template<bool Interleaved>
void Albany::MultiSTKFieldContainer<Interleaved>::saveSolnVectorT(const Tpetra_Vector& solnT,
    const Tpetra_Vector& soln_dotT, stk::mesh::Selector& sel, const Teuchos::RCP<const Tpetra_Map>& node_mapT)
{
  typedef typename AbstractSTKFieldContainer::VectorFieldType VFT;
  typedef typename AbstractSTKFieldContainer::ScalarFieldType SFT;
  Teuchos::RCP<Teuchos::FancyOStream> out = Teuchos::VerboseObjectBase::getDefaultOStream();
  *out << "IKT WARNING: calling Albany::MultiSTKFieldContainer::saveSolnVectorT with soln_dotT, but "
       << "this function has not been extended to write soln_dotT properly to the Exodus file.  Exodus "
       << "file will contain only soln, not soln_dot.\n";

  // Iterate over the on-processor nodes by getting node buckets and iterating over each bucket.
  stk::mesh::BucketVector const& all_elements = this->bulkData->get_buckets(stk::topology::NODE_RANK, sel);


  this->numNodes = node_mapT->getNodeNumElements(); // Needed for the getDOF function to work correctly
  // This is either numOwnedNodes or numOverlapNodes, depending on
  // which map is passed in

  for (stk::mesh::BucketVector::const_iterator it = all_elements.begin() ; it != all_elements.end() ; ++it) {
    const stk::mesh::Bucket& bucket = **it;
    int offset = 0;
    for(int k = 0; k < sol_index[0].size(); k++) {
      if(sol_index[0][k] == 1) { // Scalar
        SFT* field = this->metaData->template get_field<SFT>(stk::topology::NODE_RANK, sol_vector_name[0][k]);
        this->saveVectorHelperT(solnT, field, node_mapT, bucket, offset);
      }
      else {
        VFT* field = this->metaData->template get_field<VFT>(stk::topology::NODE_RANK, sol_vector_name[0][k]);
        //IKT, FIXME: saveVectorHelperT should be extended to take in soln_dotT!
        this->saveVectorHelperT(solnT, field, node_mapT, bucket, offset);
      }
      offset += sol_index[0][k];
    }
  }
}

template<bool Interleaved>
void Albany::MultiSTKFieldContainer<Interleaved>::saveSolnVectorT(const Tpetra_Vector& solnT,
    const Tpetra_Vector& soln_dotT, const Tpetra_Vector& soln_dotdotT,
    stk::mesh::Selector& sel, const Teuchos::RCP<const Tpetra_Map>& node_mapT)
{
  typedef typename AbstractSTKFieldContainer::VectorFieldType VFT;
  typedef typename AbstractSTKFieldContainer::ScalarFieldType SFT;
  Teuchos::RCP<Teuchos::FancyOStream> out = Teuchos::VerboseObjectBase::getDefaultOStream();
  *out << "IKT WARNING: calling Albany::MultiSTKFieldContainer::saveSolnVectorT with soln_dotT and "
       << "soln_dotdotT, but this function has not been extended to write soln_dotT "
       << "and soln_dotdotT properly to the Exodus file.  Exodus "
       << "file will contain only soln, not soln_dot and soln_dotdot.\n";

  // Iterate over the on-processor nodes by getting node buckets and iterating over each bucket.
  stk::mesh::BucketVector const& all_elements = this->bulkData->get_buckets(stk::topology::NODE_RANK, sel);


  this->numNodes = node_mapT->getNodeNumElements(); // Needed for the getDOF function to work correctly
  // This is either numOwnedNodes or numOverlapNodes, depending on
  // which map is passed in

  for (stk::mesh::BucketVector::const_iterator it = all_elements.begin() ; it != all_elements.end() ; ++it) {
    const stk::mesh::Bucket& bucket = **it;
    int offset = 0;
    for(int k = 0; k < sol_index[0].size(); k++) {
      if(sol_index[0][k] == 1) { // Scalar
        SFT* field = this->metaData->template get_field<SFT>(stk::topology::NODE_RANK, sol_vector_name[0][k]);
        this->saveVectorHelperT(solnT, field, node_mapT, bucket, offset);
      }
      else {
        VFT* field = this->metaData->template get_field<VFT>(stk::topology::NODE_RANK, sol_vector_name[0][k]);
        //IKT, FIXME: saveVectorHelperT should be extended to take in soln_dotT and soln_dotdotT!
        this->saveVectorHelperT(solnT, field, node_mapT, bucket, offset);
      }
      offset += sol_index[0][k];
    }
  }
}

template<bool Interleaved>
void Albany::MultiSTKFieldContainer<Interleaved>::saveVectorT(const Tpetra_Vector& field_vector, const std::string&  field_name,
    stk::mesh::Selector& field_selection, const Teuchos::RCP<const Tpetra_Map>& field_node_map, const NodalDOFManager& nodalDofManager) {

  // Iterate over the on-processor nodes by getting node buckets and iterating over each bucket.
  stk::mesh::BucketVector const& all_elements = this->bulkData->get_buckets(stk::topology::NODE_RANK, field_selection);

  if(nodalDofManager.numComponents() > 1) {
    AbstractSTKFieldContainer::VectorFieldType* field  = this->metaData->template get_field<AbstractSTKFieldContainer::VectorFieldType>(stk::topology::NODE_RANK, field_name);
    for(stk::mesh::BucketVector::const_iterator it = all_elements.begin() ; it != all_elements.end() ; ++it) {
      const stk::mesh::Bucket& bucket = **it;
      this->saveVectorHelperT(field_vector, field, field_node_map, bucket, nodalDofManager);
    }
  }
  else {
    AbstractSTKFieldContainer::ScalarFieldType* field  = this->metaData->template get_field<AbstractSTKFieldContainer::ScalarFieldType>(stk::topology::NODE_RANK, field_name);
    for(stk::mesh::BucketVector::const_iterator it = all_elements.begin() ; it != all_elements.end() ; ++it) {
      const stk::mesh::Bucket& bucket = **it;
      this->saveVectorHelperT(field_vector, field, field_node_map, bucket, nodalDofManager);
    }
  }
}

template<bool Interleaved>
void Albany::MultiSTKFieldContainer<Interleaved>::saveSolnMultiVector(const Tpetra_MultiVector& solnT,
    stk::mesh::Selector& sel, const Teuchos::RCP<const Tpetra_Map>& node_mapT) {


  typedef typename AbstractSTKFieldContainer::VectorFieldType VFT;
  typedef typename AbstractSTKFieldContainer::ScalarFieldType SFT;

  // Iterate over the on-processor nodes by getting node buckets and iterating over each bucket.
  stk::mesh::BucketVector const& all_elements = this->bulkData->get_buckets(stk::topology::NODE_RANK, sel);


  this->numNodes = node_mapT->getNodeNumElements(); // Needed for the getDOF function to work correctly
  // This is either numOwnedNodes or numOverlapNodes, depending on
  // which map is passed in

   for(stk::mesh::BucketVector::const_iterator it = all_elements.begin() ; it != all_elements.end() ; ++it) {

    const stk::mesh::Bucket& bucket = **it;

    for(int vector_component = 0; vector_component < solnT.getNumVectors(); vector_component++){

      int offset = 0;

      for(int k = 0; k < sol_index[vector_component].size(); k++) {

        if(sol_index[vector_component][k] == 1) { // Scalar

          SFT* field = this->metaData->template get_field<SFT>(
                            stk::topology::NODE_RANK, sol_vector_name[vector_component][k]);
          this->saveMultiVectorHelper(solnT, field, node_mapT, bucket, vector_component, offset);

        }

        else {

          VFT* field = this->metaData->template get_field<VFT>(
                            stk::topology::NODE_RANK, sol_vector_name[vector_component][k]);
          this->saveMultiVectorHelper(solnT, field, node_mapT, bucket, vector_component, offset);

        }

        offset += sol_index[vector_component][k];

      }

    }

  }
}

#if defined(ALBANY_EPETRA)
template<bool Interleaved>
void Albany::MultiSTKFieldContainer<Interleaved>::saveResVector(const Epetra_Vector& res,
    stk::mesh::Selector& sel, const Teuchos::RCP<const Epetra_Map>& node_map) {

  typedef typename AbstractSTKFieldContainer::VectorFieldType VFT;
  typedef typename AbstractSTKFieldContainer::ScalarFieldType SFT;

  // Iterate over the on-processor nodes by getting node buckets and iterating over each bucket.
  stk::mesh::BucketVector const& all_elements = this->bulkData->get_buckets(stk::topology::NODE_RANK, sel);
  this->numNodes = node_map->NumMyElements(); // Needed for the getDOF function to work correctly
  // This is either numOwnedNodes or numOverlapNodes, depending on
  // which map is passed in

  for(stk::mesh::BucketVector::const_iterator it = all_elements.begin() ; it != all_elements.end() ; ++it) {

    const stk::mesh::Bucket& bucket = **it;

    int offset = 0;

    for(int k = 0; k < res_index.size(); k++) {

      if(res_index[k] == 1) { // Scalar

        SFT* field = this->metaData->template get_field<SFT>(stk::topology::NODE_RANK, res_vector_name[k]);
        this->saveVectorHelper(res, field, node_map, bucket, offset);

      }

      else {

        VFT* field = this->metaData->template get_field<VFT>(stk::topology::NODE_RANK, res_vector_name[k]);
        this->saveVectorHelper(res, field, node_map, bucket, offset);

      }

      offset += res_index[k];

    }

  }
}

#endif

template<bool Interleaved>
void Albany::MultiSTKFieldContainer<Interleaved>::saveResVectorT(const Tpetra_Vector& res,
    stk::mesh::Selector& sel, const Teuchos::RCP<const Tpetra_Map>& node_map) {

  typedef typename AbstractSTKFieldContainer::VectorFieldType VFT;
  typedef typename AbstractSTKFieldContainer::ScalarFieldType SFT;

  // Iterate over the nodes by getting node buckets and iterating over each bucket.
  stk::mesh::BucketVector const& all_elements = this->bulkData->get_buckets(stk::topology::NODE_RANK, sel);
  this->numNodes = node_map->getNodeNumElements(); // Needed for the getDOF function to work correctly
  // This is either numOwnedNodes or numOverlapNodes, depending on
  // which map is passed in

  for(stk::mesh::BucketVector::const_iterator it = all_elements.begin() ; it != all_elements.end() ; ++it) {

    const stk::mesh::Bucket& bucket = **it;

    int offset = 0;

    for(int k = 0; k < res_index.size(); k++) {

      if(res_index[k] == 1) { // Scalar

        SFT* field = this->metaData->template get_field<SFT>(stk::topology::NODE_RANK, res_vector_name[k]);
        this->saveVectorHelperT(res, field, node_map, bucket, offset);

      }

      else {

        VFT* field = this->metaData->template get_field<VFT>(stk::topology::NODE_RANK, res_vector_name[k]);
        this->saveVectorHelperT(res, field, node_map, bucket, offset);

      }

      offset += res_index[k];

    }

  }
}

template<bool Interleaved>
void Albany::MultiSTKFieldContainer<Interleaved>::transferSolutionToCoords() {

  const bool MultiSTKFieldContainer_transferSolutionToCoords_not_implemented = true;
  TEUCHOS_TEST_FOR_EXCEPT(MultiSTKFieldContainer_transferSolutionToCoords_not_implemented);
  //     this->copySTKField(solution_field, this->coordinates_field);

}
