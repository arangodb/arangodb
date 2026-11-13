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
////////////////////////////////////////////////////////////////////////////////

#include "QueryContext.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Ast.h"
#include "Basics/debugging.h"
#include "Basics/Exceptions.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/StaticStrings.h"
#include "Basics/system-functions.h"
#include "Cluster/ServerState.h"
#include "ClusterEngine/ClusterEngine.h"
#include "Graph/Graph.h"
#include "Graph/GraphManager.h"
#include "Logger/LogMacros.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalDataSource.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::aql;

/// @brief creates a query
QueryContext::QueryContext(TRI_vocbase_t& vocbase,
                           transaction::OperationOrigin operationOrigin,
                           QueryId id)
    : _resourceMonitor(
          std::make_shared<ResourceMonitor>(GlobalResourceMonitor::instance())),
      _queryId(id ? id : TRI_NewServerSpecificTick()),
      _collections(&vocbase),
      _vocbase(vocbase),
      _execState(QueryExecutionState::ValueType::INVALID_STATE),
      _operationOrigin(operationOrigin),
      _numRequests(0) {
  // aql analyzers should be able to run even during recovery when AqlFeature
  // is not started. And as optimization - these queries do not need
  // queryRegistry
  if (&_vocbase != &_vocbase.server()
                        .getFeature<DatabaseFeature>()
                        .getCalculationVocbase() &&
      !AqlFeature::lease()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }
}

/// @brief destroys a query
QueryContext::~QueryContext() {
  _graphs.clear();
  if (&_vocbase != &_vocbase.server()
                        .getFeature<DatabaseFeature>()
                        .getCalculationVocbase()) {
    AqlFeature::unlease();
  }
}

TRI_vocbase_t& QueryContext::vocbase() const noexcept { return _vocbase; }

transaction::OperationOrigin QueryContext::operationOrigin() const noexcept {
  return _operationOrigin;
}

Collections& QueryContext::collections() {
  TRI_ASSERT(_execState != QueryExecutionState::ValueType::EXECUTION ||
             ClusterEngine::Mocking);
  return _collections;
}

Collections const& QueryContext::collections() const { return _collections; }

/// @brief return the names of collections used in the query
std::vector<std::string> QueryContext::collectionNames() const {
  return _collections.collectionNames();
}

/// @brief return the user that started the query
std::string const& QueryContext::user() const { return StaticStrings::Empty; }

QueryWarnings& QueryContext::warnings() { return _warnings; }

QueryWarnings const& QueryContext::warnings() const { return _warnings; }

/// @brief look up a graph either from our cache list or from the _graphs
///        collection
ResultT<graph::Graph const*> QueryContext::lookupGraphByName(
    std::string const& name) {
  TRI_ASSERT(_execState != QueryExecutionState::ValueType::EXECUTION);

  auto it = _graphs.find(name);

  if (it != _graphs.end()) {
    return it->second.get();
  }

  graph::GraphManager graphManager{_vocbase, _operationOrigin};

  auto g = graphManager.lookupGraphByName(name);

  if (g.fail()) {
    return g.result();
  }

  auto graphPtr = g.get().get();

  _graphs.emplace(name, std::move(g.get()));

  // return the raw pointer
  return graphPtr;
}

void QueryContext::addDataSource(LogicalDataSource const& ds) {
  TRI_ASSERT(_execState != QueryExecutionState::ValueType::EXECUTION);
  _queryDataSources.try_emplace(ds.guid(), ds.name());
}

std::pair<LogicalDataSource::Category, std::string>
QueryContext::addDataSourceByName(std::string_view name,
                                  AccessMode::Type accessType,
                                  bool failIfDoesNotExist) {
  // NOTE name may contain a numeric collection ID; in this case the string_view
  // returned by this function will contain the resolved datasource name
  if (name.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_ILLEGAL_NAME,
                                   "empty name collections are not supported");
  }

  std::string resolved_name{name};
  auto const dataSource = resolver().getDataSource(name);

  if (dataSource == nullptr) {
    // datasource not found...
    if (failIfDoesNotExist) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
          absl::StrCat(TRI_errno_string(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND),
                       ": name: ", name));
    }

    // still add datasource to query, simply because the AST will also be built
    // for queries that are parsed-only (e.g. via `db._parse(query);`. In this
    // case it is ok that the datasource does not exist, but we need to track
    // the names of datasources used in the query
    collections().add(std::string{name}, accessType,
                      aql::Collection::Hint::None);

    return {LogicalDataSource::Category::kCollection, resolved_name};
  }

  // query actual name from datasource... this may be different to the
  // name passed into this function, because the user may have accessed
  // the collection by its numeric id
  auto const& dataSourceName = dataSource->name();

  if (name != dataSourceName) {
    TRI_ASSERT(name.front() >= '0' && name.front() <= '9');
    resolved_name = dataSourceName;
  }

  if (dataSource->category() == LogicalDataSource::Category::kCollection) {
    // it's a collection!
    // add datasource to query
    aql::Collection::Hint hint = aql::Collection::Hint::Collection;
    if (ServerState::instance()->isDBServer()) {
      hint = aql::Collection::Hint::Shard;
    }
    collections().add(std::string(resolved_name), accessType, hint);
    if (resolved_name != name) {
      collections().add(std::string{name}, accessType,
                        hint);  // Add collection by ID as well
    }

    if (ServerState::instance()->isCoordinator()) {
      auto& ci = vocbase().server().getFeature<ClusterFeature>().clusterInfo();
      auto c = ci.getCollectionNT(vocbase().name(), resolved_name);
      if (c != nullptr) {
        auto const& names = c->realNames();

        for (auto const& n : names) {
          std::string_view shardsNameRef(n);

          auto [shardsCategory, _] = addDataSourceByName(
              shardsNameRef, accessType, failIfDoesNotExist);

          TRI_ASSERT(shardsCategory ==
                     LogicalDataSource::Category::kCollection);
        }
      }  // else { TODO Should we really not react? }
    }

  } else if (dataSource->category() == LogicalDataSource::Category::kView) {
    // it's a view!
    // add views to the collection list
    // to register them with transaction as well
    collections().add(std::string(name), accessType,
                      aql::Collection::Hint::None);
    addDataSource(*dataSource);

    // Make sure to add all collections from the view now:
    resolver().visitCollections(
        [this, accessType](LogicalCollection& col) -> bool {
          collections().add(col.name(), accessType,
                            aql::Collection::Hint::Collection);
          return true;
        },
        dataSource->id());
  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "unexpected datasource type");
  }

  return {dataSource->category(), resolved_name};
}

aql::Ast* QueryContext::ast() const { return _ast.get(); }

QueryContext::QueryApiSynchronicity QueryContext::queryApiSynchronicity()
    const noexcept {
  return _queryApiSynchronicity;
}
