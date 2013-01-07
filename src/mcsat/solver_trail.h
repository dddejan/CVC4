#pragma once

#include "cvc4_private.h"

#include <set>
#include <vector>

#include "mcsat/variable/variable_db.h"
#include "mcsat/variable/variable_table.h"
#include "mcsat/clause/clause_db.h"
#include "mcsat/clause/literal_table.h"

#include "context/cdinsert_hashmap.h"

namespace CVC4 {
namespace mcsat {

class SolverPlugin;

/** Possible values a boolean term can have */
enum BooleanValue {
  /** True */
  BOOL_TRUE,
  /** False */
  BOOL_FALSE,
  /** Unknown */
  BOOL_UNKNOWN
};


class SolverTrail {

public:

  /** Types of elements in the trail */
  enum Type {
    /** Decision on value of a Boolean typed variable */
    BOOLEAN_DECISION,
    /** Decision on value of a non-Boolean typed variable */
    SEMANTIC_DECISION,
    /** Propagation of a Boolean typed variable supported by a clause */
    CLAUSAL_PROPAGATION,
    /** Propagation of a Boolean typed variable supported by the model */
    SEMANTIC_PROPAGATION,
  };

  /**
   * The element of the trail only records the variable that became assigned and
   * the type of event that led to its assignment.
   */
  struct Element {
    /** Type of the element */
    Type type;
    /** The variable that got changed */
    Variable var;

    Element(Type type, Variable var)
    : type(type), var(var) {}
  };

  /**
   * An object that can provide a reason for the propagation
   */
  class ReasonProvider {
  public:
    /**
     * Provide the literals that imply the propagation of l. The literals that imply l should be put into
     * the reason array and must all be present in the trail before l.
     */
    virtual void explain(Literal l, std::vector<Literal>& reason) = 0;
  };

private:

  /**
   * Simple reason provider based on a clause.
   */
  class ClauseReasonProvider : public ReasonProvider {
    typedef literal_table<CRef_Strong> reasons_map;

    /** Map from literals to clausal reasons */
    reasons_map d_reasons;
  public:
    void explain(Literal l, std::vector<Literal>& reason);
    CRef_Strong& operator [] (Literal l) { return d_reasons[l]; }
  };
  
  /** Is the consistent (the last element is the cause) */
  bool d_consistent;
  
  /** Vector of propagated literals that are inconsistent */
  std::set<Literal> d_inconsistentPropagations;

  /** The actual trail */
  std::vector<Element> d_trail;

  /** Reason providers for clausal propagations */
  literal_table<ReasonProvider*> d_reasonProviders;

  /** Trail containing trail sizes per decision level */
  std::vector<size_t> d_decisionTrail;
  
  /** Number of decisions */
  size_t d_decisionLevel;
  
  /** Has there been a backtrack request */
  bool d_backtrackRequested;
  
  /** The level of the backtrack request */
  size_t d_backtrackLevel;
  
  /** Mark a new decision */
  void newDecision();
  
  /** Clause of the problem */
  const ClauseDatabase& d_problemClauses;
  
  /** Auxiliary clause important for the search (conflict analysis clauses) */
  ClauseDatabase& d_auxilaryClauses;

  /** True value */
  Variable c_TRUE;

  /** False value */
  Variable c_FALSE;

  /** Model indexed by variables */
  variable_table<Variable_Strong> d_model;
  
  /** The context of the search */
  context::Context* d_context;

  /** Reason provider for clause propagations */
  ClauseReasonProvider d_clauseReasons;

public:
  
  /** Create an empty trail with the give set of clauses */
  SolverTrail(const ClauseDatabase& clauses, context::Context* context);
  
  /** Destructor */
  ~SolverTrail();

  /** Get the i-th element of the trail */
  const Element& operator [] (size_t i) const { 
    return d_trail[i]; 
  }

  /** Get the size of the trail */
  size_t size() const { 
    return d_trail.size();     
  }
  
  /** Is the trail consistent */
  bool consistent() const {
    return d_consistent;
  }

  /** Returns the current decision level */
  size_t decisionLevel() const {
    return d_decisionLevel;
  }
  
  /** Returns the context that the trail is controlling */
  context::Context* getSearchContext() const {
    return d_context;
  }

  /** Return the value of the literal in the current trail */
  Variable value(Literal l) const {
    Variable v = d_model[l.getVariable()];
    if (l.isNegated()) {
      if (v == c_TRUE) {
        return c_FALSE;
      }
      if (v == c_FALSE) {
        return c_TRUE;
      }
    }
    return v;
  }

  /** Get the true constant */
  Variable getTrue() const { return c_TRUE; }
  /** Get the false constant */
  Variable getFalse() const { return c_FALSE; }

  /** Return the value of a Boolean value in the current trail */
  Variable value(Variable var) const {
    return d_model[var];
  }

  /**
   * Add a listener for the creation of new clauses. A context independent listener will only be notified
   * once when the clause is created. If the listener is context dependent, it will be notified when the clause is
   * created, but it will be re-notified on every pop so that it can update it's internal state.
   *
   * @param listener the listener
   * @param contextDependent whether the listener is context dependent
   */
  void addNewClauseListener(ClauseDatabase::INewClauseNotify* listener) const;

  /**
   * Request a backtrack.
   */
  void requestBacktrack(size_t decisionLevel);

  /** Token for modules to perform propagations */
  class PropagationToken {
    SolverTrail& d_trail;
  public:
    PropagationToken(SolverTrail& trail)
    : d_trail(trail) {}
    /** Semantic propagation based on current model in the trail */
    void operator () (Literal l);
    /** Clausal propagation based on the current boolean model in the trail */
    void operator () (Literal l, CRef reason);
    /** Clausal propagation with on-demand reason */
    void operator () (Literal l, ReasonProvider* reason_provider);
  };
  
  /** Token for modules to perform decisions */
  class DecisionToken {
    SolverTrail& d_trail;
  public:
    DecisionToken(SolverTrail& trail)
    : d_trail(trail) {}
    /** Decide a Boolean typed variable to a value */
    void operator () (Literal lit);
    /** Decide a non-Boolean typed variable to a value */
    void operator () (Variable variable, TNode value);
  };

  friend class PropagationToken;
  friend class DecisionToken;
};

}
}
