////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_ENDPOINT_ENDPOINT_IP_V4_H
#define ARANGODB_ENDPOINT_ENDPOINT_IP_V4_H 1

#include "Basics/Common.h"
#include "Basics/StringUtils.h"
#include "Endpoint/EndpointIp.h"

namespace arangodb {

class EndpointIpV4 final : public EndpointIp {
 public:
  EndpointIpV4(EndpointType, TransportType, EncryptionType, int, bool,
               std::string const&, uint16_t);

 public:
  int domain() const override { return AF_INET; }

  std::string hostAndPort() const override {
    return host() + ':' + arangodb::basics::StringUtils::itoa(port());
  }
};
}  // namespace arangodb

#endif
