#pragma once

#include "cvc4_public.h"

#include <string>
#include <vector>

#include "context/context.h"
#include "util/exception.h"

#include "mcsat/plugin/solver_plugin.h"

namespace CVC4 {
namespace mcsat {

class SolverPluginFactoryException : public Exception {
public:
  SolverPluginFactoryException(std::string message)
  : Exception(message) {
  }
};

class SolverPluginFactory {
public:
  static SolverPlugin* create(std::string name, const SolverTrail& d_trail, SolverPluginRequest& request)
    throw(SolverPluginFactoryException);
};

}
}


