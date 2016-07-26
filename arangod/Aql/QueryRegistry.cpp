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

#include "Aql/QueryRegistry.h"
#include "Logger/Logger.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Aql/ExecutionEngine.h"

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
      destroy(p.first, p.second, TRI_ERROR_TRANSACTION_ABORTED);
    } catch (...) {
    }
  }
}

/// @brief insert
void QueryRegistry::insert(QueryId id, Query* query, double ttl) {
  TRI_ASSERT(query != nullptr);
  TRI_ASSERT(query->trx() != nullptr);
  auto vocbase = query->vocbase();

  WRITE_LOCKER(writeLocker, _lock);

  auto m = _queries.find(vocbase->name());
  if (m == _queries.end()) {
    m = _queries.emplace(vocbase->name(),
                         std::unordered_map<QueryId, QueryInfo*>()).first;

    TRI_ASSERT(_queries.find(vocbase->name()) != _queries.end());
  }
  auto q = m->second.find(id);
  if (q == m->second.end()) {
    auto p = std::make_unique<QueryInfo>();
    p->_vocbase = vocbase;
    p->_id = id;
    p->_query = query;
    p->_isOpen = false;
    p->_timeToLive = ttl;
    p->_expires = TRI_microtime() + ttl;
    m->second.emplace(id, p.get());
    p.release();

    TRI_ASSERT(_queries.find(vocbase->name())->second.find(id) !=
                         _queries.find(vocbase->name())->second.end());

    // If we have set _makeNolockHeaders, we need to unset it:
    if (Transaction::_makeNolockHeaders != nullptr) {
      if (Transaction::_makeNolockHeaders == query->engine()->lockedShards()) {
        Transaction::_makeNolockHeaders = nullptr;
      }
      // else {
      // We have not set it, just leave it alone. This happens in particular
      // on the DBServers, who do not set lockedShards() themselves.
      // }
    }
  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "query with given vocbase and id already there");
  }
}

/// @brief open
Query* QueryRegistry::open(TRI_vocbase_t* vocbase, QueryId id) {
  // std::cout << "Taking out query with ID " << id << std::endl;
  WRITE_LOCKER(writeLocker, _lock);

  auto m = _queries.find(vocbase->name());
  if (m == _queries.end()) {
    m = _queries.emplace(vocbase->name(),
                         std::unordered_map<QueryId, QueryInfo*>()).first;
  }
  auto q = m->second.find(id);
  if (q == m->second.end()) {
    return nullptr;
  }
  QueryInfo* qi = q->second;
  if (qi->_isOpen) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "query with given vocbase and id is already open");
  }
  qi->_isOpen = true;

  // If we had set _makeNolockHeaders, we need to reset it:
  if (qi->_query->engine()->lockedShards() != nullptr) {
    if (Transaction::_makeNolockHeaders == nullptr) {
      // std::cout << "Setting _makeNolockHeaders\n";
      Transaction::_makeNolockHeaders = qi->_query->engine()->lockedShards();
    } else {
      LOG(WARN) << "Found strange lockedShards in thread, not overwriting!";
    }
  }

  return qi->_query;
}

/// @brief close
void QueryRegistry::close(TRI_vocbase_t* vocbase, QueryId id, double ttl) {
  // std::cout << "Returning query with ID " << id << std::endl;
  WRITE_LOCKER(writeLocker, _lock);

  auto m = _queries.find(vocbase->name());
  if (m == _queries.end()) {
    m = _queries.emplace(vocbase->name(),
                         std::unordered_map<QueryId, QueryInfo*>()).first;
  }
  auto q = m->second.find(id);
  if (q == m->second.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "query with given vocbase and id not found");
  }
  QueryInfo* qi = q->second;
  if (!qi->_isOpen) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "query with given vocbase and id is not open");
  }

  // If we have set _makeNolockHeaders, we need to unset it:
  if (Transaction::_makeNolockHeaders != nullptr) {
    if (Transaction::_makeNolockHeaders ==
        qi->_query->engine()->lockedShards()) {
      // std::cout << "Resetting _makeNolockHeaders to nullptr\n";
      Transaction::_makeNolockHeaders = nullptr;
    } else {
      if (Transaction::_makeNolockHeaders != nullptr) {
        if (Transaction::_makeNolockHeaders ==
            qi->_query->engine()->lockedShards()) {
          Transaction::_makeNolockHeaders = nullptr;
        }
        // else {
        // We have not set it, just leave it alone. This happens in particular
        // on the DBServers, who do not set lockedShards() themselves.
        // }
      }
    }
  }

  qi->_isOpen = false;
  qi->_expires = TRI_microtime() + qi->_timeToLive;
}

/// @brief destroy
void QueryRegistry::destroy(std::string const& vocbase, QueryId id,
                            int errorCode) {
  WRITE_LOCKER(writeLocker, _lock);

  auto m = _queries.find(vocbase);
  if (m == _queries.end()) {
    m = _queries.emplace(vocbase, std::unordered_map<QueryId, QueryInfo*>())
            .first;
  }
  auto q = m->second.find(id);
  if (q == m->second.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "query with given vocbase and id not found");
  }
  QueryInfo* qi = q->second;

  // If the query is open, we can delete it right away, if not, we need
  // to register the transaction with the current context and adjust
  // the debugging counters for transactions:
  if (!qi->_isOpen) {
    // If we had set _makeNolockHeaders, we need to reset it:
    if (qi->_query->engine()->lockedShards() != nullptr) {
      if (Transaction::_makeNolockHeaders == nullptr) {
        Transaction::_makeNolockHeaders = qi->_query->engine()->lockedShards();
      } else {
        LOG(WARN) << "Found strange lockedShards in thread, not overwriting!";
      }
    }
  }

  if (errorCode == TRI_ERROR_NO_ERROR) {
    // commit the operation
    qi->_query->trx()->commit();
  }

  // Now we can delete it:
  delete qi->_query;
  delete qi;

  q->second = nullptr;
  m->second.erase(q);
}

/// @brief destroy
void QueryRegistry::destroy(TRI_vocbase_t* vocbase, QueryId id, int errorCode) {
  destroy(vocbase->name(), id, errorCode);
}

/// @brief expireQueries
void QueryRegistry::expireQueries() {
  double now = TRI_microtime();
  std::vector<std::pair<std::string, QueryId>> toDelete;

  {
    WRITE_LOCKER(writeLocker, _lock);
    for (auto& x : _queries) {
      // x.first is a TRI_vocbase_t* and
      // x.second is a std::unordered_map<QueryId, QueryInfo*>
      for (auto& y : x.second) {
        // y.first is a QueryId and
        // y.second is a QueryInfo*
        QueryInfo*& qi = y.second;
        if (!qi->_isOpen && now > qi->_expires) {
          toDelete.emplace_back(x.first, y.first);
        }
      }
    }
  }

  for (auto& p : toDelete) {
    try {  // just in case
      destroy(p.first, p.second, TRI_ERROR_TRANSACTION_ABORTED);
    } catch (...) {
    }
  }
}
