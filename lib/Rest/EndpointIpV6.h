////////////////////////////////////////////////////////////////////////////////
/// @brief connection endpoint, IPv6-based
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_REST_ENDPOINT_IP_V6_H
#define LIB_REST_ENDPOINT_IP_V6_H 1

#include "Basics/Common.h"

#include "Rest/EndpointIp.h"

namespace triagens {
namespace rest {


class EndpointIpV6 final : public EndpointIp {
  
 public:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief creates an endpoint
  ////////////////////////////////////////////////////////////////////////////////

  EndpointIpV6(const EndpointType, const EncryptionType, const std::string&,
               int, bool, const std::string&, const uint16_t);

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief destroys an endpoint
  ////////////////////////////////////////////////////////////////////////////////

  ~EndpointIpV6();

  
 public:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief get endpoint domain
  ////////////////////////////////////////////////////////////////////////////////

  int getDomain() const { return AF_INET6; }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief get host string for HTTP requests
  ////////////////////////////////////////////////////////////////////////////////

  std::string getHostString() const {
    return '[' + getHost() + "]:" +
           triagens::basics::StringUtils::itoa(getPort());
  }
};
}
}

#endif


