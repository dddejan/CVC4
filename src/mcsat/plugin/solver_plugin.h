#pragma once

#include "cvc4_private.h"

#include "expr/node.h"
#include "expr/kind.h"

#include "context/cdo.h"
#include "context/cdlist.h"

#include "mcsat/clause/clause_db.h"
#include "mcsat/solver_trail.h"
#include "mcsat/plugin/solver_plugin_notify.h"

namespace CVC4 {
namespace mcsat {

class Solver;

class SolverPluginRequest {

  /** The solver */
  Solver* d_solver;

public:

  /** Construct a request token for the given solver */
  SolverPluginRequest(Solver* solver)
  : d_solver(solver) {}

  /**
   * Ask for a backtrack to given level. The level must be smaller than the current decision level. The clause
   * must not be satisfied, and it should propagate at the given level. If it doesn't propagate, a non-assigned
   * literal from the clause will be selected as a decision.
   */
  void backtrack(unsigned level, CRef cRef);
  
  /**
   * Ask for a search restart.
   */
  void restart();

  /**
   * Ask for garbage collection. Garbage collection will be performed at next restart and the plugins will get notified
   * pre and post garbage collection.
   */
  void gc();

};

/**
 * Base class for model based T-solvers.
 */
class SolverPlugin : public SolverPluginNotify {

private:

  SolverPlugin() CVC4_UNUSED;
  SolverPlugin(const SolverPlugin&) CVC4_UNUSED;
  SolverPlugin& operator=(const SolverPlugin&) CVC4_UNUSED;

protected:

  /** Clause database we're using */
  ClauseDatabase& d_clauseDb;

  /** The trail that the plugin should use */
  const SolverTrail& d_trail;

  /** Channel to request something from the solver */
  SolverPluginRequest& d_request;

  /** Construct the plugin. */
  SolverPlugin(ClauseDatabase& clauseDb, const SolverTrail& trail, SolverPluginRequest& request)
  : d_clauseDb(clauseDb)
  , d_trail(trail)
  , d_request(request)
  {}

  /** Get the SAT context associated to this Theory. */
  context::Context* getSearchContext() const {
    return d_trail.getSearchContext();
  }

public:

  /** Destructor */
  virtual ~SolverPlugin() {}

  /** Check the current assertions for consistency. */
  virtual void check() = 0;

  /** Perform propagation */
  virtual void propagate(SolverTrail::PropagationToken& out) = 0;

  /** Perform a decision */
  virtual void decide(SolverTrail::DecisionToken& out) = 0;

  /** String representation of the plugin (for debug purposes mainly) */
  virtual std::string toString() const = 0;
}; /* class SolverPlugin */

inline std::ostream& operator << (std::ostream& out, const SolverPlugin& plugin) {
  out << plugin.toString();
  return out;
}

} /* CVC4::mcsat namespace */
} /* CVC4 namespace */

#include "mcsat/plugin/solver_plugin_registry.h"
