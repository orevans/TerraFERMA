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


#ifndef __CONVERGENCE_FILE_H
#define __CONVERGENCE_FILE_H

#include <cstdio>
#include <fstream>
#include <string>
#include "DiagnosticsFile.h"
#include "BoostTypes.h"

namespace buckettools
{

  //*****************************************************************|************************************************************//
  // predeclarations: a circular dependency between the ConvergenceFile class and the Bucket class requires a lot of predeclarations.
  //*****************************************************************|************************************************************//
  class Bucket;
  class SystemBucket;
  typedef std::shared_ptr< SystemBucket > SystemBucket_ptr;
  class FunctionBucket;
  typedef std::shared_ptr< FunctionBucket > FunctionBucket_ptr;
  class SolverBucket;
  typedef std::shared_ptr< SolverBucket > SolverBucket_ptr;

  //*****************************************************************|************************************************************//
  // ConvergenceFile class:
  //
  // A derived class from the base statfile class intended for the output of diagnostics to file every dump period.
  // Convergence normally include things like norms of residuals.
  //*****************************************************************|************************************************************//
  class ConvergenceFile : public DiagnosticsFile
  {

  //*****************************************************************|***********************************************************//
  // Publicly available functions
  //*****************************************************************|***********************************************************//

  public:                                                            // available to everyone
    
    //***************************************************************|***********************************************************//
    // Constructors and destructors
    //***************************************************************|***********************************************************//
    
    ConvergenceFile(const std::string &name, 
                    const MPI_Comm &comm, 
                    const Bucket *bucket,
                    const std::string &systemname, 
                    const std::string &solvername);                  // specific constructor
 
    ~ConvergenceFile();                                              // default destructor
    
    //***************************************************************|***********************************************************//
    // Header writing functions
    //***************************************************************|***********************************************************//

    void write_header();                         // write header for the bucket

    //***************************************************************|***********************************************************//
    // Data writing functions
    //***************************************************************|***********************************************************//

    void write_data();                                               // write data to file for a simulation
    
  //*****************************************************************|***********************************************************//
  // Private functions
  //*****************************************************************|***********************************************************//

  private:                                                           // only available to this class

    //***************************************************************|***********************************************************//
    // Base data
    //***************************************************************|***********************************************************//

    std::string systemname_;                                         // system name

    std::string solvername_;                                         // solver name

    std::vector< FunctionBucket_ptr > fields_;

    //***************************************************************|***********************************************************//
    // Header writing functions (continued)
    //***************************************************************|***********************************************************//

    void header_iteration_();                                        // write the header for iterations 
    
    void header_bucket_();                                           // write the header for the bucket (non-constant and 
                                                                     // timestepping entries)

    void header_system_(const SystemBucket_ptr sys_ptr, 
                        const SolverBucket_ptr sol_ptr);             // write the header for a system

    void header_func_(const FunctionBucket_ptr f_ptr,                // write the header for a set of functions
                      const SolverBucket_ptr sol_ptr);

    //***************************************************************|***********************************************************//
    // Data writing functions (continued)
    //***************************************************************|***********************************************************//

    void data_iteration_();                                          // write the data about the iterations

    void data_bucket_();                                             // write the data for a steady state simulation

    void data_system_(const SystemBucket_ptr sys_ptr, 
                      const SolverBucket_ptr sol_ptr);               // write the data for a system

    void data_field_(const FunctionBucket_ptr f_ptr,                 // write the data for a set of fields
                     const SolverBucket_ptr sol_ptr);

  };
  
  typedef std::shared_ptr< ConvergenceFile > ConvergenceFile_ptr;    // define a boost shared ptr type for the class

}
#endif
