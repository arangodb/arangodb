////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_AQL_CALLSTACK_H
#define ARANGOD_AQL_AQL_CALLSTACK_H 1

#include "Aql/AqlCall.h"

#include <stack>

namespace arangodb {
namespace aql {

class AqlCallStack {
 public:
  // Initial
  AqlCallStack(AqlCall call);
  // Used in subquery
  AqlCallStack(AqlCallStack const& other, AqlCall call);
  // Used to pass between blocks
  AqlCallStack(AqlCallStack const& other);

  // Quick test is this CallStack is of local relevance, or it is sufficient to pass it through
  bool isRelevant() const;

  // Get the top most Call element (this must be relevant).
  // This is popped of the stack and caller can take responsibility for it
  AqlCall popCall();

  // Peek at the top most Call element (this must be relevant).
  // The responsibility will stay at the stack
  AqlCall const& peek() const;

  // Put another call on top of the stack.
  void pushCall(AqlCall&& call);

  // fill up all missing calls within this stack s.t. we reach depth == 0
  // This needs to be called if an executor requires to be fully executed, even if skipped,
  // even if the subquery it is located in is skipped.
  // The default operations added here will correspond to produce all Rows, unlimitted.
  // e.g. every Modification Executor needs to call this functionality, as modifictions need to be
  // performed even if skipped.
  void stackUpMissingCalls();

  // Pops one subquery level.
  // if this isRelevent it pops the top-most call from the stack.
  // if this is not revelent it reduces the depth by 1.
  // Can be savely called on every subquery Start.
  void pop();

 private:
  // The list of operations, stacked by depth (e.g. bottom element is from main query)
  std::stack<AqlCall> _operations;

  // The depth of subqueries that have not issued calls into operations,
  // as they have been skipped.
  // In most cases this will be zero.
  // However if we skip a subquery that has a nested subquery this depth will be 1 in the nested subquery.
  size_t _depth;
};

}  // namespace aql
}  // namespace arangodb

#endif