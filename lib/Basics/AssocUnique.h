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
/// @author Dr. Frank Celler
/// @author Martin Schoenert
/// @author Michael Hackstein
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_ASSOC_UNIQUE_H
#define ARANGODB_BASICS_ASSOC_UNIQUE_H 1

#include "Basics/AssocUniqueHelpers.h"
#include "Basics/Common.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>
#include <thread>

#include "Basics/AssocHelpers.h"
#include "Basics/IndexBucket.h"
#include "Basics/LocalTaskQueue.h"
#include "Basics/MutexLocker.h"
#include "Basics/PerformanceLogScope.h"
#include "Basics/gcd.h"
#include "Basics/prime-numbers.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"

namespace arangodb {
namespace basics {

////////////////////////////////////////////////////////////////////////////////
/// @brief associative array
////////////////////////////////////////////////////////////////////////////////

template <class Key, class Element, class AssocUniqueHelper>
class AssocUnique {
 private:
  typedef void UserData;
  typedef arangodb::basics::BucketPosition BucketPosition;

 public:
  typedef std::function<bool(Element&)> CallbackElementFuncType;

  typedef arangodb::basics::IndexBucket<Element, uint64_t> Bucket;

 private:
  AssocUniqueHelper _helper;
  std::vector<Bucket> _buckets;
  size_t _bucketsMask;

  std::function<std::string()> _contextCallback;

 public:
  AssocUnique(AssocUniqueHelper&& helper, size_t numberBuckets = 1,
              std::function<std::string()> contextCallback = []() -> std::string {
                return "";
              })
      : _helper(std::move(helper)), _contextCallback(contextCallback) {
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

    _buckets.resize(numberBuckets);

    try {
      for (size_t j = 0; j < numberBuckets; j++) {
        _buckets[j].allocate(initialSize());
      }
    } catch (...) {
      _buckets.clear();
      throw;
    }
  }

  ~AssocUnique() { _buckets.clear(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief adhere to the rule of five
  //////////////////////////////////////////////////////////////////////////////

  AssocUnique(AssocUnique const&) = delete;             // copy constructor
  AssocUnique(AssocUnique&&) = delete;                  // move constructor
  AssocUnique& operator=(AssocUnique const&) = delete;  // op =
  AssocUnique& operator=(AssocUnique&&) = delete;       // op =

  //////////////////////////////////////////////////////////////////////////////
  /// @brief initial preallocation size of the hash table when the table is
  /// first created
  /// setting this to a high value will waste memory but reduce the number of
  /// reallocations/repositionings necessary when the table grows
  //////////////////////////////////////////////////////////////////////////////

 private:
  static uint64_t initialSize() { return 251; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief resizes the array
  //////////////////////////////////////////////////////////////////////////////

  void resizeInternal(UserData* userData, Bucket& b, uint64_t targetSize, bool allowShrink) {
    if (b._nrAlloc > targetSize && !allowShrink) {
      return;
    }
    if (allowShrink && b._nrAlloc >= targetSize && b._nrAlloc < 1.25 * targetSize) {
      // no need to shrink
      return;
    }

    std::string const cb(_contextCallback());

    TRI_ASSERT(targetSize > 0);
    targetSize = TRI_NearPrime(targetSize);

    PerformanceLogScope logScope(std::string("unique hash-resize ") + cb +
                                 ", target size: " + std::to_string(targetSize));

    Bucket copy;
    copy.allocate(targetSize);

    if (b._nrUsed > 0) {
      Element* oldTable = b._table;
      uint64_t const oldAlloc = b._nrAlloc;
      TRI_ASSERT(oldAlloc > 0);

      uint64_t const n = copy._nrAlloc;
      TRI_ASSERT(n > 0);

      for (uint64_t j = 0; j < oldAlloc; j++) {
        Element const& element = oldTable[j];

        if (element) {
          uint64_t i, k;
          i = k = _helper.HashElement(element, true) % n;

          for (; i < n && copy._table[i]; ++i)
            ;
          if (i == n) {
            for (i = 0; i < k && copy._table[i]; ++i)
              ;
          }

          copy._table[i] = element;
          ++copy._nrUsed;
        }
      }
    }

    b = std::move(copy);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief check a resize of the hash array
  //////////////////////////////////////////////////////////////////////////////

  bool checkResize(UserData* userData, Bucket& b, uint64_t expected) {
    if (2 * b._nrAlloc < 3 * (b._nrUsed + expected)) {
      try {
        resizeInternal(userData, b, 2 * (b._nrAlloc + expected) + 1, false);
      } catch (...) {
        return false;
      }
    }
    return true;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Finds the element at the given position in the buckets.
  ///        Iterates using the given step size
  //////////////////////////////////////////////////////////////////////////////

  Element findElementSequentialBucketsRandom(UserData* userData, BucketPosition& position,
                                             uint64_t const step,
                                             BucketPosition const& initial) const {
    Element found;
    Bucket const* b = &_buckets[position.bucketId];
    do {
      found = b->_table[position.position];
      position.position += step;
      while (position.position >= b->_nrAlloc) {
        position.position -= b->_nrAlloc;
        position.bucketId = (position.bucketId + 1) % _buckets.size();
        b = &_buckets[position.bucketId];
      }
      if (position == initial) {
        // We are done. Return the last element we have in hand
        return found;
      }
    } while (!found);
    return found;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Insert a document into the given bucket
  ///        This does not resize and expects to have enough space
  //////////////////////////////////////////////////////////////////////////////

  int doInsert(UserData* userData, Element const& element, Bucket& b, uint64_t hash) {
    uint64_t const n = b._nrAlloc;
    uint64_t i = hash % n;
    uint64_t k = i;

    for (; i < n && b._table[i] &&
           !_helper.IsEqualElementElementByKey(userData, element, b._table[i]);
         ++i)
      ;
    if (i == n) {
      for (i = 0; i < k && b._table[i] &&
                  !_helper.IsEqualElementElementByKey(userData, element, b._table[i]);
           ++i)
        ;
    }

    Element const& arrayElement = b._table[i];

    if (arrayElement) {
      return TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED;
    }

    b._table[i] = element;
    b._nrUsed++;

    return TRI_ERROR_NO_ERROR;
  }

 public:
  void truncate(CallbackElementFuncType callback) {
    for (auto& b : _buckets) {
      if (callback) {
        invokeOnAllElements(callback, b);
      }
      b.deallocate();
      b.allocate(initialSize());
    }
  }

  size_t buckets() const { return _buckets.size(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks if this index is empty
  //////////////////////////////////////////////////////////////////////////////

  bool isEmpty() const {
    for (auto& b : _buckets) {
      if (b._nrUsed > 0) {
        return false;
      }
    }
    return true;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the hash array's memory usage
  //////////////////////////////////////////////////////////////////////////////

  size_t memoryUsage() const {
    size_t res = 0;
    for (auto& b : _buckets) {
      res += b.memoryUsage();
    }
    return res;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the number of elements in the hash
  //////////////////////////////////////////////////////////////////////////////

  size_t size() const {
    size_t sum = 0;
    for (auto& b : _buckets) {
      sum += static_cast<size_t>(b._nrUsed);
    }
    return sum;
  }

  size_t capacity() const {
    size_t sum = 0;
    for (auto& b : _buckets) {
      sum += static_cast<size_t>(b._nrAlloc);
    }
    return sum;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief resizes the hash table
  //////////////////////////////////////////////////////////////////////////////

  int resize(UserData* userData, size_t size) {
    size /= _buckets.size();
    for (auto& b : _buckets) {
      if (2 * (2 * size + 1) < 3 * b._nrUsed) {
        return TRI_ERROR_BAD_PARAMETER;
      }

      try {
        resizeInternal(userData, b, 2 * size + 1, false);
      } catch (...) {
        return TRI_ERROR_OUT_OF_MEMORY;
      }
    }
    return TRI_ERROR_NO_ERROR;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Appends information about statistics in the given VPackBuilder
  //////////////////////////////////////////////////////////////////////////////

  void appendToVelocyPack(VPackBuilder& builder) {
    TRI_ASSERT(builder.isOpenObject());
    builder.add("buckets", VPackValue(VPackValueType::Array));
    for (auto& b : _buckets) {
      builder.openObject();
      builder.add("nrAlloc", VPackValue(b._nrAlloc));
      builder.add("nrUsed", VPackValue(b._nrUsed));
      builder.close();
    }
    builder.close();  // buckets
    builder.add("nrBuckets", VPackValue(_buckets.size()));
    builder.add("totalUsed", VPackValue(size()));
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief finds an element equal to the given element.
  //////////////////////////////////////////////////////////////////////////////

  Element find(UserData* userData, Element const& element) const {
    uint64_t i = _helper.HashElement(element, true);
    Bucket const& b = _buckets[i & _bucketsMask];

    uint64_t const n = b._nrAlloc;
    i = i % n;
    uint64_t k = i;

    for (; i < n && b._table[i] &&
           !_helper.IsEqualElementElementByKey(userData, element, b._table[i]);
         ++i)
      ;
    if (i == n) {
      for (i = 0; i < k && b._table[i] &&
                  !_helper.IsEqualElementElementByKey(userData, element, b._table[i]);
           ++i)
        ;
    }

    // ...........................................................................
    // return whatever we found, this is nullptr if the thing was not found
    // and otherwise a valid pointer
    // ...........................................................................

    return b._table[i];
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief finds an element given a key, returns a default-constructed Element
  /// if not found
  //////////////////////////////////////////////////////////////////////////////

  Element findByKey(UserData* userData, Key const* key) const {
    uint64_t hash = _helper.HashKey(key);
    uint64_t i = hash;
    uint64_t bucketId = i & _bucketsMask;
    Bucket const& b = _buckets[static_cast<size_t>(bucketId)];

    uint64_t const n = b._nrAlloc;
    i = i % n;
    uint64_t k = i;

    for (; i < n && b._table[i] && !_helper.IsEqualKeyElement(userData, key, b._table[i]); ++i)
      ;
    if (i == n) {
      for (i = 0; i < k && b._table[i] &&
                  !_helper.IsEqualKeyElement(userData, key, b._table[i]);
           ++i)
        ;
    }

    // ...........................................................................
    // return whatever we found, this is nullptr if the thing was not found
    // and otherwise a valid pointer
    // ...........................................................................

    return b._table[i];
  }

  Element* findByKeyRef(UserData* userData, Key const* key) const {
    uint64_t hash = _helper.HashKey(key);
    uint64_t i = hash;
    uint64_t bucketId = i & _bucketsMask;
    Bucket const& b = _buckets[static_cast<size_t>(bucketId)];

    uint64_t const n = b._nrAlloc;
    i = i % n;
    uint64_t k = i;

    for (; i < n && b._table[i] && !_helper.IsEqualKeyElement(userData, key, b._table[i]); ++i)
      ;
    if (i == n) {
      for (i = 0; i < k && b._table[i] &&
                  !_helper.IsEqualKeyElement(userData, key, b._table[i]);
           ++i)
        ;
    }

    // ...........................................................................
    // return whatever we found, this is nullptr if the thing was not found
    // and otherwise a valid pointer
    // ...........................................................................

    return &b._table[i];
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief finds an element given a key, returns a default-constructed Element
  /// if not found
  /// also returns the internal hash value and the bucket position the element
  /// was found at (or would be placed into)
  //////////////////////////////////////////////////////////////////////////////

  Element findByKey(UserData* userData, Key const* key,
                    BucketPosition& position, uint64_t& hash) const {
    hash = _helper.HashKey(key);
    uint64_t i = hash;
    uint64_t bucketId = i & _bucketsMask;
    Bucket const& b = _buckets[static_cast<size_t>(bucketId)];

    uint64_t const n = b._nrAlloc;
    i = i % n;
    uint64_t k = i;

    for (; i < n && b._table[i] && !_helper.IsEqualKeyElement(userData, key, b._table[i]); ++i)
      ;
    if (i == n) {
      for (i = 0; i < k && b._table[i] &&
                  !_helper.IsEqualKeyElement(userData, key, b._table[i]);
           ++i)
        ;
    }

    // if requested, pass the position of the found element back
    // to the caller
    position.bucketId = static_cast<size_t>(bucketId);
    position.position = i;

    // ...........................................................................
    // return whatever we found, this is nullptr if the thing was not found
    // and otherwise a valid pointer
    // ...........................................................................

    return b._table[i];
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief adds an element to the array
  //////////////////////////////////////////////////////////////////////////////

  int insert(UserData* userData, Element const& element) {
    uint64_t hash = _helper.HashElement(element, true);
    Bucket& b = _buckets[hash & _bucketsMask];

    if (!checkResize(userData, b, 0)) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    return doInsert(userData, element, b, hash);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief adds an element to the array, at the specified position
  /// the caller must have calculated the correct position before.
  /// the caller must also have checked that the bucket still has some reserve
  /// space.
  /// if the method returns TRI_ERROR_UNIQUE_CONSTRAINT_VIOLATED, the element
  /// was not inserted. if it returns TRI_ERROR_OUT_OF_MEMORY, the element was
  /// inserted, but resizing afterwards failed!
  //////////////////////////////////////////////////////////////////////////////

  int insertAtPosition(UserData* userData, Element const& element,
                       BucketPosition const& position) {
    Bucket& b = _buckets[position.bucketId];
    Element const& arrayElement = b._table[position.position];

    if (arrayElement) {
      return TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED;
    }

    b._table[position.position] = element;
    b._nrUsed++;

    if (!checkResize(userData, b, 0)) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    return TRI_ERROR_NO_ERROR;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief adds multiple elements to the array
  //////////////////////////////////////////////////////////////////////////////

  void batchInsert(std::function<void*()> const& contextCreator,
                   std::function<void(void*)> const& contextDestroyer,
                   std::shared_ptr<std::vector<Element> const> data,
                   std::shared_ptr<arangodb::basics::LocalTaskQueue> queue) {
    TRI_ASSERT(queue != nullptr);
    if (data->empty()) {
      // nothing to do
      return;
    }

    std::vector<Element> const& elements = *(data.get());

    // set number of partitioners sensibly
    size_t numThreads = _buckets.size();
    if (elements.size() < numThreads) {
      numThreads = elements.size();
    }

    size_t const chunkSize = elements.size() / numThreads;

    typedef std::vector<std::pair<Element, uint64_t>> DocumentsPerBucket;
    typedef UniqueInserterTask<Element> Inserter;
    typedef UniquePartitionerTask<Element> Partitioner;

    // allocate working space and coordination tools for tasks

    std::shared_ptr<std::vector<arangodb::Mutex>> bucketMapLocker;
    bucketMapLocker.reset(new std::vector<arangodb::Mutex>(_buckets.size()));

    std::shared_ptr<std::vector<std::atomic<size_t>>> bucketFlags;
    bucketFlags.reset(new std::vector<std::atomic<size_t>>(_buckets.size()));
    for (size_t i = 0; i < bucketFlags->size(); i++) {
      (*bucketFlags)[i] = numThreads;
    }

    std::shared_ptr<std::vector<std::shared_ptr<Inserter>>> inserters;
    inserters.reset(new std::vector<std::shared_ptr<Inserter>>);
    inserters->reserve(_buckets.size());

    std::shared_ptr<std::vector<std::vector<DocumentsPerBucket>>> allBuckets;
    allBuckets.reset(new std::vector<std::vector<DocumentsPerBucket>>(_buckets.size()));

    auto doInsertBinding = [&](UserData* userData, Element const& element,
                               Bucket& b, uint64_t hashByKey) -> int {
      return doInsert(userData, element, b, hashByKey);
    };
    auto checkResizeBinding = [&](UserData* userData, Bucket& b, uint64_t expected) -> bool {
      return checkResize(userData, b, expected);
    };

    try {
      // generate inserter tasks to be dispatched later by partitioners
      for (size_t i = 0; i < allBuckets->size(); i++) {
        std::shared_ptr<Inserter> worker;
        worker.reset(new Inserter(queue, contextDestroyer, &_buckets, doInsertBinding,
                                  checkResizeBinding, i, contextCreator(), allBuckets));
        inserters->emplace_back(worker);
      }
      // queue partitioner tasks
      for (size_t i = 0; i < numThreads; ++i) {
        size_t lower = i * chunkSize;
        size_t upper = (i + 1) * chunkSize;

        if (i + 1 == numThreads) {
          // last chunk. account for potential rounding errors
          upper = elements.size();
        } else if (upper > elements.size()) {
          upper = elements.size();
        }

        std::shared_ptr<Partitioner> worker;
        worker.reset(new Partitioner(queue, AssocUniqueHelper::HashElement, contextDestroyer,
                                     data, lower, upper, contextCreator(), bucketFlags,
                                     bucketMapLocker, allBuckets, inserters));
        queue->enqueue(worker);
      }
    } catch (...) {
      queue->setStatus(TRI_ERROR_INTERNAL);
    }
  }
  //////////////////////////////////////////////////////////////////////////////
  /// @brief helper to heal a hole where we deleted something
  //////////////////////////////////////////////////////////////////////////////

  void healHole(UserData* userData, Bucket& b, uint64_t i) {
    //
    // remove item - destroy any internal memory associated with the
    // element structure
    //

    b._table[i] = Element();
    b._nrUsed--;

    uint64_t const n = b._nrAlloc;

    //
    // and now check the following places for items to move closer together
    // so that there are no gaps in the array
    //

    uint64_t k = TRI_IncModU64(i, n);

    while (b._table[k]) {
      uint64_t j = _helper.HashElement(b._table[k], true) % n;

      if ((i < k && !(i < j && j <= k)) || (k < i && !(i < j || j <= k))) {
        b._table[i] = b._table[k];
        b._table[k] = Element();
        i = k;
      }

      k = TRI_IncModU64(k, n);
    }

    if (b._nrUsed == 0) {
      resizeInternal(userData, b, initialSize(), true);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief removes an element from the array based on its key,
  /// returns nullptr if the element
  /// was not found and the old value, if it was successfully removed
  //////////////////////////////////////////////////////////////////////////////

  Element removeByKey(UserData* userData, Key const* key) {
    uint64_t hash = _helper.HashKey(key);
    uint64_t i = hash;
    Bucket& b = _buckets[i & _bucketsMask];

    uint64_t const n = b._nrAlloc;
    i = i % n;
    uint64_t k = i;

    for (; i < n && b._table[i] && !_helper.IsEqualKeyElement(userData, key, b._table[i]); ++i)
      ;
    if (i == n) {
      for (i = 0; i < k && b._table[i] &&
                  !_helper.IsEqualKeyElement(userData, key, b._table[i]);
           ++i)
        ;
    }

    Element old = b._table[i];

    if (old) {
      healHole(userData, b, i);
    }
    return old;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief removes an element from the array, returns nullptr if the element
  /// was not found and the old value, if it was successfully removed
  //////////////////////////////////////////////////////////////////////////////

  Element remove(UserData* userData, Element const& element) {
    uint64_t i = _helper.HashElement(element, true);
    Bucket& b = _buckets[i & _bucketsMask];

    uint64_t const n = b._nrAlloc;
    i = i % n;
    uint64_t k = i;

    for (; i < n && b._table[i] &&
           !_helper.IsEqualElementElement(userData, element, b._table[i]);
         ++i)
      ;
    if (i == n) {
      for (i = 0; i < k && b._table[i] &&
                  !_helper.IsEqualElementElement(userData, element, b._table[i]);
           ++i)
        ;
    }

    Element old = b._table[i];

    if (old) {
      healHole(userData, b, i);
    }

    return old;
  }

  /// @brief a method to iterate over all elements in the hash. this method
  /// can NOT be used for deleting elements
  void invokeOnAllElements(CallbackElementFuncType const& callback) {
    for (auto& b : _buckets) {
      if (b._table == nullptr) {
        continue;
      }
      if (!invokeOnAllElements(callback, b)) {
        return;
      }
    }
  }

  /// @brief a method to iterate over all elements in a bucket. this method
  /// can NOT be used for deleting elements
  bool invokeOnAllElements(CallbackElementFuncType const& callback, Bucket& b) {
    if (b._nrUsed > 0) {
      for (size_t i = 0; i < b._nrAlloc; ++i) {
        if (!b._table[i]) {
          continue;
        }
        if (!callback(b._table[i])) {
          return false;
        }
        if (b._nrUsed == 0) {
          break;
        }
      }
    }
    return true;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief a method to iterate over all elements in the hash. this method
  /// can be used for deleting elements as well
  //////////////////////////////////////////////////////////////////////////////

  void invokeOnAllElementsForRemoval(CallbackElementFuncType callback) {
    for (auto& b : _buckets) {
      if (b._table == nullptr || b._nrUsed == 0) {
        continue;
      }
      for (size_t i = 0; i < b._nrAlloc; /* no hoisting */) {
        if (!b._table[i]) {
          ++i;
          continue;
        }
        Element old = b._table[i];
        if (!callback(b._table[i])) {
          return;
        }
        if (b._nrUsed == 0) {
          break;
        }
        if (b._table[i] == old) {
          ++i;
        }
      }
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief a method to iterate over all elements in the index in
  ///        a sequential order.
  ///        Returns nullptr if all documents have been returned.
  ///        Convention: position.bucketId == SIZE_MAX indicates a new start.
  ///        Convention: position.bucketId == SIZE_MAX - 1 indicates a restart.
  ///        During a continue the total will not be modified.
  //////////////////////////////////////////////////////////////////////////////

  Element findSequential(UserData* userData, BucketPosition& position, uint64_t& total) const {
    if (position.bucketId >= _buckets.size()) {
      // bucket id is out of bounds. now handle edge cases
      if (position.bucketId < SIZE_MAX - 1) {
        return Element();
      }

      if (position.bucketId == SIZE_MAX) {
        // first call, now fill total
        total = 0;
        for (auto const& b : _buckets) {
          total += b._nrUsed;
        }

        if (total == 0) {
          return Element();
        }

        TRI_ASSERT(total > 0);
      }

      position.bucketId = 0;
      position.position = 0;
    }

    while (true) {
      Bucket const& b = _buckets[position.bucketId];
      uint64_t const n = b._nrAlloc;

      for (; position.position < n && !b._table[position.position]; ++position.position)
        ;

      if (position.position != n) {
        // found an element
        Element found = b._table[position.position];

        // move forward the position indicator one more time
        if (++position.position == n) {
          position.position = 0;
          ++position.bucketId;
        }

        return found;
      }

      // reached end
      position.position = 0;
      if (++position.bucketId >= _buckets.size()) {
        // Indicate we are done
        return Element();
      }
      // continue iteration with next bucket
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief a method to iterate over all elements in the index in
  ///        reversed sequential order.
  ///        Returns nullptr if all documents have been returned.
  ///        Convention: position === UINT64_MAX indicates a new start.
  //////////////////////////////////////////////////////////////////////////////

  Element findSequentialReverse(UserData* userData, BucketPosition& position) const {
    if (position.bucketId >= _buckets.size()) {
      // bucket id is out of bounds. now handle edge cases
      if (position.bucketId < SIZE_MAX - 1) {
        return Element();
      }

      if (position.bucketId == SIZE_MAX && isEmpty()) {
        return Element();
      }

      position.bucketId = _buckets.size() - 1;
      position.position = _buckets[position.bucketId]._nrAlloc - 1;
    }

    Bucket const* b = &_buckets[position.bucketId];
    Element found;
    do {
      found = b->_table[position.position];

      if (position.position == 0) {
        if (position.bucketId == 0) {
          // Indicate we are done
          position.bucketId = _buckets.size();
          return Element();
        }

        --position.bucketId;
        b = &_buckets[position.bucketId];
        position.position = b->_nrAlloc - 1;
      } else {
        --position.position;
      }
    } while (!found);

    return found;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief a method to iterate over all elements in the index in
  ///        a random order.
  ///        Returns nullptr if all documents have been returned.
  ///        Convention: *step === 0 indicates a new start.
  //////////////////////////////////////////////////////////////////////////////

  Element findRandom(UserData* userData, BucketPosition& initialPosition,
                     BucketPosition& position, uint64_t& step, uint64_t& total) const {
    if (step != 0 && position == initialPosition) {
      // already read all documents
      return Element();
    }
    if (step == 0) {
      // Initialize
      uint64_t used = 0;
      total = 0;
      for (auto& b : _buckets) {
        total += b._nrAlloc;
        used += b._nrUsed;
      }
      if (used == 0) {
        return Element();
      }
      TRI_ASSERT(total > 0);

      // find a co-prime for total
      while (true) {
        step = RandomGenerator::interval(UINT32_MAX) % total;
        if (step > 10 && arangodb::basics::binaryGcd<uint64_t>(total, step) == 1) {
          uint64_t initialPositionNr = 0;
          while (initialPositionNr == 0) {
            initialPositionNr = RandomGenerator::interval(UINT32_MAX) % total;
          }
          for (size_t i = 0; i < _buckets.size(); ++i) {
            if (initialPositionNr < _buckets[i]._nrAlloc) {
              position.bucketId = i;
              position.position = initialPositionNr;
              initialPosition.bucketId = i;
              initialPosition.position = initialPositionNr;
              break;
            }
            initialPositionNr -= _buckets[i]._nrAlloc;
          }
          break;
        }
      }
    }

    return findElementSequentialBucketsRandom(userData, position, step, initialPosition);
  }
};
}  // namespace basics
}  // namespace arangodb

#endif
