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


#include "ReferencePoint.h"
#include "Logger.h"
#include <dolfin.h>
#include <string>
#include <limits>

using namespace buckettools;

//*******************************************************************|************************************************************//
// specific constructor
//*******************************************************************|************************************************************//
ReferencePoint::ReferencePoint(const std::vector<double> &coord, const FunctionSpace_ptr functionspace,
                                       const GenericFunction_ptr value) :
                                       dolfin::DirichletBC(functionspace, value, subdomain_(coord, functionspace),
                                                           "pointwise")
{
                                                                     // do nothing
}

//*******************************************************************|************************************************************//
// default destructor
//*******************************************************************|************************************************************//
ReferencePoint::~ReferencePoint()
{
                                                                     // do nothing
}

//*******************************************************************|************************************************************//
// intialize a reference point from a (boost shared) pointer to a dolfin array
//*******************************************************************|************************************************************//
SubDomain_ptr ReferencePoint::subdomain_(const std::vector<double> &coord, const FunctionSpace_ptr functionspace)
{

  const uint num_sub_elements = (*(*functionspace).element()).num_sub_elements();
  if (num_sub_elements>0)
  {
    return subdomain_(coord, (*functionspace)[0]);                     // make sure we're only going to find one point
  }                                                                  // shouldn't actually get here (because of how we call this
                                                                     // constructor from TF) - hence aribtrary decision
                                                                     // just to take first subfunctionspace
  const dolfin::Mesh& mesh = *(*functionspace).mesh();

  const std::size_t gdim = mesh.geometry().dim();

  const double* pos = coord.data();
  const std::size_t dim = coord.size();
  assert(dim==gdim);
  const dolfin::Point dcoord(dim, pos);
  int cellid;
  std::vector<unsigned int> cellids = (*mesh.bounding_box_tree()).compute_entity_collisions(dcoord);
  if (cellids.size()==0)
  {
    cellid = -1;
  }
  else
  {
    cellid = cellids[0];
  }

  std::vector<double> point;
  if (cellid >= 0)
  {
    const dolfin::GenericDofMap& dofmap = *(*functionspace).dofmap();

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
        dist[i] += std::pow((coordinates[i][j] - coord[j]), 2);
      }
    }

    const uint i = std::distance(&dist[0], std::min_element(&dist[0], &dist[dist.size()]));
    std::pair<uint, uint> ownership_range = dofmap.ownership_range();
    if (cell_dofs[i] >= ownership_range.first && cell_dofs[i] < ownership_range.second)
    {
      for (uint j = 0; j < gdim; ++j)
      {
        point.push_back(coordinates[i][j]);
      }
    }
  }

  std::size_t num_processes = dolfin::MPI::size(mesh.mpi_comm());

  std::vector<std::vector<double> > points;
  dolfin::MPI::all_gather(mesh.mpi_comm(), point, points);
  assert(points.size()==num_processes);

  SubDomain_ptr subdomain = NULL;
  for (uint p = 0; p < num_processes; p++)
  {
    if (points[p].size() > 0)
    {
      subdomain.reset(new ReferencePointSubDomain(points[p]));
      break;
    }
  }

  if (!subdomain)
  {
    tf_err("Failed to find reference point near requested coordinates.", "cellid = %d", cellid);
  }

  return subdomain;

}

ReferencePointSubDomain::ReferencePointSubDomain(const std::vector<double> &point, const double &tolerance) :
                                                 dolfin::SubDomain(tolerance), point_(point)       // optional constructor
{
  // Do nothing
}

ReferencePointSubDomain::~ReferencePointSubDomain()
{
  // Do nothing
}

bool ReferencePointSubDomain::inside(const dolfin::Array<double>& x, bool on_boundary) const
{
  std::size_t n = std::max(x.size(), point_.size());
  for (std::size_t i = 0; i < n; ++i)
  {
    double xx = 0.0;
    double pp = 0.0;
    if (i < x.size())
    {
      xx = x[i];
    }
    if (i < point_.size())
    {
      pp = point_[i];
    }

    if ((pp > (xx + map_tolerance)) || (pp < (xx - map_tolerance)))
    {
      return false;
    }
  }
  return true;
}
