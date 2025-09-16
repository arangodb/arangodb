////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include <functional>
#include <memory>
#include <unordered_map>

#include "Aql/Collections.h"
#include "Basics/MemoryTypes/MemoryTypes.h"

struct TRI_vocbase_t;

namespace arangodb {

class Result;

namespace transaction {
class Methods;
class Context;
}  // namespace transaction

namespace aql {
class Collections;
class QueryContext;
class VariableGenerator;
}  // namespace aql

namespace graph {
struct BaseOptions;
class EdgeCursor;
struct ShortestPathOptions;
}  // namespace graph

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

namespace traverser {
struct TraverserOptions;

struct EdgeCursorForMultipleVertices {
  size_t _cursorId;
  size_t _depth;
  uint64_t _batchSize;
  std::vector<std::string> _vertices;
  std::vector<std::string>::iterator _nextVertex;
  graph::EdgeCursor* _cursor;
  size_t _nextBatch = 0;
  EdgeCursorForMultipleVertices(size_t cursorId, size_t depth,
                                uint64_t batchSize,
                                std::vector<std::string> vertices,
                                graph::EdgeCursor* cursor)
      : _cursorId{cursorId},
        _depth{depth},
        _batchSize{batchSize},
        _vertices{std::move(vertices)},
        _nextVertex{_vertices.begin()},
        _cursor{cursor} {
    TRI_ASSERT(_cursor != nullptr);
    rearm();
  }
  auto rearm() -> bool;
  auto hasMore() -> bool;
};

class BaseEngine {
 public:
  enum EngineType { TRAVERSER, SHORTESTPATH };

  static std::unique_ptr<BaseEngine> buildEngine(
      TRI_vocbase_t& vocbase, aql::QueryContext& query,
      arangodb::velocypack::Slice info);

  BaseEngine(TRI_vocbase_t& vocbase, aql::QueryContext& query,
             arangodb::velocypack::Slice info);

 public:
  virtual ~BaseEngine();

  // The engine is NOT copyable.
  BaseEngine(BaseEngine const&) = delete;

  void getVertexData(arangodb::velocypack::Slice vertex,
                     arangodb::velocypack::Builder& builder, bool nestedOutput);

  std::shared_ptr<transaction::Context> context() const;

  virtual EngineType getType() const = 0;

  virtual bool produceVertices() const { return true; }

  arangodb::aql::EngineId engineId() const noexcept { return _engineId; }

  virtual graph::BaseOptions const& options() const = 0;

 protected:
  arangodb::aql::EngineId const _engineId;
  arangodb::aql::QueryContext& _query;
  std::unique_ptr<transaction::Methods> _trx;
  MonitoredCollectionToShardMap _vertexShards;
};

class BaseTraverserEngine : public BaseEngine {
 public:
  // Only the Registry (friend) is allowed
  // to create and destroy engines.
  // We can get into undefined state if sth.
  // deletes an engine but the registry
  // does not get informed properly

  BaseTraverserEngine(TRI_vocbase_t& vocbase, aql::QueryContext& query,
                      arangodb::velocypack::Slice info);

  ~BaseTraverserEngine();

  // old behaviour
  void allEdges(std::vector<std::string> const& vertices, size_t depth,
                VPackBuilder& builder);

  // new behaviour
  void rearm(size_t depth, uint64_t batchSize,
             std::vector<std::string> vertices, VPackSlice variables);
  Result nextEdgeBatch(size_t batchId, VPackBuilder& builder);
  void addStatistics(VPackBuilder& builder);

  virtual void smartSearch(arangodb::velocypack::Slice,
                           arangodb::velocypack::Builder&) = 0;

  virtual void smartSearchUnified(arangodb::velocypack::Slice,
                                  arangodb::velocypack::Builder&) = 0;

  EngineType getType() const override { return TRAVERSER; }

  bool produceVertices() const override;

  // Inject all variables from VPack information
  void injectVariables(arangodb::velocypack::Slice variables);

  aql::VariableGenerator const* variables() const;

  graph::BaseOptions const& options() const override;
  std::optional<EdgeCursorForMultipleVertices> _cursor;
  size_t _nextCursorId = 0;

 protected:
  graph::EdgeCursor* getCursor(uint64_t currentDepth);

  std::unique_ptr<traverser::TraverserOptions> _opts;
  std::unordered_map<uint64_t, std::unique_ptr<graph::EdgeCursor>>
      _depthSpecificCursors;
  std::unique_ptr<graph::EdgeCursor> _generalCursor;
  aql::VariableGenerator const* _variables;
};

class ShortestPathEngine : public BaseEngine {
 public:
  // Only the Registry (friend) is allowed
  // to create and destroy engines.
  // We can get into undefined state if sth.
  // deletes an engine but the registry
  // does not get informed properly

  ShortestPathEngine(TRI_vocbase_t& vocbase, aql::QueryContext& query,
                     arangodb::velocypack::Slice info);

  ~ShortestPathEngine();

  void getEdges(arangodb::velocypack::Slice, bool backward,
                arangodb::velocypack::Builder&);

  EngineType getType() const override { return SHORTESTPATH; }

  graph::BaseOptions const& options() const override;

 private:
  void addEdgeData(arangodb::velocypack::Builder& builder, bool backward,
                   std::string_view v);

 protected:
  std::unique_ptr<graph::ShortestPathOptions> _opts;

  std::unique_ptr<graph::EdgeCursor> _forwardCursor;
  std::unique_ptr<graph::EdgeCursor> _backwardCursor;
};

class TraverserEngine : public BaseTraverserEngine {
 public:
  // Only the Registry (friend) is allowed
  // to create and destroy engines.
  // We can get into undefined state if sth.
  // deletes an engine but the registry
  // does not get informed properly

  TraverserEngine(TRI_vocbase_t& vocbase, aql::QueryContext& query,
                  arangodb::velocypack::Slice info);

  ~TraverserEngine();

  void smartSearch(arangodb::velocypack::Slice,
                   arangodb::velocypack::Builder&) override;

  void smartSearchUnified(arangodb::velocypack::Slice,
                          arangodb::velocypack::Builder&) override;
};

}  // namespace traverser
}  // namespace arangodb
