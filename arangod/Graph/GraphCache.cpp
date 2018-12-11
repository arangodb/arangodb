////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "GraphCache.h"

#include "Graph.h"

#include <velocypack/Buffer.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
#include <array>
#include <boost/variant.hpp>
#include <utility>

#include "Aql/AstNode.h"
#include "Aql/Graphs.h"
#include "Aql/Query.h"
#include "Basics/ReadLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Graph/GraphManager.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"

using namespace arangodb;
using namespace arangodb::graph;

namespace getGraphFromCacheResult {
struct Success {
  std::shared_ptr<Graph const> graph;

  explicit Success(std::shared_ptr<Graph const> graph_)
      : graph(std::move(graph_)){};
  Success& operator=(Success const& other) = default;
  Success() = delete;
};
struct Outdated {};
struct NotFound {};
struct Exception {};
}

using GetGraphFromCacheResult = boost::variant<
    getGraphFromCacheResult::Success, getGraphFromCacheResult::Outdated,
    getGraphFromCacheResult::NotFound, getGraphFromCacheResult::Exception>;

GetGraphFromCacheResult getGraphFromCache(GraphCache::CacheType const &_cache,
                                          std::string const &name,
                                          std::chrono::seconds maxAge) {
  using namespace getGraphFromCacheResult;

  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

  GraphCache::CacheType::const_iterator entryIt;
  bool entryFound;
  try {
    entryIt = _cache.find(name);
    entryFound = entryIt != _cache.end();
  } catch (...) {
    return Exception{};
  }

  if (!entryFound) {
    return NotFound{};
  }

  GraphCache::EntryType const& entry = entryIt->second;
  std::chrono::steady_clock::time_point const& insertedAt = entry.first;

  if (now - insertedAt > maxAge) {
    return Outdated{};
  }

  return Success{entry.second};
}

const std::shared_ptr<const Graph> GraphCache::getGraph(
    std::shared_ptr<transaction::Context> ctx, std::string const& name,
    std::chrono::seconds maxAge) {
  using namespace getGraphFromCacheResult;

  GetGraphFromCacheResult cacheResult = Exception{};

  // try to lookup the graph in the cache first
  {
    READ_LOCKER(guard, _lock);
    cacheResult = getGraphFromCache(_cache, name, maxAge);
  }

  // TODO The cache saves the graph names globally, not per database!
  // This must be addressed as soon as it is activated.
  /*
  if (typeid(Success) == cacheResult.type()) {
    LOG_TOPIC(TRACE, Logger::GRAPHS) << "GraphCache::getGraph('" << name
                                     << "'): Found entry in cache";

    return boost::get<Success>(cacheResult).graph;
  } else if (typeid(Outdated) == cacheResult.type()) {
    LOG_TOPIC(TRACE, Logger::GRAPHS) << "GraphCache::getGraph('" << name
                                     << "'): Cached entry outdated";
  } else if (typeid(NotFound) == cacheResult.type()) {
    LOG_TOPIC(TRACE, Logger::GRAPHS) << "GraphCache::getGraph('" << name
                                     << "'): No cache entry";
  } else if (typeid(Exception) == cacheResult.type()) {
    LOG_TOPIC(ERR, Logger::GRAPHS)
        << "GraphCache::getGraph('" << name
        << "'): An exception occured during cache lookup";
  } else {
    LOG_TOPIC(FATAL, Logger::GRAPHS) << "GraphCache::getGraph('" << name
                                     << "'): Unhandled result type "
                                     << cacheResult.type().name();

    return nullptr;
  }*/

  // if the graph wasn't found in the cache, lookup the graph and insert or
  // replace the entry. if the graph doesn't exist, erase a possible entry from
  // the cache.
  std::unique_ptr<Graph const> graph;
  try {
    WRITE_LOCKER(guard, _lock);

    std::chrono::steady_clock::time_point now =
        std::chrono::steady_clock::now();

    GraphManager gmngr{ctx->vocbase(), true};
    auto result = gmngr.lookupGraphByName(name);
    if (result.fail()) {
      if (result.is(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) ||
          result.is(TRI_ERROR_GRAPH_NOT_FOUND)) {
        _cache.erase(name);
      }
    } else {
      graph.reset(result.get());
    }

    if (graph == nullptr) {
      return nullptr;
    }

    CacheType::iterator it;
    bool insertSuccess;
    std::tie(it, insertSuccess) =
        _cache.insert({name, std::make_pair(now, graph)});

    if (!insertSuccess) {
      it->second.first = now;
      it->second.second = graph;
    }

  } catch (...) {
  };

  // graph is never set to an invalid or outdated value. So even in case of an
  // exception, if graph was set, it may be returned.
  return graph;
}
