////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

#include "Logger/LogSecurity.h"

#include "Basics/ReadLocker.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/WriteLocker.h"

#include <array>
#include <bit>
#include <ostream>
#include <string>
#include <unordered_map>

namespace arangodb {

namespace {

// indexed by bit position of the category
constexpr std::array<std::string_view, 12> kClassifiers = {
    "arango-security-event-authentication",
    "arango-security-event-access-control",
    "arango-security-event-account-management",
    "arango-security-event-system-access",
    "arango-security-event-configuration-changes",
    "arango-security-event-privileged-operations",
    "arango-security-event-data-access",
    "arango-security-event-violations",
    "arango-security-event-api-calls",
    "arango-security-event-session-management",
    "arango-security-event-audit-log-access",
    "arango-security-event-system-errors",
};

static_assert(static_cast<std::uint16_t>(Security::SystemErrors) ==
                  (1u << (kClassifiers.size() - 1)),
              "kClassifiers must have one entry per Security category");

constexpr std::uint16_t kAllBits =
    static_cast<std::uint16_t>((1u << kClassifiers.size()) - 1);

// lazily populated cache of classifier strings for combinations of more
// than one category. entries are never removed, and since std::unordered_map
// is node-based, neither rehashing nor inserting moves existing elements, so
// the characters of a stored string never move and string_views into them
// stay valid forever.
struct CombinationCache {
  basics::ReadWriteLock lock;
  std::unordered_map<std::uint16_t, std::string> classifiers;
};

CombinationCache& combinationCache() {
  // intentionally leaked, so that the cache (and string_views into it)
  // remain valid during static destruction, when logging may still happen.
  static CombinationCache cache;
  return cache;
}

std::string buildCombination(std::uint16_t bits) {
  std::string result;
  while (bits != 0) {
    if (!result.empty()) {
      result.append(kSecurityClassifierSeparator);
    }
    result.append(kClassifiers[std::countr_zero(bits)]);
    bits &= bits - 1;  // clear lowest set bit
  }
  return result;
}

}  // namespace

std::string_view securityClassifier(Security security) {
  auto bits = static_cast<std::uint16_t>(static_cast<std::uint16_t>(security) &
                                         kAllBits);

  // fast path: exactly one category, which is by far the most common case
  if (std::has_single_bit(bits)) {
    return kClassifiers[std::countr_zero(bits)];
  }
  if (bits == 0) {
    return {};
  }

  auto& cache = combinationCache();
  {
    READ_LOCKER(guard, cache.lock);
    if (auto it = cache.classifiers.find(bits); it != cache.classifiers.end()) {
      return it->second;
    }
  }

  WRITE_LOCKER(guard, cache.lock);
  // recheck: another thread may have added the combination between our
  // releasing the read lock and acquiring the write lock.
  if (auto it = cache.classifiers.find(bits); it != cache.classifiers.end()) {
    return it->second;
  }
  // build the string before inserting, so that an exception cannot leave an
  // empty entry behind.
  return cache.classifiers.emplace(bits, buildCombination(bits)).first->second;
}

std::ostream& operator<<(std::ostream& out, Security security) {
  return out << securityClassifier(security);
}

}  // namespace arangodb
