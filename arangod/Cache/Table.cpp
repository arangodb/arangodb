////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#include "Cache/Table.h"
#include "Basics/Common.h"
#include "Cache/Common.h"

#include <stdint.h>
#include <memory>
#include <stdexcept>

using namespace arangodb::cache;

const uint32_t Table::minLogSize = 8;
const uint32_t Table::maxLogSize = 32;

bool Table::GenericBucket::lock(int64_t maxTries) {
  return _state.lock(maxTries);
}

void Table::GenericBucket::unlock() {
  TRI_ASSERT(_state.isLocked());
  _state.unlock();
}

bool Table::GenericBucket::isMigrated() const {
  TRI_ASSERT(_state.isLocked());
  return _state.isSet(BucketState::Flag::migrated);
}

Table::Subtable::Subtable(std::shared_ptr<Table> source, GenericBucket* buckets,
                          uint64_t size, uint32_t mask, uint32_t shift)
    : _source(source),
      _buckets(buckets),
      _size(size),
      _mask(mask),
      _shift(shift) {}

void* Table::Subtable::fetchBucket(uint32_t hash) {
  return &(_buckets[(hash & _mask) >> _shift]);
}

bool Table::Subtable::applyToAllBuckets(std::function<bool(void*)> cb) {
  bool ok = true;
  for (uint64_t i = 0; i < _size; i++) {
    GenericBucket* bucket = &(_buckets[i]);
    ok = cb(bucket);
    if (!ok) {
      break;
    }
  }
  return ok;
}

Table::Table(uint32_t logSize)
    : _lock(),
      _disabled(true),
      _evictions(false),
      _logSize(std::min(logSize, maxLogSize)),
      _size(static_cast<uint64_t>(1) << _logSize),
      _shift(32 - _logSize),
      _mask((uint32_t)((_size - 1) << _shift)),
      _buffer(new uint8_t[(_size * BUCKET_SIZE) + Table::padding]),
      _buckets(reinterpret_cast<GenericBucket*>(
          reinterpret_cast<uint64_t>((_buffer.get() + (BUCKET_SIZE - 1))) &
          ~(static_cast<uint64_t>(BUCKET_SIZE - 1)))),
      _auxiliary(nullptr),
      _bucketClearer(defaultClearer),
      _slotsTotal(_size),
      _slotsUsed(static_cast<uint64_t>(0)) {
  memset(_buckets, 0, BUCKET_SIZE * _size);
}

uint64_t Table::allocationSize(uint32_t logSize) {
  return sizeof(Table) + (BUCKET_SIZE * (static_cast<uint64_t>(1) << logSize)) +
         Table::padding;
}

uint64_t Table::memoryUsage() const { return Table::allocationSize(_logSize); }

uint64_t Table::size() const { return _size; }

uint32_t Table::logSize() const { return _logSize; }

std::pair<void*, Table*> Table::fetchAndLockBucket(
    uint32_t hash, int64_t maxTries) {
  GenericBucket* bucket = nullptr;
  Table* source = nullptr;
  bool ok = _lock.readLock(maxTries);
  if (ok) {
    ok = !_disabled;
    if (ok) {
      bucket = &(_buckets[(hash & _mask) >> _shift]);
      source = this;
      ok = bucket->lock(maxTries);
      if (ok) {
        if (bucket->isMigrated()) {
          bucket->unlock();
          bucket = nullptr;
          source = nullptr;
          if (_auxiliary) {
            auto pair = _auxiliary->fetchAndLockBucket(hash, maxTries);
            bucket = reinterpret_cast<GenericBucket*>(pair.first);
            source = pair.second;
          }
        }
      } else {
        bucket = nullptr;
        source = nullptr;
      }
    }
    _lock.readUnlock();
  }

  return std::make_pair(bucket, source);
}

std::shared_ptr<Table> Table::setAuxiliary(std::shared_ptr<Table> table) {
  std::shared_ptr<Table> result = table;
  if (table.get() != this) {
    _lock.writeLock();
    if (table.get() == nullptr) {
      result = _auxiliary;
      _auxiliary = table;
    } else if (_auxiliary.get() == nullptr) {
      _auxiliary = table;
      result.reset();
    }
    _lock.writeUnlock();
  }
  return result;
}

void* Table::primaryBucket(uint64_t index) {
  if (!isEnabled()) {
    return nullptr;
  }
  return &(_buckets[index]);
}

std::unique_ptr<Table::Subtable> Table::auxiliaryBuckets(uint32_t index) {
  if (!isEnabled()) {
    return std::unique_ptr<Subtable>(nullptr);
  }
  GenericBucket* base;
  uint64_t size;
  uint32_t mask;
  uint32_t shift;

  _lock.readLock();
  std::shared_ptr<Table> source = _auxiliary->shared_from_this();
  TRI_ASSERT(_auxiliary.get() != nullptr);
  if (_logSize > _auxiliary->_logSize) {
    uint32_t diff = _logSize - _auxiliary->_logSize;
    base = &(_auxiliary->_buckets[index >> diff]);
    size = 1;
    mask = 0;
    shift = 0;
  } else {
    uint32_t diff = _auxiliary->_logSize - _logSize;
    base = &(_auxiliary->_buckets[index << diff]);
    size = static_cast<uint64_t>(1) << diff;
    mask = (static_cast<uint32_t>(size - 1) << _auxiliary->_shift);
    shift = _auxiliary->_shift;
  }
  _lock.readUnlock();

  return std::make_unique<Subtable>(source, base, size, mask, shift);
}

void Table::setTypeSpecifics(BucketClearer clearer, size_t slotsPerBucket) {
  _bucketClearer = clearer;
  _slotsTotal = _size * static_cast<uint64_t>(slotsPerBucket);
}

void Table::clear() {
  disable();
  if (_auxiliary.get() != nullptr) {
    throw;
  }
  for (uint64_t i = 0; i < _size; i++) {
    _bucketClearer(&(_buckets[i]));
  }
  _bucketClearer = Table::defaultClearer;
  _slotsUsed = 0;
}

void Table::disable() {
  _lock.writeLock();
  _disabled = true;
  _lock.writeUnlock();
}

void Table::enable() {
  _lock.writeLock();
  _disabled = false;
  _lock.writeUnlock();
}

bool Table::isEnabled(int64_t maxTries) {
  bool ok = _lock.readLock(maxTries);
  if (ok) {
    ok = !_disabled;
    _lock.readUnlock();
  }
  return ok;
}

bool Table::slotFilled() {
  size_t i = _slotsUsed.fetch_add(1, std::memory_order_acq_rel);
  return ((static_cast<double>(i + 1) /
           static_cast<double>(_slotsTotal)) > Table::idealUpperRatio);
}

bool Table::slotEmptied() {
  size_t i = _slotsUsed.fetch_sub(1, std::memory_order_acq_rel);
  return (((static_cast<double>(i - 1) /
            static_cast<double>(_slotsTotal)) < Table::idealLowerRatio) &&
          (_logSize > Table::minLogSize));
}

void Table::signalEvictions() {
  bool ok = _lock.writeLock(triesGuarantee);
  if (ok) {
    _evictions = true;
    _lock.writeUnlock();
  }
}

uint32_t Table::idealSize() {
  bool ok = _lock.writeLock(triesGuarantee);
  bool forceGrowth = false;
  if (ok) {
    forceGrowth = _evictions;
    _evictions = false;
    _lock.writeUnlock();
  }
  if (forceGrowth) {
    return logSize() + 1;
  }

  return (((static_cast<double>(_slotsUsed.load()) /
            static_cast<double>(_slotsTotal)) > Table::idealUpperRatio)
              ? (logSize() + 1)
              : (((static_cast<double>(_slotsUsed.load()) /
                   static_cast<double>(_slotsTotal)) < Table::idealLowerRatio)
                     ? (logSize() - 1)
                     : logSize()));
}

void Table::defaultClearer(void* ptr) {
  throw std::invalid_argument("must register a clearer");
}
