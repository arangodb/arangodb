////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
class LogTopic;
class Result;
namespace application_features {
class ApplicationServer;
}
namespace httpclient {
class SimpleHttpClient;
class SimpleHttpResult;
}  // namespace httpclient

/**
 * @brief Helper class providing utilities for client instances
 */
class ClientManager {
 public:
  /**
   * @brief Initialize a client manager using a specific log topic for output
   * @param topic Topic to log output to
   */
  explicit ClientManager(application_features::ApplicationServer& server, LogTopic& topic);
  virtual ~ClientManager();

 public:
  /**
   * @brief Initializes a client, connects to server, and verifies version
   *
   * If the client fails to connect to the server, or if the version is
   * mismatched, this will result in an error.
   *
   * @param  httpclient Output pointer will be set on success
   * @param  force      If true, an incompatible version will not result in an
   *                    error result
   * @param  logServerVersion     If true, output the server version to logs
   * @param  logDatabaseNotFound  If true, log errors when database was not
   * found
   * @return            Status code and possible error message
   */
  Result getConnectedClient(std::unique_ptr<httpclient::SimpleHttpClient>& httpClient,
                            bool force, bool logServerVersion,
                            bool logDatabaseNotFound, bool quiet);

  /**
   * @brief Initializes a client, connects to server, and verifies version
   *
   * If the client fails to connect to the server, or if the version is
   * mismatched, this will result in a fatal error which will terminate the
   * running program.
   *
   * @param  force   If true, an incompatible version will not result in a fatal
   *                 error exit condition
   * @param  logServerVersion     If true, output the server version to logs
   * @param  logDatabaseNotFound  If true, log errors when database was not
   * found
   * @return         A connected `SimpleHttpClient`
   */
  std::unique_ptr<httpclient::SimpleHttpClient> getConnectedClient(bool force, bool logServerVersion,
                                                                   bool logDatabaseNotFound);

  /**
   * @brief Conditionally prefixes a relative URI with database-specific path
   * @param  data     Pointer to `ClientFeature` instance
   * @param  location Relative URI to prefix, if it does not begin with "/_db/"
   * @return          Propertly prefixed URI
   */
  static std::string rewriteLocation(void* data, std::string const& location);

  /**
   * @brief Determines whether the ArangoDB instance is part of a cluster
   * @param  client Client to use for request
   * @return        status result; role name
   */
  std::pair<Result, std::string> getArangoIsCluster(httpclient::SimpleHttpClient& client);

  /**
   * Determines whether the ArangoDB instance is using the specified engine
   * @param  err    Reference to store error code if one occurs
   * @param  client Client to use for request
   * @param  name   Name of storage engine to check against
   * @return        status result; `true` if successful and specified engine is
   *                in use
   */
  std::pair<Result, bool> getArangoIsUsingEngine(httpclient::SimpleHttpClient& httpClient,
                                                 std::string const& name);

 private:
  application_features::ApplicationServer& _server;
  LogTopic& _topic;
};
}  // namespace arangodb

#endif
