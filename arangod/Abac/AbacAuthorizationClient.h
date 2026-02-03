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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Basics/Result.h"

namespace arangodb {

namespace application_features {
class CommunicationFeaturePhase;
}

namespace abac {

/// @brief Effect returned by the authorization service
enum class AuthorizationEffect {
  kAllow,
  kDeny,
  kUnknown
};

/// @brief Response from the ABAC authorization service
struct AuthorizationResponse {
  AuthorizationEffect effect = AuthorizationEffect::kUnknown;
  std::string message;
};

/// @brief Client for communicating with the external ABAC authorization service
///
/// This client sends batch permission requests to an external microservice
/// and receives authorization decisions (allow/deny).
///
/// The request format follows the protobuf-based API:
/// [
///   {
///     "action": "core:ReadCollection",
///     "resource": "arangodb:dbname:collection",
///     "context": {
///       "parameters": {
///         "read": {"values": ["[\"attr1\"]", "[\"attr2\",\"nested\"]"]}
///       }
///     }
///   },
///   ...
/// ]
///
/// The response format:
/// {
///   "effect": "ALLOW" | "DENY",
///   "message": "optional message"
/// }
class AbacAuthorizationClient {
 public:
  /// @brief Configuration for the authorization client
  struct Config {
    /// @brief Endpoint URL (e.g., "ssl://auth-service.example.com:8443")
    std::string endpoint;

    /// @brief Path for the authorization API (e.g., "/api/v1/authorize")
    std::string apiPath = "/api/v1/permissions/batch";

    /// @brief Request timeout in seconds
    double requestTimeout = 30.0;

    /// @brief Connection timeout in seconds
    double connectTimeout = 10.0;

    /// @brief Maximum number of connection retries
    size_t maxRetries = 3;

    /// @brief Whether to allow the query if the service is unreachable
    /// If true (fail open), queries proceed when auth service is down
    /// If false (fail closed), queries are denied when auth service is down
    bool failOpen = false;

    /// @brief JWT token for authenticating with the authorization service
    std::string jwtToken;
  };

  explicit AbacAuthorizationClient(
      application_features::CommunicationFeaturePhase& comm, Config config);

  ~AbacAuthorizationClient();

  /// @brief Check authorization for a batch of permission requests
  /// @param jsonPayload The JSON string containing the array of permission
  ///                    requests (as produced by
  ///                    AttributeDetector::getAbacRequestsJsonString())
  /// @param user The user making the request
  /// @param roles The roles assigned to the user
  /// @return Result with AuthorizationResponse on success, or error on failure
  Result checkAuthorization(std::string const& jsonPayload,
                            std::string const& user,
                            std::vector<std::string> const& roles,
                            AuthorizationResponse& response);

  /// @brief Check if the client is properly configured
  bool isConfigured() const;

  /// @brief Get the current configuration
  Config const& config() const { return _config; }

 private:
  /// @brief Parse the authorization response from the service
  Result parseResponse(std::string const& responseBody,
                       AuthorizationResponse& response);

  application_features::CommunicationFeaturePhase& _comm;
  Config _config;
};

}  // namespace abac
}  // namespace arangodb
