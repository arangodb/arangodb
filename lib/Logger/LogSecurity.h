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
/// by passing a `std::optional<Security>` (`std::nullopt` meaning "not a
/// security event").
/// The values are single bits, so that a log message can be assigned to
/// multiple categories at once by OR-ing them, e.g.
/// `Security::Authentication | Security::SessionManagement`.
enum class Security : std::uint16_t {
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

constexpr Security operator|(Security lhs, Security rhs) noexcept {
  return static_cast<Security>(static_cast<std::uint16_t>(lhs) |
                               static_cast<std::uint16_t>(rhs));
}

constexpr Security operator&(Security lhs, Security rhs) noexcept {
  return static_cast<Security>(static_cast<std::uint16_t>(lhs) &
                               static_cast<std::uint16_t>(rhs));
}

constexpr Security& operator|=(Security& lhs, Security rhs) noexcept {
  return lhs = lhs | rhs;
}

/// @brief returns true if `security` contains all bits of `category`.
constexpr bool hasSecurityCategory(Security security,
                                   Security category) noexcept {
  return (security & category) == category;
}

/// @brief separator between multiple classifiers of a security event that
/// belongs to more than one category.
inline constexpr std::string_view kSecurityClassifierSeparator = ",";

/// @brief returns the classifier literal for a single security event
/// category, e.g. "arango-security-event-authentication". If `security`
/// does not have exactly one known bit set, an empty string_view is
/// returned. Use appendSecurityClassifiers() / securityClassifiers() to
/// handle combinations of categories.
std::string_view securityClassifier(Security security) noexcept;

/// @brief appends the classifier literals of all categories contained in
/// `security` to `out`, separated by kSecurityClassifierSeparator, e.g.
/// "arango-security-event-authentication,arango-security-event-session-management".
/// Does not allocate an intermediate string; for a single category this is
/// a single append of a static literal.
void appendSecurityClassifiers(std::string& out, Security security);

/// @brief returns the classifier literals of all categories contained in
/// `security`, separated by kSecurityClassifierSeparator.
std::string securityClassifiers(Security security);

std::ostream& operator<<(std::ostream& out, Security security);

}  // namespace arangodb
