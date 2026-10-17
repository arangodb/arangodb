////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "Cache/Metadata.h"

#include "Basics/debugging.h"
#include "Cache/Cache.h"
#include "Cache/Manager.h"

#include <algorithm>
#include <limits>

namespace arangodb::cache {

Metadata::Metadata() noexcept
    : fixedSize(0),
      tableSize(0),
      maxSize(0),
      allocatedSize(0),
      deservedSize(0),
      usage(0),
      softUsageLimit(0),
      hardUsageLimit(0),
      _migrating(false),
      _resizing(false) {}

Metadata::Metadata(std::uint64_t usageLimit, std::uint64_t fixed,
                   std::uint64_t tableSize, std::uint64_t max) noexcept
    : fixedSize(fixed),
      tableSize(tableSize),
      maxSize(max),
      allocatedSize(usageLimit + fixed + tableSize +
                    Manager::kCacheRecordOverhead),
      deservedSize(allocatedSize),
      usage(0),
      softUsageLimit(usageLimit),
      hardUsageLimit(usageLimit),
      _migrating(false),
      _resizing(false) {
  TRI_ASSERT(allocatedSize <= maxSize);
  checkInvariants();
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

basics::ReadWriteSpinLock& Metadata::lock() const noexcept { return _lock; }

bool Metadata::adjustUsageIfAllowed(std::int64_t usageChange) noexcept {
  while (true) {
    std::uint64_t expected = usage.load(std::memory_order_acquire);
    std::uint64_t desired;
    if (usageChange < 0) {
      // decreasing usage is always allowed, but must not underflow
      auto decrease = static_cast<std::uint64_t>(-usageChange);
      TRI_ASSERT(expected >= decrease);
      desired = (expected >= decrease) ? expected - decrease : 0;
    } else {
      desired = expected + static_cast<std::uint64_t>(usageChange);
      if ((desired < expected) || (desired > hardUsageLimit) ||
          ((expected <= softUsageLimit) && (desired > softUsageLimit))) {
        return false;
      }
    }

    bool success = usage.compare_exchange_weak(expected, desired,
                                               std::memory_order_acq_rel,
                                               std::memory_order_relaxed);
    if (success) {
      break;
    }
  }

  return true;
}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
void Metadata::checkInvariants() const noexcept {
  TRI_ASSERT(allocatedSize == hardUsageLimit + tableSize + fixedSize +
                                  Manager::kCacheRecordOverhead);
  TRI_ASSERT(allocatedSize <= maxSize);
}
#endif

bool Metadata::adjustLimits(std::uint64_t softLimit,
                            std::uint64_t hardLimit) noexcept {
  TRI_ASSERT(_lock.isLockedWrite());
  uint64_t fixed = tableSize + fixedSize + Manager::kCacheRecordOverhead;
  // overflow-safe check for (limit + fixed <= min(deservedSize, maxSize))
  auto fits = [&](std::uint64_t limit) -> bool {
    std::uint64_t bound = std::min(deservedSize, maxSize);
    return (bound >= fixed) && (limit <= bound - fixed);
  };
  auto approve = [&]() -> bool {
    softUsageLimit = softLimit;
    hardUsageLimit = hardLimit;
    allocatedSize = hardUsageLimit + fixed;
    checkInvariants();

    return true;
  };

  // special case: start shrink to minimum, ignore deserved/max (table may be
  // too big, should shrink during process)
  if ((softLimit == Cache::kMinSize) && hardLimit == hardUsageLimit) {
    return approve();
  }

  // special case: finalize shrinking case above
  if ((softLimit == Cache::kMinSize) && (hardLimit == Cache::kMinSize) &&
      (usage <= hardLimit)) {
    return approve();
  }

  // general case: start shrinking
  if ((hardLimit == hardUsageLimit) && (softLimit < hardLimit) &&
      fits(softLimit)) {
    return approve();
  }

  // general case: finish shrinking
  if ((softLimit == softUsageLimit) && (softLimit == hardLimit) &&
      (usage <= hardLimit)) {
    return approve();
  }

  // general case: adjust both above usage but below deserved/maxSize
  if ((softLimit == hardLimit) && (usage <= hardLimit) && fits(hardLimit)) {
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
  std::uint64_t fixed = fixedSize + tableSize + Manager::kCacheRecordOverhead;
  // cap the growth factor without overflowing
  constexpr std::uint64_t kMax = std::numeric_limits<std::uint64_t>::max();
  std::uint64_t maxGrowth =
      (hardUsageLimit > kMax / 4) ? kMax : 4 * hardUsageLimit;
  return ((Cache::kMinSize + fixed) >= deservedSize)
             ? Cache::kMinSize
             : std::min((deservedSize - fixed), maxGrowth);
}

bool Metadata::migrationAllowed(std::uint64_t newTableSize) noexcept {
  TRI_ASSERT(_lock.isLocked());
  std::uint64_t bound = std::min(deservedSize, maxSize);
  std::uint64_t fixed =
      fixedSize + newTableSize + Manager::kCacheRecordOverhead;
  return (bound >= fixed) && (hardUsageLimit <= bound - fixed);
}

void Metadata::changeTable(std::uint64_t newTableSize) noexcept {
  TRI_ASSERT(_lock.isLockedWrite());
  tableSize = newTableSize;
  allocatedSize =
      hardUsageLimit + fixedSize + tableSize + Manager::kCacheRecordOverhead;
  checkInvariants();
}

}  // namespace arangodb::cache
