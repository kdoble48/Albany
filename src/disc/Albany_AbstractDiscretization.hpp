//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

//IK, 9/12/14: Epetra ifdef'ed out if ALBANY_EPETRA_EXE turned off.

#ifndef ALBANY_ABSTRACTDISCRETIZATION_HPP
#define ALBANY_ABSTRACTDISCRETIZATION_HPP

#include "Albany_DiscretizationUtils.hpp"

#if defined(ALBANY_EPETRA)
#include "Epetra_Map.h"
#include "Epetra_Vector.h"
#include "Epetra_CrsGraph.h"
#endif

#include "Shards_CellTopologyData.h"
#include "Shards_Array.hpp"
#include "Albany_StateInfoStruct.hpp"
#include "Albany_NodalDOFManager.hpp"
#include "Albany_AbstractMeshStruct.hpp"

#ifdef ALBANY_CONTACT
#include "Albany_ContactManager.hpp"
#endif

namespace Albany {

class AbstractDiscretization {
  public:

    typedef std::map<std::string,Teuchos::RCP<Albany::AbstractDiscretization> > SideSetDiscretizationsType;

    //! Constructor
    AbstractDiscretization() {};

    //! Destructor
    virtual ~AbstractDiscretization() {};

#if defined(ALBANY_EPETRA)
    //! Get Epetra DOF map
    virtual Teuchos::RCP<const Epetra_Map> getMap() const = 0;
#endif
    //! Get Tpetra DOF map
    virtual Teuchos::RCP<const Tpetra_Map> getMapT() const = 0;
    //! Get Tpetra DOF map
    virtual Teuchos::RCP<const Tpetra_Map> getMapT(const std::string& field_name) const = 0;

#if defined(ALBANY_EPETRA)
    //! Get Epetra overlapped DOF map
    virtual Teuchos::RCP<const Epetra_Map> getOverlapMap() const = 0;
#endif

    //! Get Tpetra overlapped DOF map
    virtual Teuchos::RCP<const Tpetra_Map> getOverlapMapT() const = 0;
    //! Get field overlapped DOF map
    virtual Teuchos::RCP<const Tpetra_Map> getOverlapMapT(const std::string& field_name) const = 0;

#if defined(ALBANY_EPETRA)
    //! Get Epetra Jacobian graph
    virtual Teuchos::RCP<const Epetra_CrsGraph> getJacobianGraph() const = 0;
#endif
    //! Get Tpetra Jacobian graph
    virtual Teuchos::RCP<const Tpetra_CrsGraph> getJacobianGraphT() const = 0;

#ifdef ALBANY_AERAS 
    //! Get implicit Tpetra Jacobian graph (for Aeras hyperviscosity)
    virtual Teuchos::RCP<const Tpetra_CrsGraph> getImplicitJacobianGraphT() const = 0;
#endif

    //! Get Epetra overlap Jacobian graph
#if defined(ALBANY_EPETRA)
    virtual Teuchos::RCP<const Epetra_CrsGraph> getOverlapJacobianGraph() const = 0;
#endif
    //! Get Tpetra overlap Jacobian graph
    virtual Teuchos::RCP<const Tpetra_CrsGraph> getOverlapJacobianGraphT() const = 0;
#ifdef ALBANY_AERAS 
    //! Get implicit Tpetra Jacobian graph (for Aeras hyperviscosity)
    virtual Teuchos::RCP<const Tpetra_CrsGraph> getImplicitOverlapJacobianGraphT() const = 0;
#endif

#if defined(ALBANY_EPETRA)
    //! Get Epetra Node map
    virtual Teuchos::RCP<const Epetra_Map> getNodeMap() const = 0;
#endif

    //! Get Tpetra Node map
    virtual Teuchos::RCP<const Tpetra_Map> getNodeMapT() const = 0;

    //! Get Field Node map
    virtual Teuchos::RCP<const Tpetra_Map> getNodeMapT(const std::string& field_name) const = 0;

#if defined(ALBANY_EPETRA)
    //! Get overlapped Node map
    virtual Teuchos::RCP<const Epetra_Map> getOverlapNodeMap() const = 0;
#endif

    //! Get overlapped Node map
    virtual Teuchos::RCP<const Tpetra_Map> getOverlapNodeMapT() const = 0;

    //! Returns boolean telling code whether explicit scheme is used (needed for Aeras problems only) 
    virtual bool isExplicitScheme() const = 0;

    //! Get Field Node map
    virtual Teuchos::RCP<const Tpetra_Map> getOverlapNodeMapT(const std::string& field_name) const = 0;

    //! Get Node set lists
    virtual const NodeSetList& getNodeSets() const = 0;
    virtual const NodeSetGIDsList& getNodeSetGIDs() const = 0;
    virtual const NodeSetCoordList& getNodeSetCoords() const = 0;

    //! Get Side set lists
    virtual const SideSetList& getSideSets(const int ws) const = 0;

    using WorksetConn = Kokkos::View<LO***, Kokkos::LayoutRight, PHX::Device>;
    using Conn = typename Albany::WorksetArray<WorksetConn>::type;

    //! Get map from (Ws, El, Local Node, Eq) -> unkLID
    virtual const Conn& getWsElNodeEqID() const = 0;

    //! Get map from (Ws, El, Local Node) -> unkGID
    virtual const WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<GO> > >::type&
      getWsElNodeID() const = 0;

    //! Get IDArray for (Ws, Local Node, nComps) -> (local) NodeLID, works for both scalar and vector fields
    virtual const std::vector<IDArray>& getElNodeEqID(const std::string& field_name) const = 0;

    //! Get Dof Manager of field field_name
    virtual const NodalDOFManager& getDOFManager(const std::string& field_name) const = 0;

    //! Get Dof Manager of field field_name
    virtual const NodalDOFManager& getOverlapDOFManager(const std::string& field_name) const = 0;

    //! Retrieve coodinate ptr_field (ws, el, node)
    virtual const WorksetArray<Teuchos::ArrayRCP<Teuchos::ArrayRCP<double*> > >::type& getCoords() const = 0;

    //! Get coordinates (overlap map).
    virtual const Teuchos::ArrayRCP<double>& getCoordinates() const = 0;
    //! Set coordinates (overlap map) for mesh adaptation.
    virtual void setCoordinates(const Teuchos::ArrayRCP<const double>& c) = 0;

    //! The reference configuration manager handles updating the reference
    //! configuration. This is only relevant, and also only optional, in the
    //! case of mesh adaptation.
    virtual void setReferenceConfigurationManager(const Teuchos::RCP<AAdapt::rc::Manager>& rcm) = 0;

#ifdef ALBANY_CONTACT
    //! Get the contact manager
    virtual Teuchos::RCP<const Albany::ContactManager> getContactManager() const = 0;
#endif

    virtual const WorksetArray<Teuchos::ArrayRCP<double> >::type& getSphereVolume() const = 0;

    virtual const WorksetArray<Teuchos::ArrayRCP<double*> >::type& getLatticeOrientation() const = 0;

    //! Print the coords for mesh debugging
    virtual void printCoords() const = 0;

    //! Get sideSet discretizations map
    virtual const SideSetDiscretizationsType& getSideSetDiscretizations() const = 0;

    //! Get the map side_id->side_set_elem_id
    virtual const std::map<std::string,std::map<GO,GO>>& getSideToSideSetCellMap() const = 0;

    //! Get the map side_node_id->side_set_cell_node_id
    virtual const std::map<std::string,std::map<GO,std::vector<int> > >& getSideNodeNumerationMap() const = 0;

    //! Get MeshStruct
    virtual Teuchos::RCP<Albany::AbstractMeshStruct> getMeshStruct() const = 0;

    //! Get stateArrays
    virtual Albany::StateArrays& getStateArrays() = 0;

    //! Get nodal parameters state info struct
    virtual const Albany::StateInfoStruct& getNodalParameterSIS() const = 0;

    //! Retrieve Vector (length num worksets) of element block names
    virtual const WorksetArray<std::string>::type& getWsEBNames() const = 0;

    //! Retrieve Vector (length num worksets) of Physics Index
    virtual const WorksetArray<int>::type& getWsPhysIndex() const = 0;

    //! Retrieve connectivity map from elementGID to workset
    virtual WsLIDList&  getElemGIDws() = 0;
    virtual const WsLIDList&  getElemGIDws() const = 0;

#if defined(ALBANY_EPETRA)
    //! Get solution vector from mesh database
    virtual Teuchos::RCP<Epetra_Vector> getSolutionField(bool overlapped=false) const = 0;
#endif
    virtual Teuchos::RCP<Tpetra_Vector> getSolutionFieldT(bool overlapped=false) const = 0;

    virtual Teuchos::RCP<Tpetra_MultiVector> getSolutionMV(bool overlapped=false) const = 0;

    virtual void getFieldT(Tpetra_Vector &field_vector, const std::string& field_name) const = 0;

    //! Flag if solution has a restart values -- used in Init Cond
    virtual bool hasRestartSolution() const = 0;

    //! Does the underlying discretization support MOR?
    virtual bool supportsMOR() const = 0;

    //! File time of restart solution
    virtual double restartDataTime() const = 0;

    //! Get number of spatial dimensions
    virtual int getNumDim() const = 0;

    //! Get number of total DOFs per node
    virtual int getNumEq() const = 0;

    //! Set the field vector into mesh database
    virtual void setFieldT(const Tpetra_Vector &field_vector, const std::string& field_name, bool overlapped) = 0;

    //! Set the residual field for output - Tpetra version
    virtual void setResidualFieldT(const Tpetra_Vector& residual) = 0;

#if defined(ALBANY_EPETRA)
    //! Write the solution to the output file
    virtual void writeSolution(const Epetra_Vector& solution, const double time, const bool overlapped = false) = 0;
    virtual void writeSolution(const Epetra_Vector& solution, const Epetra_Vector& solution_dot, 
                               const double time, const bool overlapped = false) = 0;
#endif

    //! Write the solution to the output file - Tpetra version. Calls next two together.
    virtual void writeSolutionT(const Tpetra_Vector &solutionT, const double time, const bool overlapped = false) = 0;
    virtual void writeSolutionT(const Tpetra_Vector &solutionT, const Tpetra_Vector &solution_dotT, 
                                const double time, const bool overlapped = false) = 0;
    virtual void writeSolutionT(const Tpetra_Vector &solutionT, const Tpetra_Vector &solution_dotT, 
                                const Tpetra_Vector &solution_dotdotT, 
                                const double time, const bool overlapped = false) = 0;
    virtual void writeSolutionMV(const Tpetra_MultiVector &solutionT, const double time, const bool overlapped = false) = 0;
    //! Write the solution to the mesh database.
    virtual void writeSolutionToMeshDatabaseT(const Tpetra_Vector &solutionT, const double time, const bool overlapped = false) = 0;
    virtual void writeSolutionToMeshDatabaseT(const Tpetra_Vector &solutionT, 
                                              const Tpetra_Vector &solution_dotT, 
                                              const double time, const bool overlapped = false) = 0;
    virtual void writeSolutionToMeshDatabaseT(const Tpetra_Vector &solutionT, 
                                              const Tpetra_Vector &solution_dotT, 
                                              const Tpetra_Vector &solution_dotdotT, 
                                              const double time, const bool overlapped = false) = 0;
    virtual void writeSolutionMVToMeshDatabase(const Tpetra_MultiVector &solutionT, const double time, const bool overlapped = false) = 0;
    //! Write the solution to file. Must call writeSolutionT first.
    virtual void writeSolutionToFileT(const Tpetra_Vector &solutionT, const double time, const bool overlapped = false) = 0;
    virtual void writeSolutionMVToFile(const Tpetra_MultiVector &solutionT, const double time, const bool overlapped = false) = 0;

    //! Get Numbering for layered mesh (mesh structred in one direction)
    virtual Teuchos::RCP<LayeredMeshNumbering<LO> > getLayeredMeshNumbering() = 0;

  private:

    //! Private to prohibit copying
    AbstractDiscretization(const AbstractDiscretization&);

    //! Private to prohibit copying
    AbstractDiscretization& operator=(const AbstractDiscretization&);

};

}

#endif // ALBANY_ABSTRACTDISCRETIZATION_HPP
