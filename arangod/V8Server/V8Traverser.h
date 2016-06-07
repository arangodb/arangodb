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

class DepthFirstTraverser;

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


class SingleServerTraversalPath : public TraversalPath {
 public:
  explicit SingleServerTraversalPath(
      arangodb::basics::EnumeratedPath<std::string, std::string> const& path,
      DepthFirstTraverser* traverser)
      : _traverser(traverser), _path(path) {}

  ~SingleServerTraversalPath() {}

  void pathToVelocyPack(arangodb::Transaction*,
                        arangodb::velocypack::Builder&) override;

  void lastEdgeToVelocyPack(arangodb::Transaction*,
                            arangodb::velocypack::Builder&) override;

  void lastVertexToVelocyPack(arangodb::Transaction*,
                              arangodb::velocypack::Builder&) override;

 private:

  void getDocumentByIdentifier(Transaction*, std::string const&,
                               arangodb::velocypack::Builder&);

  DepthFirstTraverser* _traverser;

  arangodb::basics::EnumeratedPath<std::string, std::string> _path;

  arangodb::velocypack::Builder _searchBuilder;

};

class DepthFirstTraverser : public Traverser {

  friend class SingleServerTraversalPath;

 private:

  class VertexGetter : public arangodb::basics::VertexGetter<std::string, std::string> {
   public:
    explicit VertexGetter(DepthFirstTraverser* traverser)
        : _traverser(traverser) {}

    virtual ~VertexGetter() = default;

    virtual bool getVertex(std::string const&, std::string const&, size_t,
                           std::string&) override;
    virtual void reset();

   protected:
    DepthFirstTraverser* _traverser;
  };

  class UniqueVertexGetter : public VertexGetter {
   public:
    explicit UniqueVertexGetter(DepthFirstTraverser* traverser)
        : VertexGetter(traverser) {}

    ~UniqueVertexGetter() = default;

    bool getVertex(std::string const&, std::string const&, size_t,
                    std::string&) override;

    void reset() override;

   private:
    std::unordered_set<std::string> _returnedVertices;
  };



  //////////////////////////////////////////////////////////////////////////////
  /// @brief callable class to load edges based on opts.
  //////////////////////////////////////////////////////////////////////////////

  class EdgeGetter {
   public:
    EdgeGetter(DepthFirstTraverser* traverser,
                        TraverserOptions const& opts,
                        Transaction* trx)
        : _traverser(traverser), _opts(opts), _trx(trx) {}

    //////////////////////////////////////////////////////////////////////////////
    /// @brief Function to fill the list of edges properly.
    //////////////////////////////////////////////////////////////////////////////

    void operator()(std::string const&, std::vector<std::string>&,
                    arangodb::velocypack::ValueLength*&, size_t&, bool&);

   private:

    //////////////////////////////////////////////////////////////////////////////
    /// @brief Get the next valid cursor
    //////////////////////////////////////////////////////////////////////////////

    bool nextCursor(std::string const&, size_t&,
                    arangodb::velocypack::ValueLength*&);

    //////////////////////////////////////////////////////////////////////////////
    /// @brief Get the next edge
    //////////////////////////////////////////////////////////////////////////////

    void nextEdge(std::string const&, size_t&,
                  arangodb::velocypack::ValueLength*&,
                  std::vector<std::string>&);

    DepthFirstTraverser* _traverser;

    //////////////////////////////////////////////////////////////////////////////
    /// @brief Cache for indexes. Maps collectionName to Index
    //////////////////////////////////////////////////////////////////////////////

    std::unordered_map<std::string, std::pair<TRI_voc_cid_t, EdgeIndex*>>
        _indexCache;

    //////////////////////////////////////////////////////////////////////////////
    /// @brief Traverser options
    //////////////////////////////////////////////////////////////////////////////

    TraverserOptions _opts;

    //////////////////////////////////////////////////////////////////////////////
    /// @brief Pointer to active transaction
    ///        All Edge Collections have to be properly locked before traversing!
    //////////////////////////////////////////////////////////////////////////////

    Transaction* _trx;

    //////////////////////////////////////////////////////////////////////////////
    /// @brief Stack of all active cursors
    //////////////////////////////////////////////////////////////////////////////

    std::stack<std::shared_ptr<OperationCursor>> _cursors;

    //////////////////////////////////////////////////////////////////////////////
    /// @brief Stack of all active cursor batches
    //////////////////////////////////////////////////////////////////////////////

    std::stack<std::shared_ptr<OperationResult>> _results;

    //////////////////////////////////////////////////////////////////////////////
    /// @brief Stack of positions in the cursors
    //////////////////////////////////////////////////////////////////////////////

    std::stack<arangodb::velocypack::ValueLength> _posInCursor;

    //////////////////////////////////////////////////////////////////////////////
    /// @brief velocyPack builder to create temporary search values
    //////////////////////////////////////////////////////////////////////////////

    arangodb::velocypack::Builder _builder;

  };



  //////////////////////////////////////////////////////////////////////////////
  /// @brief internal cursor to enumerate the paths of a graph
  //////////////////////////////////////////////////////////////////////////////

  std::unique_ptr<arangodb::basics::PathEnumerator<
      std::string, std::string, arangodb::velocypack::ValueLength>> _enumerator;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief internal getter to extract an edge
  //////////////////////////////////////////////////////////////////////////////

  EdgeGetter _edgeGetter;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief internal getter to extract an edge
  //////////////////////////////////////////////////////////////////////////////

  std::unique_ptr<VertexGetter> _vertexGetter;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief a vector containing all required edge collection structures
  //////////////////////////////////////////////////////////////////////////////

  std::vector<TRI_document_collection_t*> _edgeCols;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Outer top level transaction
  //////////////////////////////////////////////////////////////////////////////

  Transaction* _trx;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Cache for vertex documents
  //////////////////////////////////////////////////////////////////////////////

  std::unordered_map<std::string,
                     std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
      _vertices;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Cache for edge documents
  //////////////////////////////////////////////////////////////////////////////

  std::unordered_map<std::string,
                     std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
      _edges;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Shared builder to create temporary objects like search values
  //////////////////////////////////////////////////////////////////////////////

  arangodb::velocypack::Builder _builder;

 public:
  DepthFirstTraverser(
      TraverserOptions&, Transaction*,
      std::unordered_map<size_t, std::vector<TraverserExpression*>> const*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Reset the traverser to use another start vertex
  //////////////////////////////////////////////////////////////////////////////

  void setStartVertex(std::string const& v) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Get the next possible path in the graph.
  //////////////////////////////////////////////////////////////////////////////

  TraversalPath* next() override;

 private:
  bool edgeMatchesConditions(arangodb::velocypack::Slice, size_t);

  bool vertexMatchesConditions(std::string const&, size_t);

  std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>> fetchVertexData(
      std::string const&);
};
}
}

////////////////////////////////////////////////////////////////////////////////
/// @brief callback to weight an edge
////////////////////////////////////////////////////////////////////////////////

typedef std::function<double(arangodb::velocypack::Slice const)>
    WeightCalculatorFunction;

////////////////////////////////////////////////////////////////////////////////
/// @brief Information required internally of the traverser.
///        Used to easily pass around collections.
///        Also offer abstraction to extract edges.
////////////////////////////////////////////////////////////////////////////////

class EdgeCollectionInfo {

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief the underlying transaction
  //////////////////////////////////////////////////////////////////////////////

  arangodb::Transaction* _trx;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief edge collection name
  //////////////////////////////////////////////////////////////////////////////

  std::string _collectionName;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief index id
  //////////////////////////////////////////////////////////////////////////////

  arangodb::Transaction::IndexHandle _indexId;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Temporary builder for index search values
  ///        NOTE: Single search builder is NOT thread-save
  //////////////////////////////////////////////////////////////////////////////

  VPackBuilder _searchBuilder;

  WeightCalculatorFunction _weighter;

  TRI_edge_direction_e _forwardDir;

  TRI_edge_direction_e _backwardDir;

  std::vector<arangodb::traverser::TraverserExpression*> _unused;

 public:
  EdgeCollectionInfo(arangodb::Transaction* trx,
                     std::string const& collectionName,
                     TRI_edge_direction_e const direction,
                     WeightCalculatorFunction weighter);

////////////////////////////////////////////////////////////////////////////////
/// @brief Get edges for the given direction and start vertex.
////////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<arangodb::OperationCursor> getEdges(std::string const&);

  std::shared_ptr<arangodb::OperationCursor> getEdges(arangodb::velocypack::Slice const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief Get edges for the given direction and start vertex. On Coordinator.
////////////////////////////////////////////////////////////////////////////////

  int getEdgesCoordinator(arangodb::velocypack::Slice const&,
                          arangodb::velocypack::Builder&);

////////////////////////////////////////////////////////////////////////////////
/// @brief Get edges for the given direction and start vertex. Reverse version
////////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<arangodb::OperationCursor> getReverseEdges(std::string const&);

  std::shared_ptr<arangodb::OperationCursor> getReverseEdges(arangodb::velocypack::Slice const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief Get edges for the given direction and start vertex. Reverse version on Coordinator.
////////////////////////////////////////////////////////////////////////////////

  int getReverseEdgesCoordinator(arangodb::velocypack::Slice const&,
                                 arangodb::velocypack::Builder&);

  double weightEdge(arangodb::velocypack::Slice const);
  
  arangodb::Transaction* trx() const { return _trx; }

////////////////////////////////////////////////////////////////////////////////
/// @brief Return name of the wrapped collection
////////////////////////////////////////////////////////////////////////////////

  std::string const& getName(); 

};

////////////////////////////////////////////////////////////////////////////////
/// @brief Wrapper for the neighbors computation
////////////////////////////////////////////////////////////////////////////////

void TRI_RunNeighborsSearch(
    std::vector<EdgeCollectionInfo*> const& collectionInfos,
    arangodb::traverser::NeighborsOptions const& opts,
    std::unordered_set<arangodb::velocypack::Slice,
                       arangodb::basics::VelocyPackHelper::VPackStringHash,
                       arangodb::basics::VelocyPackHelper::VPackStringEqual>& visited,
    std::vector<arangodb::velocypack::Slice>& distinct);

#endif
