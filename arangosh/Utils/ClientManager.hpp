////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOSH_UTILS_CLIENT_MANAGER_H
#define ARANGOSH_UTILS_CLIENT_MANAGER_H 1

#include <memory>
#include <string>

namespace arangodb {
class Result;
namespace httpclient {
class SimpleHttpClient;
class SimpleHttpResult;
}  // namespace httpclient

/**
 * @brief Helper class providing utilities for client instances
 */
class ClientManager {
 public:
  ClientManager();
  virtual ~ClientManager();

 public:

  /**
   * @brief Initailizes a client, connects to server, and verifies version
   *
   * If the client fails to connect to the server, or if the version is
   * mismatched, this will result in a fatal error which will terminate the
   * running program.
   *
   * @param  force   If true, an incompatible version will not result in a fatal
   *                 error exit condition
   * @param  verbose If true, output the server version to logs
   * @return         A connected `SimpleHttpClient`
   */
  std::unique_ptr<httpclient::SimpleHttpClient> getConnectedClient(
      bool force = false, bool verbose = false) noexcept;

 protected:

  /**
   * @brief Conditionally prefixes a relative URI with database-specific path
   * @param  data     Pointer to `ClientFeature` instance
   * @param  location Relative URI to prefix, if it does not begin with "/_db/"
   * @return          Propertly prefixed URI
   */
  static std::string rewriteLocation(void* data,
                                     std::string const& location) noexcept;

  /**
   * @brief Extracts error message [and number] from `SimpleHttpResult`
   * @param  result `SimpleHttpResult` to extract from
   * @param  err    Reference to set error number
   * @return        Extracted error message
   */
  static std::string getHttpErrorMessage(httpclient::SimpleHttpResult* result,
                                         int& err) noexcept;

  /**
   * @brief Determines whether the ArangoDB instance is part of a cluster
   * @param  err    Reference to store error code if one occurs
   * @param  client Client to use for request
   * @return        `true` if it is part of a cluster
   */
  bool getArangoIsCluster(int& err,
                          httpclient::SimpleHttpClient& client) noexcept;

  /**
   * Determines whether the ArangoDB instance is using the specified engine
   * @param  err    Reference to store error code if one occurs
   * @param  client Client to use for request
   * @param  name   Name of storage engine to check against
   * @return        `true` if specified engine is in use
   */
  bool getArangoIsUsingEngine(int& err,
                              httpclient::SimpleHttpClient& httpClient,
                              std::string const& name) noexcept;
};
}  // namespace arangodb

#endif
