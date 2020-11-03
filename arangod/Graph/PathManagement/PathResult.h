////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_GRAPH_PATHMANAGEMENT_PATH_RESULT_H
#define ARANGODB_GRAPH_PATHMANAGEMENT_PATH_RESULT_H 1

#include <numeric>

namespace arangodb {

namespace velocypack {
class Builder;
}  // namespace velocypack

namespace graph {

template <class Step>
class PathResult {
 public:
  explicit PathResult();
  auto clear() -> void;
  auto appendVertex(typename Step::Vertex v) -> void;
  auto prependVertex(typename Step::Vertex v) -> void;
  auto appendEdge(typename Step::Edge e) -> void;
  auto prependEdge(typename Step::Edge e) -> void;
  auto toVelocyPack(arangodb::velocypack::Builder& builder) -> void;
  auto isEmpty() const -> bool;

  // Check uniqueness
  auto isValid() const -> bool;

 private:
  std::vector<typename Step::Vertex> _vertices;
  std::vector<typename Step::Edge> _edges;
};
}  // namespace graph
}  // namespace arangodb

#endif  // ARANGODB_GRAPH_PATHMANAGEMENT_PATH_RESULT_H