////////////////////////////////////////////////////////////////////////////////
/// @brief allow to register AQL Queries in a central place in memory
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Copyright 2014, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/QueryRegistry.h"
#include "Basics/WriteLocker.h"
#include "Basics/ReadLocker.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                           the QueryRegistry class
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                          constructors/destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

QueryRegistry::~QueryRegistry () {
  std::vector<std::pair<TRI_vocbase_t*, QueryId>> toDelete;
  {
    WRITE_LOCKER(_lock);
    for (auto& x : _queries) {
      // x.first is a TRI_vocbase_t* and
      // x.second is a std::unordered_map<QueryId, QueryInfo*>
      for (auto& y : x.second) {
        // y.first is a QueryId and
        // y.second is a QueryInfo*
        toDelete.emplace_back(x.first, y.first);
      }
    }
  }
  for (auto& p : toDelete) {
    try {  // just in case
      destroy(p.first, p.second);
    }
    catch (...) {
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert
////////////////////////////////////////////////////////////////////////////////

void QueryRegistry::insert (TRI_vocbase_t* vocbase,
                            QueryId id,
                            Query* query,
                            double ttl) {

  WRITE_LOCKER(_lock);

  auto m = _queries.find(vocbase);
  if (m == _queries.end()) {
    m = _queries.emplace(vocbase, std::unordered_map<QueryId, QueryInfo*>()).first;
  }
  auto q = m->second.find(id);
  if (q == m->second.end()) {
    std::unique_ptr<QueryInfo> p(new QueryInfo());
    p->_vocbase = vocbase;
    p->_id = id;
    p->_query = query;
    p->_isOpen = false;
    p->_timeToLive = ttl;
    p->_expires = TRI_microtime() + ttl;
    m->second.insert(make_pair(id, p.release()));
    // A query that is being shelved must unregister its transaction
    // with the current context:
    query->trx()->unregisterTransactionWithContext();
    // Also, we need to count down the debugging counters for transactions:
    triagens::arango::TransactionBase::increaseNumbers(-1, -1);
  }
  else {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                     "query with given vocbase and id already there");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief open
////////////////////////////////////////////////////////////////////////////////

Query* QueryRegistry::open (TRI_vocbase_t* vocbase, QueryId id) {

  WRITE_LOCKER(_lock);

  auto m = _queries.find(vocbase);
  if (m == _queries.end()) {
    m = _queries.emplace(vocbase, std::unordered_map<QueryId, QueryInfo*>()).first;
  }
  auto q = m->second.find(id);
  if (q == m->second.end()) {
    return nullptr;
  }
  QueryInfo* qi = q->second;
  if (qi->_isOpen) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                "query with given vocbase and id is already open");
  }
  qi->_isOpen = true;

  // A query that is being opened must register its transaction
  // with the current context:
  qi->_query->trx()->registerTransactionWithContext();
  // Also, we need to count up the debugging counters for transactions:
  triagens::arango::TransactionBase::increaseNumbers(1, 1);

  return qi->_query;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief close
////////////////////////////////////////////////////////////////////////////////
        
void QueryRegistry::close (TRI_vocbase_t* vocbase, QueryId id, double ttl) {

  WRITE_LOCKER(_lock);

  auto m = _queries.find(vocbase);
  if (m == _queries.end()) {
    m = _queries.emplace(vocbase, std::unordered_map<QueryId, QueryInfo*>()).first;
  }
  auto q = m->second.find(id);
  if (q == m->second.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                "query with given vocbase and id not found");
  }
  QueryInfo* qi = q->second;
  if (! qi->_isOpen) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                "query with given vocbase and id is not open");
  }

  // A query that is being closed must unregister its transaction
  // with the current context:
  qi->_query->trx()->unregisterTransactionWithContext();
  // Also, we need to count down the debugging counters for transactions:
  triagens::arango::TransactionBase::increaseNumbers(1, 1);

  qi->_isOpen = false;
  qi->_expires = TRI_microtime() + qi->_timeToLive;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy
////////////////////////////////////////////////////////////////////////////////

void QueryRegistry::destroy (TRI_vocbase_t* vocbase, QueryId id) {
  WRITE_LOCKER(_lock);

  auto m = _queries.find(vocbase);
  if (m == _queries.end()) {
    m = _queries.emplace(vocbase, std::unordered_map<QueryId, QueryInfo*>()).first;
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
  if (! qi->_isOpen) {
    qi->_query->trx()->registerTransactionWithContext();
    // Also, we need to count down the debugging counters for transactions:
    triagens::arango::TransactionBase::increaseNumbers(1, 1);
  }

  // Now we can delete it:
  delete qi->_query;
  delete qi;

  q->second = nullptr;
  m->second.erase(q);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief expireQueries
////////////////////////////////////////////////////////////////////////////////

void QueryRegistry::expireQueries () {
  std::vector<std::pair<TRI_vocbase_t*, QueryId>> toDelete;
  {
    WRITE_LOCKER(_lock);
    double now = TRI_microtime();
    for (auto& x : _queries) {
      // x.first is a TRI_vocbase_t* and
      // x.second is a std::unordered_map<QueryId, QueryInfo*>
      for (auto& y : x.second) {
        // y.first is a QueryId and
        // y.second is a QueryInfo*
        QueryInfo*& qi(y.second);
        if (! qi->_isOpen && now > qi->_expires) {
          toDelete.emplace_back(x.first, y.first);
        }
      }
    }
  }
  for (auto& p : toDelete) {
    try {  // just in case
      destroy(p.first, p.second);
    }
    catch (...) {
    }
  }
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


