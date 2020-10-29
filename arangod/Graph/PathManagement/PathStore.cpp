////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "PathStore.h"

using namespace arangodb;

namespace arangodb {

namespace aql {
class AqlValue;
}

namespace graph {

template <class Step>
PathStore<Step>::PathStore() {
  // performance optimization: just reserve a little more as per default
  _schreier.reserve(32);
  reset(false);
}

template <class Step>
void PathStore<Step>::reset(bool clear) {
  if (clear) {
    _schreier.clear();
  }
  _schreierIndex = 0;
  _lastReturned = 0;
}

template <class Step>
void PathStore<Step>::setStartVertex(Step startVertex) {
  reset();
  _schreier.emplace_back(startVertex);
}

template <class Step>
size_t PathStore<Step>::append(Step step) {
  TRI_ASSERT(size() > 0);
  TRI_ASSERT(step.getPrevious() >= 0);
  TRI_ASSERT(step.getPrevious() >= _schreierIndex);
  _schreier.emplace_back(step);
  _schreierIndex++;

  return _schreierIndex;
}

template <class Step>
bool PathStore<Step>::testPath(Step step) {
  // TODO: needs to be implemented
  return true;
}

}  // namespace graph
}  // namespace arangodb