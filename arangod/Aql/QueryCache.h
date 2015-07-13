////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, query cache
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_QUERY_CACHE_H
#define ARANGODB_AQL_QUERY_CACHE_H 1

#include "Basics/Common.h"
#include "Basics/JsonHelper.h"
#include "Basics/Mutex.h"
#include "Basics/ReadWriteLock.h"

struct TRI_json_t;
struct TRI_vocbase_s;

namespace triagens {
  namespace aql {

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief cache mode
////////////////////////////////////////////////////////////////////////////////

    enum QueryCacheMode {
      CACHE_ALWAYS_OFF,
      CACHE_ALWAYS_ON,
      CACHE_ON_DEMAND
    };

// -----------------------------------------------------------------------------
// --SECTION--                                      struct QueryCacheResultEntry
// -----------------------------------------------------------------------------

    struct QueryCacheResultEntry {
      QueryCacheResultEntry () = delete;

      QueryCacheResultEntry (uint64_t,
                             char const*,
                             size_t, 
                             struct TRI_json_t*,
                             std::vector<std::string> const&);

      ~QueryCacheResultEntry ();

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the element can be destroyed, and delete it if yes
////////////////////////////////////////////////////////////////////////////////

      void tryDelete ();

////////////////////////////////////////////////////////////////////////////////
/// @brief use the element, so it cannot be deleted meanwhile
////////////////////////////////////////////////////////////////////////////////

      void use ();

////////////////////////////////////////////////////////////////////////////////
/// @brief unuse the element, so it can be deleted if required
////////////////////////////////////////////////////////////////////////////////

      void unuse ();

// -----------------------------------------------------------------------------
// --SECTION--                                                  member variables
// -----------------------------------------------------------------------------

      uint64_t const                  _hash;
      char*                           _queryString;
      size_t const                    _queryStringLength;
      struct TRI_json_t*              _queryResult;
      std::vector<std::string> const  _collections;
      QueryCacheResultEntry*          _prev;
      QueryCacheResultEntry*          _next;
      std::atomic<uint32_t>           _refCount;
      std::atomic<uint32_t>           _deletionRequested;
      
    };

// -----------------------------------------------------------------------------
// --SECTION--                                  class QueryCacheResultEntryGuard
// -----------------------------------------------------------------------------

    class QueryCacheResultEntryGuard {

      QueryCacheResultEntryGuard (QueryCacheResultEntryGuard const&) = delete;
      QueryCacheResultEntryGuard& operator= (QueryCacheResultEntryGuard const&) = delete;
      QueryCacheResultEntryGuard () = delete;

      public:

        explicit QueryCacheResultEntryGuard (QueryCacheResultEntry* entry) 
          : _entry(entry) {
        
        }

        ~QueryCacheResultEntryGuard () {
          if (_entry != nullptr) {
            _entry->unuse();
          }
        }

        QueryCacheResultEntry* get () {
          return _entry;
        }

      private:
 
        QueryCacheResultEntry* _entry;
      
    };

// -----------------------------------------------------------------------------
// --SECTION--                                    struct QueryCacheDatabaseEntry
// -----------------------------------------------------------------------------

    struct QueryCacheDatabaseEntry {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      QueryCacheDatabaseEntry (QueryCacheDatabaseEntry const&) = delete;
      QueryCacheDatabaseEntry& operator= (QueryCacheDatabaseEntry const&) = delete;

////////////////////////////////////////////////////////////////////////////////
/// @brief create a database-specific cache
////////////////////////////////////////////////////////////////////////////////
     
      QueryCacheDatabaseEntry ();

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a database-specific cache
////////////////////////////////////////////////////////////////////////////////

      ~QueryCacheDatabaseEntry ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup a query result in the database-specific cache
////////////////////////////////////////////////////////////////////////////////

      QueryCacheResultEntry* lookup (uint64_t, 
                                     char const*,
                                     size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief store a query result in the database-specific cache
////////////////////////////////////////////////////////////////////////////////

      void store (uint64_t,
                  QueryCacheResultEntry*);

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidate all entries for the given collections in the 
/// database-specific cache
////////////////////////////////////////////////////////////////////////////////

      void invalidate (std::vector<char const*> const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidate all entries for a collection in the database-specific 
/// cache
////////////////////////////////////////////////////////////////////////////////

      void invalidate (char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief enforce maximum number of results
////////////////////////////////////////////////////////////////////////////////

      void enforceMaxResults (size_t);

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether the element can be destroyed, and delete it if yes
////////////////////////////////////////////////////////////////////////////////

      void tryDelete (QueryCacheResultEntry*);

////////////////////////////////////////////////////////////////////////////////
/// @brief unlink the result entry from the list
////////////////////////////////////////////////////////////////////////////////
  
      void unlink (QueryCacheResultEntry*);

////////////////////////////////////////////////////////////////////////////////
/// @brief link the result entry to the end of the list
////////////////////////////////////////////////////////////////////////////////
  
      void link (QueryCacheResultEntry*);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief hash table that maps query hashes to query results
////////////////////////////////////////////////////////////////////////////////

      std::unordered_map<uint64_t, QueryCacheResultEntry*> _entriesByHash;

////////////////////////////////////////////////////////////////////////////////
/// @brief hash table that contains all collection-specific query results
/// maps from collection names to a set of query results as defined in 
/// _entriesByHash
////////////////////////////////////////////////////////////////////////////////

      std::unordered_map<std::string, std::unordered_set<uint64_t>> _entriesByCollection;

////////////////////////////////////////////////////////////////////////////////
/// @brief beginning of linked list of result entries
////////////////////////////////////////////////////////////////////////////////

      QueryCacheResultEntry* _head;

////////////////////////////////////////////////////////////////////////////////
/// @brief end of linked list of result entries
////////////////////////////////////////////////////////////////////////////////

      QueryCacheResultEntry* _tail;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of elements in this cache
////////////////////////////////////////////////////////////////////////////////

      size_t _numElements;

    };

// -----------------------------------------------------------------------------
// --SECTION--                                                  class QueryCache
// -----------------------------------------------------------------------------

    class QueryCache {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      public:

        QueryCache (QueryCache const&) = delete;
        QueryCache& operator= (QueryCache const&) = delete;
      
////////////////////////////////////////////////////////////////////////////////
/// @brief create cache
////////////////////////////////////////////////////////////////////////////////

        QueryCache ();

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the cache
////////////////////////////////////////////////////////////////////////////////

        ~QueryCache ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief return the query cache properties
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::Json properties ();

////////////////////////////////////////////////////////////////////////////////
/// @brief return the cache properties
////////////////////////////////////////////////////////////////////////////////

        void properties (std::pair<std::string, size_t>&);

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the cache properties
////////////////////////////////////////////////////////////////////////////////

        void setProperties (std::pair<std::string, size_t> const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief test whether the cache might be active
/// this is a quick test that may save the caller from further bothering
/// about the query cache if case it returns `false`
////////////////////////////////////////////////////////////////////////////////

        bool mayBeActive () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether or not the query cache is enabled
////////////////////////////////////////////////////////////////////////////////

        QueryCacheMode mode () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief return a string version of the mode
////////////////////////////////////////////////////////////////////////////////

        static std::string modeString (QueryCacheMode);

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup a query result in the cache
////////////////////////////////////////////////////////////////////////////////

        QueryCacheResultEntry* lookup (struct TRI_vocbase_s*,
                                       uint64_t,
                                       char const*,
                                       size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief store a query in the cache
/// if the call is successful, the cache has taken over ownership for the 
/// query result!
////////////////////////////////////////////////////////////////////////////////

        QueryCacheResultEntry* store (struct TRI_vocbase_s*, 
                                      uint64_t,
                                      char const*,
                                      size_t,
                                      struct TRI_json_t*,
                                      std::vector<std::string> const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidate all queries for the given collections
////////////////////////////////////////////////////////////////////////////////

        void invalidate (struct TRI_vocbase_s*,
                         std::vector<char const*> const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidate all queries for a particular collection
////////////////////////////////////////////////////////////////////////////////

        void invalidate (struct TRI_vocbase_s*,
                         char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidate all queries for a particular database
////////////////////////////////////////////////////////////////////////////////

        void invalidate (struct TRI_vocbase_s*);

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidate all queries
////////////////////////////////////////////////////////////////////////////////

        void invalidate ();

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes a query string
////////////////////////////////////////////////////////////////////////////////

        uint64_t hashQueryString (char const*,
                                  size_t) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief get the pointer to the global query cache
////////////////////////////////////////////////////////////////////////////////

        static QueryCache* instance ();

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief enforce maximum number of results in each database-specific cache
////////////////////////////////////////////////////////////////////////////////

        void enforceMaxResults (size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief determine which part of the cache to use for the cache entries
////////////////////////////////////////////////////////////////////////////////

        unsigned int getPart (struct TRI_vocbase_s const*) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidate all entries in the cache part
/// note that the caller of this method must hold the write lock
////////////////////////////////////////////////////////////////////////////////

        void invalidate (unsigned int);

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the maximum number of elements in the cache
////////////////////////////////////////////////////////////////////////////////

        void setMaxResults (size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief enable or disable the query cache
////////////////////////////////////////////////////////////////////////////////

        void setMode (QueryCacheMode);

////////////////////////////////////////////////////////////////////////////////
/// @brief enable or disable the query cache
////////////////////////////////////////////////////////////////////////////////

        void setMode (std::string const&);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief number of R/W locks for the query cache
////////////////////////////////////////////////////////////////////////////////

        static uint64_t const NumberOfParts = 8;

////////////////////////////////////////////////////////////////////////////////
/// @brief protect mode changes with a mutex
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::Mutex _propertiesLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief read-write lock for the cache
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::ReadWriteLock _entriesLock[NumberOfParts];

////////////////////////////////////////////////////////////////////////////////
/// @brief cached query entries, organized per database
////////////////////////////////////////////////////////////////////////////////

        std::unordered_map<struct TRI_vocbase_s*, QueryCacheDatabaseEntry*> _entries[NumberOfParts];

    };

  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
