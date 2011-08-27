
#include "BoostTypes.h"
#include "SolverBucket.h"
#include "SystemBucket.h"
#include "Bucket.h"
#include <dolfin.h>
#include <string>

using namespace buckettools;

//*******************************************************************|************************************************************//
// default constructor
//*******************************************************************|************************************************************//
SolverBucket::SolverBucket()
{
                                                                     // do nothing
}

//*******************************************************************|************************************************************//
// specific constructor
//*******************************************************************|************************************************************//
SolverBucket::SolverBucket(SystemBucket* system) : system_(system)
{
                                                                     // do nothing
}

//*******************************************************************|************************************************************//
// default destructor
//*******************************************************************|************************************************************//
SolverBucket::~SolverBucket()
{
  empty_();                                                          // empty the solver bucket data structures

  PetscErrorCode perr;                                               // petsc error code

  if(type()=="SNES")
  {
    perr = SNESDestroy(snes_); CHKERRV(perr);                        // destroy the snes object
  }

  if(type()=="Picard")
  {
    perr = KSPDestroy(ksp_); CHKERRV(perr);                          // destroy the ksp object
  }

}

//*******************************************************************|************************************************************//
// solve the bilinear system described by the forms in the solver bucket
//*******************************************************************|************************************************************//
void SolverBucket::solve()
{
  PetscErrorCode perr;

  if (type()=="SNES")                                                // this is a petsc snes solver - FIXME: switch to an enumerated type
  {
    *work_ = (*(*system_).function()).vector();                      // set the work vector to the function vector
    perr = SNESSolve(snes_, PETSC_NULL, *(*work_).vec());            // call petsc to perform a snes solve
    CHKERRV(perr);
    SNESConvergedReason reason;                                      // check what the convergence reason was
    perr = SNESGetConvergedReason(snes_, &reason); CHKERRV(perr);     
    std::cout << "SNESConvergedReason " << reason << std::endl;      // print - FIXME: proper logging here
    (*(*system_).function()).vector() = *work_;                      // update the function
  }
  else if (type()=="Picard")                                         // this is a hand-rolled picard iteration - FIXME: switch to enum
  {

    uint it = 0;                                                     // an iteration counter
    double aerror = (*res_).norm("l2");                              // work out the initial absolute l2 error (this should be
                                                                     // initialized to the right value on the first pass and still
                                                                     // be the correct value from the previous sweep (if this stops
                                                                     // being the case it will be necessary to assemble the residual
                                                                     // here too)
    double aerror0 = aerror;                                         // record the initial absolute error
    double rerror = aerror/aerror0;                                  // relative error, starts out as 1.

    dolfin::info("%u Error (absolute, relative) = %g, %g\n", 
                                              it, aerror, rerror);

    (*(*system_).iteratedfunction()).vector() =                      // system iterated function gets set to the function values
                                  (*(*system_).function()).vector();

    while (it < minits_ ||                                           // loop for the minimum number of iterations or
          (it < maxits_ && rerror > rtol_ && aerror > atol_))        // until the max is reached or a tolerance criterion is
    {                                                                // satisfied
      it++;                                                          // increment iteration counter

      dolfin::assemble(*matrix_, *bilinear_, false);                 // assemble bilinear form
      dolfin::assemble(*rhs_, *linear_, false);                      // assemble linear form
      for(std::vector<BoundaryCondition_ptr>::const_iterator bc =    // loop over the collected vector of system bcs
                                      (*system_).bcs_begin(); 
                                  bc != (*system_).bcs_end(); bc++)
      {
        (*(*bc)).apply(*matrix_, *rhs_);                             // apply the bcs to the matrix and rhs (hopefully this
      }                                                              // maintains any symmetry)

      if (bilinearpc_)                                               // if there's a pc associated
      {
        assert(matrixpc_);
        dolfin::assemble(*matrixpc_, *bilinearpc_, false);           // assemble the pc
        for(std::vector<BoundaryCondition_ptr>::const_iterator bc = 
                                          (*system_).bcs_begin(); 
                                  bc != (*system_).bcs_end(); bc++)
        {
          (*(*bc)).apply(*matrixpc_, *rhs_);                         // apply the collected vector of system bcs
        }

        perr = KSPSetOperators(ksp_, *(*matrix_).mat(),              // set the ksp operators with two matrices
                                      *(*matrixpc_).mat(), 
                                        SAME_NONZERO_PATTERN); 
        CHKERRV(perr);
      }
      else
      {
        perr = KSPSetOperators(ksp_, *(*matrix_).mat(),              // set the ksp operators with the same matrices
                                      *(*matrix_).mat(), 
                                        SAME_NONZERO_PATTERN); 
        CHKERRV(perr);
      }

      perr = KSPSetUp(ksp_); CHKERRV(perr);                          // set up the ksp

      *work_ = (*(*system_).iteratedfunction()).vector();            // set the work vector to the iterated function
      perr = KSPSolve(ksp_, *(*rhs_).vec(), *(*work_).vec());        // perform a linear solve
      CHKERRV(perr);
      (*(*system_).iteratedfunction()).vector() = *work_;            // update the iterated function with the work vector

      assert(residual_);
      dolfin::assemble(*res_, *residual_, false);                    // assemble the residual
      for(std::vector<BoundaryCondition_ptr>::const_iterator bc = 
                                      (*system_).bcs_begin(); 
                                  bc != (*system_).bcs_end(); bc++)
      {                                                              // apply bcs to residual
        (*(*bc)).apply(*res_, (*(*system_).iteratedfunction()).vector());
      }

      aerror = (*res_).norm("l2");                                   // work out absolute error
      rerror = aerror/aerror0;                                       // and relative error
      dolfin::info("%u Error (absolute, relative) = %g, %g\n", 
                                            it, aerror, rerror);
                                                                     // and decide to loop or not...

    }

    (*(*system_).function()).vector() =                              // update the function values with the iterated values
                      (*(*system_).iteratedfunction()).vector();

  }
  else                                                               // don't know what solver type this is
  {
    dolfin::error("Unknown solver type.");
  }

}

//*******************************************************************|************************************************************//
// assemble all linear forms (this includes initializing the vectors if necessary)
//*******************************************************************|************************************************************//
void SolverBucket::assemble_linearforms(const bool &reset_tensor)
{
  assert(linear_);
  if(!rhs_)                                                          // do we have a rhs_ vector?
  {
    rhs_.reset(new dolfin::PETScVector);                             // no, allocate one
  }
  dolfin::assemble(*rhs_, *linear_, reset_tensor);                   // and assemble it

  if(residual_)                                                      // do we have a residual_ form?
  {                                                                  // yes...
    if(!res_)                                                        // do we have a res_ vector?
    {
      res_.reset(new dolfin::PETScVector);                           // no, allocate one
    }
    dolfin::assemble(*res_, *residual_, reset_tensor);               // and assemble it
  }
}

//*******************************************************************|************************************************************//
// assemble all bilinear forms (this includes initializing the matrices if necessary)
//*******************************************************************|************************************************************//
void SolverBucket::assemble_bilinearforms(const bool &reset_tensor)
{
  assert(bilinear_);
  if(!matrix_)                                                       // do we have a matrix_ matrix?
  {
    matrix_.reset(new dolfin::PETScMatrix);                          // no, allocate one
  }
  dolfin::assemble(*matrix_, *bilinear_, reset_tensor);              // and assemble it

  if(bilinearpc_)                                                    // do we have a pc form?
  {
    if(!matrixpc_)                                                   // do we have a pc matrix?
    {
      matrixpc_.reset(new dolfin::PETScMatrix);                      // no, allocate one
    }
    dolfin::assemble(*matrixpc_, *bilinearpc_, reset_tensor);        // and assemble it
  }
}

//*******************************************************************|************************************************************//
// loop over the forms in this solver bucket and attach the coefficients they request using the parent bucket data maps
//*******************************************************************|************************************************************//
void SolverBucket::attach_form_coeffs()
{
  (*(*system_).bucket()).attach_coeffs(forms_begin(), forms_end());
}

//*******************************************************************|************************************************************//
// perform some preassembly on the matrices and complete the snes setup (including setting up the context)
//*******************************************************************|************************************************************//
void SolverBucket::initialize_matrices()
{
  PetscErrorCode perr;                                               // petsc error code variable

  if (type()=="SNES")                                                // if this is a snes object then initialize the context
  {
    ctx_.linear       = linear_;
    ctx_.bilinear     = bilinear_;
    ctx_.bilinearpc   = bilinearpc_;
    ctx_.bcs          = (*system_).bcs();
    ctx_.iteratedfunction = (*system_).iteratedfunction();
  }

  assemble_bilinearforms(true);                                      // perform preassembly of the bilinear forms
  assemble_linearforms(true);                                        // perform preassembly of the linear forms
  
  uint syssize = (*(*system_).function()).vector().size();           // set up a work vector of the correct (system) size
  work_.reset( new dolfin::PETScVector(syssize) ); 

  if(type()=="SNES")                                                 // again, if the type is snes complete the set up
  {
    assert(!res_);                                                   // initialize the residual vector (should only be associated
    res_.reset( new dolfin::PETScVector(syssize) );                  // before now for picard solvers)

    perr = SNESSetFunction(snes_, *(*res_).vec(),                    // set the snes function to use the newly allocated residual vector
                                    FormFunction, (void *) &ctx_); 
    CHKERRV(perr);

    if (bilinearpc_)                                                 // if we have a pc form
    {
      assert(matrixpc_);
      perr = SNESSetJacobian(snes_, *(*matrix_).mat(),               // set the snes jacobian to have two matrices
                  *(*matrixpc_).mat(), FormJacobian, (void *) &ctx_); 
      CHKERRV(perr);
    }
    else                                                             // otherwise
    {
      perr = SNESSetJacobian(snes_, *(*matrix_).mat(),               // set the snes jacobian to have the same matrix twice
                    *(*matrix_).mat(), FormJacobian, (void *) &ctx_); 
      CHKERRV(perr);
    }

  }

}

//*******************************************************************|************************************************************//
// register a (boost shared) pointer to a form in the solver bucket data maps
//*******************************************************************|************************************************************//
void SolverBucket::register_form(Form_ptr form, const std::string &name)
{
  Form_it f_it = forms_.find(name);                                  // check if this name already exists
  if (f_it != forms_.end())
  {
    dolfin::error("Form named \"%s\" already exists in solver.",     // if it does, issue an error
                                                  name.c_str());
  }
  else
  {
    forms_[name] = form;                                             // if not, register the form in the maps
  }
}

//*******************************************************************|************************************************************//
// return a boolean indicating if the solver bucket contains a form with the given name
//*******************************************************************|************************************************************//
bool SolverBucket::contains_form(const std::string &name)                   
{
  Form_it f_it = forms_.find(name);
  return f_it != forms_.end();
}

//*******************************************************************|************************************************************//
// return a (boost shared) pointer to a form from the solver bucket data maps
//*******************************************************************|************************************************************//
Form_ptr SolverBucket::fetch_form(const std::string &name)
{
  Form_it f_it = forms_.find(name);                                  // check if this name already exists
  if (f_it == forms_.end())
  {
    dolfin::error("Form named \"%s\" does not exist in solver.",     // if it doesn't, issue an error
                                                    name.c_str());
  }
  else
  {
    return (*f_it).second;                                           // if it does, return it
  }
}

//*******************************************************************|************************************************************//
// return an iterator to the beginning of the forms_ map
//*******************************************************************|************************************************************//
Form_it SolverBucket::forms_begin()
{
  return forms_.begin();
}

//*******************************************************************|************************************************************//
// return a constant iterator to the beginning of the forms_ map
//*******************************************************************|************************************************************//
Form_const_it SolverBucket::forms_begin() const
{
  return forms_.begin();
}

//*******************************************************************|************************************************************//
// return an iterator to the end of the forms_ map
//*******************************************************************|************************************************************//
Form_it SolverBucket::forms_end()
{
  return forms_.end();
}

//*******************************************************************|************************************************************//
// return a constant iterator to the end of the forms_ map
//*******************************************************************|************************************************************//
Form_const_it SolverBucket::forms_end() const
{
  return forms_.end();
}

//*******************************************************************|************************************************************//
// return a string describing the contents of the solver bucket
//*******************************************************************|************************************************************//
const std::string SolverBucket::str(int indent) const
{
  std::stringstream s;
  std::string indentation (indent*2, ' ');
  s << indentation << "SolverBucket " << name() << std::endl;
  indent++;
  s << forms_str(indent);
  return s.str();
}

//*******************************************************************|************************************************************//
// return a string describing the forms in the solver bucket
//*******************************************************************|************************************************************//
const std::string SolverBucket::forms_str(const int &indent) const
{
  std::stringstream s;
  std::string indentation (indent*2, ' ');
  for ( Form_const_it f_it = forms_.begin(); f_it != forms_.end(); f_it++ )
  {
    s << indentation << "Form " << (*f_it).first  << std::endl;
  }
  return s.str();
}

//*******************************************************************|************************************************************//
// empty the data structures in the solver bucket
//*******************************************************************|************************************************************//
void SolverBucket::empty_()
{
  forms_.clear();
}

