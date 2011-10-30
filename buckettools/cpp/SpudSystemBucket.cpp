
#include "BoostTypes.h"
#include "SpudSystemBucket.h"
#include "SystemSolversWrapper.h"
#include "SpudBase.h"
#include "SpudFunctionBucket.h"
#include "SpudSolverBucket.h"
#include <dolfin.h>
#include <string>
#include <spud>

using namespace buckettools;

//*******************************************************************|************************************************************//
// specific constructor
//*******************************************************************|************************************************************//
SpudSystemBucket::SpudSystemBucket(const std::string &optionpath, 
                                            Bucket* bucket) : 
                                            optionpath_(optionpath), 
                                            SystemBucket(bucket)
{
                                                                     // do nothing
}

//*******************************************************************|************************************************************//
// default destructor
//*******************************************************************|************************************************************//
SpudSystemBucket::~SpudSystemBucket()
{
                                                                     // do nothing
}

//*******************************************************************|************************************************************//
// fill the system bucket data structures assuming the buckettools schema
//*******************************************************************|************************************************************//
void SpudSystemBucket::fill()
{
  fill_base_();                                                      // fill in the base data (could be called from the
                                                                     // constructor?)

  fill_systemfunction_();                                            // register the functionspace and system functions

  fill_fields_();                                                    // initialize the fields (subfunctions) of this system
 
  fill_coeffs_();                                                    // initialize the coefficient expressions (and constants)
                                                                     // (can't do coefficient functions now because it's unlikely we 
                                                                     // have all the coefficient functionspaces)

  fill_solvers_();                                                   // initialize the nonlinear solvers in this system

}

//*******************************************************************|************************************************************//
// loop over the coefficients and allocate any that are coefficient functions
//*******************************************************************|************************************************************//
void SpudSystemBucket::allocate_coeff_function()
{
  
  for (FunctionBucket_it f_it = coeffs_begin(); f_it != coeffs_end();// loop over all coefficients
                                                              f_it++) 
  {                                                                  // recast as a spud derived class and initialize
    (*(boost::dynamic_pointer_cast< SpudFunctionBucket >((*f_it).second))).allocate_coeff_function();
  }                                                                  // (check that this is a coefficient function within this
                                                                     // function)

}

//*******************************************************************|************************************************************//
// attach coefficients to forms and functionals then initialize matrices described by this system's forms
//*******************************************************************|************************************************************//
void SpudSystemBucket::initialize()
{
  dolfin::info("Attaching coeffs for system %s", name().c_str());
  attach_all_coeffs_();                                              // attach the coefficients to form and functionals

  for (FunctionBucket_it f_it = fields_begin(); f_it != fields_end();
                                                              f_it++)
  {
    (*(boost::dynamic_pointer_cast< SpudFunctionBucket >((*f_it).second))).initialize_field();
  } 

  for (FunctionBucket_it f_it = coeffs_begin(); f_it != coeffs_end();
                                                              f_it++)
  {
    (*(boost::dynamic_pointer_cast< SpudFunctionBucket >((*f_it).second))).initialize_coeff_expression();
  } 

                                                                     // after this point, we are allowed to start calling evals on
                                                                     // some of the expressions that have just been initialized
                                                                     // (this wasn't allowed up until now as all cpp expressions
                                                                     // potentially need initializing before eval will return the
                                                                     // right answer)
                                                                     // NOTE: even now there are potential inter dependencies! We
                                                                     // just deal with them by evaluating things in the order the
                                                                     // user specified.

  for (FunctionBucket_it f_it = coeffs_begin(); f_it != coeffs_end();
                                                              f_it++)
  {
    (*(boost::dynamic_pointer_cast< SpudFunctionBucket >((*f_it).second))).initialize_coeff_function();
  } 

  if (fields_size()>0)
  {
    apply_ic_();                                                     // apply the initial condition to the system function
    apply_bc_();                                                     // apply the boundary conditions we just collected
  }

  for (SolverBucket_it s_it = solvers_begin(); s_it != solvers_end();// loop over the solver buckets
                                                              s_it++)
  {
    (*(*s_it).second).initialize_matrices();                         // perform a preassembly of all the matrices to set up
                                                                     // sparsities etc.
  }
}

//*******************************************************************|************************************************************//
// make a partial copy of the provided system bucket with the data necessary for writing the diagnostics file(s)
//*******************************************************************|************************************************************//
void SpudSystemBucket::copy_diagnostics(SystemBucket_ptr &system, Bucket_ptr &bucket) const
{

  if(!system)
  {
    system.reset( new SpudSystemBucket(optionpath_, &(*bucket)) );
  }

  SystemBucket::copy_diagnostics(system, bucket);

}

//*******************************************************************|************************************************************//
// return a string describing the contents of the spud system
//*******************************************************************|************************************************************//
const std::string SpudSystemBucket::str(int indent) const
{
  std::stringstream s;
  std::string indentation (indent*2, ' ');
  s << indentation << "SystemBucket " << name() << " (" 
                                << optionpath() << ")" << std::endl;
  indent++;
  s << fields_str(indent);
  s << coeffs_str(indent);
  s << solvers_str(indent);
  return s.str();
}

//*******************************************************************|************************************************************//
// fill the system bucket base data 
//*******************************************************************|************************************************************//
void SpudSystemBucket::fill_base_()
{
  std::stringstream buffer;                                          // optionpath buffer
  Spud::OptionError serr;                                            // spud error code

  buffer.str(""); buffer << optionpath() << "/name";                 // get the system name
  serr = Spud::get_option(buffer.str(), name_); 
  spud_err(buffer.str(), serr);

  buffer.str(""); buffer << optionpath() << "/ufl_symbol";           // get the system ufl symbol
  serr = Spud::get_option(buffer.str(), uflsymbol_); 
  spud_err(buffer.str(), serr);

  std::string meshname;                                              // get the system mesh
  buffer.str(""); buffer << optionpath() << "/mesh/name";
  serr = Spud::get_option(buffer.str(), meshname); 
  spud_err(buffer.str(), serr);
  mesh_ = (*bucket_).fetch_mesh(meshname);                           // and extract it from the bucket

  std::string location;
  buffer.str(""); buffer << optionpath() << "/solve/name";
  Spud::get_option(buffer.str(), location);
  if (location=="in_timeloop")
  {
    solve_location_ = SOLVE_TIMELOOP;
  }
  else if (location=="at_start")
  {
    solve_location_ = SOLVE_START;
  }
  else if (location=="with_diagnostics")
  {
    solve_location_ = SOLVE_DIAGNOSTICS;
  }
  else
  {
    dolfin::error("Unknown solve location for system %s.", name().c_str());
  }

  change_calculated_.reset( new bool(false) );                       // assume the change hasn't been calculated yet

  solved_.reset( new bool(false) );                                  // assume the system hasn't been solved yet

}

//*******************************************************************|************************************************************//
// fill the system functionspace and function data 
//*******************************************************************|************************************************************//
void SpudSystemBucket::fill_systemfunction_()
{
  std::stringstream buffer;                                          // optionpath buffer
  Spud::OptionError serr;                                            // spud error code

  buffer.str("");  buffer << optionpath() << "/field";               // find out how many fields we have
  int nfields = Spud::option_count(buffer.str());
  if (nfields==0)                                                    // and don't do anything if there are no fields
  {
    return;
  }

  functionspace_ = ufc_fetch_functionspace(name(), mesh());          // fetch the first functionspace we can grab from the ufc 
                                                                     // for this system

  function_.reset( new dolfin::Function(functionspace_) );           // declare the function on this functionspace
  buffer.str(""); buffer << name() << "::Function";
  (*function_).rename( buffer.str(), buffer.str() );

  oldfunction_.reset( new dolfin::Function(functionspace_) );        // declare the old function on this functionspace
  buffer.str(""); buffer << name() << "::OldFunction";
  (*oldfunction_).rename( buffer.str(), buffer.str() );

  iteratedfunction_.reset( new dolfin::Function(functionspace_) );   // declare the iterated function on this functionspace
  buffer.str(""); buffer << name() << "::IteratedFunction";
  (*iteratedfunction_).rename( buffer.str(), buffer.str() );

  changefunction_.reset( new dolfin::Function(functionspace_) );     // declare the change in the function between timesteps
  buffer.str(""); buffer << name() << "::TimestepChange";
  (*changefunction_).rename( buffer.str(), buffer.str() );

}

//*******************************************************************|************************************************************//
// fill in the data about each field (or subfunction) of this system 
//*******************************************************************|************************************************************//
void SpudSystemBucket::fill_fields_()
{
  std::stringstream buffer;                                          // optionpath buffer

                                                                     // prepare the system initial condition expression:
  uint component = 0;                                                // initialize a counter for the scalar components of this
                                                                     // system
  std::map< uint, Expression_ptr > icexpressions;                    // set up a map from component to initial condition expression

  buffer.str("");  buffer << optionpath() << "/field";               // find out how many fields we have
  int nfields = Spud::option_count(buffer.str());
  for (uint i = 0; i < nfields; i++)                                 // loop over the fields in the options dictionary
  {
    buffer.str(""); buffer << optionpath() << "/field[" << i << "]";

                                                                     // declare a new field function bucket assuming this system is
                                                                     // its parent
    SpudFunctionBucket_ptr field(new SpudFunctionBucket( buffer.str(), this ));
    (*field).fill_field(i);                                          // fill in this field (providing its index in the system)
    register_field(field, (*field).name());                          // register this field in the system bucket
                                  
                                                                     // insert the field's initial condition expression into a 
                                                                     // temporary system map:
    uint_Expression_it e_it = icexpressions.find(component);         // check if this component already exists
    if (e_it != icexpressions.end())
    {
      dolfin::error(                                                 // if it does, issue an error
      "IC Expression with component number %d already exists in icexpressions map.", 
                                                        component);
    }
    else
    {
      icexpressions[component] = (*field).icexpression();            // if it doesn't, insert it into the map
    }

    component += (*(*field).icexpression()).value_size();            // increment the component count by the size of this field
                                                                     // (i.e. no. of scalar components)
  }

  collect_bcs_();                                                    // collect all the bcs together for convenience later
  collect_ics_(component, icexpressions);                            // collect all the ics together into a new initial condition expression

}

//*******************************************************************|************************************************************//
// fill in the data about each coefficient expression (or constant) of this system 
//*******************************************************************|************************************************************//
void SpudSystemBucket::fill_coeffs_()
{
  std::stringstream buffer;                                          // optionpath buffer
  
  buffer.str("");  buffer << optionpath() << "/coefficient";
  int ncoeffs = Spud::option_count(buffer.str());                    // find out how many coefficients we have
  for (uint i = 0; i < ncoeffs; i++)                                 // and loop over them
  {
    buffer.str(""); buffer << optionpath() << "/coefficient[" 
                                                        << i << "]";

                                                                     // initialize a new function bucket for this coefficient
                                                                     // (regardless of type!) assuming this system is its parent
    SpudFunctionBucket_ptr coeff( new SpudFunctionBucket( buffer.str(), this ) );
    (*coeff).fill_coeff(i);                                          // fill the coefficient (this won't do much for coefficient
                                                                     // functions)
    register_coeff(coeff, (*coeff).name());                          // register this coefficient in the system

  }

}

//*******************************************************************|************************************************************//
// fill in the data about each solver of this system 
//*******************************************************************|************************************************************//
void SpudSystemBucket::fill_solvers_()
{
  std::stringstream buffer;                                          // optionpath buffer
  
  buffer.str("");  buffer << optionpath() << "/nonlinear_solver";
  int nsolvers = Spud::option_count(buffer.str());                   // find out how many nonlinear solvers there are
  for (uint i = 0; i < nsolvers; i++)                                // loop over them
  {
    buffer.str(""); buffer << optionpath() << "/nonlinear_solver[" 
                                                        << i << "]";

                                                                     // initialize a new solver bucket assuming this system is its
                                                                     // parent
    SpudSolverBucket_ptr solver( new SpudSolverBucket( buffer.str(), this ) );
    (*solver).fill();                                                // fill in the data about this solver bucket
    register_solver(solver, (*solver).name());                       // register the solver bucket in the system
  }
}

