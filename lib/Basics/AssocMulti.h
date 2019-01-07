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
/// @author Max Neunhoeffer
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_ASSOC_MULTI_H
#define ARANGODB_BASICS_ASSOC_MULTI_H 1

// Activate for additional debugging:
// #define TRI_CHECK_MULTI_POINTER_HASH 1

#include "Basics/AssocHelpers.h"
#include "Basics/AssocMultiHelpers.h"
#include "Basics/Common.h"
#include "Basics/IndexBucket.h"
#include "Basics/LocalTaskQueue.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/PerformanceLogScope.h"
#include "Basics/prime-numbers.h"
#include "Logger/Logger.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>
#include <thread>

#ifdef TRI_CHECK_MULTI_POINTER_HASH
#include <iostream>
#endif

namespace arangodb {
namespace basics {

////////////////////////////////////////////////////////////////////////////////
/// @brief associative array of pointers, tolerating repeated keys.
///
/// This is a data structure that can store pointers to elements. Each element
/// has a unique key (for example a certain attribute) and multiple
/// elements in the associative array can have the same key. Every element
/// can be at most once in the array.
/// We want to offer constant time complexity for the following
/// operations:
///  - insert pointer to a element into the array
///  - lookup pointer to a element in the array
///  - delete pointer to a element from the array
///  - find one pointer to a element with a given key
/// Furthermore, we want to offer O(n) complexity for the following
/// operation:
///  - find all pointers whose elements have a given key k, where n is
///    the number of elements in the array with this key
/// To this end, we use a hash table and ask the user to provide the following:
///  - a way to hash elements by their keys, and to hash keys themselves,
///  - a way to hash elements by their full identity
///  - a way to compare a key to the key of a given element
///  - a way to compare two elements, either by their keys or by their full
///    identities.
/// To avoid unnecessary comparisons the user can guarantee that s/he will
/// only try to store non-identical elements into the array. This enables
/// the code to skip comparisons which would otherwise be necessary to
/// ensure uniqueness.
/// The idea of the algorithm is as follows: Each slot in the hash table
/// contains a pointer to the actual element, as well as two unsigned
/// integers "prev" and "next" (being indices in the hash table) to
/// organize a linked list of entries, *within the same hash table*. All
/// elements with the same key are kept in a doubly linked list. The first
/// element in such a linked list is kept at the position determined by
/// its hash with respect to its key (or in the first free slot after this
/// position). All further elements in such a linked list are kept at the
/// position determined by its hash with respect to its full identity
/// (or in the first free slot after this position). Provided the hash
/// table is large enough and the hash functions distribute well enough,
/// this gives the proposed complexity.
///
////////////////////////////////////////////////////////////////////////////////

template <class Key, class Element, class IndexType, bool useHashCache, class AssocMultiHelper>
class AssocMulti {
 private:
  typedef void UserData;

 public:
  static IndexType const INVALID_INDEX = ((IndexType)0) - 1;

  typedef std::function<bool(Element&)> CallbackElementFuncType;

 private:
  typedef Entry<Element, IndexType, useHashCache> EntryType;

  typedef arangodb::basics::IndexBucket<EntryType, IndexType, SIZE_MAX> Bucket;

  AssocMultiHelper _helper;
  std::vector<Bucket> _buckets;
  size_t _bucketsMask;

#ifdef TRI_INTERNAL_STATS
  uint64_t _nrFinds;    // statistics: number of lookup calls
  uint64_t _nrAdds;     // statistics: number of insert calls
  uint64_t _nrRems;     // statistics: number of remove calls
  uint64_t _nrResizes;  // statistics: number of resizes

  uint64_t _nrProbes;   // statistics: number of misses in FindElementPlace
                        // and LookupByElement, used by insert, lookup and
                        // remove
  uint64_t _nrProbesF;  // statistics: number of misses while looking up
  uint64_t _nrProbesD;  // statistics: number of misses while removing
#endif

  std::function<std::string()> _contextCallback;
  IndexType _initialSize;

 public:
  AssocMulti(AssocMultiHelper&& helper, size_t numberBuckets = 1, IndexType initialSize = 64,
             std::function<std::string()> contextCallback = []() -> std::string {
               return "";
             })
      : _helper(std::move(helper)),
#ifdef TRI_INTERNAL_STATS
        _nrFinds(0),
        _nrAdds(0),
        _nrRems(0),
        _nrResizes(0),
        _nrProbes(0),
        _nrProbesF(0),
        _nrProbesD(0),
#endif
        _contextCallback(contextCallback),
        _initialSize(initialSize) {

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
        _buckets[j].allocate(_initialSize);
      }
    } catch (...) {
      _buckets.clear();
      throw;
    }
  }

  ~AssocMulti() { _buckets.clear(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the memory used by the hash table
  //////////////////////////////////////////////////////////////////////////////

  size_t memoryUsage() const {
    size_t res = 0;
    for (auto& b : _buckets) {
      res += b.memoryUsage();
    }
    return res;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief size(), return the number of items stored
  //////////////////////////////////////////////////////////////////////////////

  size_t size() const {
    size_t res = 0;
    for (auto& b : _buckets) {
      res += static_cast<size_t>(b._nrUsed);
    }
    return res;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Appends information about statistics in the given VPackBuilder
  //////////////////////////////////////////////////////////////////////////////

  void appendToVelocyPack(VPackBuilder& builder) {
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
  /// @brief capacity(), return the number of allocated items
  //////////////////////////////////////////////////////////////////////////////

  size_t capacity() const {
    size_t res = 0;
    for (auto& b : _buckets) {
      res += static_cast<size_t>(b._nrAlloc);
    }
    return res;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the element at position.
  /// this may return a default-constructed Element if not found
  //////////////////////////////////////////////////////////////////////////////

  Element at(Bucket& b, size_t position) const {
    return b._table[position].value;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief adds a key/element to the array
  //////////////////////////////////////////////////////////////////////////////

  Element insert(UserData* userData, Element const& element, bool overwrite, bool checkEquality) {
    // if the checkEquality flag is not set, we do not check for element
    // equality we use this flag to speed up initial insertion into the
    // index, i.e. when the index is built for a collection and we know
    // for sure no duplicate elements will be inserted

#ifdef TRI_CHECK_MULTI_POINTER_HASH
    check(userData, true, true);
#endif

    // compute the hash by the key only first
    uint64_t hashByKey = _helper.HashElement(element, true);
    Bucket& b = _buckets[hashByKey & _bucketsMask];

    auto result = doInsert(userData, element, hashByKey, b, overwrite, checkEquality);

#ifdef TRI_CHECK_MULTI_POINTER_HASH
    check(userData, true, true);
#endif

    return result;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief adds multiple elements to the array
  //////////////////////////////////////////////////////////////////////////////

  void batchInsert(std::function<void*()> const& contextCreator,
                   std::function<void(void*)> const& contextDestroyer,
                   std::shared_ptr<std::vector<Element> const> data,
                   std::shared_ptr<LocalTaskQueue> queue) {
    if (data->empty()) {
      // nothing to do
      return;
    }

    std::vector<Element> const& elements = *(data.get());

    // set the number of partitioners sensibly
    size_t numThreads = _buckets.size();
    if (elements.size() < numThreads) {
      numThreads = elements.size();
    }

    size_t const chunkSize = elements.size() / numThreads;

    typedef std::vector<std::pair<Element, uint64_t>> DocumentsPerBucket;
    typedef MultiInserterTask<Element, IndexType, useHashCache> Inserter;
    typedef MultiPartitionerTask<Element, IndexType, useHashCache> Partitioner;

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
                               uint64_t hashByKey, Bucket& b, bool const overwrite,
                               bool const checkEquality) -> Element {
      return doInsert(userData, element, hashByKey, b, overwrite, checkEquality);
    };

    try {
      // create inserter tasks to be dispatched later by partitioners
      for (size_t i = 0; i < allBuckets->size(); i++) {
        std::shared_ptr<Inserter> worker;
        worker.reset(new Inserter(queue, contextDestroyer, &_buckets, doInsertBinding,
                                  i, contextCreator(), allBuckets));
        inserters->emplace_back(worker);
      }
      // enqueue partitioner tasks
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
        worker.reset(new Partitioner(queue, AssocMultiHelper::HashElement, contextDestroyer,
                                     data, lower, upper, contextCreator(), bucketFlags,
                                     bucketMapLocker, allBuckets, inserters));
        queue->enqueue(worker);
      }
    } catch (...) {
      queue->setStatus(TRI_ERROR_INTERNAL);
    }

#ifdef TRI_CHECK_MULTI_POINTER_HASH
    {
      auto& checkFn = check;
      auto callback = [&contextCreator, &contextDestroyer, &checkFn]() -> void {
        if (queue->status() == TRI_ERROR_NO_ERROR) {
          void* userData = contextCreator();
          checkFn(userData, true, true);
          contextDestroyer(userData);
        }
      };
      std::shared_ptr<arangodb::basics::LocalCallbackTask> cbTask;
      cbTask.reset(new arangodb::basics::LocalCallbackTask(queue, callback));
      queue->enqueueCallback(cbTask);
    }
#endif
  }

  void truncate(CallbackElementFuncType callback) {
    for (auto& b : _buckets) {
      invokeOnAllElements(callback, b);
      b.deallocate();
      b.allocate(_initialSize);
    }
  }

  /// @brief a method to iterate over all elements in the hash
  void invokeOnAllElements(CallbackElementFuncType const& callback) {
    for (auto& b : _buckets) {
      if (b._table == nullptr || b._nrUsed == 0) {
        continue;
      }

      if (!invokeOnAllElements(callback, b)) {
        return;
      }
    }
  }

  /// @brief a method to iterate over all elements in the hash
  bool invokeOnAllElements(CallbackElementFuncType const& callback, Bucket& b) {
    for (size_t i = 0; i < b._nrAlloc; ++i) {
      if (!b._table[i].value) {
        continue;
      }
      if (!callback(b._table[i].value)) {
        return false;
      }
    }
    return true;
  }

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief adds a key/element to the array
  //////////////////////////////////////////////////////////////////////////////

  Element doInsert(UserData* userData, Element const& element, uint64_t hashByKey,
                   Bucket& b, bool const overwrite, bool const checkEquality) {
    // if the checkEquality flag is not set, we do not check for element
    // equality we use this flag to speed up initial insertion into the
    // index, i.e. when the index is built for a collection and we know
    // for sure no duplicate elements will be inserted

    // if we were adding and the table is more than 2/3 full, extend it
    if (2 * b._nrAlloc < 3 * b._nrUsed) {
      resizeInternal(userData, b, 2 * b._nrAlloc + 1);
    }

#ifdef TRI_INTERNAL_STATS
    // update statistics
    _nrAdds++;
#endif

    IndexType hashIndex = hashToIndex(hashByKey);
    IndexType i = hashIndex % b._nrAlloc;

    // If this slot is free, just use it:
    if (!b._table[i].value) {
      b._table[i].value = element;
      b._table[i].next = INVALID_INDEX;
      b._table[i].prev = INVALID_INDEX;
      if (useHashCache) {
        b._table[i].writeHashCache(hashByKey);
      }
      b._nrUsed++;
      // no collision generated here!
      return Element();
    }

    // Now find the first slot with an entry with the same key
    // that is the start of a linked list, or a free slot:
    while (b._table[i].value &&
           (b._table[i].prev != INVALID_INDEX ||
            (useHashCache && b._table[i].readHashCache() != hashByKey) ||
            !_helper.IsEqualElementElementByKey(userData, element, b._table[i].value))) {
      i = incr(b, i);
#ifdef TRI_INTERNAL_STATS
      // update statistics
      _ProbesA++;
#endif
    }

    // If this is free, we are the first with this key:
    if (!b._table[i].value) {
      b._table[i].value = element;
      b._table[i].next = INVALID_INDEX;
      b._table[i].prev = INVALID_INDEX;
      if (useHashCache) {
        b._table[i].writeHashCache(hashByKey);
      }
      b._nrUsed++;
      // no collision generated here either!
      return Element();
    }

    // Otherwise, entry i points to the beginning of the linked
    // list of which we want to make element a member. Perhaps an
    // equal element is right here:
    if (checkEquality &&
        _helper.IsEqualElementElement(userData, element, b._table[i].value)) {
      Element old = b._table[i].value;
      if (overwrite) {
        TRI_ASSERT(!useHashCache || b._table[i].readHashCache() == hashByKey);
        b._table[i].value = element;
      }
      return old;
    }

    // Now find a new home for element in this linked list:
    uint64_t hashByElm;
    IndexType j = findElementPlace(userData, b, element, checkEquality, hashByElm);

    Element old = b._table[j].value;

    // if we found an element, return
    if (old) {
      if (overwrite) {
        if (useHashCache) {
          b._table[j].writeHashCache(hashByElm);
        }
        b._table[j].value = element;
      }
      return old;
    }

    // add a new element to the associative array and linked list (in pos 2):
    b._table[j].value = element;
    b._table[j].next = b._table[i].next;
    b._table[j].prev = i;
    if (useHashCache) {
      b._table[j].writeHashCache(hashByElm);
    }
    b._table[i].next = j;
    // Finally, we need to find the successor to patch it up:
    if (b._table[j].next != INVALID_INDEX) {
      b._table[b._table[j].next].prev = j;
    }
    b._nrUsed++;
    b._nrCollisions++;

    return Element();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief insertFirst, special version of insert, when it is known that the
  /// element is the first in the hash with its key, and the hash of the key
  /// is already known. This is for example the case when resizing.
  //////////////////////////////////////////////////////////////////////////////

  IndexType insertFirst(UserData* userData, Bucket& b, Element const& element,
                        uint64_t hashByKey) {
#ifdef TRI_CHECK_MULTI_POINTER_HASH
    check(userData, true, true);
#endif

#ifdef TRI_INTERNAL_STATS
    // update statistics
    _nrAdds++;
#endif

    IndexType hashIndex = hashToIndex(hashByKey);
    IndexType i = hashIndex % b._nrAlloc;

    // If this slot is free, just use it:
    if (!b._table[i].value) {
      b._table[i].value = element;
      b._table[i].next = INVALID_INDEX;
      b._table[i].prev = INVALID_INDEX;
      if (useHashCache) {
        b._table[i].writeHashCache(hashByKey);
      }
      b._nrUsed++;
// no collision generated here!
#ifdef TRI_CHECK_MULTI_POINTER_HASH
      check(userData, true, true);
#endif
      return i;
    }

    // Now find the first slot with an entry with the same key
    // that is the start of a linked list, or a free slot:
    while (b._table[i].value) {
      i = incr(b, i);
#ifdef TRI_INTERNAL_STATS
      // update statistics
      _ProbesA++;
#endif
    }

    // We are the first with this key:
    b._table[i].value = element;
    b._table[i].next = INVALID_INDEX;
    b._table[i].prev = INVALID_INDEX;
    if (useHashCache) {
      b._table[i].writeHashCache(hashByKey);
    }
    b._nrUsed++;
// no collision generated here either!
#ifdef TRI_CHECK_MULTI_POINTER_HASH
    check(userData, true, true);
#endif
    return i;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief insertFurther, special version of insert, when it is known
  /// that the element is not the first in the hash with its key, and
  /// the hash of the key and the element is already known. This is for
  /// example the case when resizing.
  //////////////////////////////////////////////////////////////////////////////

  void insertFurther(UserData* userData, Bucket& b, Element const& element,
                     uint64_t hashByKey, uint64_t hashByElm, IndexType firstPosition) {
#ifdef TRI_CHECK_MULTI_POINTER_HASH
    check(userData, true, true);
#endif

#ifdef TRI_INTERNAL_STATS
    // update statistics
    _nrAdds++;
#endif

    // We already know the beginning of the doubly linked list:

    // Now find a new home for element in this linked list:
    IndexType hashIndex = hashToIndex(hashByElm);
    IndexType j = hashIndex % b._nrAlloc;

    while (b._table[j].value) {
      j = incr(b, j);
#ifdef TRI_INTERNAL_STATS
      _nrProbes++;
#endif
    }

    // add the element to the hash and linked list (in pos 2):
    b._table[j].value = element;
    b._table[j].next = b._table[firstPosition].next;
    b._table[j].prev = firstPosition;
    if (useHashCache) {
      b._table[j].writeHashCache(hashByElm);
    }
    b._table[firstPosition].next = j;
    // Finally, we need to find the successor to patch it up:
    if (b._table[j].next != INVALID_INDEX) {
      b._table[b._table[j].next].prev = j;
    }
    b._nrUsed++;
    b._nrCollisions++;

#ifdef TRI_CHECK_MULTI_POINTER_HASH
    check(userData, true, true);
#endif
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief lookups an element given an element
  //////////////////////////////////////////////////////////////////////////////

 public:
  Element lookup(UserData* userData, Element const& element) const {
    IndexType i;

#ifdef TRI_INTERNAL_STATS
    // update statistics
    _nrFinds++;
#endif

    Bucket* b;
    i = lookupByElement(userData, element, b);
    return b->_table[i].value;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief lookups an element given a key
  //////////////////////////////////////////////////////////////////////////////

  std::vector<Element>* lookupByKey(UserData* userData, Key const* key,
                                    size_t limit = 0) const {
    auto result = std::make_unique<std::vector<Element>>();
    lookupByKey(userData, key, *result, limit);
    return result.release();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief lookups an element given a key
  ///        Accepts a result vector as input. The result of this lookup will
  ///        be appended to the given vector.
  ///        This function returns as soon as limit many elements are inside
  ///        the given vector, no matter if the come from this lookup or
  ///        have been in the result before.
  //////////////////////////////////////////////////////////////////////////////

  void lookupByKey(UserData* userData, Key const* key,
                   std::vector<Element>& result, size_t limit = 0) const {
    if (limit > 0 && result.size() >= limit) {
      return;
    }

    // compute the hash
    uint64_t hashByKey = _helper.HashKey(key);
    Bucket const& b = _buckets[hashByKey & _bucketsMask];
    IndexType hashIndex = hashToIndex(hashByKey);
    IndexType i = hashIndex % b._nrAlloc;

#ifdef TRI_INTERNAL_STATS
    // update statistics
    _nrFinds++;
#endif

    // search the table
    while (b._table[i].value &&
           (b._table[i].prev != INVALID_INDEX ||
            (useHashCache && b._table[i].readHashCache() != hashByKey) ||
            !_helper.IsEqualKeyElement(userData, key, b._table[i].value))) {
      i = incr(b, i);
#ifdef TRI_INTERNAL_STATS
      _nrProbesF++;
#endif
    }

    if (b._table[i].value) {
      // We found the beginning of the linked list:

      do {
        result.push_back(b._table[i].value);
        i = b._table[i].next;
      } while (i != INVALID_INDEX && (limit == 0 || result.size() < limit));
    }

    // return whatever we found
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief looks up all elements with the same key as a given element
  //////////////////////////////////////////////////////////////////////////////

  std::vector<Element>* lookupWithElementByKey(UserData* userData, Element const& element,
                                               size_t limit = 0) const {
    auto result = std::make_unique<std::vector<Element>>();
    lookupWithElementByKey(userData, element, *result, limit);
    return result.release();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief looks up all elements with the same key as a given element
  ///        Accepts a result vector as input. The result of this lookup will
  ///        be appended to the given vector.
  ///        This function returns as soon as limit many elements are inside
  ///        the given vector, no matter if the come from this lookup or
  ///        have been in the result before.
  //////////////////////////////////////////////////////////////////////////////

  void lookupWithElementByKey(UserData* userData, Element const& element,
                              std::vector<Element>& result, size_t limit = 0) const {
    if (limit > 0 && result.size() >= limit) {
      // The vector is full, nothing to do.
      return;
    }

    // compute the hash
    uint64_t hashByKey = _helper.HashElement(element, true);
    Bucket const& b = _buckets[hashByKey & _bucketsMask];
    IndexType hashIndex = hashToIndex(hashByKey);
    IndexType i = hashIndex % b._nrAlloc;

#ifdef TRI_INTERNAL_STATS
    // update statistics
    _nrFinds++;
#endif

    // search the table
    while (b._table[i].value &&
           (b._table[i].prev != INVALID_INDEX ||
            (useHashCache && b._table[i].readHashCache() != hashByKey) ||
            !_helper.IsEqualElementElementByKey(userData, element, b._table[i].value))) {
      i = incr(b, i);
#ifdef TRI_INTERNAL_STATS
      _nrProbesF++;
#endif
    }

    if (b._table[i].value) {
      // We found the beginning of the linked list:

      do {
        result.push_back(b._table[i].value);
        i = b._table[i].next;
      } while (i != INVALID_INDEX && (limit == 0 || result.size() < limit));
    }
    // return whatever we found
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief looks up all elements with the same key as a given element,
  /// continuation
  //////////////////////////////////////////////////////////////////////////////

  std::vector<Element>* lookupWithElementByKeyContinue(UserData* userData,
                                                       Element const& element,
                                                       size_t limit = 0) const {
    auto result = std::make_unique<std::vector<Element>>();
    lookupWithElementByKeyContinue(userData, element, *result.get(), limit);
    return result.release();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief looks up all elements with the same key as a given element,
  ///        continuation.
  ///        Accepts a result vector as input. The result of this lookup will
  ///        be appended to the given vector.
  ///        This function returns as soon as limit many elements are inside
  ///        the given vector, no matter if the come from this lookup or
  ///        have been in the result before.
  //////////////////////////////////////////////////////////////////////////////

  void lookupWithElementByKeyContinue(UserData* userData, Element const& element,
                                      std::vector<Element>& result, size_t limit = 0) const {
    if (limit > 0 && result.size() >= limit) {
      // The vector is full, nothing to do.
      return;
    }

    uint64_t hashByKey = _helper.HashElement(element, true);
    Bucket const& b = _buckets[hashByKey & _bucketsMask];
    uint64_t hashByElm;
    IndexType i = findElementPlace(userData, b, element, true, hashByElm);
    if (!b._table[i].value) {
      // This can only happen if the element was the first in its doubly
      // linked list (after all, the caller guaranteed that element was
      // the last of a previous lookup). To cover this case, we have to
      // look in the position given by the hashByKey:
      i = hashToIndex(hashByKey) % b._nrAlloc;

      // Now find the first slot with an entry with the same key
      // that is the start of a linked list, or a free slot:
      while (b._table[i].value &&
             (b._table[i].prev != INVALID_INDEX ||
              (useHashCache && b._table[i].readHashCache() != hashByKey) ||
              !_helper.IsEqualElementElementByKey(userData, element, b._table[i].value))) {
        i = incr(b, i);
#ifdef TRI_INTERNAL_STATS
        _nrProbes++;
#endif
      }

      if (!b._table[i].value) {
        // This cannot really happen, but we handle it gracefully anyway
        return;
      }
    }

    // continue search of the table
    while (true) {
      i = b._table[i].next;
      if (i == INVALID_INDEX || (limit != 0 && result.size() >= limit)) {
        break;
      }
      result.push_back(b._table[i].value);
    }

    // return whatever we found
    return;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief looks up all elements with the same key as a given element,
  /// continuation
  //////////////////////////////////////////////////////////////////////////////

  std::vector<Element>* lookupByKeyContinue(UserData* userData, Element const& element,
                                            size_t limit = 0) const {
    return lookupWithElementByKeyContinue(userData, element, limit);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief looks up all elements with the same key as a given element,
  ///        continuation
  ///        Accepts a result vector as input. The result of this lookup will
  ///        be appended to the given vector.
  ///        This function returns as soon as limit many elements are inside
  ///        the given vector, no matter if the come from this lookup or
  ///        have been in the result before.
  //////////////////////////////////////////////////////////////////////////////

  void lookupByKeyContinue(UserData* userData, Element const& element,
                           std::vector<Element>& result, size_t limit = 0) const {
    lookupWithElementByKeyContinue(userData, element, result, limit);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief removes an element from the array, caller is responsible to free
  /// it
  //////////////////////////////////////////////////////////////////////////////

  Element remove(UserData* userData, Element const& element) {
    IndexType j = 0;

#ifdef TRI_INTERNAL_STATS
    // update statistics
    _nrRems++;
#endif

#ifdef TRI_CHECK_MULTI_POINTER_HASH
    check(userData, true, true);
#endif
    Bucket* b;
    IndexType i = lookupByElement(userData, element, b);
    if (!b->_table[i].value) {
      return Element();
    }

    Element old = b->_table[i].value;
    // We have to delete entry i
    if (b->_table[i].prev == INVALID_INDEX) {
      // This is the first in its linked list.
      j = b->_table[i].next;
      if (j == INVALID_INDEX) {
        // The only one in its linked list, simply remove it and heal
        // the hole:
        invalidateEntry(*b, i);
#ifdef TRI_CHECK_MULTI_POINTER_HASH
        check(userData, false, false);
#endif
        healHole(userData, *b, i);
        // this element did not create a collision
      } else {
        // There is at least one successor in position j.
        b->_table[j].prev = INVALID_INDEX;
        moveEntry(*b, j, i);
        if (useHashCache) {
          // We need to exchange the hashCache value by that of the key:
          b->_table[i].writeHashCache(_helper.HashElement(b->_table[i].value, true));
        }
#ifdef TRI_CHECK_MULTI_POINTER_HASH
        check(userData, false, false);
#endif
        healHole(userData, *b, j);
        b->_nrCollisions--;  // one collision less
      }
    } else {
      // This one is not the first in its linked list
      j = b->_table[i].prev;
      b->_table[j].next = b->_table[i].next;
      j = b->_table[i].next;
      if (j != INVALID_INDEX) {
        // We are not the last in the linked list.
        b->_table[j].prev = b->_table[i].prev;
      }
      invalidateEntry(*b, i);
#ifdef TRI_CHECK_MULTI_POINTER_HASH
      check(userData, false, false);
#endif
      healHole(userData, *b, i);
      b->_nrCollisions--;
    }
    b->_nrUsed--;
#ifdef TRI_CHECK_MULTI_POINTER_HASH
    check(userData, true, true);
#endif
    // return success
    return old;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief resize the array
  //////////////////////////////////////////////////////////////////////////////

  int resize(UserData* userData, size_t size) noexcept {
    size /= _buckets.size();
    for (auto& b : _buckets) {
      if (2 * (2 * size + 1) < 3 * b._nrUsed) {
        return TRI_ERROR_BAD_PARAMETER;
      }

      try {
        resizeInternal(userData, b, 2 * size + 1);
      } catch (...) {
        return TRI_ERROR_OUT_OF_MEMORY;
      }
    }
    return TRI_ERROR_NO_ERROR;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return selectivity, this is a number s with 0.0 < s <= 1.0. If
  /// s == 1.0 this means that every document is identified uniquely by its
  /// key. It is computed as
  ///    number of different keys/number of elements in table
  //////////////////////////////////////////////////////////////////////////////

  double selectivity() {
    size_t nrUsed = 0;
    size_t nrCollisions = 0;
    for (auto& b : _buckets) {
      nrUsed += b._nrUsed;
      nrCollisions += b._nrCollisions;
    }
    return nrUsed > 0
               ? static_cast<double>(nrUsed - nrCollisions) / static_cast<double>(nrUsed)
               : 1.0;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief iteration over all pointers in the hash array, the callback
  /// function is called on the Element for each thingy stored in the hash
  //////////////////////////////////////////////////////////////////////////////

  void iterate(UserData* userData, std::function<void(Element&)> callback) {
    for (auto& b : _buckets) {
      for (IndexType i = 0; i < b._nrAlloc; i++) {
        if (b._table[i].value) {
          callback(userData, b._table[i].value);
        }
      }
    }
  }

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief increment IndexType by 1 modulo _nrAlloc:
  //////////////////////////////////////////////////////////////////////////////

  inline IndexType incr(Bucket const& b, IndexType i) const {
    IndexType dummy = (++i) - b._nrAlloc;
    return i < b._nrAlloc ? i : dummy;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief resize the array, internal method
  //////////////////////////////////////////////////////////////////////////////

  void resizeInternal(UserData* userData, Bucket& b, size_t targetSize) {
    std::string const cb(_contextCallback());

    targetSize = TRI_NearPrime(targetSize);

    PerformanceLogScope logScope(std::string("multi hash-resize ") + cb +
                                 ", target size: " + std::to_string(targetSize));

    Bucket copy;
    copy.allocate(targetSize);

#ifdef TRI_INTERNAL_STATS
    _nrResizes++;
#endif

    // table is already clear by allocate, copy old data
    if (b._nrUsed > 0) {
      EntryType* oldTable = b._table;
      IndexType const oldAlloc = b._nrAlloc;
      TRI_ASSERT(oldAlloc > 0);

      for (IndexType j = 0; j < oldAlloc; j++) {
        if (oldTable[j].value && oldTable[j].prev == INVALID_INDEX) {
          // This is a "first" one in its doubly linked list:
          uint64_t hashByKey;
          if (useHashCache) {
            hashByKey = oldTable[j].readHashCache();
          } else {
            hashByKey = _helper.HashElement(oldTable[j].value, true);
          }
          IndexType insertPosition =
              insertFirst(userData, copy, oldTable[j].value, hashByKey);
          // Now walk to the end of the list:
          IndexType k = j;
          while (oldTable[k].next != INVALID_INDEX) {
            k = oldTable[k].next;
          }
          // Now insert all of them backwards, not repeating k:
          while (k != j) {
            uint64_t hashByElm;
            if (useHashCache) {
              hashByElm = oldTable[k].readHashCache();
            } else {
              hashByElm = _helper.HashElement(oldTable[k].value, false);
            }
            insertFurther(userData, copy, oldTable[k].value, hashByKey,
                          hashByElm, insertPosition);
            k = oldTable[k].prev;
          }
        }
      }
    }

    b = std::move(copy);
  }

#ifdef TRI_CHECK_MULTI_POINTER_HASH

  //////////////////////////////////////////////////////////////////////////////
  /// @brief internal debugging check function
  //////////////////////////////////////////////////////////////////////////////

  bool check(UserData* userData, bool checkCount, bool checkPositions) const {
    std::cout << "Performing AssocMulti check " << checkCount << checkPositions
              << std::endl;
    bool ok = true;
    for (auto& b : _buckets) {
      IndexType i, ii, j, k;

      IndexType count = 0;

      for (i = 0; i < b._nrAlloc; i++) {
        if (b._table[i].value) {
          count++;
          if (b._table[i].prev != INVALID_INDEX) {
            if (b._table[b._table[i].prev].next != i) {
              std::cout << "Alarm prev " << i << std::endl;
              ok = false;
            }
          }

          if (b._table[i].next != INVALID_INDEX) {
            if (b._table[b._table[i].next].prev != i) {
              std::cout << "Alarm next " << i << std::endl;
              ok = false;
            }
          }
          ii = i;
          j = b._table[ii].next;
          while (j != INVALID_INDEX) {
            if (j == i) {
              std::cout << "Alarm cycle " << i << std::endl;
              ok = false;
              break;
            }
            ii = j;
            j = b._table[ii].next;
          }
        }
      }
      if (checkCount && count != b._nrUsed) {
        std::cout << "Alarm _nrUsed wrong " << b._nrUsed << " != " << count
                  << "!" << std::endl;
        ok = false;
      }
      if (checkPositions) {
        for (i = 0; i < b._nrAlloc; i++) {
          if (b._table[i].value) {
            IndexType hashIndex;
            if (b._table[i].prev == INVALID_INDEX) {
              // We are the first in a linked list.
              uint64_t hashByKey = _helper.HashElement(b._table[i].value, true);
              hashIndex = hashToIndex(hashByKey);
              j = hashIndex % b._nrAlloc;
              if (useHashCache && b._table[i].readHashCache() != hashByKey) {
                std::cout << "Alarm hashCache wrong " << i << std::endl;
              }
              for (k = j; k != i;) {
                if (!b._table[k].value ||
                    (b._table[k].prev == INVALID_INDEX &&
                     _helper.IsEqualElementElementByKey(userData, b._table[i].value,
                                                        b._table[k].value))) {
                  ok = false;
                  std::cout << "Alarm pos bykey: " << i << std::endl;
                }
                k = incr(b, k);
              }
            } else {
              // We are not the first in a linked list.
              uint64_t hashByElm = _helper.HashElement(b._table[i].value, false);
              hashIndex = hashToIndex(hashByElm);
              j = hashIndex % b._nrAlloc;
              if (useHashCache && b._table[i].readHashCache() != hashByElm) {
                std::cout << "Alarm hashCache wrong " << i << std::endl;
              }
              for (k = j; k != i;) {
                if (!b._table[k].value ||
                    _helper.IsEqualElementElement(userData, b._table[i].value,
                                                  b._table[k].value)) {
                  ok = false;
                  std::cout << "Alarm unique: " << k << ", " << i << std::endl;
                }
                k = incr(b, k);
              }
            }
          }
        }
      }
    }
    if (!ok) {
      std::cout << "Something is wrong!" << std::endl;
    }
    return ok;
  }

#endif

  //////////////////////////////////////////////////////////////////////////////
  /// @brief find an element or its place using the element hash function
  //////////////////////////////////////////////////////////////////////////////

  inline IndexType findElementPlace(UserData* userData, Bucket const& b,
                                    Element const& element, bool checkEquality,
                                    uint64_t& hashByElm) const {
    // This either finds a place to store element or an entry in
    // the table that is equal to element. If checkEquality is
    // set to false, the caller guarantees that there is no entry
    // that compares equal to element in the table, which saves a
    // lot of element comparisons. This function always returns a
    // pointer into the table, which is either empty or points to
    // an entry that compares equal to element.

    hashByElm = _helper.HashElement(element, false);
    IndexType hashindex = hashToIndex(hashByElm);
    IndexType i = hashindex % b._nrAlloc;

    while (b._table[i].value &&
           (!checkEquality || (useHashCache && b._table[i].readHashCache() != hashByElm) ||
            !_helper.IsEqualElementElement(userData, element, b._table[i].value))) {
      i = incr(b, i);
#ifdef TRI_INTERNAL_STATS
      _nrProbes++;
#endif
    }
    return i;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief find an element or its place by key or element identity
  //////////////////////////////////////////////////////////////////////////////

  IndexType lookupByElement(UserData* userData, Element const& element, Bucket*& buck) const {
    // This performs a complete lookup for an element. It returns a slot
    // number. This slot is either empty or contains an element that
    // compares equal to element.
    uint64_t hashByKey = _helper.HashElement(element, true);
    Bucket const& b = _buckets[hashByKey & _bucketsMask];
    buck = const_cast<Bucket*>(&b);
    IndexType hashIndex = hashToIndex(hashByKey);
    IndexType i = hashIndex % b._nrAlloc;

    // Now find the first slot with an entry with the same key
    // that is the start of a linked list, or a free slot:
    while (b._table[i].value &&
           (b._table[i].prev != INVALID_INDEX ||
            (useHashCache && b._table[i].readHashCache() != hashByKey) ||
            !_helper.IsEqualElementElementByKey(userData, element, b._table[i].value))) {
      i = incr(b, i);
#ifdef TRI_INTERNAL_STATS
      _nrProbes++;
#endif
    }

    if (b._table[i].value) {
      // It might be right here!
      if (_helper.IsEqualElementElement(userData, element, b._table[i].value)) {
        return i;
      }

      // Now we have to look for it in its hash position:
      uint64_t hashByElm;
      IndexType j = findElementPlace(userData, b, element, true, hashByElm);

      // We have either found an equal element or nothing:
      return j;
    }

    // If we get here, no element with the same key is in the array, so
    // we will not be able to find it anywhere!
    return i;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief helper to decide whether something is between to places
  //////////////////////////////////////////////////////////////////////////////

  static inline bool isBetween(IndexType from, IndexType x, IndexType to) {
    // returns whether or not x is behind from and before or equal to
    // to in the cyclic order. If x is equal to from, then the result is
    // always false. If from is equal to to, then the result is always
    // true.
    return (from < to) ? (from < x && x <= to) : (x > from || x <= to);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief helper to invalidate a slot
  //////////////////////////////////////////////////////////////////////////////

  inline void invalidateEntry(Bucket& b, IndexType i) {
    b._table[i].value = Element();
    b._table[i].next = INVALID_INDEX;
    b._table[i].prev = INVALID_INDEX;
    if (useHashCache) {
      b._table[i].writeHashCache(0);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief helper to move an entry from one slot to another
  //////////////////////////////////////////////////////////////////////////////

  inline void moveEntry(Bucket& b, IndexType from, IndexType to) {
    // Moves an entry, adjusts the linked lists, but does not take care
    // for the hole. to must be unused. from can be any element in a
    // linked list.
    b._table[to] = b._table[from];
    if (b._table[to].prev != INVALID_INDEX) {
      b._table[b._table[to].prev].next = to;
    }
    if (b._table[to].next != INVALID_INDEX) {
      b._table[b._table[to].next].prev = to;
    }
    invalidateEntry(b, from);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief helper to heal a hole where we deleted something
  //////////////////////////////////////////////////////////////////////////////

  void healHole(UserData* userData, Bucket& b, IndexType i) {
    IndexType j = incr(b, i);

    while (b._table[j].value) {
      // Find out where this element ought to be:
      // If it is the start of one of the linked lists, we need to hash
      // by key, otherwise, we hash by the full identity of the element:
      uint64_t hash = _helper.HashElement(b._table[j].value, b._table[j].prev == INVALID_INDEX);
      IndexType hashIndex = hashToIndex(hash);
      IndexType k = hashIndex % b._nrAlloc;
      if (!isBetween(i, k, j)) {
        // we have to move j to i:
        moveEntry(b, j, i);
        i = j;  // Now heal this hole at j,
                // j will be incremented right away
      }
      j = incr(b, j);
#ifdef TRI_INTERNAL_STATS
      _nrProbesD++;
#endif
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief convert a 64bit hash value to an index of type IndexType
  //////////////////////////////////////////////////////////////////////////////

  inline IndexType hashToIndex(uint64_t const h) const {
    return static_cast<IndexType>(sizeof(IndexType) == 8 ? h : TRI_64To32(h));
  }
};

}  // namespace basics
}  // namespace arangodb

#endif
