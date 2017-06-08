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
#include "Basics/Common.h"
#include "Cache/Cache.h"
#include "Cache/Manager.h"
#include "Cache/State.h"

#include <stdint.h>
#include <algorithm>
#include <atomic>

using namespace arangodb::cache;

Metadata::Metadata()
    : fixedSize(0),
      tableSize(0),
      maxSize(0),
      allocatedSize(0),
      deservedSize(0),
      usage(0),
      softUsageLimit(0),
      hardUsageLimit(0),
      _state() {}

Metadata::Metadata(uint64_t usageLimit, uint64_t fixed, uint64_t table,
                   uint64_t max)
    : fixedSize(fixed),
      tableSize(table),
      maxSize(max),
      allocatedSize(usageLimit + fixed + table + Manager::cacheRecordOverhead),
      deservedSize(allocatedSize),
      usage(0),
      softUsageLimit(usageLimit),
      hardUsageLimit(usageLimit),
      _state() {
  TRI_ASSERT(allocatedSize <= maxSize);
}

Metadata::Metadata(Metadata const& other)
    : fixedSize(other.fixedSize),
      tableSize(other.tableSize),
      maxSize(other.maxSize),
      allocatedSize(other.allocatedSize),
      deservedSize(other.deservedSize),
      usage(other.usage),
      softUsageLimit(other.softUsageLimit),
      hardUsageLimit(other.hardUsageLimit),
      _state(other._state) {}

Metadata& Metadata::operator=(Metadata const& other) {
  if (this != &other) {
    _state = other._state;
    fixedSize = other.fixedSize;
    tableSize = other.tableSize;
    maxSize = other.maxSize;
    allocatedSize = other.allocatedSize;
    deservedSize = other.deservedSize;
    usage = other.usage;
    softUsageLimit = other.softUsageLimit;
    hardUsageLimit = other.hardUsageLimit;
  }

  return *this;
}

void Metadata::lock() { _state.lock(); }

void Metadata::unlock() {
  TRI_ASSERT(isLocked());
  _state.unlock();
}

bool Metadata::isLocked() const { return _state.isLocked(); }

bool Metadata::adjustUsageIfAllowed(int64_t usageChange) {
  TRI_ASSERT(isLocked());

  if (usageChange < 0) {
    usage -= static_cast<uint64_t>(-usageChange);
    return true;
  }

  if ((static_cast<uint64_t>(usageChange) + usage <= softUsageLimit) ||
      ((usage > softUsageLimit) &&
       (static_cast<uint64_t>(usageChange) + usage <= hardUsageLimit))) {
    usage += static_cast<uint64_t>(usageChange);
    return true;
  }

  return false;
}

bool Metadata::adjustLimits(uint64_t softLimit, uint64_t hardLimit) {
  TRI_ASSERT(isLocked());
  uint64_t fixed = tableSize + fixedSize + Manager::cacheRecordOverhead;
  auto approve = [&]() -> bool {
    softUsageLimit = softLimit;
    hardUsageLimit = hardLimit;
    allocatedSize = hardUsageLimit + fixed;

    return true;
  };

  // special case: start shrink to minimum, ignore deserved/max (table may be
  // too big, should shrink during process)
  if ((softLimit == Cache::minSize) && hardLimit == hardUsageLimit) {
    return approve();
  }

  // special case: finalize shrinking case above
  if ((softLimit == Cache::minSize) && (hardLimit == Cache::minSize) &&
      (usage < hardLimit)) {
    return approve();
  }

  // general case: start shrinking
  if ((hardLimit == hardUsageLimit) && (softLimit < hardLimit) &&
      (softLimit + fixed <= std::min(deservedSize, maxSize))) {
    return approve();
  }

  // general case: finish shrinking
  if ((softLimit == softUsageLimit) && (softLimit == hardLimit) &&
      (usage <= hardLimit)) {
    return approve();
  }

  // general case: adjust both above usage but below deserved/maxSize
  if ((softLimit == hardLimit) && (usage <= hardLimit) &&
      ((hardLimit + fixed) <= std::min(deservedSize, maxSize))) {
    return approve();
  }

  return false;
}

uint64_t Metadata::adjustDeserved(uint64_t deserved) {
  TRI_ASSERT(isLocked());
  deservedSize = std::min(deserved, maxSize);
  return deservedSize;
}

uint64_t Metadata::newLimit() {
  TRI_ASSERT(isLocked());
  uint64_t fixed = fixedSize + tableSize + Manager::cacheRecordOverhead;
  return ((Cache::minSize + fixed) >= deservedSize)
             ? Cache::minSize
             : std::min((deservedSize - fixed), 4 * hardUsageLimit);
}

bool Metadata::migrationAllowed(uint64_t newTableSize) {
  TRI_ASSERT(isLocked());
  return (hardUsageLimit + fixedSize + newTableSize +
              Manager::cacheRecordOverhead <=
          std::min(deservedSize, maxSize));
}

void Metadata::changeTable(uint64_t newTableSize) {
  tableSize = newTableSize;
  allocatedSize =
      hardUsageLimit + fixedSize + tableSize + Manager::cacheRecordOverhead;
}

bool Metadata::isSet(State::Flag flag) const {
  TRI_ASSERT(isLocked());
  return _state.isSet(flag);
}

void Metadata::toggleFlag(State::Flag flag) {
  TRI_ASSERT(isLocked());
  _state.toggleFlag(flag);
}
