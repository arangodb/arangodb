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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace arangodb {

struct GeneralServerOptions {
  double keepAliveTimeout = 300.0;
  uint64_t telemetricsMaxRequestsPerInterval = 3;
  bool startedListening = false;
  bool allowEarlyConnections = false;
  bool handleContentEncodingForUnauthenticatedRequests = false;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  bool enableTelemetrics = false;
#else
  bool enableTelemetrics = true;
#endif
  bool proxyCheck = true;
  bool returnQueueTimeHeader = true;
  bool permanentRootRedirect = true;
  uint64_t compressResponseThreshold = 0;
  std::vector<std::string> trustedProxies;
  std::vector<std::string> accessControlAllowOrigins;
  std::string redirectRootTo = "/_admin/aardvark/index.html";
  std::string supportInfoApiPolicy = "admin";
  std::string optionsApiPolicy = "jwt";
  uint64_t numIoThreads;  // initialized in ctor

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  std::vector<std::string> failurePoints;
#endif

  GeneralServerOptions();
};

}  // namespace arangodb