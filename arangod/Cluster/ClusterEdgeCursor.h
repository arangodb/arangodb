////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_CLUSTER_CLUSTER_EDGE_CURSOR_H
#define ARANGOD_CLUSTER_CLUSTER_EDGE_CURSOR_H 1

#include "Graph/EdgeCursor.h"
#include "Graph/TraverserOptions.h"

namespace arangodb {

namespace graph {
struct BaseOptions;
class ClusterTraverserCache;
}  // namespace graph

namespace traverser {

class Traverser;

class ClusterEdgeCursor : public graph::EdgeCursor {
 public:
  explicit ClusterEdgeCursor(graph::BaseOptions const* opts);

  ~ClusterEdgeCursor() = default;

  bool next(EdgeCursor::Callback const& callback) override;

  void readAll(EdgeCursor::Callback const& callback) override;

  /// @brief number of HTTP requests performed.
  size_t httpRequests() const override { return _httpRequests; }

 protected:
  std::vector<arangodb::velocypack::Slice> _edgeList;

  size_t _position;
  arangodb::graph::BaseOptions const* _opts;
  arangodb::graph::ClusterTraverserCache* _cache;
  size_t _httpRequests;
};

class ClusterTraverserEdgeCursor final : public ClusterEdgeCursor {
 public:

  explicit ClusterTraverserEdgeCursor(traverser::TraverserOptions const* opts);
  
  void rearm(arangodb::velocypack::StringRef vid, uint64_t depth) override;

  traverser::TraverserOptions const* traverserOptions() const;
};

class ClusterShortestPathEdgeCursor final : public ClusterEdgeCursor {
 public:

  explicit ClusterShortestPathEdgeCursor(graph::BaseOptions const* opts, bool backward);
  
  void rearm(arangodb::velocypack::StringRef vid, uint64_t depth) override;

 private:
  bool const _backward;
};

}  // namespace traverser
}  // namespace arangodb

#endif
