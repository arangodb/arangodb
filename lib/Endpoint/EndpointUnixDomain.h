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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/operating-system.h"
#include "Basics/socket-utils.h"
#include "Endpoint/Endpoint.h"

#ifdef ARANGODB_HAVE_DOMAIN_SOCKETS

#include <sys/socket.h>
#include <string>

namespace arangodb {
class EndpointUnixDomain final : public Endpoint {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates an endpoint
  //////////////////////////////////////////////////////////////////////////////

  EndpointUnixDomain(EndpointType, int, std::string const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief destroys an endpoint
  //////////////////////////////////////////////////////////////////////////////

  ~EndpointUnixDomain();

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief connect the endpoint
  //////////////////////////////////////////////////////////////////////////////

  TRI_socket_t connect(double, double) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief disconnect the endpoint
  //////////////////////////////////////////////////////////////////////////////

  // cppcheck-suppress virtualCallInConstructor; bogus
  void disconnect() override;

  int domain() const override { return AF_UNIX; }
  int port() const override { return 0; }
  std::string host() const override { return "localhost"; }
  std::string hostAndPort() const override { return "localhost"; }
  std::string path() { return _path; }

 private:
  std::string const _path;
};
}  // namespace arangodb

#endif
