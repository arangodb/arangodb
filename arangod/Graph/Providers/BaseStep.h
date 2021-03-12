////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GRAPH_PROVIDERS_BASE_STEP_H
#define ARANGOD_GRAPH_PROVIDERS_BASE_STEP_H 1

#include <numeric>

namespace arangodb {
namespace graph {

template <class StepDetails>
class BaseStep {
 public:
  BaseStep() : _previous{std::numeric_limits<size_t>::max()} {}

  BaseStep(size_t prev) : _previous{prev} {}

  size_t getPrevious() const { return _previous; }

  bool isFirst() const {
    return _previous == std::numeric_limits<size_t>::max();
  }

  bool isLooseEnd() const {return static_cast<StepDetails*>(this)->isLooseEnd();}

 private:
  size_t const _previous;
};
}  // namespace graph
}  // namespace arangodb

#endif
