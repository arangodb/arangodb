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

      QueryCacheResultEntry (char const*,
                             size_t, 
                             struct TRI_json_t*);

      ~QueryCacheResultEntry ();

      size_t               queryStringLength;
      struct TRI_json_t*   queryResult;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                    struct QueryCacheDatabaseEntry
// -----------------------------------------------------------------------------

    struct QueryCacheDatabaseEntry {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      QueryCacheDatabaseEntry () = delete;
      QueryCacheDatabaseEntry (QueryCacheDatabaseEntry const&) = delete;
      QueryCacheDatabaseEntry& operator= (QueryCacheDatabaseEntry const&) = delete;

////////////////////////////////////////////////////////////////////////////////
/// @brief create a database-specific cache
////////////////////////////////////////////////////////////////////////////////
     
      explicit QueryCacheDatabaseEntry (struct TRI_vocbase_s*);

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

      std::shared_ptr<QueryCacheResultEntry> lookup (uint64_t, 
                                                     char const*,
                                                     size_t) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief store a query result in the database-specific cache
////////////////////////////////////////////////////////////////////////////////

      void store (uint64_t,
                  char const*,
                  size_t,
                  struct TRI_json_t*,
                  std::vector<std::string> const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidate all entries for a collection in the database-specific 
/// cache
////////////////////////////////////////////////////////////////////////////////

      void invalidate (std::string const&);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

      struct TRI_vocbase_s* vocbase;

      std::unordered_map<uint64_t, std::shared_ptr<QueryCacheResultEntry>> entriesByHash;

      std::unordered_map<std::string, std::unordered_set<uint64_t>> entriesByCollection;

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
/// @brief return whether or not the query cache is enabled
////////////////////////////////////////////////////////////////////////////////

        QueryCacheMode mode () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief enable or disable the query cache
////////////////////////////////////////////////////////////////////////////////

        void mode (QueryCacheMode);

////////////////////////////////////////////////////////////////////////////////
/// @brief enable or disable the query cache
////////////////////////////////////////////////////////////////////////////////

        void mode (std::string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup a query result in the cache
////////////////////////////////////////////////////////////////////////////////

        std::shared_ptr<QueryCacheResultEntry> lookup (struct TRI_vocbase_s*,
                                                       uint64_t,
                                                       char const*,
                                                       size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief store a query in the cache
/// if the call is successful, the cache has taken over ownership for the 
/// query result!
////////////////////////////////////////////////////////////////////////////////

        void store (struct TRI_vocbase_s*, 
                    uint64_t,
                    char const*,
                    size_t,
                    struct TRI_json_t*,
                    std::vector<std::string> const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidate all queries for a particular collection
////////////////////////////////////////////////////////////////////////////////

        void invalidate (struct TRI_vocbase_s*,
                         std::string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidate all queries for a particular database
////////////////////////////////////////////////////////////////////////////////

        void invalidate (struct TRI_vocbase_s*);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the pointer to the global query cache
////////////////////////////////////////////////////////////////////////////////

        static QueryCache* instance ();

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief read-write lock for the cache
////////////////////////////////////////////////////////////////////////////////

        triagens::basics::ReadWriteLock _lock;

////////////////////////////////////////////////////////////////////////////////
/// @brief cached queries
////////////////////////////////////////////////////////////////////////////////

        std::unordered_map<struct TRI_vocbase_s*, QueryCacheDatabaseEntry*> _entries;

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
