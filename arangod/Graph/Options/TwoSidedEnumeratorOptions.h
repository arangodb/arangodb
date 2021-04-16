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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_GRAPH_OPTIONS_GRAPH_OPTIONS_H
#define ARANGODB_GRAPH_OPTIONS_GRAPH_OPTIONS_H 1

#include <numeric>
#include <cstddef>

namespace arangodb {

namespace graph {

struct TwoSidedEnumeratorOptions {
 public:
  TwoSidedEnumeratorOptions(size_t minDepth, size_t maxDepth);

  ~TwoSidedEnumeratorOptions();

  size_t getMinDepth() const;
  size_t getMaxDepth() const;

 private:
  size_t _minDepth;
  size_t _maxDepth;
};
}  // namespace graph
}  // namespace arangodb

#endif
