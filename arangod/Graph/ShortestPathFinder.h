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

#ifndef ARANGODB_GRAPH_SHORTEST_PATH_FINDER_H
#define ARANGODB_GRAPH_SHORTEST_PATH_FINDER_H 1

#include "Basics/Common.h"

#include <velocypack/Slice.h>

namespace arangodb {
namespace graph {
class ShortestPathResult;
struct ShortestPathOptions;

class ShortestPathFinder {
 protected:
  explicit ShortestPathFinder(ShortestPathOptions& options);

 public:
  virtual ~ShortestPathFinder() = default;

  virtual bool shortestPath(arangodb::velocypack::Slice const& start,
                            arangodb::velocypack::Slice const& target,
                            arangodb::graph::ShortestPathResult& result) = 0;

  void destroyEngines();

  ShortestPathOptions& options() const;

  virtual void clear() = 0;

  /// @brief return number of HTTP requests made, and reset it to 0
  size_t getAndResetHttpRequests() {
    size_t value = _httpRequests;
    _httpRequests = 0;
    return value;
  }

  void incHttpRequests(size_t requests) { _httpRequests += requests; }

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief The options to modify this shortest path computation
  //////////////////////////////////////////////////////////////////////////////
  ShortestPathOptions& _options;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Number of HTTP requests made
  //////////////////////////////////////////////////////////////////////////////
  size_t _httpRequests;
};

}  // namespace graph
}  // namespace arangodb

#endif
