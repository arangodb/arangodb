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
  qi->_isOpen = false;
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
  delete qi->_query;
  delete qi;
  q->second = nullptr;
  m->second.erase(q);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief expireQueries
////////////////////////////////////////////////////////////////////////////////

void expireQueries () {
  // TODO
}

// -----------------------------------------------------------------------------
// --SECTION--
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


