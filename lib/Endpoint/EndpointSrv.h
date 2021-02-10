////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2016, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_ENDPOINT_ENDPOINT_SRV_H
#define ARANGODB_ENDPOINT_ENDPOINT_SRV_H 1

#include <memory>
#include <string>

#include "Basics/socket-utils.h"
#include "Endpoint/Endpoint.h"

namespace arangodb {
class EndpointSrv final : public Endpoint {
 public:
  explicit EndpointSrv(std::string const&);

  ~EndpointSrv();

 public:
  bool isConnected() const override;
  TRI_socket_t connect(double, double) override;
  void disconnect() override;
  int domain() const override;
  int port() const override;
  std::string host() const override;
  std::string hostAndPort() const override;

 private:
  std::unique_ptr<Endpoint> _endpoint;
};
}  // namespace arangodb

#endif
