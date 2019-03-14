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

#ifndef ARANGODB_BASICS_ASSOC_UNIQUE_HELPERS_H
#define ARANGODB_BASICS_ASSOC_UNIQUE_HELPERS_H 1

#include "Basics/Common.h"

#include "Basics/IndexBucket.h"
#include "Basics/LocalTaskQueue.h"
#include "Basics/MutexLocker.h"

namespace arangodb {
namespace basics {

struct BucketPosition {
  size_t bucketId;
  uint64_t position;

  BucketPosition() : bucketId(SIZE_MAX), position(0) {}

  void reset() {
    bucketId = SIZE_MAX - 1;
    position = 0;
  }

  bool operator==(BucketPosition const& other) const {
    return position == other.position && bucketId == other.bucketId;
  }
};

template <class Element>
class UniqueInserterTask final : public LocalTask {
 private:
  typedef arangodb::basics::IndexBucket<Element, uint64_t> Bucket;
  typedef std::vector<std::pair<Element, uint64_t>> DocumentsPerBucket;

  std::function<void(void*)> _contextDestroyer;
  std::vector<Bucket>* _buckets;
  std::function<int(void*, Element const&, Bucket&, uint64_t)> _doInsert;
  std::function<bool(void*, Bucket&, uint64_t)> _checkResize;

  size_t _i;
  void* _userData;

  std::shared_ptr<std::vector<std::vector<DocumentsPerBucket>>> _allBuckets;

 public:
  UniqueInserterTask(std::shared_ptr<LocalTaskQueue> queue,
                     std::function<void(void*)> contextDestroyer,
                     std::vector<Bucket>* buckets,
                     std::function<int(void*, Element const&, Bucket&, uint64_t)> doInsert,
                     std::function<bool(void*, Bucket&, uint64_t)> checkResize,
                     size_t i, void* userData,
                     std::shared_ptr<std::vector<std::vector<DocumentsPerBucket>>> allBuckets)
      : LocalTask(queue),
        _contextDestroyer(contextDestroyer),
        _buckets(buckets),
        _doInsert(doInsert),
        _checkResize(checkResize),
        _i(i),
        _userData(userData),
        _allBuckets(allBuckets) {}

  void run() override {
    // actually insert them
    try {
      Bucket& b = (*_buckets)[static_cast<size_t>(_i)];

      for (auto const& it : (*_allBuckets)[_i]) {
        uint64_t expected = it.size();
        if (!_checkResize(_userData, b, expected)) {
          _queue->setStatus(TRI_ERROR_OUT_OF_MEMORY);
          _queue->join();
          return;
        }
        for (auto const& it2 : it) {
          int status = _doInsert(_userData, it2.first, b, it2.second);
          if (status != TRI_ERROR_NO_ERROR) {
            _queue->setStatus(status);
            _queue->join();
            _contextDestroyer(_userData);
            return;
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

template <class Element>
class UniquePartitionerTask final : public LocalTask {
 private:
  typedef UniqueInserterTask<Element> Inserter;
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
  UniquePartitionerTask(std::shared_ptr<LocalTaskQueue> queue,
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
        MUTEX_LOCKER(mutexLocker, (*_bucketMapLocker)[i]);
        (*_allBuckets)[i].emplace_back(std::move(partitions[i]));
        (*_bucketFlags)[i]--;
        if ((*_bucketFlags)[i].load() == 0) {
          // queue inserter for bucket i
          _queue->enqueue((*_inserters)[i]);
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
