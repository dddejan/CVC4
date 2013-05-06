#include "theory/rewriter.h"
#include "mcsat/rules/ackermann_rule.h"

using namespace CVC4;
using namespace mcsat;
using namespace rules;

AckermannRule::AckermannRule(ClauseDatabase& clauseDB, const SolverTrail& trail) 
: ProofRule("mcsat::ackermann_rule", clauseDB, trail)
{
}

static Literal getLiteral(Node node) {
  node = theory::Rewriter::rewrite(node);
  bool negated = node.getKind() == kind::NOT;
  Node atom = negated ? node[0] : node;
  Variable variable = VariableDatabase::getCurrentDB()->getVariable(atom);
  return Literal(variable, negated);
}

CRef AckermannRule::apply(Variable f1, Variable f2, SolverTrail::PropagationToken& propToken) {
  std::vector<Literal> lits;
  
  TNode f1node = f1.getNode();
  TNode f2node = f2.getNode();

  Assert(f1node.getKind() == kind::APPLY_UF);
  Assert(f2node.getKind() == kind::APPLY_UF);
  Assert(f1node.getOperator() == f2node.getOperator());
  
  // x_i = y_i
  for (unsigned i = 0; i < f1node.getNumChildren(); ++ i) {
    // Applications always have variable children
    Literal eqLit = getLiteral(f1node[i].eqNode(f2node[i]));
    // Propagate the equality is true
    unsigned level = 0;
    if (!f1node[i].isConst()) {
      Assert(VariableDatabase::getCurrentDB()->hasVariable(f1node[i]));
      Variable f1child = VariableDatabase::getCurrentDB()->getVariable(f1node[i]);
      Assert(d_trail.hasValue(f1child));
      level = std::max(level, d_trail.decisionLevel(f1child));
    }
    if (!f2node[i].isConst()) {
      Assert(VariableDatabase::getCurrentDB()->hasVariable(f2node[i]));
      Variable f2child = VariableDatabase::getCurrentDB()->getVariable(f2node[i]);
      Assert(d_trail.hasValue(f2child));
      level = std::max(level, d_trail.decisionLevel(f2child));
    }
    propToken(eqLit, level);
    lits.push_back(~eqLit);
  }
  
  // |- f(x) = f(y)
  Literal eqLit = getLiteral(f1node.eqNode(f2node));
  unsigned level = std::max(d_trail.decisionLevel(f1), d_trail.decisionLevel(f2));
  propToken(~eqLit, level);
  lits.push_back(eqLit);
  
  return commit(lits);
}
