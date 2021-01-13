////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "Cache/Metadata.h"

#include "Basics/debugging.h"
#include "Cache/Cache.h"
#include "Cache/Manager.h"
#include "Logger/LogMacros.h"

#include <algorithm>
#include <atomic>
#include <cstdint>

namespace arangodb::cache {

Metadata::Metadata()
    : fixedSize(0),
      tableSize(0),
      maxSize(0),
      allocatedSize(0),
      deservedSize(0),
      usage(0),
      softUsageLimit(0),
      hardUsageLimit(0),
      _lock(),
      _migrating(false),
      _resizing(false) {}

Metadata::Metadata(uint64_t usageLimit, std::uint64_t fixed,
                   std::uint64_t table, std::uint64_t max)
    : fixedSize(fixed),
      tableSize(table),
      maxSize(max),
      allocatedSize(usageLimit + fixed + table + Manager::cacheRecordOverhead),
      deservedSize(allocatedSize),
      usage(0),
      softUsageLimit(usageLimit),
      hardUsageLimit(usageLimit),
      _lock(),
      _migrating(false),
      _resizing(false) {
  TRI_ASSERT(allocatedSize <= maxSize);
}

Metadata::Metadata(Metadata&& other) noexcept
    : fixedSize(other.fixedSize),
      tableSize(other.tableSize),
      maxSize(other.maxSize),
      allocatedSize(other.allocatedSize),
      deservedSize(other.deservedSize),
      usage(other.usage.load()),
      softUsageLimit(other.softUsageLimit),
      hardUsageLimit(other.hardUsageLimit),
      _lock(std::move(other._lock)),
      _migrating(other._migrating),
      _resizing(other._resizing) {}

Metadata& Metadata::operator=(Metadata&& other) noexcept {
  if (this != &other) {
    _lock = std::move(other._lock);
    _migrating = other._migrating;
    _resizing = other._resizing;
    fixedSize = other.fixedSize;
    tableSize = other.tableSize;
    maxSize = other.maxSize;
    allocatedSize = other.allocatedSize;
    deservedSize = other.deservedSize;
    usage = other.usage.load();
    softUsageLimit = other.softUsageLimit;
    hardUsageLimit = other.hardUsageLimit;
  }

  return *this;
}

basics::ReadWriteSpinLock& Metadata::lock() const { return _lock; }

bool Metadata::adjustUsageIfAllowed(std::int64_t usageChange) noexcept {
  while (true) {
    std::uint64_t expected = usage.load(std::memory_order_acquire);
    std::uint64_t desired =
        (usageChange < 0) ? expected - static_cast<std::uint64_t>(-usageChange)
                          : expected + static_cast<std::uint64_t>(usageChange);

    if ((desired > hardUsageLimit) ||
        ((expected <= softUsageLimit) && (desired > softUsageLimit))) {
      return false;
    }

    bool success = usage.compare_exchange_weak(expected, desired, std::memory_order_acq_rel,
                                               std::memory_order_relaxed);
    if (success) {
      break;
    }
  }

  return true;
}

bool Metadata::adjustLimits(std::uint64_t softLimit, std::uint64_t hardLimit) noexcept {
  TRI_ASSERT(_lock.isLockedWrite());
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
  if ((softLimit == Cache::minSize) && (hardLimit == Cache::minSize) && (usage <= hardLimit)) {
    return approve();
  }

  // general case: start shrinking
  if ((hardLimit == hardUsageLimit) && (softLimit < hardLimit) &&
      (softLimit + fixed <= std::min(deservedSize, maxSize))) {
    return approve();
  }

  // general case: finish shrinking
  if ((softLimit == softUsageLimit) && (softLimit == hardLimit) && (usage <= hardLimit)) {
    return approve();
  }

  // general case: adjust both above usage but below deserved/maxSize
  if ((softLimit == hardLimit) && (usage <= hardLimit) &&
      ((hardLimit + fixed) <= std::min(deservedSize, maxSize))) {
    return approve();
  }

  return false;
}

std::uint64_t Metadata::adjustDeserved(std::uint64_t deserved) noexcept {
  TRI_ASSERT(_lock.isLockedWrite());
  deservedSize = std::min(deserved, maxSize);
  return deservedSize;
}

std::uint64_t Metadata::newLimit() const noexcept {
  TRI_ASSERT(_lock.isLocked());
  std::uint64_t fixed = fixedSize + tableSize + Manager::cacheRecordOverhead;
  return ((Cache::minSize + fixed) >= deservedSize)
             ? Cache::minSize
             : std::min((deservedSize - fixed), 4 * hardUsageLimit);
}

bool Metadata::migrationAllowed(std::uint64_t newTableSize) noexcept {
  TRI_ASSERT(_lock.isLocked());
  return (hardUsageLimit + fixedSize + newTableSize + Manager::cacheRecordOverhead <=
          std::min(deservedSize, maxSize));
}

void Metadata::changeTable(std::uint64_t newTableSize) noexcept {
  TRI_ASSERT(_lock.isLockedWrite());
  tableSize = newTableSize;
  allocatedSize = hardUsageLimit + fixedSize + tableSize + Manager::cacheRecordOverhead;
}

}  // namespace arangodb::cache