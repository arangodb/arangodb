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

#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>

namespace arangodb {

/// @brief security event category of a log message
/// (see
/// https://github.com/arangodb/platform-architecture/blob/main/adr/logging/LOG-001-security-log-event-categories.md).
/// Whether or not a log message is a security event at all is expressed
/// by passing a `std::optional<LogSecurity>` (`std::nullopt` meaning "not a
/// security event").
/// The values are single bits, so that a log message can be assigned to
/// multiple categories at once by OR-ing them, e.g.
/// `LogSecurity::Authentication | LogSecurity::SessionManagement`.
enum class LogSecurity : std::uint16_t {
  Authentication = 1 << 0,
  AccessControl = 1 << 1,
  AccountManagement = 1 << 2,
  SystemAccess = 1 << 3,
  ConfigurationChanges = 1 << 4,
  PrivilegedOperations = 1 << 5,
  DataAccess = 1 << 6,
  Violations = 1 << 7,
  ApiCalls = 1 << 8,
  SessionManagement = 1 << 9,
  AuditLogAccess = 1 << 10,
  SystemErrors = 1 << 11,
};

constexpr LogSecurity operator|(LogSecurity lhs, LogSecurity rhs) noexcept {
  return static_cast<LogSecurity>(static_cast<std::uint16_t>(lhs) |
                                  static_cast<std::uint16_t>(rhs));
}

constexpr LogSecurity& operator|=(LogSecurity& lhs, LogSecurity rhs) noexcept {
  return lhs = lhs | rhs;
}

/// @brief bit mask of all known security categories.
inline constexpr std::uint16_t kAllSecurityBits = static_cast<std::uint16_t>(
    (static_cast<std::uint16_t>(LogSecurity::SystemErrors) << 1) - 1);

/// @brief returns true if `security` contains at least one category and no
/// bits other than those of known categories.
constexpr bool isValidSecurity(LogSecurity security) noexcept {
  auto bits = static_cast<std::uint16_t>(security);
  return bits != 0 && (bits & ~kAllSecurityBits) == 0;
}

/// @brief separator between multiple classifiers of a security event that
/// belongs to more than one category.
inline constexpr std::string_view kSecurityClassifierSeparator = ",";

/// @brief appends the classifier of `security` to `out`, e.g.
/// "arango-security-event-authentication" for a single category, or
/// "arango-security-event-authentication,arango-security-event-session-management"
/// for a combination of categories. Multiple classifiers are ordered by bit
/// position (not by the order of OR-ing) and separated by
/// kSecurityClassifierSeparator. Unknown bits are ignored; if no known bit is
/// set, nothing is appended.
void appendSecurityClassifier(std::string& out, LogSecurity security);

/// @brief writes the classifier of `security` to `out`, in the same format
/// as appendSecurityClassifier().
std::ostream& operator<<(std::ostream& out, LogSecurity security);

}  // namespace arangodb
