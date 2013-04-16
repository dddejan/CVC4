#include "solver.h"

#include "theory/rewriter.h"
#include "plugin/solver_plugin_factory.h"
#include "rules/proof_rule.h"
#include "util/node_visitor.h"

#include <algorithm>

using namespace std;

using namespace CVC4;
using namespace CVC4::mcsat;

template<typename T>
class ScopedValue {
  T& d_toWatch;
  T d_oldValue;
public:
  ScopedValue(T& toWatch, T newValue)
  : d_toWatch(toWatch)
  , d_oldValue(toWatch)
  {
    toWatch = newValue;
  }
  ~ScopedValue() {
    d_toWatch = d_oldValue;
  } 
};

Solver::Solver(context::UserContext* userContext, context::Context* searchContext) 
: d_variableDatabase(searchContext)
, d_clauseFarm(searchContext)
, d_clauseDatabase(d_clauseFarm.newClauseDB("problem_clauses"))
, d_trail(searchContext)
, d_rule_Resolution(d_clauseDatabase, d_trail)
, d_featuresDispatch(d_trail)
, d_request(false)
, d_backtrackRequested(false)
, d_backtrackLevel(0)
, d_restartRequested(false)
, d_gcRequested(false)
, d_learntClausesScoreMax(1.0)
, d_learntClausesScoreIncrease(1.0)
, d_removeITE(userContext)
, d_variableRegister(d_variableDatabase)
{
  // Add some engines
  addPlugin("CVC4::mcsat::CNFPlugin");
  addPlugin("CVC4::mcsat::FMPlugin");
  addPlugin("CVC4::mcsat::BCPEngine");
}

Solver::~Solver() {
  for (unsigned i = 0; i < d_plugins.size(); ++ i) {
    delete d_plugins[i];
    delete d_pluginRequests[i];
  }
}

void Solver::addAssertion(TNode assertion, bool process) {
  VariableDatabase::SetCurrent scoped(&d_variableDatabase);
  Debug("mcsat::solver") << "Solver::addAssertion(" << assertion << ")" << endl; 

  // Remove ITEs
  IteSkolemMap skolemMap;
  std::vector<Node> assertionVector;
  assertionVector.push_back(assertion);
  d_removeITE.run(assertionVector, skolemMap);

  for (unsigned i = 0; i < assertionVector.size(); ++ i) {
    // Normalize and remember
    Node normalized = theory::Rewriter::rewrite(assertionVector[i]);
    d_assertions.push_back(normalized);

    // Register all the variables wit the database
    NodeVisitor<VariableRegister>::run(d_variableRegister, normalized);

    // Notify the plugins about the new assertion
    d_notifyDispatch.notifyAssertion(normalized);

    // Run propagation immediately if requested
    if (process) {
      // Propagate the added clauses
      propagate(SolverTrail::PropagationToken::PROPAGATION_INIT);
    }
  }
}

void Solver::processRequests() {

  if (d_backtrackRequested) {
    Debug("mcsat::solver") << "Solver::processBacktrackRequests()" << std::endl;

    // Pop to the requested level
    std::vector<Variable> variablesUnset;
    d_trail.popToLevel(d_backtrackLevel, variablesUnset);

    // Notify the plugins about the backjump variables
    d_notifyDispatch.notifyBackjump(variablesUnset);
    
    // All backtrack clauses are to the same level, but some might propagate and some are decision inducing clauses.
    // If there any propagating clauses, we just do the propagations to ensure progress.
    bool somePropagates = false;
    std::vector<bool> propagates;
    std::set<CRef>::iterator it = d_backtrackClauses.begin();
    for (; it != d_backtrackClauses.end(); ++ it) {
      Clause& clause = it->getClause();
      Debug("mcsat::solver") << "Solver::processBacktrackRequests(): processing " << clause << std::endl;
      if (clause.size() == 1 || d_trail.isFalse(clause[1])) {
        propagates.push_back(true);
        somePropagates = true;
	Debug("mcsat::solver") << "Solver::processBacktrackRequests(): propagates" << std::endl;
      } else {
        propagates.push_back(false);
	Debug("mcsat::solver") << "Solver::processBacktrackRequests(): doesn't propagates " << std::endl;
      }
    }

    if (somePropagates) {
      Debug("mcsat::solver") << "Solver::processBacktrackRequests(): we have a propagation" << std::endl;
      // Propagate all the added clauses that propagate
      std::set<CRef>::iterator it = d_backtrackClauses.begin();
      SolverTrail::PropagationToken propagate(d_trail, SolverTrail::PropagationToken::PROPAGATION_INIT);
      for (unsigned i = 0; it != d_backtrackClauses.end(); ++ it, ++ i) {
        if (propagates[i]) {
          Clause& clause = it->getClause();
          propagate(clause[0], *it);
        }
      }
    } else {
      Debug("mcsat::solver") << "Solver::processBacktrackRequests(): we do a decision" << std::endl;
      // Decision token
      SolverTrail::DecisionToken decide(d_trail);
      LiteralVector litOptions;

      // Pick the literals for the decision
      Clause& c = d_backtrackClauses.begin()->getClause();
      for (unsigned i = 0; i < c.size(); ++ i) {
        if (d_trail.hasValue(c[i].getVariable())) {
          Assert(d_trail.isFalse(c[i]));
        } else {
          litOptions.push_back(c[i]);
	  break;
        }
      }
      Assert(litOptions.size() > 0);

      // Aks for a decision
      d_featuresDispatch.decide(decide, litOptions);
      if (!decide.used()) {
        // If nobody decided, we decide
        decide(litOptions[0]);
      }
    }

    // Clear the requests
    d_backtrackRequested = false;
    d_backtrackClauses.clear();
  }
  
  if (d_restartRequested) {
    
    Notice() << "mcsat::Solver: Restarting at " << d_stats.conflicts.getData() << " conflicts" << std::endl;
    
    std::vector<Variable> variablesUnset;
    // Restart to level 0
    d_trail.popToLevel(0, variablesUnset);
    // Notify of backjump
    d_notifyDispatch.notifyBackjump(variablesUnset);
    // Notify of the restart 
    d_notifyDispatch.notifyRestart();

    if (true || d_gcRequested) {
      performGC();
    }

    d_restartRequested = false;
    d_gcRequested = false;
    ++ d_stats.restarts;
  }

  // Requests have been processed
  d_request = false;
}

void Solver::requestRestart() {
  d_request = true;
  d_restartRequested = true;
}

void Solver::requestPropagate() {
  d_request = true;
}

void Solver::requestGC() {
  d_gcRequested = true;
}


void Solver::requestBacktrack(unsigned level, CRef cRef) {
  Assert(level < d_trail.decisionLevel(), "Don't try to fool backtracking to do your propagation");

  Debug("mcsat::solver") << "Solver::requestBacktrack(" << level << ", " << cRef << ")" << std::endl;

  if (!d_backtrackRequested) {
    // First time request
    d_backtrackLevel = level;
  } else {
    if (d_backtrackLevel > level) {
      // Even lower backtrack
      d_backtrackLevel = level;
      d_backtrackClauses.clear();
    }
  }

  // Interrupt the dispatch
  d_featuresDispatch.interrupt();

  d_request = true;
  d_backtrackRequested = true;
  d_backtrackClauses.insert(cRef);
}


void Solver::propagate(SolverTrail::PropagationToken::Mode mode) {
  Debug("mcsat::solver") << "Solver::propagate(" << mode << ")" << std::endl;

  bool propagationDone = false;
  while (d_trail.consistent() && !d_request && !propagationDone) {
    // Token for the plugins to propagate at
    SolverTrail::PropagationToken propagationOut(d_trail, mode);
    // Let all plugins propagate
    d_featuresDispatch.propagate(propagationOut);
    // If the token wasn't used, we're done
    propagationDone = !propagationOut.used();
  }
}

bool Solver::check() {
  
  Debug("mcsat::solver::search") << "Solver::check()" << std::endl;

  // Search while not all variables assigned 
  while (true) {
    
    // If a backtrack was requested then
    processRequests();

    // Normal propagate
    propagate(SolverTrail::PropagationToken::PROPAGATION_NORMAL);

    // If inconsistent, perform conflict analysis
    if (!d_trail.consistent()) {

      Debug("mcsat::solver::search") << "Solver::check(): Conflict" << std::endl;
      Debug("mcsat::trail") << d_trail << std::endl;
     
      ++ d_stats.conflicts;
      
      // Notify of a new conflict situation
      d_notifyDispatch.notifyConflict();
      
      // If the conflict is at level 0, we're done
      if (d_trail.decisionLevel() == 0) {
        return false;
      }

      // Analyze the conflict
      analyzeConflicts();

      // Start over with propagation
      continue;
    }
    
    // Process any requests
    if (d_request) {
      continue;
    }

    // Clauses processed, propagators done, we're ready for a decision
    SolverTrail::DecisionToken decisionOut(d_trail);
    Debug("mcsat::solver::search") << "Solver::check(): trying decision" << std::endl;
    d_featuresDispatch.decide(decisionOut);

    // If no decisions were made we are done, do a completeness check
    if (!decisionOut.used()) {
      Debug("mcsat::solver::search") << "Solver::check(): no decisions, doing fullcheck" << std::endl;
      // Do complete propagation
      propagate(SolverTrail::PropagationToken::PROPAGATION_COMPLETE);
      // If no-one has anything to say, we're done
      if (!d_request) {
	break;
      }
    } else {
      // We made a new decision
      ++ d_stats.decisions;
    }
    
  }

  // Since we're here, we're satisfiable
  if (Debug.isOn("mcsat::model")) {
    const std::vector<Variable>& vars = d_variableRegister.getVariables();
    for (unsigned i = 0; i < vars.size(); ++ i) {
      Debug("mcsat::model") << vars[i] << "\t->\t" << d_trail.value(vars[i]) << std::endl; 
    }
  }
  
  return true;
}

void Solver::addPlugin(std::string pluginId) {

  d_pluginRequests.push_back(new SolverPluginRequest(this));
  SolverPlugin* plugin = SolverPluginFactory::create(pluginId, d_clauseDatabase, d_trail, *d_pluginRequests.back());
  d_plugins.push_back(plugin);
  d_notifyDispatch.addPlugin(plugin);
  d_featuresDispatch.addPlugin(plugin);

  Notice() << "mcsat::Solver: added plugin " << *plugin << std::endl;
}

void Solver::analyzeConflicts() {

  // Conflicts to resolve
  std::vector<SolverTrail::InconsistentPropagation> conflicts;
  d_trail.getInconsistentPropagations(conflicts);

  Assert(conflicts.size() > 0);
  Debug("mcsat::solver::analyze") << "Solver::analyzeConflicts()" << std::endl;

  for (unsigned c = 0; c < conflicts.size(); ++ c) {

    // Get the current conflict
    SolverTrail::InconsistentPropagation& conflictPropagation = conflicts[c];

    // Clause in conflict
    Clause& conflictingClause = conflictPropagation.reason.getClause();
    Debug("mcsat::solver::analyze") << "Solver::analyzeConflicts(): analyzing " << conflictingClause << std::endl;

    // This is a useful clause
    if (conflictingClause.getRuleId() == d_rule_Resolution.getRuleId()) {
      bumpClause(conflictPropagation.reason);
    }

    // The level at which the clause is in conflict
    unsigned conflictLevel = 0;
    for (unsigned i = 0; i < conflictingClause.size(); ++ i) {
      conflictLevel = std::max(conflictLevel, d_trail.decisionLevel(conflictingClause[i].getVariable()));
    }
    Debug("mcsat::solver::analyze") << "Solver::analyzeConflicts(): in conflict at level " << conflictLevel << std::endl;
    Debug("mcsat::solver::analyze") << "Solver::analyzeConflicts(): trail: " << d_trail << std::endl;

    // The Boolean resolution rule
    d_rule_Resolution.start(conflictPropagation.reason);
    d_notifyDispatch.notifyConflictResolution(conflictPropagation.reason);
    
    // Set of variables in the current resolvent that have a reason
    VariableHashSet varsWithReason;
    // Set of variables in the from the current level that we have seen alrady
    VariableHashSet varsSeen;

    // Number of literals in the current resolvent
    unsigned varsAtConflictLevel = 0;

    // Setup the initial variable info
    for (unsigned i = 0; i < conflictingClause.size(); ++ i) {
      Literal lit = conflictingClause[i];
      Variable var = lit.getVariable();
      // We resolve literals at the conflict level
      if (d_trail.decisionLevel(var) == conflictLevel) {
        if (varsSeen.count(var) == 0) {
          varsAtConflictLevel ++;
          varsSeen.insert(var);
          if (d_trail.hasReason(~lit)) {
            varsWithReason.insert(var);
          }
        }
      }
    }

    // Resolve out all literals at the top level
    Assert(d_trail.size(conflictLevel) >= 1);
    unsigned trailIndex = d_trail.size(conflictLevel) - 1;

    // While there is more than one literal at current conflict level (UIP)
    while (!d_trail[trailIndex].isDecision() && varsAtConflictLevel > 1) {

      Debug("mcsat::solver::analyze") << "Solver::analyzeConflicts(): can be resolved: " << varsWithReason << std::endl;
      Debug("mcsat::solver::analyze") << "Solver::analyzeConflicts(): seen: " << varsSeen << std::endl;
      Debug("mcsat::solver::analyze") << "Solver::analyzeConflicts(): at index " << trailIndex << std::endl;

      // Find the next literal to resolve
      while (!d_trail[trailIndex].isDecision() && varsWithReason.find(d_trail[trailIndex].var) == varsWithReason.end()) {
        -- trailIndex;
      }

      // If we hit the decision then it must be either a UIP or a semantic decision
      if (d_trail[trailIndex].isDecision()) {
        Assert(d_trail[trailIndex].type == SolverTrail::SEMANTIC_DECISION || varsAtConflictLevel == 1);
        break;
      }

      // We can resolve the variable, so let's do it
      Variable varToResolve = d_trail[trailIndex].var;
      Debug("mcsat::solver::analyze") << "Solver::analyzeConflicts(): resolving: " << varToResolve << " at " << trailIndex << std::endl;

      // If the variable is false, take the negated literal
      Literal literalToResolve(varToResolve, d_trail.isFalse(varToResolve));

      // Get the reason of the literal
      CRef literalReason = d_trail.reason(literalToResolve);
      Clause& literalReasonClause = literalReason.getClause();
      Assert(literalReasonClause[0].getVariable() == varToResolve);

      // Resolve the literal (propagations should always have first literal propagating)
      d_rule_Resolution.resolve(literalReason, 0);
      d_notifyDispatch.notifyConflictResolution(literalReason);

      // We removed one literal
      -- varsAtConflictLevel;

      // Update the info for the resolved literals
      Assert(literalReasonClause[0].getVariable() == varToResolve);
      for (unsigned i = 1; i < literalReasonClause.size(); ++ i) {
	Literal lit = literalReasonClause[i];
        Variable var = lit.getVariable();
        Debug("mcsat::solver::analyze") << "Solver::analyzeConflicts(): checking reason lit: " << lit << " at " << d_trail.decisionLevel(var) << std::endl;
        // We resolve literals at the conflict level
        if (d_trail.decisionLevel(var) == conflictLevel) {
          Debug("mcsat::solver::analyze") << "Solver::analyzeConflicts(): checking reason var: " << (varsSeen.count(var) == 0 ? "unseen" : "seen") << std::endl;
          if (varsSeen.count(var) == 0) {
            varsAtConflictLevel ++;
            varsSeen.insert(var);
            if (d_trail.hasReason(~lit)) {
              varsWithReason.insert(var);
            }
          }
        }
      }

      // Done with this trail element
      -- trailIndex;
    }

    // Finish the resolution
    CRef resolvent = d_rule_Resolution.finish();
    Debug("mcsat::solver::analyze") << "Solver::analyzeConflicts(): resolvent: " << resolvent << std::endl;

    if (resolvent.getClause().getRuleId() == d_rule_Resolution.getRuleId()) {
      // If this is a new clause, so we manage it's deletion
      d_learntClausesScore[resolvent] = d_learntClausesScoreMax;
    } 
    
    // If number of variables in the resolvent is > 1, it must be that these 
    // literals are semantically true. This also means that the clause 
    // propagates at the conflict level, and we can't resolve the literals.
    // So, we go back and decide one of them 
    if (varsAtConflictLevel > 1) {
      Assert(d_trail[trailIndex].type == SolverTrail::SEMANTIC_DECISION);
      requestBacktrack(conflictLevel - 1, resolvent);
    }
  }
}

void Solver::bumpClause(CRef cRef) {
  std::hash_map<CRef, double, CRefHashFunction>::iterator find = d_learntClausesScore.find(cRef);
  Assert(find != d_learntClausesScore.end());

  find->second += d_learntClausesScoreIncrease;

  if (find->second > 1e20) {
    std::hash_map<CRef, double, CRefHashFunction>::iterator it = d_learntClausesScore.begin();
    std::hash_map<CRef, double, CRefHashFunction>::iterator it_end = d_learntClausesScore.begin();
    for (; it != it_end; ++ it)  {
      it->second = it->second * 1e-20;
    }
    // TODO: decay clause activities
    // d_learntClausesScoreIncrease *= 1e-20;
  }
}

struct learnt_cmp_by_score {

  const std::hash_map<CRef, double, CRefHashFunction>& d_scoreMap;

  learnt_cmp_by_score(const std::hash_map<CRef, double, CRefHashFunction>& map)
  : d_scoreMap(map)
  {}

  bool operator () (const CRef& c1, const CRef& c2) const {
    return d_scoreMap.find(c1)->second < d_scoreMap.find(c2)->second;
  }

};

void Solver::shrinkLearnts() {
  learnt_cmp_by_score cmp(d_learntClausesScore);
  std::sort(d_learntClauses.begin(), d_learntClauses.end(), cmp);
  d_learntClauses.resize(d_learntClauses.size() / 2);
}

void Solver::performGC() {

  // Shrink learnt clauses we care about
  shrinkLearnts();

  // List of clauses to keep
  std::set<CRef> clausesToKeep;
  // List of variables to keep
  std::set<Variable> variablesToKeep;

  // Input variables
  std::vector<Variable>& inputVariables = d_variableRegister.getVariables();
  variablesToKeep.insert(inputVariables.begin(), inputVariables.end());

  // Learnt clauses we decide to keep
  clausesToKeep.insert(d_learntClauses.begin(), d_learntClauses.end());

  // Ask the trail
  d_trail.gcMark(variablesToKeep, clausesToKeep);

  // Dispatch marking to the plugins
  for(unsigned i = 0; i < d_plugins.size(); ++ i) {
    // List of clauses to keep
    std::set<CRef> p_clausesToKeep;
    // List of variables to keep
    std::set<Variable> p_variablesToKeep;
    // Ask the plugin
    d_plugins[i]->gcMark(p_variablesToKeep, p_clausesToKeep);
    // Remember
    variablesToKeep.insert(p_variablesToKeep.begin(), p_variablesToKeep.end());
    clausesToKeep.insert(p_clausesToKeep.begin(), p_clausesToKeep.end());
  }

  // Go through the clauses and get the variables
  std::set<CRef>::const_iterator c_it = clausesToKeep.begin();
  std::set<CRef>::const_iterator c_it_end = clausesToKeep.end();
  for (; c_it != c_it_end; ++ c_it) {
    Clause& clause = (*c_it).getClause();
    for (unsigned i = 0; i < clause.size(); ++ i) {
      variablesToKeep.insert(clause[i].getVariable());
    }
  }

  // Do the variable GC
  VariableGCInfo variableRelocationInfo;
  d_variableDatabase.performGC(variablesToKeep, variableRelocationInfo);

  // Do the clause GC
  ClauseRelocationInfo clauseRelocationInfo;
  d_clauseFarm.performGC(clausesToKeep, variableRelocationInfo, clauseRelocationInfo);

  // Clauses
  clauseRelocationInfo.relocate(d_learntClauses);

  // The trail
  d_trail.gcRelocate(variableRelocationInfo, clauseRelocationInfo);
  for (unsigned i = 0; i < d_plugins.size(); ++ i) {
    d_plugins[i]-> gcRelocate(variableRelocationInfo, clauseRelocationInfo);
  }
}
