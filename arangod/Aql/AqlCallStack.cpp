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

#include "AqlCallStack.h"

// TODO: This class is not yet memory efficient or optimized in any way.
// it might be reimplement soon to have the above features, Focus now is on
// the API we want to use.

using namespace arangodb;
using namespace arangodb::aql;

AqlCallStack::AqlCallStack(AqlCall call, bool compatibilityMode3_6)
    : _operations{{std::move(call)}},
      _depth(0),
      _compatibilityMode3_6(compatibilityMode3_6) {}

AqlCallStack::AqlCallStack(AqlCallStack const& other, AqlCall call)
    : _operations{other._operations}, _depth(0) {
  // We can only use this constructor on relevant levels
  // Alothers need to use passThrough constructor
  TRI_ASSERT(other._depth == 0);
  _operations.push(std::move(call));
  _compatibilityMode3_6 = other._compatibilityMode3_6;
}

AqlCallStack::AqlCallStack(AqlCallStack const& other)
    : _operations{other._operations},
      _depth(other._depth),
      _compatibilityMode3_6(other._compatibilityMode3_6) {}

bool AqlCallStack::isRelevant() const { return _depth == 0; }

AqlCall AqlCallStack::popCall() {
  TRI_ASSERT(isRelevant());
  TRI_ASSERT(_compatibilityMode3_6 || !_operations.empty());
  if (_compatibilityMode3_6 && _operations.empty()) {
    // This is only for compatibility with 3.6
    // there we do not have the stack beeing passed-through
    // in AQL, we only have a single call.
    // We can only get into this state in the abscence of
    // LIMIT => we always do an unlimted softLimit call
    // to the upwards subquery.
    // => Simply put another fetchAll Call on the stack.
    // This code is to be removed in the next version after 3.7
    _operations.push(AqlCall{});
  }
  auto call = _operations.top();
  _operations.pop();
  return call;
}

AqlCall const& AqlCallStack::peek() const {
  TRI_ASSERT(isRelevant());
  TRI_ASSERT(!_operations.empty());
  return _operations.top();
}

void AqlCallStack::pushCall(AqlCall&& call) {
  // TODO is this correct on subqueries?
  TRI_ASSERT(isRelevant());
  _operations.push(call);
}

void AqlCallStack::stackUpMissingCalls() {
  while (!isRelevant()) {
    // For every depth, we add an additional default call.
    // The default is to produce unlimited many results,
    // using DefaultBatchSize each.
    _operations.emplace(AqlCall{});
    _depth--;
  }
  TRI_ASSERT(isRelevant());
}

void AqlCallStack::pop() {
  if (isRelevant()) {
    // We have one element to pop
    std::ignore = popCall();
  } else {
    _depth--;
  }
}

auto AqlCallStack::increaseSubqueryDepth() -> void {
  // Avoid overflow. If you actually have a subquery nesting of size_t many subqueries
  // there is a rather high chance that your query will not perform well.
  TRI_ASSERT(_depth < std::numeric_limits<size_t>::max() - 2);
  _depth++;
  TRI_ASSERT(!isRelevant());
}