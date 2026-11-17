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

#include <ostream>

namespace arangodb {

std::string_view securityClassifier(Security security) noexcept {
  switch (security) {
    case Security::None:
      return {};
    case Security::Authentication:
      return "arango-security-event-authentication";
    case Security::AccessControl:
      return "arango-security-event-access-control";
    case Security::AccountManagement:
      return "arango-security-event-account-management";
    case Security::SystemAccess:
      return "arango-security-event-system-access";
    case Security::ConfigurationChanges:
      return "arango-security-event-configuration-changes";
    case Security::PrivilegedOperations:
      return "arango-security-event-privileged-operations";
    case Security::DataAccess:
      return "arango-security-event-data-access";
    case Security::Violations:
      return "arango-security-event-violations";
    case Security::ApiCalls:
      return "arango-security-event-api-calls";
    case Security::SessionManagement:
      return "arango-security-event-session-management";
    case Security::AuditLogAccess:
      return "arango-security-event-audit-log-access";
    case Security::SystemErrors:
      return "arango-security-event-system-errors";
  }
  return {};
}

std::ostream& operator<<(std::ostream& out, Security security) {
  return out << securityClassifier(security);
}

}  // namespace arangodb
