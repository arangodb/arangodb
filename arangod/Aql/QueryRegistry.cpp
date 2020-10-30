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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "QueryRegistry.h"
#include <Cluster/CallbackGuard.h>

#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/Query.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Basics/system-functions.h"
#include "Cluster/ServerState.h"
#include "Cluster/TraverserEngine.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Transaction/Methods.h"
#include "Transaction/Status.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::cluster;

QueryRegistry::~QueryRegistry() {
  disallowInserts();
  destroyAll();
}

/// @brief insert
void QueryRegistry::insertQuery(std::unique_ptr<ClusterQuery> query, double ttl, cluster::CallbackGuard guard) {
  TRI_ASSERT(query != nullptr);
  TRI_ASSERT(query->state() != QueryExecutionState::ValueType::INITIALIZATION);
  LOG_TOPIC("77778", DEBUG, arangodb::Logger::AQL)
      << "Register query with id " << query->id() << " : " << query->queryString();
  auto& vocbase = query->vocbase();

  if (vocbase.isDropped()) {
    // don't register any queries for dropped databases
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  QueryId qId = query->id();

  TRI_IF_FAILURE("QueryRegistryInsertException1") { 
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  // create the query info object outside of the lock
  auto p = std::make_unique<QueryInfo>(std::move(query), ttl, std::move(guard));
  TRI_ASSERT(p->_expires != 0);

  TRI_IF_FAILURE("QueryRegistryInsertException2") { 
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  // now insert into table of running queries
  WRITE_LOCKER(writeLocker, _lock);
  if (_disallowInserts) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }

  try {
    for (auto& pair : p->_query->snippets()) {
      if (pair.first != 0) { // skip the root snippet
        auto result = _engines.try_emplace(pair.first, EngineInfo(pair.second.get(), p.get()));
        TRI_ASSERT(result.second);
        TRI_ASSERT(result.first->second._type == EngineType::Execution);
        p->_numEngines++;
      }
    }
    for (auto& pair : p->_query->traversers()) {
      auto result = _engines.try_emplace(pair.first, EngineInfo(pair.second.get(), p.get()));
      TRI_ASSERT(result.second);
      TRI_ASSERT(result.first->second._type == EngineType::Graph);
      p->_numEngines++;
    }

    auto result = _queries[vocbase.name()].try_emplace(qId, std::move(p));
    if (!result.second) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL, "query with given vocbase and id already there");
    }
  
  } catch (...) {
    // revert engine registration
    for (auto& pair : p->_query->snippets()) {
      _engines.erase(pair.first);
    }
    for (auto& pair : p->_query->traversers()) {
      _engines.erase(pair.first);
    }
    // no need to revert last insert
    throw;
  }
}

/// @brief open
void* QueryRegistry::openEngine(EngineId id, EngineType type) {
  LOG_TOPIC("8c204", DEBUG, arangodb::Logger::AQL) << "Opening engine with id " << id;
  // std::cout << "Taking out query with ID " << id << std::endl;
  WRITE_LOCKER(writeLocker, _lock);
  
  auto it = _engines.find(id);
  if (it == _engines.end()) {
    LOG_TOPIC("c3ae4", DEBUG, arangodb::Logger::AQL)
    << "Found no engine with id " << id;
    return nullptr;
  }
  
  EngineInfo& ei = it->second;
  
  if (ei._type != type) {
    LOG_TOPIC("c3af5", DEBUG, arangodb::Logger::AQL)
    << "Engine with id " << id << " has other type";
    return nullptr;
  }
  
  if (ei._isOpen) {
    LOG_TOPIC("7c2a3", DEBUG, arangodb::Logger::AQL)
        << "Engine with id " << id << " is already open";
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_LOCKED, "query with given vocbase and id is already open");
  }
  
  ei._isOpen = true;
  if (ei._queryInfo) {
    ei._queryInfo->_expires = TRI_microtime() + ei._queryInfo->_timeToLive;
    ei._queryInfo->_numOpen++;

    // #warning the "isModificationQuery()" is probably too coarse-grained here.
    // previously the "isModificationQuery()" always returned false on a DB server.
    // now that we made it return the true value, the assertion is triggered.
    // TODO: need to sort this out.
    // TRI_ASSERT(ei._queryInfo->_numOpen == 1 || !ei._queryInfo->_query->isModificationQuery());
    LOG_TOPIC("b1cfd", TRACE, arangodb::Logger::AQL) << "opening engine " << id << ", query id: " << ei._queryInfo->_query->id() << ", numOpen: " << ei._queryInfo->_numOpen;
  } else {
    LOG_TOPIC("50eff", TRACE, arangodb::Logger::AQL) << "opening engine " << id << ", no query";
  }

  return ei._engine;
}

/// @brief close
void QueryRegistry::closeEngine(EngineId engineId) {
  LOG_TOPIC("3f0c9", DEBUG, arangodb::Logger::AQL) << "returning engine with id " << engineId;
  WRITE_LOCKER(writeLocker, _lock);
  
  auto it = _engines.find(engineId);
  if (it == _engines.end()) {
    LOG_TOPIC("97351", DEBUG, arangodb::Logger::AQL)
    << "Found no engine with id " << engineId;
    return;
  }
  
  EngineInfo& ei = it->second;
  if (!ei._isOpen) {
    LOG_TOPIC("45b8e", DEBUG, arangodb::Logger::AQL) << "engine id " << engineId << " was not open.";
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "engine with given vocbase and id is not open");
  }
  
  ei._isOpen = false;
  
  if (ei._queryInfo) {
    TRI_ASSERT(ei._queryInfo->_numOpen > 0);
    ei._queryInfo->_numOpen--;
    if (!ei._queryInfo->_query->killed() && ei._queryInfo->_expires != 0) {
      ei._queryInfo->_expires = TRI_microtime() + ei._queryInfo->_timeToLive;
    }
    LOG_TOPIC("5ecdc", TRACE, arangodb::Logger::AQL) << "closing engine " << engineId << ", query id: " << ei._queryInfo->_query->id() << ", numOpen: " << ei._queryInfo->_numOpen;
  } else {
    LOG_TOPIC("ae981", TRACE, arangodb::Logger::AQL) << "closing engine " << engineId << ", no query";
  }
}

/// @brief destroy
// cppcheck-suppress virtualCallInConstructor
std::unique_ptr<ClusterQuery> QueryRegistry::destroyQuery(std::string const& vocbase, QueryId id,
                                                          int errorCode) {
  std::unique_ptr<QueryInfo> queryInfo;

  {
    WRITE_LOCKER(writeLocker, _lock);

    auto m = _queries.find(vocbase);

    if (m == _queries.end()) {
      // database not found. this can happen as a consequence of a race between garbage collection
      // and query shutdown
      return nullptr;
    }
    
    auto q = m->second.find(id);

    if (q == m->second.end()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER, "query with given id not found in query registry");
    }

    if (q->second->_numOpen > 0) {
      // query in use by another thread/request
      if (errorCode == TRI_ERROR_QUERY_KILLED) {
        q->second->_query->kill();
      }
      q->second->_expires = 0.0;
      return nullptr;
    }

    // move query into our unique ptr, so we can process it outside
    // of the lock
    queryInfo = std::move(q->second);

    // remove query from the table of running queries
    m->second.erase(q);
    
    // remove engines
    for (auto const& pair : queryInfo->_query->snippets()) {
#ifndef ARANGODB_ENABLE_MAINTAINER_MODE
      _engines.erase(pair.first);
#else
      
      auto it = _engines.find(pair.first);
      if (it != _engines.end()) {
        TRI_ASSERT(it->second._queryInfo != nullptr);
        TRI_ASSERT(!it->second._isOpen);
        _engines.erase(it);
      }
#endif
    }
    for (auto& pair : queryInfo->_query->traversers()) {
      _engines.erase(pair.first);
    }

    if (m->second.empty()) {
      // clear empty entries in database-to-queries map
      _queries.erase(m);
    }
  }

  TRI_ASSERT(queryInfo != nullptr);
  TRI_ASSERT(queryInfo->_numOpen == 0);

  LOG_TOPIC("6756c", DEBUG, arangodb::Logger::AQL)
      << "query with id " << id << " is now destroyed";
  
  return std::move(queryInfo->_query);
}

/// used for a legacy shutdown
bool QueryRegistry::destroyEngine(EngineId engineId, int errorCode) {
  std::string vocbase;
  QueryId qId = 0;
  
  {
    WRITE_LOCKER(writeLocker, _lock);

    auto it = _engines.find(engineId);
    if (it == _engines.end()) {
      LOG_TOPIC("2b5e6", DEBUG, arangodb::Logger::AQL)
        << "Found no engine with id " << engineId;
      return false;
    }

    EngineInfo& ei = it->second;
    if (ei._isOpen) {
      if (ei._queryInfo && errorCode == TRI_ERROR_QUERY_KILLED) {
        ei._queryInfo->_query->kill();
        ei._queryInfo->_expires = 0.0;
      }
      LOG_TOPIC("b342e", DEBUG, arangodb::Logger::AQL) << "engine id " << engineId << " is open.";
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL, "engine with given vocbase and id is open");
    }

    if (ei._queryInfo) {
      TRI_ASSERT(ei._queryInfo->_numEngines > 0);
      ei._queryInfo->_numEngines--;
      if (ei._queryInfo->_numEngines == 0) { // shutdown Query
        qId = ei._queryInfo->_query->id();
        vocbase = ei._queryInfo->_query->vocbase().name();
      } else {
        ei._queryInfo->_expires = TRI_microtime() + ei._queryInfo->_timeToLive;
      }
    }
    
    _engines.erase(it);
  }

  if (qId != 0 && !vocbase.empty()) {  // simon: old shutdown case
    destroyQuery(vocbase, qId, errorCode);
  }
  
  return true;
}

void QueryRegistry::destroy(std::string const& vocbase) {
  {
    WRITE_LOCKER(writeLocker, _lock);

    auto m = _queries.find(vocbase);

    if (m == _queries.end()) {
      return;
    }

    for (auto& it : (*m).second) {
      it.second->_expires = 0.0;
      if (it.second->_numOpen > 0) {
        // query in use by another thread/request
        it.second->_query->kill();
      }
    }
  }

  expireQueries();
}

/// @brief expireQueries
void QueryRegistry::expireQueries() {
  double now = TRI_microtime();
  std::vector<std::pair<std::string, QueryId>> toDelete;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  std::vector<QueryId> queriesLeft;
#endif

  {
    WRITE_LOCKER(writeLocker, _lock);
    for (auto& x : _queries) {
      // x.first is a TRI_vocbase_t* and
      // x.second is a std::unordered_map<QueryId, std::unique_ptr<QueryInfo>>
      for (auto& y : x.second) {
        // y.first is a QueryId and
        // y.second is an std::unique_ptr<QueryInfo>
        std::unique_ptr<QueryInfo> const& qi = y.second;
        if (qi->_numOpen == 0 && now > qi->_expires) {
          toDelete.emplace_back(x.first, y.first);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
        } else {
          queriesLeft.emplace_back(y.first);
#endif
        }
      }
    }
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (!queriesLeft.empty()) {
    LOG_TOPIC("4f142", DEBUG, arangodb::Logger::AQL)
        << "queries left in QueryRegistry: " << queriesLeft;
  }
#endif
  for (auto& p : toDelete) {
    try {  // just in case
      LOG_TOPIC("e95dc", DEBUG, arangodb::Logger::AQL)
          << "timeout or RebootChecker alert for query with id " << p.second;
      destroyQuery(p.first, p.second, TRI_ERROR_TRANSACTION_ABORTED);
    } catch (...) {
    }
  }
}

/// @brief return number of registered queries
size_t QueryRegistry::numberRegisteredQueries() {
  READ_LOCKER(readLocker, _lock);
  size_t sum = 0;
  for (auto const& m : _queries) {
    sum += m.second.size();
  }
  return sum;
}

/// @brief for shutdown, we need to shut down all queries:
void QueryRegistry::destroyAll() try {
  std::vector<std::pair<std::string, QueryId>> allQueries;
  {
    READ_LOCKER(readlock, _lock);
    for (auto& p : _queries) {
      for (auto& q : p.second) {
        allQueries.emplace_back(p.first, q.first);
      }
    }
  }
  for (auto& p : allQueries) {
    try {
      LOG_TOPIC("df275", DEBUG, arangodb::Logger::AQL)
          << "Timeout for query with id " << p.second << " due to shutdown";
      destroyQuery(p.first, p.second, TRI_ERROR_SHUTTING_DOWN);
    } catch (...) {
      // ignore any errors here
    }
  }

  size_t count = numberRegisteredQueries();
  if (count > 0) {
    LOG_TOPIC("43bf8", INFO, arangodb::Logger::AQL)
        << "number of remaining queries in query registry at shutdown: " << count;
  }
} catch (std::exception const& ex) {
  LOG_TOPIC("30bdb", INFO, arangodb::Logger::AQL)
        << "caught exception during shutdown of query registry: " << ex.what();
}

void QueryRegistry::disallowInserts() {
  WRITE_LOCKER(writelock, _lock);
  _disallowInserts = true;
  // from here on, there shouldn't be any more inserts into the registry
}

/// use on coordinator to register snippets
void QueryRegistry::registerSnippets(SnippetList const& snippets) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  WRITE_LOCKER(guard, _lock);
  if (_disallowInserts) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }
  for (auto& pair : snippets) {
    if (pair.first != 0) { // skip the root snippet
      auto result = _engines.try_emplace(pair.first, EngineInfo(pair.second.get(), nullptr));
      TRI_ASSERT(result.second);
    }
  }
}

void QueryRegistry::unregisterSnippets(SnippetList const& snippets) noexcept {
  TRI_ASSERT(ServerState::instance()->isCoordinator());

  while(true) {
    WRITE_LOCKER(guard, _lock);
    size_t remain = snippets.size();
    for (auto& pair : snippets) {
      auto it = _engines.find(pair.first);
      if (it == _engines.end()) {
        remain--;
        continue;
      }
      if (it->second._isOpen) { // engine still in use
        LOG_TOPIC("33cfb", WARN, arangodb::Logger::AQL)
          << "engine snippet '" << pair.first << "' is still in use";
        continue;
      }
      _engines.erase(it);
      remain--;
    }
    guard.unlock();
    if (remain == 0) {
      break;
    }
    std::this_thread::yield();
  }
}

QueryRegistry::QueryInfo::QueryInfo(std::unique_ptr<ClusterQuery> query, double ttl, cluster::CallbackGuard guard)
    : _vocbase(&(query->vocbase())),
      _query(std::move(query)),
      _timeToLive(ttl),
      _expires(TRI_microtime() + ttl),
      _numEngines(0),
      _numOpen(0),
      _rebootTrackerCallbackGuard(std::move(guard))
{}

QueryRegistry::QueryInfo::~QueryInfo() = default;
