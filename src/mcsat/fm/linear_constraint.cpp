#include "mcsat/fm/linear_constraint.h"
#include "mcsat/variable/variable_db.h"
#include "mcsat/solver_trail.h"

#include "theory/rewriter.h"

using namespace CVC4;
using namespace mcsat;
using namespace fm;

void LinearConstraint::getVariables(std::vector<Variable>& vars) {
  var_to_rational_map::const_iterator it = d_coefficients.begin();
  var_to_rational_map::const_iterator it_end = d_coefficients.end();
  while (it != it_end) {
    Variable var = it->first;
    if (!var.isNull()) {
      vars.push_back(var);
    }
    ++ it;
  }
}

void LinearConstraint::swap(LinearConstraint& c) {
  d_coefficients.swap(c.d_coefficients);
  std::swap(d_kind, c.d_kind);
}

bool LinearConstraint::evaluate(const SolverTrail& trail) const {

  Rational lhsValue;
  const_iterator it = begin();
  const_iterator it_end = end();
  for (; it != it_end; ++ it) {
    Variable var = it->first;
    if (var.isNull()) {
      lhsValue += it->second;
    } else {
      Assert(trail.hasValue(var));
      lhsValue += trail.value(var).getConst<Rational>() * it->second;
    }
  }

  switch (d_kind) {
  case kind::LT:
      return lhsValue < 0;
    case kind::LEQ:
      return lhsValue <= 0;
    case kind::GT:
      return lhsValue > 0;
    case kind::GEQ:
      return lhsValue >= 0;
    case kind::EQUAL:
      return lhsValue == 0;
    case kind::DISTINCT:
      return lhsValue != 0;
    default:
      Unreachable();
      break;
    }

  return true;
}


bool LinearConstraint::parse(Literal constraint, LinearConstraint& out) {

  Debug("mcsat::fm") << "LinearConstraint::parse(" << constraint << ")" << std::endl;

  out.clear();

  Variable var = constraint.getVariable();
  TNode node = var.getNode();

  // Multiplier for LHS and RHS
  Rational m = 1;

  // Set the kind of the constraint
  out.d_kind = node.getKind();
  if (constraint.isNegated()) {
    out.d_kind = negateKind(out.d_kind);
  }

  // If < or <=, multiply with -1
  if (out.d_kind == kind::LT || out.d_kind == kind::LEQ) {
    m = -m;
    out.d_kind = flipKind(out.d_kind);
  }
  
  // Parse the LHS
  bool linear = parse(node[0],  m, out.d_coefficients);

  // If linear parse the RHS
  if (linear) {
    linear = parse(node[1], -m, out.d_coefficients);
  }

  Debug("mcsat::fm") << "LinearConstraint::parse(" << constraint << ") => " << out << std::endl;

  return linear;
}

bool LinearConstraint::parse(TNode term, Rational mult, var_to_rational_map& coefficientMap) {

  Debug("mcsat::fm") << "LinearConstraint::parse(" << term << ")" << std::endl;

  VariableDatabase& db = *VariableDatabase::getCurrentDB();

  switch (term.getKind()) {
    case kind::CONST_RATIONAL:
      coefficientMap[Variable::null] += mult*term.getConst<Rational>();
      break;
    case kind::VARIABLE: {
      Assert(db.hasVariable(term));
      Variable var = db.getVariable(term);
      coefficientMap[var] += mult;
      break;
    }
    case kind::MULT: {
      if (term.getNumChildren() == 2 && term[0].isConst()) {
        return parse(term[1], mult*term[0].getConst<Rational>(), coefficientMap);
      } else {
        return false;
      }
      break;
    }
    case kind::PLUS:
      for (unsigned i = 0; i < term.getNumChildren(); ++ i) {
        bool linear = parse(term[i], mult, coefficientMap);
        if (!linear) {
          return false;
        }
      }
      break;
    case kind::MINUS: {
      bool linear;
      linear = parse(term[0],  mult, coefficientMap);
      if (!linear) {
        return false;
      }
      return parse(term[1], -mult, coefficientMap);
      break;
    }
    case kind::UMINUS:
      return parse(term[0], -mult, coefficientMap);
      break;
    default:
      Unreachable();
  }

  return true;
}

void LinearConstraint::toStream(std::ostream& out) const {
  out << "LinearConstraint[" << d_kind << ", ";
  fm::var_to_rational_map::const_iterator it = d_coefficients.begin();
  fm::var_to_rational_map::const_iterator it_end = d_coefficients.end();
  bool first = true;
  while (it != it_end) {
    out << (first ? "" : "+");
    if (it->first.isNull()) {
      out << it->second;
    } else {
      out << "(" << it->second << "*" << it->first << ")";
    }
    first = false;
    ++ it;
  }
  out << "]";
}

Kind LinearConstraint::negateKind(Kind kind) {
  switch (kind) {
  case kind::LT:
    // not (a < b) = (a >= b)
    return kind::GEQ;
  case kind::LEQ:
    // not (a <= b) = (a > b)
    return kind::GT;
  case kind::GT:
    // not (a > b) = (a <= b)
    return kind::LEQ;
  case kind::GEQ:
    // not (a >= b) = (a < b)
    return kind::LT;
  case kind::EQUAL:
    // not (a = b) = (a != b)
    return kind::DISTINCT;
  default:
    Unreachable();
    break;
  }
  return kind::LAST_KIND;
}

Kind LinearConstraint::flipKind(Kind kind) {
  switch (kind) {
  case kind::LT:
    return kind::GT;
  case kind::LEQ:
    return kind::GEQ;
  case kind::GT:
    return kind::GT;
  case kind::GEQ:
    return kind::LEQ;
  default:
    return kind;
  }
}

Rational LinearConstraint::getCoefficient(Variable var) const {
  const_iterator find = d_coefficients.find(var);
  if (find == d_coefficients.end()) {
    return 0;
  } else {
    return find->second;
  }
}

void LinearConstraint::multiply(Rational c) {
  Assert(c > 0);
  var_to_rational_map::iterator it = d_coefficients.begin();
  var_to_rational_map::iterator it_end = d_coefficients.end();
  for (; it != it_end; ++ it) {
    it->second *= c;
  }
}
  
void LinearConstraint::add(const LinearConstraint& other, Rational c) {

  // Figure out the resulting kind
  switch (d_kind) {
  case kind::EQUAL:
    // The result is whatever the other one is
    d_kind = other.d_kind;
    break;
  case kind::GT:
    // GT + any other constraint is GT, so we stay the same
    break;
  case kind::GEQ:
    // GEQ + EQ  = GEQ
    // GEQ + GEQ = GEQ
    // GEQ + GT  = GT
    if (other.d_kind == kind::GT) {
      d_kind = kind::GT;
    }
    break;
  default:
    Unreachable();
  }

  // Add up the terms
  const_iterator it = other.begin();
  const_iterator it_end = other.end();
  for (; it != it_end; ++ it) {
    Rational newValue = (d_coefficients[it->first] += c*it->second);
    if (newValue.isZero() && !it->first.isNull()) {
      d_coefficients.erase(it->first);
    }
  }
}

Literal LinearConstraint::getLiteral() const {
  
  Assert(d_coefficients.size() >= 1);
  
  NodeManager* nm = NodeManager::currentNM();
  VariableDatabase* db = VariableDatabase::getCurrentDB();

  // Construct the sum
  Node sum;
  if (d_coefficients.size() > 1) {
    NodeBuilder<> sumBuilder(kind::PLUS);
    const_iterator it = begin();
    const_iterator it_end = end();
    for (; it != it_end; ++ it) {
      Variable x = it->first;
      Rational a = it->second;
      Node xNode = x.getNode();
      Node aNode = nm->mkConst<Rational>(a);
      sumBuilder << nm->mkNode(kind::MULT, aNode, xNode);
    }
    sum = sumBuilder;
  } else {
    // We only have the constant
    Assert(d_coefficients.begin()->first.isNull());
    sum = nm->mkConst<Rational>(d_coefficients.begin()->second);
  }

  // Construct the constraint 
  Node node = nm->mkNode(d_kind, sum, nm->mkConst<Rational>(0));

  // Normalize the constraint
  Node normalized = theory::Rewriter::rewrite(node);

  // Is the normalized constraint negated 
  bool negated = normalized.getKind() == kind::NOT;
  // Get the atom of the literal
  TNode atom = negated ? normalized[0] : normalized;
  // Construct the variable (this spawns notifications)
  Variable variable = db->getVariable(atom);

  // Return the literal
  return Literal(variable, negated);
}
