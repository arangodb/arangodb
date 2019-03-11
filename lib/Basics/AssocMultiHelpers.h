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

#ifndef ARANGODB_BASICS_ASSOC_MULTI_HELPERS_H
#define ARANGODB_BASICS_ASSOC_MULTI_HELPERS_H 1

#include "Basics/Common.h"
#include "Basics/IndexBucket.h"
#include "Basics/LocalTaskQueue.h"
#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"

namespace arangodb {
namespace basics {

template <class Element, class IndexType, bool useHashCache>
struct Entry {
 private:
  uint64_t hashCache;  // cache the hash value, this stores the
                       // hashByKey for the first element in the
                       // linked list and the hashByElm for all
                       // others
 public:
  Element value;   // the data stored in this slot
  IndexType next;  // index of the data following in the linked
                   // list of all items with the same key
  IndexType prev;  // index of the data preceding in the linked
                   // list of all items with the same key
  uint64_t readHashCache() const { return hashCache; }
  void writeHashCache(uint64_t v) { hashCache = v; }

  Entry() : hashCache(0), value(), next(INVALID_INDEX), prev(INVALID_INDEX) {}

 private:
  static IndexType const INVALID_INDEX = ((IndexType)0) - 1;
};

template <class Element, class IndexType>
struct Entry<Element, IndexType, false> {
  Element value;   // the data stored in this slot
  IndexType next;  // index of the data following in the linked
                   // list of all items with the same key
  IndexType prev;  // index of the data preceding in the linked
                   // list of all items with the same key
  uint64_t readHashCache() const { return 0; }
  void writeHashCache(uint64_t v) { TRI_ASSERT(false); }

  Entry() : value(), next(INVALID_INDEX), prev(INVALID_INDEX) {}

 private:
  static IndexType const INVALID_INDEX = ((IndexType)0) - 1;
};

template <class Element, class IndexType, bool useHashCache>
class MultiInserterTask final : public LocalTask {
 private:
  typedef Entry<Element, IndexType, useHashCache> EntryType;
  typedef arangodb::basics::IndexBucket<EntryType, IndexType> Bucket;
  typedef std::vector<std::pair<Element, uint64_t>> DocumentsPerBucket;

  std::function<void(void*)> _contextDestroyer;
  std::vector<Bucket>* _buckets;
  std::function<Element(void*, Element const&, uint64_t, Bucket&, bool const, bool const)> _doInsert;

  size_t _i;
  void* _userData;

  std::shared_ptr<std::vector<std::vector<DocumentsPerBucket>>> _allBuckets;

 public:
  MultiInserterTask(
      std::shared_ptr<LocalTaskQueue> queue,
      std::function<void(void*)> contextDestroyer, std::vector<Bucket>* buckets,
      std::function<Element(void*, Element const&, uint64_t, Bucket&, bool const, bool const)> doInsert,
      size_t i, void* userData,
      std::shared_ptr<std::vector<std::vector<DocumentsPerBucket>>> allBuckets)
      : LocalTask(queue),
        _contextDestroyer(contextDestroyer),
        _buckets(buckets),
        _doInsert(doInsert),
        _i(i),
        _userData(userData),
        _allBuckets(allBuckets) {}

  void run() override {
    // sort first so we have a deterministic insertion order
    std::sort((*_allBuckets)[_i].begin(), (*_allBuckets)[_i].end(),
              [](DocumentsPerBucket const& lhs, DocumentsPerBucket const& rhs) -> bool {
                if (lhs.empty() && rhs.empty()) {
                  return false;
                }
                if (lhs.empty() && !rhs.empty()) {
                  return true;
                }
                if (rhs.empty() && !lhs.empty()) {
                  return false;
                }

                return lhs[0].first < rhs[0].first;
              });
    // now actually insert them
    try {
      Bucket& b = (*_buckets)[static_cast<size_t>(_i)];

      for (auto const& it : (*_allBuckets)[_i]) {
        for (auto const& it2 : it) {
          _doInsert(_userData, it2.first, it2.second, b, true, false);
        }
      }
    } catch (...) {
      _queue->setStatus(TRI_ERROR_INTERNAL);
    }

    _contextDestroyer(_userData);
    _queue->join();
  }
};

template <class Element, class IndexType, bool useHashCache>
class MultiPartitionerTask final : public LocalTask {
 private:
  typedef MultiInserterTask<Element, IndexType, useHashCache> Inserter;
  typedef std::vector<std::pair<Element, uint64_t>> DocumentsPerBucket;

  std::function<uint64_t(Element const&, bool)> _hashElement;
  std::function<void(void*)> _contextDestroyer;
  std::shared_ptr<std::vector<Element> const> _data;
  std::vector<Element> const* _elements;

  size_t _lower;
  size_t _upper;
  void* _userData;

  std::shared_ptr<std::vector<std::atomic<size_t>>> _bucketFlags;
  std::shared_ptr<std::vector<arangodb::Mutex>> _bucketMapLocker;
  std::shared_ptr<std::vector<std::vector<DocumentsPerBucket>>> _allBuckets;
  std::shared_ptr<std::vector<std::shared_ptr<Inserter>>> _inserters;

  uint64_t _bucketsMask;

 public:
  MultiPartitionerTask(std::shared_ptr<LocalTaskQueue> queue,
                       std::function<uint64_t(Element const&, bool)> hashElement,
                       std::function<void(void*)> const& contextDestroyer,
                       std::shared_ptr<std::vector<Element> const> data,
                       size_t lower, size_t upper, void* userData,
                       std::shared_ptr<std::vector<std::atomic<size_t>>> bucketFlags,
                       std::shared_ptr<std::vector<arangodb::Mutex>> bucketMapLocker,
                       std::shared_ptr<std::vector<std::vector<DocumentsPerBucket>>> allBuckets,
                       std::shared_ptr<std::vector<std::shared_ptr<Inserter>>> inserters)
      : LocalTask(queue),
        _hashElement(hashElement),
        _contextDestroyer(contextDestroyer),
        _data(data),
        _elements(_data.get()),
        _lower(lower),
        _upper(upper),
        _userData(userData),
        _bucketFlags(bucketFlags),
        _bucketMapLocker(bucketMapLocker),
        _allBuckets(allBuckets),
        _inserters(inserters),
        _bucketsMask(_allBuckets->size() - 1) {}

  void run() override {
    try {
      std::vector<DocumentsPerBucket> partitions;
      partitions.resize(_allBuckets->size());  // initialize to number of buckets

      for (size_t i = _lower; i < _upper; ++i) {
        uint64_t hashByKey = _hashElement((*_elements)[i], true);
        auto bucketId = hashByKey & _bucketsMask;

        partitions[bucketId].emplace_back((*_elements)[i], hashByKey);
      }

      // transfer ownership to the central map
      for (size_t i = 0; i < partitions.size(); ++i) {
        {
          MUTEX_LOCKER(mutexLocker, (*_bucketMapLocker)[i]);
          (*_allBuckets)[i].emplace_back(std::move(partitions[i]));
          (*_bucketFlags)[i]--;
          if ((*_bucketFlags)[i].load() == 0) {
            // queue inserter for bucket i
            _queue->enqueue((*_inserters)[i]);
          }
        }
      }
    } catch (...) {
      _queue->setStatus(TRI_ERROR_INTERNAL);
    }

    _contextDestroyer(_userData);
    _queue->join();
  }
};

}  // namespace basics
}  // namespace arangodb

#endif
