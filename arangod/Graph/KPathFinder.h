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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_GRAPH_K_PATHS_FINDER_H
#define ARANGODB_GRAPH_K_PATHS_FINDER_H 1

namespace arangodb {

namespace velocypack {
class Builder;
class StringRef;
}  // namespace velocypack

namespace graph {

struct ShortestPathOptions;

class KPathFinder {
 private:
  using VertexRef = arangodb::velocypack::StringRef;
  // We have two balls, one arround source, one around target, and try to find intersections of the balls
  struct Ball {
    void reset();
  };

 public:
  explicit KPathFinder(ShortestPathOptions& options);
  ~KPathFinder();

  bool hasMore() const;

  void reset();
  // get the next available path serialized in the builder
  bool getNextPathAql(arangodb::velocypack::Builder& result);

 private:
  Ball _left{};
  Ball _right{};
  bool _done{false};
};

}  // namespace graph
}  // namespace arangodb

#endif