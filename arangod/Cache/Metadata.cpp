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

#include "Cache/Metadata.h"
#include "Cache/Cache.h"
#include "Cache/State.h"

#include <atomic>
#include <cstdint>

using namespace arangodb::cache;

Metadata::Metadata(uint64_t limit)
    : _state(),
      _cache(nullptr),
      _usage(0),
      _softLimit(limit),
      _hardLimit(limit),
      _table(nullptr),
      _auxiliaryTable(nullptr),
      _logSize(0),
      _auxiliaryLogSize(0) {}

Metadata::Metadata(Metadata const& other)
    : _state(other._state),
      _cache(other._cache),
      _usage(other._usage),
      _softLimit(other._softLimit),
      _hardLimit(other._hardLimit),
      _table(other._table),
      _auxiliaryTable(other._auxiliaryTable),
      _logSize(other._logSize),
      _auxiliaryLogSize(other._auxiliaryLogSize) {}

void Metadata::link(std::shared_ptr<Cache> cache) {
  lock();
  _cache = cache;
  unlock();
}

void Metadata::lock() { _state.lock(); }

void Metadata::unlock() {
  TRI_ASSERT(isLocked());
  _state.unlock();
}

bool Metadata::isLocked() const { return _state.isLocked(); }

std::shared_ptr<Cache> Metadata::cache() const {
  TRI_ASSERT(isLocked());
  return _cache;
}

uint32_t Metadata::logSize() const {
  TRI_ASSERT(isLocked());
  return _logSize;
}

uint32_t Metadata::auxiliaryLogSize() const {
  TRI_ASSERT(isLocked());
  return _auxiliaryLogSize;
}

uint8_t* Metadata::table() const {
  TRI_ASSERT(isLocked());
  return _table;
}

uint8_t* Metadata::auxiliaryTable() const {
  TRI_ASSERT(isLocked());
  return _auxiliaryTable;
}

uint64_t Metadata::usage() const {
  TRI_ASSERT(isLocked());
  return _usage;
}

uint64_t Metadata::softLimit() const {
  TRI_ASSERT(isLocked());
  return _softLimit;
}

uint64_t Metadata::hardLimit() const {
  TRI_ASSERT(isLocked());
  return _hardLimit;
}

bool Metadata::adjustUsageIfAllowed(int64_t usageChange) {
  TRI_ASSERT(isLocked());

  if (usageChange < 0) {
    _usage -= static_cast<uint64_t>(-usageChange);
    return true;
  }

  if ((static_cast<uint64_t>(usageChange) + _usage <= _softLimit) ||
      ((_usage > _softLimit) &&
       (static_cast<uint64_t>(usageChange) + _usage <= _hardLimit))) {
    _usage += static_cast<uint64_t>(usageChange);
    return true;
  }

  return false;
}

bool Metadata::adjustLimits(uint64_t softLimit, uint64_t hardLimit) {
  TRI_ASSERT(isLocked());

  if (hardLimit < _usage) {
    return false;
  }

  _softLimit = softLimit;
  _hardLimit = hardLimit;

  return true;
}

void Metadata::grantAuxiliaryTable(uint8_t* table, uint32_t logSize) {
  TRI_ASSERT(isLocked());
  _auxiliaryTable = table;
  _auxiliaryLogSize = logSize;
}

void Metadata::swapTables() {
  TRI_ASSERT(isLocked());
  std::swap(_table, _auxiliaryTable);
  std::swap(_logSize, _auxiliaryLogSize);
}

uint8_t* Metadata::releaseTable() {
  TRI_ASSERT(isLocked());
  uint8_t* table = _table;
  _table = nullptr;
  _logSize = 0;
  return table;
}

uint8_t* Metadata::releaseAuxiliaryTable() {
  TRI_ASSERT(isLocked());
  uint8_t* table = _auxiliaryTable;
  _auxiliaryTable = nullptr;
  _auxiliaryLogSize = 0;
  return table;
}

bool Metadata::isSet(State::Flag flag) const {
  TRI_ASSERT(isLocked());
  return _state.isSet(flag);
}

void Metadata::toggleFlag(State::Flag flag) {
  TRI_ASSERT(isLocked());
  _state.toggleFlag(flag);
}
