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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdlib.h>

#include <boost/algorithm/string.hpp>

#include <fuerte/fuerte.h>

namespace f = ::arangodb::fuerte;

extern std::string myEndpoint;

// setupEndpointFromEnv configures the given connection builder
// with endpoint settings specified in the environment.
inline void setupEndpointFromEnv(f::ConnectionBuilder& cbuilder) {
  char const* tmp = getenv("TEST_ENDPOINT");
  std::string str;
  if (!tmp) {  // No settings in environment, set a default
    str = myEndpoint;
  } else {
    str.assign(tmp);
  }
  //  std::cout << "Test running against endpoint '" << str << "'" << std::endl;
  cbuilder.endpoint(str);
}

// setupAuthenticationFromEnv configures the given connection builder
// with authentication settings specified in the environment.
extern std::string myAuthentication;

inline void setupAuthenticationFromEnv(f::ConnectionBuilder& cbuilder) {
  char const* tmp = getenv("TEST_AUTHENTICATION");
  std::string auth;
  if (!tmp) {
    // No authentication settings in environment, set a default
    auth = myAuthentication;
  } else {
    auth.assign(tmp);
  }

  std::vector<std::string> parts;
  boost::split(parts, auth, boost::is_any_of(":"));
  if (parts[0] == "basic") {
    if (parts.size() != 3) {
      throw std::invalid_argument(
          "Expected username & password for basic authentication");
    }
    cbuilder.authenticationType(f::AuthenticationType::Basic);
    cbuilder.user(parts[1]);
    cbuilder.password(parts[2]);
  } else if (parts[0] == "jwt") {
    if (parts.size() != 3) {
      throw std::invalid_argument(
          "Expected username & password for jwt authentication");
    }
    cbuilder.authenticationType(f::AuthenticationType::Jwt);
    cbuilder.user(parts[1]);
    cbuilder.password(parts[2]);
  } else {
    throw std::invalid_argument("Unknown authentication: " + parts[0]);
  }
}
