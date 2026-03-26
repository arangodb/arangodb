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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <cstdint>

#include <fuerte/ApiVersion.h>

namespace arangodb {

/// @brief Central configuration for API versioning
struct ApiVersion {
  // The list of all supported API versions (both stable and deprecated)
  static constexpr uint32_t supportedApiVersions[] = {0};

  // The list of deprecated API versions (subset of supportedApiVersions)
  static constexpr uint32_t deprecatedApiVersions[] = {};

  // The default API version used when no /_arango/vX prefix is specified.
  // Value is defined in fuerte so that fuerte can use it without depending on
  // server headers.
  static constexpr uint32_t defaultApiVersion =
      fuerte::ApiVersion::defaultApiVersion;

  // The experimental API version (accessed via /_arango/experimental).
  // Value is defined in fuerte so that fuerte can use it without depending on
  // server headers.
  static constexpr uint32_t experimentalApiVersion =
      fuerte::ApiVersion::experimentalApiVersion;

  // Helper function to get the number of supported API versions
  static constexpr size_t numSupportedApiVersions() {
    return sizeof(supportedApiVersions) / sizeof(supportedApiVersions[0]);
  }

  // Helper function to check if an API version is supported
  static constexpr bool isApiVersionSupported(uint32_t version) {
    for (size_t i = 0; i < numSupportedApiVersions(); ++i) {
      if (supportedApiVersions[i] == version) {
        return true;
      }
    }
    return false;
  }

  // Helper function to check if an API version is deprecated
  static constexpr bool isApiVersionDeprecated(uint32_t version) {
    constexpr size_t numDeprecated =
        sizeof(deprecatedApiVersions) / sizeof(deprecatedApiVersions[0]);
    for (size_t i = 0; i < numDeprecated; ++i) {
      if (deprecatedApiVersions[i] == version) {
        return true;
      }
    }
    return false;
  }

  // Helper function to get the maximum API version
  // This is the maximum of all supported/deprecated versions and the
  // experimental version
  static constexpr uint32_t maxApiVersion() {
    uint32_t maxVersion = experimentalApiVersion;
    for (size_t i = 0; i < numSupportedApiVersions(); ++i) {
      if (supportedApiVersions[i] > maxVersion) {
        maxVersion = supportedApiVersions[i];
      }
    }
    return maxVersion;
  }
};

}  // namespace arangodb
