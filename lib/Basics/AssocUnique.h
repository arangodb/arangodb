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
/// @author Michael hackstein
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2006-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_HASH_INDEX_HASH__ARRAY_H
#define ARANGODB_HASH_INDEX_HASH__ARRAY_H 1

#include "Basics/Common.h"
#include "Basics/gcd.h"
#include "Basics/JsonHelper.h"
#include "Basics/logging.h"
#include "Basics/MutexLocker.h"
#include "Basics/random.h"

namespace triagens {
  namespace basics {

// -----------------------------------------------------------------------------
// --SECTION--                                       UNIQUE ASSOCIATIVE POINTERS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief associative array
////////////////////////////////////////////////////////////////////////////////

    template <class Key, class Element>
      class AssocUnique {

        public:

          typedef std::function<uint64_t(Key const*)> HashKeyFuncType;
          typedef std::function<uint64_t(Element const*)> HashElementFuncType;
          typedef std::function<bool(Key const*, uint64_t hash, Element const*)> 
            IsEqualKeyElementFuncType;
          typedef std::function<bool(Element const*, Element const*)> 
            IsEqualElementElementFuncType;

          typedef std::function<void(Element*)> CallbackElementFuncType;

        private:
          struct Bucket {

            uint64_t _nrAlloc; // the size of the table
            uint64_t _nrUsed;  // the number of used entries

            Element** _table; // the table itself, aligned to a cache line boundary
          };

          std::vector<Bucket> _buckets;
          size_t _bucketsMask;

          HashKeyFuncType const _hashKey;
          HashElementFuncType const _hashElement;
          IsEqualKeyElementFuncType const _isEqualKeyElement;
          IsEqualElementElementFuncType const _isEqualElementElement;

          std::function<std::string()> _contextCallback;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        public:

          AssocUnique (HashKeyFuncType hashKey,
              HashElementFuncType hashElement,
              IsEqualKeyElementFuncType isEqualKeyElement,
              IsEqualElementElementFuncType isEqualElementElement,
              size_t numberBuckets = 1,
              std::function<std::string()> contextCallback = [] () -> std::string { return ""; }) 
            : _hashKey(hashKey), 
              _hashElement(hashElement),
              _isEqualKeyElement(isEqualKeyElement),
              _isEqualElementElement(isEqualElementElement),
              _contextCallback(contextCallback) {

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
                  b._table = new Element* [b._nrAlloc];

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

          ~AssocUnique () {
            for (auto& b : _buckets) {
              delete [] b._table;
              b._table = nullptr;
              b._nrAlloc = 0;
            }
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief adhere to the rule of five
////////////////////////////////////////////////////////////////////////////////

          AssocUnique (AssocUnique const&) = delete;  // copy constructor
          AssocUnique (AssocUnique&&) = delete;       // move constructor
          AssocUnique& operator= (AssocUnique const&) = delete;  // op =
          AssocUnique& operator= (AssocUnique&&) = delete;       // op =

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
/// @brief resizes the array
////////////////////////////////////////////////////////////////////////////////

          void resizeInternal (Bucket& b,
                               uint64_t targetSize,
                               bool allowShrink) {

            if (b._nrAlloc >= targetSize && ! allowShrink) {
              return;
            }

            // only log performance infos for indexes with more than this number of entries
            static uint64_t const NotificationSizeThreshold = 131072; 

            double start = TRI_microtime();
            if (targetSize > NotificationSizeThreshold) {
              LOG_ACTION("index-resize %s, target size: %llu", 
                  _contextCallback().c_str(),
                  (unsigned long long) targetSize);
            }

            Element** oldTable    = b._table;
            uint64_t oldAlloc = b._nrAlloc;

            TRI_ASSERT(targetSize > 0);

            // This might throw, is catched outside
            b._table = new Element* [targetSize];

            for (uint64_t i = 0; i < targetSize; i++) {
              b._table[i] = nullptr;
            }

            b._nrAlloc = targetSize;

            if (b._nrUsed > 0) {
              uint64_t const n = b._nrAlloc;

              for (uint64_t j = 0; j < oldAlloc; j++) {
                Element* element = oldTable[j];

                if (element != nullptr) {
                  uint64_t i, k;
                  i = k = _hashElement(element) % n;

                  for (; i < n && b._table[i] != nullptr; ++i);
                  if (i == n) {
                    for (i = 0; i < k && b._table[i] != nullptr; ++i);
                  }

                  b._table[i] = element;
                }
              }
            }

            delete [] oldTable;

            LOG_TIMER((TRI_microtime() - start),
                "index-resize %s, target size: %llu", 
                _contextCallback().c_str(),
                (unsigned long long) targetSize);
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief check a resize of the hash array
////////////////////////////////////////////////////////////////////////////////

          bool checkResize (Bucket& b, uint64_t expected) {
            if (2 * (b._nrAlloc + expected) < 3 * b._nrUsed) {
              try {
                resizeInternal(b, 2 * (b._nrAlloc + expected) + 1, false);
              }
              catch (...) {
                return false;
              }
            }
            return true;
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief Finds the element at the given position in concatenated buckets.
///        Say we have 2 Buckets each of size 5. Position 7 would be in
///        the second bucket at Position 2 (7 - Bucket1.size())
////////////////////////////////////////////////////////////////////////////////

          Element* findElementSequentialBucktes (uint64_t position) const {
            for (auto& b : _buckets) {
              if (position >= b._nrAlloc) {
                position -= b._nrAlloc;
              }
              else {
                return b._table[position];
              }
            }
            return nullptr;
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief Insert a document into the given bucket
///        This does not resize and expects to have enough space
////////////////////////////////////////////////////////////////////////////////

          int doInsert (Key const* key,
                        Element* element,
                        Bucket& b,
                        uint64_t hash) {

            uint64_t const n = b._nrAlloc;
            uint64_t i = hash % n;
            uint64_t k = i;

            for (; i < n && b._table[i] != nullptr && 
                ! _isEqualKeyElement(key, hash, b._table[i]); ++i);
            if (i == n) {
              for (i = 0; i < k && b._table[i] != nullptr && 
                  ! _isEqualKeyElement(key, hash, b._table[i]); ++i);
            }

            Element* arrayElement = b._table[i];

            if (arrayElement != nullptr) {
              return TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED;
            }

            b._table[i] = static_cast<Element*>(element);
            TRI_ASSERT(b._table[i] != nullptr);
            b._nrUsed++;

            return TRI_ERROR_NO_ERROR;
          }


// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

        public:

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if this index is empty
////////////////////////////////////////////////////////////////////////////////

          bool isEmpty () const {
            for (auto& b : _buckets) {
              if (b._nrUsed > 0) {
                return false;
              }
            }
            return true;
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the hash array's memory usage
////////////////////////////////////////////////////////////////////////////////

          size_t memoryUsage () const {
            size_t sum = 0;
            for (auto& b : _buckets) {
              sum += static_cast<size_t>(b._nrAlloc * sizeof(Element*));
            }
            return sum;
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the number of elements in the hash
////////////////////////////////////////////////////////////////////////////////

          size_t size () const {
            size_t sum = 0;
            for (auto& b : _buckets) {
              sum += static_cast<size_t>(b._nrUsed);
            }
            return sum;
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the hash table
////////////////////////////////////////////////////////////////////////////////

          int resize (size_t size) {
            for (auto& b : _buckets) {
              try {
                resizeInternal(b,
                    (uint64_t) (3 * size / 2 + 1) / _buckets.size(), 
                    false);
              }
              catch (...) {
                return TRI_ERROR_OUT_OF_MEMORY;
              }
            }
            return TRI_ERROR_NO_ERROR;
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief Appends information about statistics in the given json.
////////////////////////////////////////////////////////////////////////////////
          
          void appendToJson (TRI_memory_zone_t* zone, triagens::basics::Json& json) {
            triagens::basics::Json bkts(zone, triagens::basics::Json::Array);
            for (auto& b : _buckets) {
              triagens::basics::Json bucketInfo(zone, triagens::basics::Json::Object);
              bucketInfo("nrAlloc", triagens::basics::Json(static_cast<double>(b._nrAlloc)));
              bucketInfo("nrUsed", triagens::basics::Json(static_cast<double>(b._nrUsed)));
              bkts.add(bucketInfo);
            }
            json("buckets", bkts);
            json("nrBuckets", triagens::basics::Json(static_cast<double>(_buckets.size())));
            json("totalUsed", triagens::basics::Json(static_cast<double>(size())));
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an element equal to the given element.
////////////////////////////////////////////////////////////////////////////////

          Element* find (Element const* element) const {
            uint64_t i = _hashElement(element);
            Bucket const& b = _buckets[i & _bucketsMask];

            uint64_t const n = b._nrAlloc;
            i = i % n;
            uint64_t k = i;

            for (; i < n && b._table[i] != nullptr && 
                ! _isEqualElementElement(element, b._table[i]); ++i);
            if (i == n) {
              for (i = 0; i < k && b._table[i] != nullptr && 
                  ! _isEqualElementElement(element, b._table[i]); ++i);
            }

            // ...........................................................................
            // return whatever we found, this is nullptr if the thing was not found
            // and otherwise a valid pointer
            // ...........................................................................

            return b._table[i];
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an element given a key, returns NULL if not found
////////////////////////////////////////////////////////////////////////////////

          Element* findByKey (Key const* key) const {
            uint64_t hash = _hashKey(key);
            uint64_t i = hash;
            Bucket const& b = _buckets[i & _bucketsMask];

            uint64_t const n = b._nrAlloc;
            i = i % n;
            uint64_t k = i;

            for (; i < n && b._table[i] != nullptr && 
                ! _isEqualKeyElement(key, hash, b._table[i]); ++i);
            if (i == n) {
              for (i = 0; i < k && b._table[i] != nullptr && 
                  ! _isEqualKeyElement(key, hash, b._table[i]); ++i);
            }

            // ...........................................................................
            // return whatever we found, this is nullptr if the thing was not found
            // and otherwise a valid pointer
            // ...........................................................................

            return b._table[i];
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the array
////////////////////////////////////////////////////////////////////////////////

          int insert (Key const* key,
                      Element* element,
                      bool isRollback) {

            uint64_t hash = _hashKey(key);
            Bucket& b = _buckets[hash & _bucketsMask];

            if (! checkResize(b, 0)) {
              return TRI_ERROR_OUT_OF_MEMORY;
            }

            return doInsert(key, element, b, hash);
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief adds multiple elements to the array
////////////////////////////////////////////////////////////////////////////////

          int batchInsert (std::vector<std::pair<Key const*, Element const*>> const* data,
                           size_t numThreads) {

            std::atomic<int> res(TRI_ERROR_NO_ERROR);
            std::vector<std::pair<Key const*, Element const*>> const& elements = *(data);

            if (elements.size() < numThreads) {
              numThreads = elements.size();
            }
            if (numThreads > _buckets.size()) {
              numThreads = _buckets.size();
            }

            size_t const chunkSize = elements.size() / numThreads;

            typedef std::vector<std::pair<std::pair<Key const*, Element const*>, uint64_t>> DocumentsPerBucket;
            triagens::basics::Mutex bucketMapLocker;

            std::unordered_map<uint64_t, std::vector<DocumentsPerBucket>> allBuckets;

            // partition the work into some buckets
            {
              auto partitioner = [&] (size_t lower, size_t upper) -> void {
                try {
                  std::unordered_map<uint64_t, DocumentsPerBucket> partitions;

                  for (size_t i = lower; i < upper; ++i) {
                    uint64_t hash = _hashKey(elements[i].first);
                    auto bucketId = hash & _bucketsMask;

                    auto it = partitions.find(bucketId);

                    if (it == partitions.end()) {
                      it = partitions.emplace(bucketId, DocumentsPerBucket()).first;
                    }

                    (*it).second.emplace_back(std::make_pair(elements[i], hash));
                  }

                  // transfer ownership to the central map
                  MUTEX_LOCKER(bucketMapLocker);

                  for (auto& it : partitions) {
                    auto it2 = allBuckets.find(it.first);

                    if (it2 == allBuckets.end()) {
                      it2 = allBuckets.emplace(it.first, std::vector<DocumentsPerBucket>()).first;
                    }

                    (*it2).second.emplace_back(std::move(it.second));
                  }
                }
                catch (...) {
                  res = TRI_ERROR_INTERNAL;
                }
              };

              std::vector<std::thread> threads;
              threads.reserve(numThreads);

              try {
                for (size_t i = 0; i < numThreads; ++i) {
                  size_t lower = i * chunkSize;
                  size_t upper = (i + 1) * chunkSize;

                  if (i + 1 == numThreads) {
                    // last chunk. account for potential rounding errors
                    upper = elements.size();
                  }
                  else if (upper > elements.size()) {
                    upper = elements.size();
                  }

                  threads.emplace_back(std::thread(partitioner, lower, upper));
                }
              }
              catch (...) {
                res = TRI_ERROR_INTERNAL;
              }

              for (size_t i = 0; i < threads.size(); ++i) {
                // must join threads, otherwise the program will crash
                threads[i].join();
              }
            }

            if (res.load() != TRI_ERROR_NO_ERROR) {
              return res.load();
            }

            // now the data is partitioned...

            // now insert the bucket data in parallel
            {
              auto inserter = [&] (size_t chunk) -> void {
                try {
                  for (auto const& it : allBuckets) {
                    uint64_t bucketId = it.first;

                    if (bucketId % numThreads != chunk) {
                      // we're not responsible for this bucket!
                      continue;
                    }

                    // we're responsible for this bucket!
                    Bucket& b = _buckets[bucketId];
                    uint64_t expected = 0;

                    for (auto const& it2 : it.second) {
                      expected += it2.size();
                    }

                    if (! checkResize(b, expected)) {
                      res = TRI_ERROR_OUT_OF_MEMORY;
                      return;
                    }
                    
                    for (auto const& it2 : it.second) {
                      for (auto const& it3 : it2) {
                        doInsert(it3.first.first, const_cast<Element*>(it3.first.second), b, it3.second);
                      }
                    }
                  }
                }
                catch (...) {
                  res = TRI_ERROR_INTERNAL;
                }
              };

              std::vector<std::thread> threads;
              threads.reserve(numThreads);

              try {
                for (size_t i = 0; i < numThreads; ++i) {
                  threads.emplace_back(std::thread(inserter, i));
                }
              }
              catch (...) {
                res = TRI_ERROR_INTERNAL;
              }

              for (size_t i = 0; i < threads.size(); ++i) {
                // must join threads, otherwise the program will crash
                threads[i].join();
              }
            }

            return res.load();
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief helper to heal a hole where we deleted something
////////////////////////////////////////////////////////////////////////////////

          void healHole (Bucket& b, uint64_t i) {

            // ...........................................................................
            // remove item - destroy any internal memory associated with the 
            // element structure
            // ...........................................................................

            b._table[i] = nullptr;
            b._nrUsed--;

            uint64_t const n = b._nrAlloc;

            // ...........................................................................
            // and now check the following places for items to move closer together
            // so that there are no gaps in the array
            // ...........................................................................

            uint64_t k = TRI_IncModU64(i, n);

            while (b._table[k] != nullptr) {
              uint64_t j = _hashElement(b._table[k]) % n;

              if ((i < k && ! (i < j && j <= k)) || (k < i && ! (i < j || j <= k))) {
                b._table[i] = b._table[k];
                b._table[k] = nullptr;
                i = k;
              }

              k = TRI_IncModU64(k, n);
            }

            if (b._nrUsed == 0) {
              resizeInternal(b, initialSize(), true);
            }

          }

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array based on its key,
/// returns nullptr if the element
/// was not found and the old value, if it was successfully removed
////////////////////////////////////////////////////////////////////////////////

          Element* removeByKey (Key const* key) {
            uint64_t hash = _hashKey(key);
            uint64_t i = hash;
            Bucket& b = _buckets[i & _bucketsMask];

            uint64_t const n = b._nrAlloc;
            i = i % n;
            uint64_t k = i;

            for (; i < n && b._table[i] != nullptr && 
                ! _isEqualKeyElement(key, hash, b._table[i]); ++i);
            if (i == n) {
              for (i = 0; i < k && b._table[i] != nullptr && 
                  ! _isEqualKeyElement(key, hash, b._table[i]); ++i);
            }

            Element* old = b._table[i];

            if (old != nullptr) {
              healHole(b, i);
            }
            return old;
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array, returns nullptr if the element
/// was not found and the old value, if it was successfully removed
////////////////////////////////////////////////////////////////////////////////

          Element* remove (Element const* element) {
            uint64_t i = _hashElement(element);
            Bucket& b = _buckets[i & _bucketsMask];

            uint64_t const n = b._nrAlloc;
            i = i % n;
            uint64_t k = i;

            for (; i < n && b._table[i] != nullptr && 
                ! _isEqualElementElement(element, b._table[i]); ++i);
            if (i == n) {
              for (i = 0; i < k && b._table[i] != nullptr && 
                  ! _isEqualElementElement(element, b._table[i]); ++i);
            }

            Element* old = b._table[i];

            if (old != nullptr) {
              healHole(b, i);
            }

            return old;
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief a method to iterate over all elements in the hash
////////////////////////////////////////////////////////////////////////////////

          void invokeOnAllElements (CallbackElementFuncType callback) {
            for (auto& b : _buckets) {
              if (b._table != nullptr) {
                for (size_t i = 0; i < b._nrAlloc; ++i) {
                  if (b._table[i] != nullptr) {
                    callback(b._table[i]);
                  }
                }
              }
            }
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief a method to iterate over all elements in the index in
///        a sequential order.
///        Returns nullptr if all documents have been returned.
///        Convention: position === 0 indicates a new start.
////////////////////////////////////////////////////////////////////////////////

          Element* findSequential (uint64_t& position,
                                   uint64_t& total) const {
            if (position == 0) {
              if (isEmpty()) {
                return nullptr;
              }
              // Fill Total
              total = 0;
              for (auto& b : _buckets) {
                total += b._nrAlloc;
              }
              TRI_ASSERT(total > 0);
            }

            Element* res = nullptr;
            do {
              res = findElementSequentialBucktes(position);
              position++;
            } 
            while (position < total && res == nullptr);

            return res;
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief a method to iterate over all elements in the index in
///        reversed sequential order.
///        Returns nullptr if all documents have been returned.
///        Convention: position === UINT64_MAX indicates a new start.
////////////////////////////////////////////////////////////////////////////////

          Element* findSequentialReverse (uint64_t& position) const {
            if (position == UINT64_MAX) {
              if (isEmpty()) {
                return nullptr;
              }

              // Fill Total
              uint64_t position = 0;
              for (auto& b : _buckets) {
                position += b._nrAlloc;
              }
              TRI_ASSERT(position > 0);
            }

            Element* res = nullptr;
            do {
              res = findElementSequentialBucktes(position);
              position--;
            } 
            while (position > 0 && res == nullptr);

            return res;
          }

////////////////////////////////////////////////////////////////////////////////
/// @brief a method to iterate over all elements in the index in
///        a random order.
///        Returns nullptr if all documents have been returned.
///        Convention: *step === 0 indicates a new start.
////////////////////////////////////////////////////////////////////////////////

          Element* findRandom (uint64_t& initialPosition,
                               uint64_t& position,
                               uint64_t& step,
                               uint64_t& total) const {
            if (step != 0 && position == initialPosition) {
              // already read all documents
              return nullptr;
            }
            if (step == 0) {
              // Initialize
              if (isEmpty()) {
                return nullptr;
              }
              total = 0;
              for (auto& b : _buckets) {
                total += b._nrAlloc;
              }
              TRI_ASSERT(total > 0);

              // find a co-prime for total
              while (true) {
                step = TRI_UInt32Random() % total;
                if (step > 10 && triagens::basics::binaryGcd<uint64_t>(total, step) == 1) {
                  while (initialPosition == 0) {
                    initialPosition = TRI_UInt32Random() % total;
                  }
                  position = initialPosition;
                  break;
                }
              }
            }

            // Find documents
            Element* res = nullptr; 
            do {
              res = findElementSequentialBucktes(position);
              position += step;
              position = position % total;
            } 
            while (initialPosition != position && res == nullptr);

            return res;
          }

      };
  } // namespace basics
} // namespace triagens

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
