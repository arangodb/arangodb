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

#include "RestServer/FedRAMPHigh.h"

#include "Logger/LoggerOptions.h"

#include <string>

namespace arangodb {

ResultT<bool> isFedRAMPHighEnabled(std::string_view value) {
  if (value == "on") {
    return true;
  }
  if (value == "off") {
    return false;
  }
  return Result{TRI_ERROR_BAD_PARAMETER,
                "invalid value '" + std::string{value} +
                    "' for environment variable '" + kFedRAMPHighEnvVar +
                    "'. allowed values are 'on' and 'off'"};
}

void applyFedRAMPHighLoggerDefaults(LoggerOptions& options) noexcept {
  options.alwaysLogSecurityMessages = true;
}

Result checkFedRAMPHighLoggerOptions(LoggerOptions const& options) {
  if (!options.alwaysLogSecurityMessages) {
    return {TRI_ERROR_BAD_PARAMETER,
            "'--log.always-log-security-messages' must not be set to false "
            "in FedRAMP High mode"};
  }
  return {};
}

}  // namespace arangodb
