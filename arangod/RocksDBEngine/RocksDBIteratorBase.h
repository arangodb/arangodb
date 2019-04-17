////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ENGINE_ROCKSDB_ITERATOR_BASE_H
#define ARANGOD_ROCKSDB_ENGINE_ROCKSDB_ITERATOR_BASE_H 1

#include "Indexes/Index.h"
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBMethods.h"

#include <rocksdb/iterator.h>
#include <rocksdb/slice.h>

namespace arangodb {

template<typename ComparatorType>
class RocksDBIterator {
 public:
  RocksDBIterator(RocksDBIterator const& other) = delete;
  RocksDBIterator& operator=(RocksDBIterator const& other) = delete;

  RocksDBIterator(ComparatorType const* comparator,
                  RocksDBKeyBounds&& bounds, bool reverse)
      : _cmp(comparator),
        _state(IteratorState::MustSeek),
        _reverse(reverse),
        _bounds(std::move(bounds)) {
  }

  ~RocksDBIterator() {}

  void initialize(RocksDBMethods* mthds, 
                  rocksdb::ColumnFamilyHandle* columnFamily,
                  rocksdb::ReadOptions&& options) {
    // we need to have a pointer to a slice for the upper bound
    // so we need to assign the slice to an instance variable here
    if (_reverse) {
      _rangeBound = _bounds.start();
      options.iterate_lower_bound = &_rangeBound;
    } else {
      _rangeBound = _bounds.end();
      options.iterate_upper_bound = &_rangeBound;
    }

    _iterator = mthds->NewIterator(options, columnFamily);
  }
  
  inline rocksdb::Slice key() const {
    return _iterator->key();
  }

  inline rocksdb::Slice value() const {
    return _iterator->value();
  }
  
  void reset() {
    /// reset the state
    _state = IteratorState::MustSeek;
  }
  
  /// @brief handles all state transitions for the rocksdb::Iterator state
  bool prepareRead() {
    TRI_ASSERT(_iterator != nullptr);

    if (_state != IteratorState::Done) {
      if (_state == IteratorState::MustAdvance) {
        if (_reverse) {
          _iterator->Prev();
        } else {
          _iterator->Next();
        }
      } else if (_state == IteratorState::MustSeek) {
        if (_reverse) {
          _iterator->SeekForPrev(_bounds.end());
        } else {
          _iterator->Seek(_bounds.start());
        }
      }

      if (!_iterator->Valid() || outOfRange()) {
        _state = IteratorState::Done;
      } else {
        _state = IteratorState::MustAdvance;
      }
    } // note done

    return _state != IteratorState::Done;
  }
  
 private:
  bool outOfRange() const {
    if (_reverse) {
      return (_cmp->Compare(_iterator->key(), _bounds.start()) < 0);
    } else {
      return (_cmp->Compare(_iterator->key(), _bounds.end()) > 0);
    }
  }

  enum IteratorState : uint32_t {
    MustSeek,
    MustAdvance,
    Done
  };

 private:
  std::unique_ptr<rocksdb::Iterator> _iterator;
  ComparatorType const* _cmp;
  IteratorState _state;
  bool const _reverse;
  RocksDBKeyBounds _bounds;
  // used for iterate_upper_bound iterate_lower_bound
  rocksdb::Slice _rangeBound;
};

}

#endif
