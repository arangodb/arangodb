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
#include <string_view>

namespace arangodb {

/// @brief security event category of a log message
/// (see
/// https://github.com/arangodb/platform-architecture/blob/main/adr/logging/LOG-001-security-log-event-categories.md).
/// Whether or not a log message is a security event at all is expressed
/// by passing a `std::optional<Security>` (`std::nullopt` meaning "not a
/// security event").
enum class Security : std::uint8_t {
  Authentication,
  AccessControl,
  AccountManagement,
  SystemAccess,
  ConfigurationChanges,
  PrivilegedOperations,
  DataAccess,
  Violations,
  ApiCalls,
  SessionManagement,
  AuditLogAccess,
  SystemErrors,
};

/// @brief returns the classifier literal for a security event category,
/// e.g. "arango-security-event-authentication".
std::string_view securityClassifier(Security security) noexcept;

std::ostream& operator<<(std::ostream& out, Security security);

}  // namespace arangodb
