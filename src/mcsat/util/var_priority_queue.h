#pragma once

#include "mcsat/variable/variable.h"

#include <ext/pb_ds/priority_queue.hpp>

namespace CVC4 {
namespace mcsat {
namespace util {

/**
 * A priority queue for the variables of certain types. Note: THIS IS ONLY FOR VARIABLES OF A SINGLE TYPE.
 */
class VariablePriorityQueue {

  /** Scores of variables */
  std::vector<double> d_variableScores;

  /** Max over the score of all the variables */
  double d_variableScoresMax;

  /** Compare variables according to their current score */
  class VariableScoreCmp {
    std::vector<double>& d_variableScores;
  public:
    VariableScoreCmp(std::vector<double>& variableScores)
    : d_variableScores(variableScores) {}
    bool operator() (const Variable& v1, const Variable& v2) const {
      return d_variableScores[v1.index()] < d_variableScores[v2.index()];
    }
  } d_variableScoreCmp;

  /** Priority queue type we'll be using to keep the variables to pick */
  typedef __gnu_pbds::priority_queue<Variable, VariableScoreCmp> variable_queue;

  /** Priority queue for the variables */
  variable_queue d_variableQueue;

  /** Position in the variable queue */
  std::vector<variable_queue::point_iterator> d_variableQueuePositions;

  /** How much to increase the score per bump */
  double d_variableScoreIncreasePerBump;

  /** Max score before we scale everyone back */
  double d_maxScoreBeforeScaling;

public:

  /** Construct a variable queue for variables of given type */
  VariablePriorityQueue();

  /** Add new variable to track */
  void newVariable(Variable var);

  /** Get the top variable */
  Variable pop();

  /** Is the queue empty */
  bool empty() const;

  /** Enqueues the variable for decision making */
  void enqueue(Variable var);

  /** Is the variable in queue */
  bool inQueue(Variable var) const;

  /** Bump the score of the variable */
  void bumpVariable(Variable var);

  /** Get the score of a variable */
  double getScore(Variable var) const;

};

}
}
}




