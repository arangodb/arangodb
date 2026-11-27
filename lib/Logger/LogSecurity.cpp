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
#include <string>

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

static_assert(static_cast<std::uint16_t>(LogSecurity::SystemErrors) ==
                  (1u << (kClassifiers.size() - 1)),
              "kClassifiers must have one entry per LogSecurity category");

static_assert(kAllSecurityBits == (1u << kClassifiers.size()) - 1,
              "kAllSecurityBits must cover exactly the LogSecurity categories");

// calls `append(part)` for each classifier of `security` and for each
// separator between them, in order of ascending bit position. unknown bits
// are ignored.
template<typename F>
void forEachClassifierPart(LogSecurity security, F&& append) {
  auto bits = static_cast<std::uint16_t>(static_cast<std::uint16_t>(security) &
                                         kAllSecurityBits);
  bool first = true;
  while (bits != 0) {
    if (!first) {
      append(kSecurityClassifierSeparator);
    }
    first = false;
    append(kClassifiers[std::countr_zero(bits)]);
    bits &= bits - 1;  // clear lowest set bit
  }
}

}  // namespace

void appendSecurityClassifier(std::string& out, LogSecurity security) {
  forEachClassifierPart(security,
                        [&out](std::string_view part) { out.append(part); });
}

}  // namespace arangodb
