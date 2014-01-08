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


#include "GenericDetectors.h"
#include <dolfin.h>
#include <string>

using namespace buckettools;

//*******************************************************************|************************************************************//
// default constructor
//*******************************************************************|************************************************************//
GenericDetectors::GenericDetectors() : number_detectors_(0), meshdim_(0), name_("uninitialized_string")
{
                                                                     // do nothing
}

//*******************************************************************|************************************************************//
// specific constructor
//*******************************************************************|************************************************************//
GenericDetectors::GenericDetectors(const uint &number_detectors, 
                                   const uint &meshdim, 
                                   const std::string &name) : 
                                   number_detectors_(number_detectors), 
                                   meshdim_(meshdim), name_(name)
{
                                                                     // do nothing
}

//*******************************************************************|************************************************************//
// default destructor
//*******************************************************************|************************************************************//
GenericDetectors::~GenericDetectors()
{
  clean_();                                                          // empty the data structures
}
  
//*******************************************************************|************************************************************//
// base eval implementation - evaluates a function at the detector positions and returns a vector of arrays of values
//*******************************************************************|************************************************************//
void GenericDetectors::eval(std::vector< Array_double_ptr > &values,
                            const dolfin::GenericFunction &function,
                            Mesh_ptr mesh)
{
  Array_double_ptr 
            value(new dolfin::Array<double>(function.value_size())); // set up a pointer to be reset later to contain the
  
  assert(values.empty());                                            // check the values are empty

  double* pos;
  uint dim;
  std::vector<unsigned int> ids;
  int id;
  
  std::map< Mesh_ptr, std::vector< int > >::const_iterator c_it = cell_ids_.find(mesh);
  bool have_cells = ( c_it != cell_ids_.end() );
  std::vector< int > cellids;
  if (have_cells)
  {
    cellids = (*c_it).second;
  }

  for (uint i = 0; i<positions_.size(); i++)                         // loop over the detector positions
  {
    value.reset(new dolfin::Array<double>(function.value_size()));   // reset the value

    if( have_cells )
    {
      id = cellids[i];
    }
    else
    {
      pos = (*positions_[i]).data(); 
      dim = (*positions_[i]).size();
      const dolfin::Point point(dim, pos);
      ids = (*(*mesh).bounding_box_tree()).compute_collisions(point);
      if (ids.size() == 0)
      {
        id = -1;
      }
      else
      {
        id = ids[0];
      }
      cellids.push_back(id);
    }

    if(id==-1)
    {
      dolfin::error("Unable to find cell in mesh.");
    }
    else
    {
      const dolfin::Cell cell(*mesh, id);
      ufc::cell ufc_cell;
      cell.get_cell_data(ufc_cell);
      function.eval(*value, *positions_[i], ufc_cell);                 // use the dolfin eval to evaluate the function
      values.push_back(value);                                         // record the value
    }
  }

  if (!have_cells)
  {
    cell_ids_[mesh] = cellids;
  }

}

//*******************************************************************|************************************************************//
// return an iterator to the beginning of the positions_ vector
//*******************************************************************|************************************************************//
std::vector< Array_double_ptr >::iterator GenericDetectors::begin()
{
  return positions_.begin();
}

//*******************************************************************|************************************************************//
// return a constant iterator to the beginning of the positions_ vector
//*******************************************************************|************************************************************//
std::vector< Array_double_ptr >::const_iterator GenericDetectors::begin() const
{
  return positions_.begin();
}

//*******************************************************************|************************************************************//
// return an iterator to the end of the positions_ vector
//*******************************************************************|************************************************************//
std::vector< Array_double_ptr >::iterator GenericDetectors::end()
{
  return positions_.end();
}

//*******************************************************************|************************************************************//
// return a constant iterator to the end of the positions_ vector
//*******************************************************************|************************************************************//
std::vector< Array_double_ptr >::const_iterator GenericDetectors::end() const
{
  return positions_.end();
}

//*******************************************************************|************************************************************//
// return a string describing the positions of the detectors
//*******************************************************************|************************************************************//
const std::string GenericDetectors::str() const
{
  std::stringstream s;
  
  for (uint i = 0; i < positions_.size(); i++)                       // loop over the positions
  {
    s << "detector " << i << std::endl;
    s << (*positions_[i]).str(true);                                 // use the dolfin array str output
  }
  
  return s.str();
}

//*******************************************************************|************************************************************//
// empty the data structures in the genericdetectors class
//*******************************************************************|************************************************************//
void GenericDetectors::clean_()
{
  while (!positions_.empty())                                        // empty the positions vector
  {
    positions_.pop_back();
  }
}

