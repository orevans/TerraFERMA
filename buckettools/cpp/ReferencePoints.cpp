// Copyright (C) 2013 Columbia University in the City of New York and others.
//
// Please see the AUTHORS file in the main source directory for a full list
// of contributors.
//
// This file is part of TerraFERMA.
//
// TerraFERMA is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// TerraFERMA is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with TerraFERMA. If not, see <http://www.gnu.org/licenses/>.


#include "ReferencePoints.h"
#include "Logger.h"
#include <dolfin.h>
#include <string>
#include <limits>

using namespace buckettools;

//*******************************************************************|************************************************************//
// default constructor
//*******************************************************************|************************************************************//
ReferencePoints::ReferencePoints() : name_("uninitialized_string")
{
                                                                     // do nothing
}

//*******************************************************************|************************************************************//
// specific constructor
//*******************************************************************|************************************************************//
ReferencePoints::ReferencePoints(const Array_double_ptr coord, const FunctionSpace_ptr functionspace, 
                                          const std::string &name) : name_(name), functionspace_(functionspace)
{
  init_(coord);                                                      // initialize the detector 
}

//*******************************************************************|************************************************************//
// specific constructor
//*******************************************************************|************************************************************//
ReferencePoints::ReferencePoints(const std::vector<double> &coord, const FunctionSpace_ptr functionspace, 
                                          const std::string &name) : name_(name), functionspace_(functionspace)
{
  init_(coord);                                                      // initialize the detector
}

//*******************************************************************|************************************************************//
// default destructor
//*******************************************************************|************************************************************//
ReferencePoints::~ReferencePoints()
{
                                                                     // do nothing
}

//*******************************************************************|************************************************************//
// apply to a matrix
//*******************************************************************|************************************************************//
void ReferencePoints::apply(dolfin::GenericMatrix& A) const
{
  apply(&A, 0, 0);
}

//*******************************************************************|************************************************************//
// apply to the rhs of a linear problem
//*******************************************************************|************************************************************//
void ReferencePoints::apply(dolfin::GenericVector& b) const
{
  apply(0, &b, 0);
}

//*******************************************************************|************************************************************//
// apply to the matrix and rhs of a linear problem
//*******************************************************************|************************************************************//
void ReferencePoints::apply(dolfin::GenericMatrix& A, dolfin::GenericVector& b) const
{
  apply(&A, &b, 0);
}

//*******************************************************************|************************************************************//
// apply to the rhs of a nonlinear problem
//*******************************************************************|************************************************************//
void ReferencePoints::apply(dolfin::GenericVector& b, const dolfin::GenericVector& x) const
{
  apply(0, &b, &x);
}

//*******************************************************************|************************************************************//
// apply to the matrix and rhs of a nonlinear problem
//*******************************************************************|************************************************************//
void ReferencePoints::apply(dolfin::GenericMatrix& A,
                            dolfin::GenericVector& b,
                            const dolfin::GenericVector& x) const
{
  apply(&A, &b, &x);
}

//*******************************************************************|************************************************************//
// apply to the matrix and rhs of a nonlinear problem (implementation)
//*******************************************************************|************************************************************//
void ReferencePoints::apply(dolfin::GenericMatrix* A,
                            dolfin::GenericVector* b,
                            const dolfin::GenericVector* x) const
{
  check_arguments_(A, b, x);                                         // check arguments

  const double spring = std::numeric_limits<double>::max()*std::numeric_limits<double>::epsilon();

  const uint size = dof_.size();
  std::vector<double> values(size, 0.0);

  if (x)                                                             // deal with nonlinear problems
  {
    (*x).get_local(&values[0], dof_.size(), &dof_[0]);
  }

  log(INFO, "Applying reference points to linear system.");

  if (b)
  {
    if (A)
    {
      for (uint i = 0; i < size; i++)
      {
        values[i] = values[i]*spring;
      }
    }

    (*b).set(&values[0], size, &dof_[0]);
    (*b).apply("insert");
  }

  if (A)
  {
    std::vector< const std::vector<dolfin::la_index>* > block_dofs(2);
    std::vector< dolfin::la_index > row(1);
    for (uint i = 0; i < 2; ++i )
    {
      block_dofs[i] = &row;
    }
    
    for (uint i = 0; i < size; i++)
    {
      row[0] = dof_[i];
      (*A).add(&spring, block_dofs);
    }
    
    (*A).apply("add");
  }
}

//*******************************************************************|************************************************************//
// check the validity of the arguments to apply
//*******************************************************************|************************************************************//
void ReferencePoints::check_arguments_(dolfin::GenericMatrix* A, dolfin::GenericVector* b,
                                        const dolfin::GenericVector* x) const
{
  assert(functionspace_);

                                                                     // Check matrix and vector dimensions
  if (A && x && (*A).size(0) != (*x).size())
  {
    tf_err("Matrix and vector supplied to ReferencePoints incompatible.",
           "Matrix dimension (%d rows) does not match vector dimension (%d) for application of reference points",
                 (*A).size(0), (*x).size());
  }

  if (A && b && (*A).size(0) != (*b).size())
  {
    tf_err("Matrix and vector supplied to ReferencePoints incompatible.",
           "Matrix dimension (%d rows) does not match vector dimension (%d) for application of reference points",
                 (*A).size(0), (*b).size());
  }

  if (x && b && (*x).size() != (*b).size())
  {
    tf_err("Vectors supplied to ReferencePoints incompatible.",
           "Vector dimension (%d rows) does not match vector dimension (%d) for application of reference points",
                 (*x).size(), (*b).size());
  }

                                                                     // Check dimension of function space
  if (A && (*A).size(0) < (*functionspace_).dim())
  {
    tf_err("Functionspace and matrix supplied to ReferencePoints incompatible.",
           "Dimension of function space (%d) too large for application of reference points to linear system (%d rows)",
                 (*functionspace_).dim(), (*A).size(0));
  }

  if (x && (*x).size() < (*functionspace_).dim())
  {
    tf_err("Functionspace and vector supplied to ReferencePoints incompatible.",
           "Dimension of function space (%d) too large for application of reference points to linear system (%d rows)",
                 (*functionspace_).dim(), (*x).size());
  }

  if (b && (*b).size() < (*functionspace_).dim())
  {
    tf_err("Functionspace and vector supplied to ReferencePoints incompatible.",
           "Dimension of function space (%d) too large for application of reference points to linear system (%d rows)",
                 (*functionspace_).dim(), (*b).size());
  }

}

//*******************************************************************|************************************************************//
// intialize a reference point from a (boost shared) pointer to a dolfin array
//*******************************************************************|************************************************************//
void ReferencePoints::init_(const Array_double_ptr coord)
{
  if(position_)
  {
    tf_err("Intializing already initialized reference point.", "position_ associated.");
  }
  
  position_ = coord;

  const dolfin::Mesh& mesh = *(*functionspace_).mesh();
  const dolfin::GenericDofMap& dofmap = *(*functionspace_).dofmap();

  const std::size_t gdim = mesh.geometry().dim();

  double* pos = (*position_).data();
  uint dim = (*position_).size();
  assert(dim==gdim);
  const dolfin::Point point(dim, pos);
  int cellid;
  std::vector<unsigned int> cellids = (*mesh.bounding_box_tree()).compute_entity_collisions(point);
  if (cellids.size()==0)
  {
    cellid = -1;
  }
  else
  {
    cellid = cellids[0];
  }

  if (cellid >= 0)
  {
    const dolfin::Cell cell(mesh, cellid);

    boost::multi_array<double, 2> coordinates(boost::extents[dofmap.cell_dimension(cellid)][gdim]);
    std::vector<double> vertex_coordinates;
    cell.get_vertex_coordinates(vertex_coordinates);
    dofmap.tabulate_coordinates(coordinates, vertex_coordinates, cell);

    const std::vector<dolfin::la_index>& cell_dofs = dofmap.cell_dofs(cellid);

    std::vector<double> dist(dofmap.cell_dimension(cellid), 0.0);
    for (uint i = 0; i < dofmap.cell_dimension(cellid); ++i)
    {
      for (uint j = 0; j < gdim; ++j)
      {
        dist[i] += std::pow((coordinates[i][j] - (*position_)[j]), 2);
      }
    }

    const uint i = std::distance(&dist[0], std::min_element(&dist[0], &dist[dist.size()]));
    std::pair<uint, uint> ownership_range = dofmap.ownership_range();
    if (cell_dofs[i] >= ownership_range.first && cell_dofs[i] < ownership_range.second)
    {
      dof_.push_back(cell_dofs[i]);
    }
  }

}

//*******************************************************************|************************************************************//
// intialize a reference point from a std vector
//*******************************************************************|************************************************************//
void ReferencePoints::init_(const std::vector<double> &coord)
{
  Array_double_ptr arraypoint(new dolfin::Array<double>(coord.size()));
  for (uint i = 0; i<coord.size(); i++)
  {
    (*arraypoint)[i] = coord[i];
  }
  
  init_(arraypoint);
}

