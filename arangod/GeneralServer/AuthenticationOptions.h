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

#pragma once

#include <string>
#include <vector>

namespace arangodb {

// TODO Should be renamed to AuthOptions, as it contains both authentication and
//      authorization options.
struct AuthenticationOptions {
  bool authenticationUnixSockets = true;
  bool authenticationSystemOnly = true;
  bool active = true;
  std::string externalRBACservice;  // empty string means deactivated RBAC
  double authenticationTimeout = 0.0;
  double sessionTimeout = 3600.0;        // 1 hour in seconds
  double minimalJwtExpiryTime = 10.0;    // 10 seconds
  double maximalJwtExpiryTime = 3600.0;  // 3600 seconds

  std::string jwtSecretProgramOption;
  std::string jwtSecretKeyfileProgramOption;
  std::string jwtSecretFolderProgramOption;
  bool jwtSecretIsES256 = false;  // true if the active secret uses ES256

#ifdef USE_ENTERPRISE
  /// verification only secrets
  std::vector<std::string> jwtPassiveSecrets;
#endif
};

}  // namespace arangodb
