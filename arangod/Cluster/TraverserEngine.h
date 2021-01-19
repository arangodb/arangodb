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

#ifndef ARANGOD_CLUSTER_TRAVERSER_ENGINE_H
#define ARANGOD_CLUSTER_TRAVERSER_ENGINE_H 1

#include <memory>
#include <unordered_map>

#include "Aql/Collections.h"
#include "Basics/Common.h"

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
class EdgeCursor;
struct ShortestPathOptions;
}

namespace velocypack {
class Builder;
class Slice;
class StringRef;
}  // namespace velocypack

namespace traverser {
struct TraverserOptions;

class BaseEngine {

 public:
  enum EngineType { TRAVERSER, SHORTESTPATH };

  static std::unique_ptr<BaseEngine> BuildEngine(
      TRI_vocbase_t& vocbase, aql::QueryContext& query,
      arangodb::velocypack::Slice info);

  BaseEngine(TRI_vocbase_t& vocbase, aql::QueryContext& query,
             arangodb::velocypack::Slice info);

 public:
  virtual ~BaseEngine();

  // The engine is NOT copyable.
  BaseEngine(BaseEngine const&) = delete;

  void getVertexData(arangodb::velocypack::Slice vertex, 
                     arangodb::velocypack::Builder& builder,
                     bool nestedOutput);

  std::shared_ptr<transaction::Context> context() const;

  virtual EngineType getType() const = 0;

  virtual bool produceVertices() const { return true; }
  
  arangodb::aql::EngineId engineId() const { return _engineId; }

 protected:
  arangodb::aql::EngineId const _engineId;
  arangodb::aql::QueryContext& _query;
  transaction::Methods* _trx;
  std::unordered_map<std::string, std::vector<std::string>> _vertexShards;
};

class BaseTraverserEngine : public BaseEngine {
 public:
  // Only the Registry (friend) is allowed
  // to create and destroy engines.
  // We can get into undefined state if sth.
  // deletes an engine but the registry
  // does not get informed properly

  BaseTraverserEngine(TRI_vocbase_t& vocbase,
                      aql::QueryContext& query,
                      arangodb::velocypack::Slice info);

  ~BaseTraverserEngine();

  void getEdges(arangodb::velocypack::Slice, size_t, arangodb::velocypack::Builder&);

  graph::EdgeCursor* getCursor(arangodb::velocypack::StringRef nextVertex, uint64_t currentDepth);

  virtual void smartSearch(arangodb::velocypack::Slice, arangodb::velocypack::Builder&) = 0;

  virtual void smartSearchBFS(arangodb::velocypack::Slice,
                              arangodb::velocypack::Builder&) = 0;

  virtual void smartSearchWeighted(arangodb::velocypack::Slice,
                                   arangodb::velocypack::Builder&) = 0;

  EngineType getType() const override { return TRAVERSER; }

  bool produceVertices() const override;

  // Inject all variables from VPack information
  void injectVariables(arangodb::velocypack::Slice variables);

  aql::VariableGenerator const* variables() const;

 protected:
  std::unique_ptr<traverser::TraverserOptions> _opts;
  std::vector<std::unique_ptr<graph::EdgeCursor>> _cursors;
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

  void getEdges(arangodb::velocypack::Slice, bool backward, arangodb::velocypack::Builder&);

  EngineType getType() const override { return SHORTESTPATH; }

 private:
  void addEdgeData(arangodb::velocypack::Builder& builder, bool backward, arangodb::velocypack::StringRef v);
 
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

  void smartSearch(arangodb::velocypack::Slice, arangodb::velocypack::Builder&) override;

  void smartSearchBFS(arangodb::velocypack::Slice, arangodb::velocypack::Builder&) override;

  void smartSearchWeighted(arangodb::velocypack::Slice, arangodb::velocypack::Builder&) override;
};

}  // namespace traverser
}  // namespace arangodb

#endif
