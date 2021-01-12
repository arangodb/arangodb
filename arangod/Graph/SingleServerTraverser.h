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

#ifndef ARANGOD_SINGLE_SERVER_TRAVERSER_H
#define ARANGOD_SINGLE_SERVER_TRAVERSER_H 1

#include "Aql/AqlValue.h"
#include "Graph/Traverser.h"
#include "VocBase/voc-types.h"

namespace arangodb {

namespace graph {
struct BaseOptions;
}  // namespace graph

namespace traverser {

class EnumeratedPath;
class PathEnumerator;

class SingleServerTraverser final : public Traverser {
 public:
  explicit SingleServerTraverser(TraverserOptions*);

  ~SingleServerTraverser();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Reset the traverser to use another start vertex
  //////////////////////////////////////////////////////////////////////////////

  void setStartVertex(std::string const& v) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief No engines on single server
  //////////////////////////////////////////////////////////////////////////////
  void destroyEngines() override {}
  void clear() override;

 protected:
  /// @brief Function to load the other sides vertex of an edge
  ///        Returns true if the vertex passes filtering conditions
  ///        Adds the _id of the vertex into the given vector

  bool getVertex(arangodb::velocypack::Slice edge, arangodb::traverser::EnumeratedPath& path) override;

  /// @brief Function to load the other sides vertex of an edge
  ///        Returns true if the vertex passes filtering conditions
  bool getSingleVertex(arangodb::velocypack::Slice edge,
                       arangodb::velocypack::StringRef const sourceVertexId, uint64_t depth,
                       arangodb::velocypack::StringRef& targetVertexId) override;


  bool getVertex(arangodb::velocypack::StringRef vertex, size_t depth) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Function to fetch the real data of a vertex into an AQLValue
  //////////////////////////////////////////////////////////////////////////////

  aql::AqlValue fetchVertexData(arangodb::velocypack::StringRef) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Function to add the real data of a vertex into a velocypack builder
  //////////////////////////////////////////////////////////////////////////////

  void addVertexToVelocyPack(arangodb::velocypack::StringRef, arangodb::velocypack::Builder&) override;

 private:
  /// @brief build the (single) path enumerator of this traverser
  void createEnumerator();
};
}  // namespace traverser
}  // namespace arangodb

#endif
