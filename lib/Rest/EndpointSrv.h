////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_REST_ENDPOINT_SRV_H
#define ARANGODB_REST_ENDPOINT_SRV_H 1

#include "Rest/Endpoint.h"

namespace arangodb {
namespace rest {
class EndpointSrv final : public Endpoint {
 public:
  explicit EndpointSrv(std::string const&);

  ~EndpointSrv();

 public:
  bool isConnected() const override;
  TRI_socket_t connect(double, double) override;
  void disconnect() override;
  bool initIncoming(TRI_socket_t) override;
  int getDomain() const override;
  int getPort() const override;
  std::string getHost() const override;
  std::string getHostString() const override;

 private:
  std::unique_ptr<Endpoint> _endpoint;
};
}
}

#endif
