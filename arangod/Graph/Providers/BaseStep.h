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

#pragma once

#include <velocypack/HashedStringRef.h>

#include <numeric>

namespace arangodb {

namespace graph {

template<class StepDetails>
class BaseStep {
 public:
  BaseStep(size_t prev = std::numeric_limits<size_t>::max(), size_t depth = 0,
           double weight = 1.0)
      : _previous{prev}, _depth{depth}, _weight(weight) {}

  size_t getPrevious() const { return _previous; }

  bool isFirst() const {
    return _previous == std::numeric_limits<size_t>::max();
  }

  bool isLooseEnd() const {
    return static_cast<StepDetails*>(this)->isLooseEnd();
  }

  size_t getDepth() const { return _depth; }

  double getWeight() const { return _weight; }

  ResultT<std::pair<std::string, size_t>> extractCollectionName(
      arangodb::velocypack::HashedStringRef const& idHashed) const {
    size_t pos = idHashed.find('/');
    if (pos == std::string::npos) {
      // Invalid input. If we get here somehow we managed to store invalid
      // _from/_to values or the traverser did a let an illegal start through
      TRI_ASSERT(false);
      return Result{TRI_ERROR_GRAPH_INVALID_EDGE,
                    "edge contains invalid value " + idHashed.toString()};
    }

    std::string colName = idHashed.substr(0, pos).toString();
    return std::make_pair(colName, pos);
  }

 private:
  size_t _previous;
  size_t _depth;
  double _weight;
};
}  // namespace graph
}  // namespace arangodb
