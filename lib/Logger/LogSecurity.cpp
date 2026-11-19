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

#include <array>
#include <bit>
#include <ostream>

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

// calls `cb` with the classifier of every known category in `bits`,
// in ascending bit order
template<typename F>
void forEachClassifier(std::uint16_t bits, F&& cb) {
  bits &= kAllBits;
  while (bits != 0) {
    cb(kClassifiers[std::countr_zero(bits)]);
    bits &= bits - 1;  // clear lowest set bit
  }
}

}  // namespace

std::string_view securityClassifier(Security security) noexcept {
  auto bits = static_cast<std::uint16_t>(security);
  if (std::has_single_bit(bits) && (bits & kAllBits) != 0) {
    return kClassifiers[std::countr_zero(bits)];
  }
  return {};
}

void appendSecurityClassifiers(std::string& out, Security security) {
  auto bits = static_cast<std::uint16_t>(security);
  // fast path: exactly one category, which is by far the most common case
  if (std::has_single_bit(bits) && (bits & kAllBits) != 0) {
    out.append(kClassifiers[std::countr_zero(bits)]);
    return;
  }
  bool first = true;
  forEachClassifier(bits, [&](std::string_view classifier) {
    if (!first) {
      out.append(kSecurityClassifierSeparator);
    }
    first = false;
    out.append(classifier);
  });
}

std::string securityClassifiers(Security security) {
  std::string result;
  appendSecurityClassifiers(result, security);
  return result;
}

std::ostream& operator<<(std::ostream& out, Security security) {
  bool first = true;
  forEachClassifier(static_cast<std::uint16_t>(security),
                    [&](std::string_view classifier) {
                      if (!first) {
                        out << kSecurityClassifierSeparator;
                      }
                      first = false;
                      out << classifier;
                    });
  return out;
}

}  // namespace arangodb
