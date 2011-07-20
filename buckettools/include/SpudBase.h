
#ifndef __SPUD_BASE_H
#define __SPUD_BASE_H

#include "BoostTypes.h"
#include <dolfin.h>
#include <spud>

namespace buckettools
{
  // A collection of tools that are spud (and schema) specific but do not depend on any class
  // objects in the bucketools library

  // Initialize an expression based on a optionpath (and size and shape for vectors and tensors)
  Expression_ptr initialize_expression(const std::string &optionpath, const int &size, const std::vector<int> &shape);

  void spud_err(const std::string &optionpath, const Spud::OptionError &error);

}

#endif