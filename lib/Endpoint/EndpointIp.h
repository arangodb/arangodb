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
/// @author Dr. Oreste Costa-Panaia
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_ENDPOINT_ENDPOINT_IP_H
#define ARANGODB_ENDPOINT_ENDPOINT_IP_H 1

#include <cstdint>
#include <string>

#include "Basics/socket-utils.h"
#include "Endpoint/Endpoint.h"

struct addrinfo;

namespace arangodb {
class EndpointIp : public Endpoint {
 protected:
  EndpointIp(DomainType, EndpointType, TransportType, EncryptionType, int, bool,
             std::string const&, uint16_t const);

 public:
  ~EndpointIp();

 public:
  static uint16_t const _defaultPortHttp;
  static uint16_t const _defaultPortVst;
  static char const* _defaultHost;

 private:
  TRI_socket_t connectSocket(addrinfo const*, double, double);

 public:
  TRI_socket_t connect(double, double) override final;
  // cppcheck-suppress virtualCallInConstructor; bogus
  void disconnect() override final;
  bool initIncoming(TRI_socket_t) override final;

  int port() const override { return _port; }
  std::string host() const override { return _host; }

  bool reuseAddress() const { return _reuseAddress; }

 private:
  std::string const _host;
  uint16_t const _port;
  bool const _reuseAddress;
};
}  // namespace arangodb

#endif
