////////////////////////////////////////////////////////////////////////////////
/// @brief associative array implementation
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
/// @author Dr. Frank Celler
/// @author Martin Schoenert
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2006-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_HASH_INDEX_HASH__ARRAY_H
#define ARANGODB_HASH_INDEX_HASH__ARRAY_H 1

#include "Basics/Common.h"

#include "Indexes/Index.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace triagens {
  namespace arango {
    class HashIndex;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 TRI_hash_array_t
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief associative array
////////////////////////////////////////////////////////////////////////////////

class TRI_hash_array_t {
    size_t _numFields; // the number of fields indexes

    struct Bucket {

      uint64_t _nrAlloc; // the size of the table
      uint64_t _nrUsed;  // the number of used entries

      TRI_index_element_t** _table; // the table itself, aligned to a cache line boundary
    };

    std::vector<Bucket> _buckets;
    size_t _bucketsMask;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

  public:
    TRI_hash_array_t (size_t numFields, size_t numberBuckets = 1)
      : _numFields(numFields) {

      // Make the number of buckets a power of two:
      size_t ex = 0;
      size_t nr = 1;
      numberBuckets >>= 1;
      while (numberBuckets > 0) {
        ex += 1;
        numberBuckets >>= 1;
        nr <<= 1;
      }
      numberBuckets = nr;
      _bucketsMask = nr - 1;

      try {
        for (size_t j = 0; j < numberBuckets; j++) {
          _buckets.emplace_back();
          Bucket& b = _buckets.back();
          b._nrAlloc = initialSize();
          b._table = nullptr;

          // may fail...
          b._table = new TRI_index_element_t* [b._nrAlloc];

          for (uint64_t i = 0; i < b._nrAlloc; i++) {
            b._table[i] = nullptr;
          }
        }
      }
      catch (...) {
        for (auto& b : _buckets) {
          delete [] b._table;
          b._table = nullptr;
          b._nrAlloc = 0;
        }
        throw;
      }
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

    ~TRI_hash_array_t () {
      for (auto& b : _buckets) {
        for (size_t i = 0; i < b._nrAlloc; i++) {
          TRI_index_element_t* p = b._table[i];
          if (p != nullptr) {
            TRI_index_element_t::free(p);
          }
        }
        delete [] b._table;
        b._table = nullptr;
        b._nrAlloc = 0;
      }
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief adhere to the rule of five
////////////////////////////////////////////////////////////////////////////////

    TRI_hash_array_t (TRI_hash_array_t const&) = delete;  // copy constructor
    TRI_hash_array_t (TRI_hash_array_t&&) = delete;       // move constructor
    TRI_hash_array_t& operator= (TRI_hash_array_t const&) = delete;  // op =
    TRI_hash_array_t& operator= (TRI_hash_array_t&&) = delete;       // op =

////////////////////////////////////////////////////////////////////////////////
/// @brief initial preallocation size of the hash table when the table is
/// first created
/// setting this to a high value will waste memory but reduce the number of
/// reallocations/repositionings necessary when the table grows
////////////////////////////////////////////////////////////////////////////////

  private:

    static uint64_t initialSize () {
      return 251;
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief determines if a key corresponds to an element
////////////////////////////////////////////////////////////////////////////////

    bool isEqualKeyElement (TRI_index_search_value_t const* left,
                            TRI_index_element_t const* right) const;


////////////////////////////////////////////////////////////////////////////////
/// @brief given a key generates a hash integer
////////////////////////////////////////////////////////////////////////////////

    uint64_t hashKey (TRI_index_search_value_t const* key) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief given an element generates a hash integer
////////////////////////////////////////////////////////////////////////////////

    uint64_t hashElement (TRI_index_element_t const* element) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief resize the hash array
////////////////////////////////////////////////////////////////////////////////

    int resizeInternal (triagens::arango::HashIndex* hashIndex,
                        Bucket& b,
                        uint64_t targetSize,
                        bool allowShrink);

////////////////////////////////////////////////////////////////////////////////
/// @brief check a resize of the hash array
////////////////////////////////////////////////////////////////////////////////

    bool checkResize (triagens::arango::HashIndex* hashIndex,
                      Bucket& b);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the hash array's memory usage
////////////////////////////////////////////////////////////////////////////////

  public:
    size_t memoryUsage ();

////////////////////////////////////////////////////////////////////////////////
/// @brief get the number of elements in the hash
////////////////////////////////////////////////////////////////////////////////

    size_t size () {
      size_t sum = 0;
      for (auto& b : _buckets) {
        sum += static_cast<size_t>(b._nrUsed);
      }
      return sum;
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the hash table
////////////////////////////////////////////////////////////////////////////////

    int resize (triagens::arango::HashIndex*, size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an element given a key, returns NULL if not found
////////////////////////////////////////////////////////////////////////////////

    TRI_index_element_t* findByKey (TRI_index_search_value_t* key) const;

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the array
////////////////////////////////////////////////////////////////////////////////

    int insert (triagens::arango::HashIndex*,
                TRI_index_search_value_t const* key,
                TRI_index_element_t const* element,
                bool isRollback);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

    int remove (triagens::arango::HashIndex*,
                TRI_index_element_t* element);

};

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
