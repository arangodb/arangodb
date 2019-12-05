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

AqlCallStack::AqlCallStack(AqlCall call)
    : _operations{{std::move(call)}}, _depth(0) {}

AqlCallStack::AqlCallStack(AqlCallStack const& other, AqlCall call)
    : _operations{other._operations}, _depth(0) {
  // We can only use this constructor on relevant levels
  // Alothers need to use passThrough constructor
  TRI_ASSERT(other._depth == 0);
  _operations.push(std::move(call));
}

AqlCallStack::AqlCallStack(AqlCallStack const& other)
    : _operations{other._operations}, _depth(other._depth) {}

bool AqlCallStack::isRelevant() const { return _depth == 0; }

AqlCall AqlCallStack::popCall() {
  TRI_ASSERT(isRelevant());
  TRI_ASSERT(!_operations.empty());
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
    TRI_ASSERT(!_operations.empty());
    _operations.pop();
    // We can never pop the main query, so one element needs to stay
    TRI_ASSERT(!_operations.empty());
  }
}