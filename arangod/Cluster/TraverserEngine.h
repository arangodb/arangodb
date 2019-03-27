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

#ifndef ARANGOD_CLUSTER_TRAVERSER_ENGINE_H
#define ARANGOD_CLUSTER_TRAVERSER_ENGINE_H 1

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
class Query;
}  // namespace aql

namespace graph {
struct ShortestPathOptions;
}

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

namespace traverser {
struct TraverserOptions;

class BaseEngine {
  friend class TraverserEngineRegistry;

 public:
  enum EngineType { TRAVERSER, SHORTESTPATH };

 protected:
  // These are private on purpose.
  // Only the Registry (friend) is allowed
  // to create and destroy engines.
  // We can get into undefined state if sth.
  // deletes an engine but the registry
  // does not get informed properly

  static std::unique_ptr<BaseEngine> BuildEngine(
      TRI_vocbase_t& vocbase, std::shared_ptr<transaction::Context> const& ctx,
      arangodb::velocypack::Slice info, bool needToLock);

  BaseEngine(TRI_vocbase_t& vocbase, std::shared_ptr<transaction::Context> const& ctx,
             arangodb::velocypack::Slice info, bool needToLock);

 public:
  virtual ~BaseEngine();

  // The engine is NOT copyable.
  BaseEngine(BaseEngine const&) = delete;

  void getVertexData(arangodb::velocypack::Slice, arangodb::velocypack::Builder&);

  bool lockCollection(std::string const&);

  Result lockAll();

  std::shared_ptr<transaction::Context> context() const;

  virtual EngineType getType() const = 0;

 protected:
  arangodb::aql::Query* _query;
  std::shared_ptr<transaction::Methods> _trx;
  arangodb::aql::Collections _collections;
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
                      std::shared_ptr<transaction::Context> const& ctx,
                      arangodb::velocypack::Slice info, bool needToLock);

  virtual ~BaseTraverserEngine();

  void getEdges(arangodb::velocypack::Slice, size_t, arangodb::velocypack::Builder&);

  void getVertexData(arangodb::velocypack::Slice, size_t, arangodb::velocypack::Builder&);

  virtual void smartSearch(arangodb::velocypack::Slice, arangodb::velocypack::Builder&) = 0;

  virtual void smartSearchBFS(arangodb::velocypack::Slice,
                              arangodb::velocypack::Builder&) = 0;

  EngineType getType() const override { return TRAVERSER; }

 protected:
  std::unique_ptr<traverser::TraverserOptions> _opts;
};

class ShortestPathEngine : public BaseEngine {
 public:
  // Only the Registry (friend) is allowed
  // to create and destroy engines.
  // We can get into undefined state if sth.
  // deletes an engine but the registry
  // does not get informed properly

  ShortestPathEngine(TRI_vocbase_t& vocbase,
                     std::shared_ptr<transaction::Context> const& ctx,
                     arangodb::velocypack::Slice info, bool needToLock);

  virtual ~ShortestPathEngine();

  void getEdges(arangodb::velocypack::Slice, bool backward, arangodb::velocypack::Builder&);

  EngineType getType() const override { return SHORTESTPATH; }

 protected:
  std::unique_ptr<graph::ShortestPathOptions> _opts;
};

class TraverserEngine : public BaseTraverserEngine {
 public:
  // Only the Registry (friend) is allowed
  // to create and destroy engines.
  // We can get into undefined state if sth.
  // deletes an engine but the registry
  // does not get informed properly

  TraverserEngine(TRI_vocbase_t& vocbase, std::shared_ptr<transaction::Context> const& ctx,
                  arangodb::velocypack::Slice info, bool needToLock);

  ~TraverserEngine();

  void smartSearch(arangodb::velocypack::Slice, arangodb::velocypack::Builder&) override;

  void smartSearchBFS(arangodb::velocypack::Slice, arangodb::velocypack::Builder&) override;
};

}  // namespace traverser
}  // namespace arangodb

#endif
