////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_V8_SERVER_V8_TRAVERSER_H
#define ARANGOD_V8_SERVER_V8_TRAVERSER_H 1

#include "Basics/VelocyPackHelper.h"
#include "VocBase/ExampleMatcher.h"
#include "VocBase/Traverser.h"

namespace arangodb {

struct OperationCursor;

namespace velocypack {
class Slice;
}
}

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef the template instantiation of the PathFinder
////////////////////////////////////////////////////////////////////////////////

typedef arangodb::basics::DynamicDistanceFinder<
    arangodb::velocypack::Slice, arangodb::velocypack::Slice, double,
    arangodb::traverser::ShortestPath> ArangoDBPathFinder;

typedef arangodb::basics::ConstDistanceFinder<arangodb::velocypack::Slice,
                                              arangodb::velocypack::Slice,
                                              arangodb::basics::VelocyPackHelper::VPackStringHash, 
                                              arangodb::basics::VelocyPackHelper::VPackStringEqual,
                                              arangodb::traverser::ShortestPath>
    ArangoDBConstDistancePathFinder;

namespace arangodb {
namespace traverser {

// Forward declaration
class EdgeCollectionInfo;

// A collection of shared options used in several functions.
// Should not be used directly, use specialization instead.
struct BasicOptions {

  arangodb::Transaction* _trx;

 protected:
  std::unordered_map<std::string, arangodb::ExampleMatcher*> _edgeFilter;
  std::unordered_map<std::string, arangodb::ExampleMatcher*> _vertexFilter;

  explicit BasicOptions(arangodb::Transaction* trx)
      : _trx(trx), useEdgeFilter(false), useVertexFilter(false) {}

  virtual ~BasicOptions() {
    // properly clean up the mess
    for (auto& it : _edgeFilter) {
      delete it.second;
    }
    for (auto& it : _vertexFilter) {
      delete it.second;
    }
  }

 public:
  std::string start;
  bool useEdgeFilter;
  bool useVertexFilter;


 public:

  arangodb::Transaction* trx() { return _trx; }

  void addEdgeFilter(v8::Isolate* isolate, v8::Handle<v8::Value> const& example,
                     std::string const&,
                     std::string& errorMessage);

  void addEdgeFilter(arangodb::velocypack::Slice const& example,
                     std::string const&);

  void addVertexFilter(v8::Isolate* isolate,
                       v8::Handle<v8::Value> const& example,
                       arangodb::Transaction* trx,
                       std::string const&,
                       std::string& errorMessage);

  bool matchesEdge(arangodb::velocypack::Slice) const;

  virtual bool matchesVertex(std::string const&, std::string const&,
                             arangodb::velocypack::Slice) const;

};

struct NeighborsOptions : BasicOptions {
 private:
  std::unordered_set<std::string> _explicitCollections;

 public:
  TRI_edge_direction_e direction;
  uint64_t minDepth;
  uint64_t maxDepth;
  arangodb::velocypack::Builder startBuilder; 

  explicit NeighborsOptions(arangodb::Transaction* trx)
      : BasicOptions(trx), direction(TRI_EDGE_OUT), minDepth(1), maxDepth(1) {}

  bool matchesVertex(std::string const&, std::string const&,
                     arangodb::velocypack::Slice) const override;

  bool matchesVertex(std::string const&) const;
  bool matchesVertex(arangodb::velocypack::Slice const&) const;

  void addCollectionRestriction(std::string const&);

  void setStart(std::string const& id);

  arangodb::velocypack::Slice getStart() const;
};

struct ShortestPathOptions : BasicOptions {
 public:
  std::string direction;
  bool useWeight;
  std::string weightAttribute;
  double defaultWeight;
  bool bidirectional;
  bool multiThreaded;
  std::string end;
  arangodb::velocypack::Builder startBuilder;
  arangodb::velocypack::Builder endBuilder;

  explicit ShortestPathOptions(arangodb::Transaction* trx);
  bool matchesVertex(std::string const&, std::string const&,
                     arangodb::velocypack::Slice) const override;

  void setStart(std::string const&);
  void setEnd(std::string const&);

  arangodb::velocypack::Slice getStart() const;
  arangodb::velocypack::Slice getEnd() const;

};

}
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Wrapper for the neighbors computation
////////////////////////////////////////////////////////////////////////////////

void TRI_RunNeighborsSearch(
    std::vector<arangodb::traverser::EdgeCollectionInfo*> const& collectionInfos,
    arangodb::traverser::NeighborsOptions const& opts,
    std::unordered_set<arangodb::velocypack::Slice,
                       arangodb::basics::VelocyPackHelper::VPackStringHash,
                       arangodb::basics::VelocyPackHelper::VPackStringEqual>& visited,
    std::vector<arangodb::velocypack::Slice>& distinct);

#endif
