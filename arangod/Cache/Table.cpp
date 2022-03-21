////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>

#include "Cache/Table.h"

#include "Basics/Exceptions.h"
#include "Basics/SpinLocker.h"
#include "Basics/debugging.h"
#include "Basics/voc-errors.h"
#include "Cache/Cache.h"
#include "Cache/Common.h"
#include "Cache/PlainBucket.h"
#include "Cache/TransactionalBucket.h"

namespace arangodb::cache {

using SpinLocker = ::arangodb::basics::SpinLocker;

Table::GenericBucket::GenericBucket() noexcept : _state{}, _padding{} {}

bool Table::GenericBucket::lock(std::uint64_t maxTries) noexcept {
  return _state.lock(maxTries);
}

void Table::GenericBucket::unlock() noexcept {
  TRI_ASSERT(_state.isLocked());
  _state.unlock();
}

void Table::GenericBucket::clear() {
  _state.lock(std::numeric_limits<std::uint64_t>::max(),
              [this]() noexcept -> void {
                _state.clear();
                for (size_t i = 0; i < paddingSize; i++) {
                  _padding[i] = static_cast<std::uint8_t>(0);
                }
                _state.unlock();
              });
}

bool Table::GenericBucket::isMigrated() const noexcept {
  TRI_ASSERT(_state.isLocked());
  return _state.isSet(BucketState::Flag::migrated);
}

Table::Subtable::Subtable(std::shared_ptr<Table> source, GenericBucket* buckets,
                          std::uint64_t size, std::uint32_t mask,
                          std::uint32_t shift)
    : _source(std::move(source)),
      _buckets(buckets),
      _size(size),
      _mask(mask),
      _shift(shift) {}

void* Table::Subtable::fetchBucket(std::uint32_t hash) noexcept {
  return &(_buckets[(hash & _mask) >> _shift]);
}

std::vector<Table::BucketLocker> Table::Subtable::lockAllBuckets() {
  std::vector<Table::BucketLocker> guards;
  guards.reserve(_size);
  for (std::uint64_t i = 0; i < _size; i++) {
    guards.emplace_back(&(_buckets[i]), _source.get(), Cache::triesGuarantee);
  }
  return guards;
}

template<typename BucketType>
bool Table::Subtable::applyToAllBuckets(std::function<bool(BucketType&)> cb) {
  bool ok = true;
  for (std::uint64_t i = 0; i < _size; i++) {
    GenericBucket* bucket = &(_buckets[i]);
    TRI_ASSERT(bucket != nullptr);
    ok = cb(*reinterpret_cast<BucketType*>(bucket));
    if (!ok) {
      break;
    }
  }
  return ok;
}
template bool Table::Subtable::applyToAllBuckets<PlainBucket>(
    std::function<bool(PlainBucket&)>);
template bool Table::Subtable::applyToAllBuckets<TransactionalBucket>(
    std::function<bool(TransactionalBucket&)>);

Table::BucketLocker::BucketLocker() noexcept
    : _bucket(nullptr), _source(nullptr), _locked(false) {}

Table::BucketLocker::BucketLocker(void* bucket, Table* source,
                                  std::uint64_t maxAttempts)
    : _bucket(reinterpret_cast<GenericBucket*>(bucket)),
      _source(source),
      _locked(this->bucket<Table::GenericBucket>().lock(maxAttempts)) {
  if (!_locked) {
    _bucket = nullptr;
    _source = nullptr;
  }
}

Table::BucketLocker::BucketLocker(BucketLocker&& other) noexcept {
  steal(std::move(other));
}

Table::BucketLocker::~BucketLocker() { release(); }

Table::BucketLocker& Table::BucketLocker::operator=(
    BucketLocker&& other) noexcept {
  if (this != &other) {
    release();
    steal(std::move(other));
  }
  return *this;
}

bool Table::BucketLocker::isValid() const noexcept {
  return _bucket != nullptr;
}

bool Table::BucketLocker::isLocked() const noexcept {
  TRI_ASSERT(!_locked || isValid());
  return _locked;
}

Table* Table::BucketLocker::source() const noexcept { return _source; }

template<typename BucketType>
BucketType& Table::BucketLocker::bucket() const {
  if (!isValid()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "attempted to dereference invalid bucket pointer");
  }
  return *reinterpret_cast<BucketType*>(_bucket);
}
template PlainBucket& Table::BucketLocker::bucket<PlainBucket>() const;
template TransactionalBucket& Table::BucketLocker::bucket<TransactionalBucket>()
    const;

void Table::BucketLocker::release() noexcept {
  if (isValid() && isLocked()) {
    _bucket->unlock();
    _locked = false;
  }
  _bucket = nullptr;
  _source = nullptr;
}

void Table::BucketLocker::steal(Table::BucketLocker&& other) noexcept {
  _bucket = other._bucket;
  _source = other._source;
  _locked = other._locked;
  other._bucket = nullptr;
  other._source = nullptr;
  other._locked = false;
}

Table::Table(std::uint32_t logSize)
    : _lock(),
      _disabled(true),
      _evictions(false),
      _logSize(std::min(logSize, kMaxLogSize)),
      _size(static_cast<std::uint64_t>(1) << _logSize),
      _shift(32 - _logSize),
      _mask(static_cast<std::uint32_t>((_size - 1) << _shift)),
      _buffer(new std::uint8_t[(_size * BUCKET_SIZE) + Table::padding]),
      _buckets(reinterpret_cast<GenericBucket*>(
          reinterpret_cast<std::uint64_t>((_buffer.get() + (BUCKET_SIZE - 1))) &
          ~(static_cast<std::uint64_t>(BUCKET_SIZE - 1)))),
      _auxiliary(nullptr),
      _bucketClearer(defaultClearer),
      _slotsTotal(_size),
      _slotsUsed(static_cast<std::uint64_t>(0)) {
  for (std::size_t i = 0; i < _size; i++) {
    // use placement new in order to properly initialize the bucket
    new (_buckets + i) GenericBucket();
  }
}

Table::~Table() {
  for (std::size_t i = 0; i < _size; i++) {
    // retrieve pointer to bucket
    GenericBucket* b = _buckets + i;
    // call dtor
    b->~GenericBucket();
  }
}

std::uint64_t Table::memoryUsage() const noexcept {
  return allocationSize(_logSize);
}

std::uint64_t Table::size() const noexcept { return _size; }

std::uint32_t Table::logSize() const noexcept { return _logSize; }

Table::BucketLocker Table::fetchAndLockBucket(std::uint32_t hash,
                                              std::uint64_t maxTries) {
  BucketLocker bucketGuard;

  SpinLocker guard(SpinLocker::Mode::Read, _lock,
                   static_cast<std::size_t>(maxTries));

  if (guard.isLocked()) {
    if (!_disabled) {
      bucketGuard =
          BucketLocker(&(_buckets[(hash & _mask) >> _shift]), this, maxTries);
      if (bucketGuard.isLocked()) {
        if (bucketGuard.bucket<GenericBucket>().isMigrated()) {
          bucketGuard.release();
          if (_auxiliary) {
            bucketGuard = _auxiliary->fetchAndLockBucket(hash, maxTries);
          }
        }
      }
    }
  }

  return bucketGuard;
}

std::shared_ptr<Table> Table::setAuxiliary(std::shared_ptr<Table> table) {
  if (table.get() != this) {
    SpinLocker guard(SpinLocker::Mode::Write, _lock);
    if (table == nullptr || _auxiliary == nullptr) {
      std::swap(_auxiliary, table);
    }
  }
  return table;
}

void* Table::primaryBucket(uint64_t index) noexcept {
  if (!isEnabled()) {
    return nullptr;
  }
  return &(_buckets[index]);
}

std::unique_ptr<Table::Subtable> Table::auxiliaryBuckets(std::uint32_t index) {
  if (!isEnabled()) {
    return std::unique_ptr<Subtable>();
  }
  GenericBucket* base;
  std::uint64_t size;
  std::uint32_t mask;
  std::uint32_t shift;

  std::shared_ptr<Table> source;
  {
    SpinLocker guard(SpinLocker::Mode::Read, _lock);
    source = _auxiliary->shared_from_this();
    TRI_ASSERT(_auxiliary.get() != nullptr);
    if (_logSize > _auxiliary->_logSize) {
      std::uint32_t diff = _logSize - _auxiliary->_logSize;
      base = &(_auxiliary->_buckets[index >> diff]);
      size = 1;
      mask = 0;
      shift = 0;
    } else {
      std::uint32_t diff = _auxiliary->_logSize - _logSize;
      base = &(_auxiliary->_buckets[index << diff]);
      size = static_cast<std::uint64_t>(1) << diff;
      mask = (static_cast<std::uint32_t>(size - 1) << _auxiliary->_shift);
      shift = _auxiliary->_shift;
    }
  }

  return std::make_unique<Subtable>(std::move(source), base, size, mask, shift);
}

void Table::setTypeSpecifics(BucketClearer clearer,
                             std::size_t slotsPerBucket) {
  _bucketClearer = clearer;
  _slotsTotal = _size * static_cast<std::uint64_t>(slotsPerBucket);
}

void Table::clear() {
  TRI_ASSERT(_auxiliary == nullptr);
  if (_auxiliary != nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "unexpected auxiliary state");
  }
  disable();
  for (std::uint64_t i = 0; i < _size; i++) {
    _bucketClearer(&(_buckets[i]));
  }
  _slotsUsed = 0;
  _bucketClearer = Table::defaultClearer;
}

void Table::disable() noexcept {
  SpinLocker guard(SpinLocker::Mode::Write, _lock);
  _disabled = true;
}

void Table::enable() noexcept {
  SpinLocker guard(SpinLocker::Mode::Write, _lock);
  _disabled = false;
}

bool Table::isEnabled(std::uint64_t maxTries) noexcept {
  SpinLocker guard(SpinLocker::Mode::Read, _lock,
                   static_cast<std::size_t>(maxTries));
  return guard.isLocked() && !_disabled;
}

bool Table::slotFilled() noexcept {
  size_t i = _slotsUsed.fetch_add(1, std::memory_order_acq_rel);
  return ((static_cast<double>(i + 1) / static_cast<double>(_slotsTotal)) >
          Table::idealUpperRatio);
}

void Table::slotsFilled(std::uint64_t numSlots) noexcept {
  _slotsUsed.fetch_add(numSlots, std::memory_order_acq_rel);
}

bool Table::slotEmptied() noexcept {
  size_t i = _slotsUsed.fetch_sub(1, std::memory_order_acq_rel);
  TRI_ASSERT(i > 0);
  return (((static_cast<double>(i - 1) / static_cast<double>(_slotsTotal)) <
           Table::idealLowerRatio) &&
          (_logSize > Table::kMinLogSize));
}

void Table::slotsEmptied(std::uint64_t numSlots) noexcept {
  size_t previous = _slotsUsed.fetch_sub(numSlots, std::memory_order_acq_rel);
  TRI_ASSERT(numSlots <= previous);
}

void Table::signalEvictions() noexcept {
  SpinLocker guard(SpinLocker::Mode::Write, _lock);
  _evictions = true;
}

std::uint32_t Table::idealSize() noexcept {
  bool forceGrowth = false;
  {
    SpinLocker guard(SpinLocker::Mode::Write, _lock);
    forceGrowth = _evictions;
    _evictions = false;
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

void Table::defaultClearer(void* /*ptr*/) {
  throw std::invalid_argument("must register a clearer");
}

}  // namespace arangodb::cache
