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
#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/Query.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Basics/system-functions.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "Transaction/Methods.h"

using namespace arangodb;
using namespace arangodb::aql;

QueryRegistry::~QueryRegistry() {
  std::vector<std::pair<std::string, QueryId>> toDelete;

  {
    WRITE_LOCKER(writeLocker, _lock);

    try {
      for (auto& x : _queries) {
        // x.first is a TRI_vocbase_t* and
        // x.second is a std::unordered_map<QueryId, QueryInfo*>
        for (auto& y : x.second) {
          // y.first is a QueryId and
          // y.second is a QueryInfo*
          toDelete.emplace_back(x.first, y.first);
        }
      }
    } catch (...) {
      // the emplace_back() above might fail
      // prevent throwing exceptions in the destructor
    }
  }

  // note: destroy() will acquire _lock itself, so it must be called without
  // holding the lock
  for (auto& p : toDelete) {
    try {  // just in case
      destroy(p.first, p.second, TRI_ERROR_TRANSACTION_ABORTED, false);
    } catch (...) {
    }
  }
}

/// @brief insert
void QueryRegistry::insert(QueryId id, Query* query, double ttl,
                           bool isPrepared, bool keepLease) {
  TRI_ASSERT(query != nullptr);
  TRI_ASSERT(query->trx() != nullptr);
  LOG_TOPIC("77778", DEBUG, arangodb::Logger::AQL)
      << "Register query with id " << id << " : " << query->queryString();
  auto& vocbase = query->vocbase();

  if (vocbase.isDropped()) {
    // don't register any queries for dropped databases
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  // create the query info object outside of the lock
  auto p = std::make_unique<QueryInfo>(id, query, ttl, isPrepared);
  p->_isOpen = keepLease;

  // now insert into table of running queries
  {
    WRITE_LOCKER(writeLocker, _lock);
    if (_disallowInserts) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
    }

    auto result = _queries[vocbase.name()].emplace(id, std::move(p));
    if (!result.second) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL, "query with given vocbase and id already there");
    }
  }
}

/// @brief open
Query* QueryRegistry::open(TRI_vocbase_t* vocbase, QueryId id) {
  LOG_TOPIC("8c204", DEBUG, arangodb::Logger::AQL) << "Open query with id " << id;
  // std::cout << "Taking out query with ID " << id << std::endl;
  WRITE_LOCKER(writeLocker, _lock);

  auto m = _queries.find(vocbase->name());
  if (m == _queries.end()) {
    LOG_TOPIC("c3ae4", DEBUG, arangodb::Logger::AQL)
        << "Found no queries for DB: " << vocbase->name();
    return nullptr;
  }
  auto q = m->second.find(id);
  if (q == m->second.end()) {
    LOG_TOPIC("780f7", DEBUG, arangodb::Logger::AQL) << "Query id " << id << " not found in registry";
    return nullptr;
  }

  std::unique_ptr<QueryInfo>& qi = q->second;
  if (qi->_isOpen) {
    LOG_TOPIC("7c2a3", DEBUG, arangodb::Logger::AQL) << "Query with id " << id << " is already in open";
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "query with given vocbase and id is already open");
  }

  qi->_isOpen = true;

  if (!qi->_isPrepared) {
    try {
      qi->_query->prepare(this);
    } catch (...) {
      qi->_isOpen = false;
      qi->_expires = 0.0;
      throw;
    }
    qi->_isPrepared = true;
  }

  LOG_TOPIC("50eff", DEBUG, arangodb::Logger::AQL) << "Query with id " << id << " is now in use";
  return qi->_query;
}

/// @brief close
void QueryRegistry::close(TRI_vocbase_t* vocbase, QueryId id, double ttl) {
  LOG_TOPIC("3f0c9", DEBUG, arangodb::Logger::AQL) << "returning query with id " << id;
  WRITE_LOCKER(writeLocker, _lock);

  auto m = _queries.find(vocbase->name());
  if (m == _queries.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "query with given vocbase and id not found");
  }
  auto q = m->second.find(id);
  if (q == m->second.end()) {
    LOG_TOPIC("6671d", DEBUG, arangodb::Logger::AQL) << "Query id " << id << " not found in registry";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "query with given vocbase and id not found");
  }
  std::unique_ptr<QueryInfo>& qi = q->second;
  if (!qi->_isOpen) {
    LOG_TOPIC("b342e", DEBUG, arangodb::Logger::AQL) << "query id " << id << " was not open.";
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "query with given vocbase and id is not open");
  }

  if (!qi->_isPrepared) {
    qi->_isPrepared = true;
    try {
      qi->_query->prepare(this);
    } catch (...) {
      qi->_isOpen = false;
      qi->_expires = 0.0;
      throw;
    }
  }

  qi->_isOpen = false;
  qi->_expires = TRI_microtime() + qi->_timeToLive;
  LOG_TOPIC("ae981", DEBUG, arangodb::Logger::AQL) << "query with id " << id << " is now returned.";
}

/// @brief destroy
void QueryRegistry::destroy(std::string const& vocbase, QueryId id, int errorCode, bool ignoreOpened) {
  std::unique_ptr<QueryInfo> queryInfo;

  {
    WRITE_LOCKER(writeLocker, _lock);

    auto m = _queries.find(vocbase);

    if (m == _queries.end()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER, "query with given vocbase and id not found");
    }

    auto q = m->second.find(id);

    if (q == m->second.end()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER, "query with given vocbase and id not found");
    }

    if (q->second->_isOpen && !ignoreOpened) {
      // query in use by another thread/request
      q->second->_query->kill();
      q->second->_expires = 0.0;
      return;
    }

    // move query into our unique ptr, so we can process it outside
    // of the lock
    queryInfo = std::move(q->second);

    // remove query from the table of running queries
    m->second.erase(q);

    if (m->second.empty()) {
      // clear empty entries in database-to-queries map
      _queries.erase(m);
    }
  }

  TRI_ASSERT(queryInfo != nullptr);
  TRI_ASSERT(!queryInfo->_isOpen);

  if (!queryInfo->_isPrepared) {
    queryInfo->_isPrepared = true;
    queryInfo->_query->prepare(this);
  }

  // If the query was open, we can delete it right away, if not, we need
  // to register the transaction with the current context and adjust
  // the debugging counters for transactions:
  if (errorCode == TRI_ERROR_NO_ERROR) {
    // commit the operation
    queryInfo->_query->trx()->commit();
  }
  LOG_TOPIC("6756c", DEBUG, arangodb::Logger::AQL) << "query with id " << id << " is now destroyed";
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
      if (it.second->_isOpen) {
        // query in use by another thread/request
        it.second->_query->kill();
      }
    }
  }

  expireQueries();
}

ResultT<bool> QueryRegistry::isQueryInUse(TRI_vocbase_t* vocbase, QueryId id) {
  LOG_TOPIC("d9870", DEBUG, arangodb::Logger::AQL) << "Test if query with id " << id << " is in use.";

  READ_LOCKER(readLocker, _lock);

  auto m = _queries.find(vocbase->name());
  if (m == _queries.end()) {
    LOG_TOPIC("22107", DEBUG, arangodb::Logger::AQL)
        << "Found no queries for DB: " << vocbase->name();
    return ResultT<bool>::error(TRI_ERROR_QUERY_NOT_FOUND);
  }
  auto q = m->second.find(id);
  if (q == m->second.end()) {
    LOG_TOPIC("48f28", DEBUG, arangodb::Logger::AQL) << "Query id " << id << " not found in registry";
    return ResultT<bool>::error(TRI_ERROR_QUERY_NOT_FOUND);
  }
  return ResultT<bool>::success(q->second->_isOpen);
}

/// @brief expireQueries
void QueryRegistry::expireQueries() {
  double now = TRI_microtime();
  std::vector<std::pair<std::string, QueryId>> toDelete;
  std::vector<QueryId> queriesLeft;

  {
    WRITE_LOCKER(writeLocker, _lock);
    for (auto& x : _queries) {
      // x.first is a TRI_vocbase_t* and
      // x.second is a std::unordered_map<QueryId, std::unique_ptr<QueryInfo>>
      for (auto& y : x.second) {
        // y.first is a QueryId and
        // y.second is an std::unique_ptr<QueryInfo>
        std::unique_ptr<QueryInfo> const& qi = y.second;
        if (!qi->_isOpen && now > qi->_expires) {
          toDelete.emplace_back(x.first, y.first);
        } else {
          queriesLeft.emplace_back(y.first);
        }
      }
    }
  }

  if (!queriesLeft.empty()) {
    LOG_TOPIC("4f142", TRACE, arangodb::Logger::AQL) << "queries left in QueryRegistry: " << queriesLeft;
  }

  for (auto& p : toDelete) {
    try {  // just in case
      LOG_TOPIC("e95dc", DEBUG, arangodb::Logger::AQL) << "timeout for query with id " << p.second;
      destroy(p.first, p.second, TRI_ERROR_TRANSACTION_ABORTED, false);
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
void QueryRegistry::destroyAll() {
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
      destroy(p.first, p.second, TRI_ERROR_SHUTTING_DOWN, false);
    } catch (...) {
      // ignore any errors here
    }
  }

  size_t count = 0;
  {    
    READ_LOCKER(readlock, _lock);
    for (auto& p : _queries) {
      count += p.second.size();
    }
  }
  if (count > 0) {
    LOG_TOPIC("43bf8", INFO, arangodb::Logger::AQL)
        << "number of remaining queries in query registry at shutdown: " << count;
  }
}

void QueryRegistry::disallowInserts() {
  WRITE_LOCKER(writelock, _lock);
  _disallowInserts = true;
  // from here on, there shouldn't be any more inserts into the registry
}


QueryRegistry::QueryInfo::QueryInfo(QueryId id, Query* query, double ttl, bool isPrepared)
    : _vocbase(&(query->vocbase())),
      _id(id),
      _query(query),
      _isOpen(false),
      _isPrepared(isPrepared),
      _timeToLive(ttl),
      _expires(TRI_microtime() + ttl) {}

QueryRegistry::QueryInfo::~QueryInfo() { delete _query; }
