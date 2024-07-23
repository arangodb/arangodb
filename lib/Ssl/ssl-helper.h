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

#include <cstdint>
#include <string>
#include <unordered_set>

#include <boost/asio/ssl/context.hpp>

#include <openssl/opensslv.h>

#include "Basics/asio_ns.h"

namespace arangodb {
// SSL protocol methods
enum SslProtocol {
  SSL_UNKNOWN = 0,
  // newer versions of OpenSSL do not support SSLv2 by default.
  // from https://www.openssl.org/news/cl110.txt:
  //   Changes between 1.0.2f and 1.0.2g [1 Mar 2016]
  //   * Disable SSLv2 default build, default negotiation and weak ciphers.
  //   SSLv2
  //     is by default disabled at build-time.  Builds that are not configured
  //     with "enable-ssl2" will not support SSLv2.
  SSL_V2 = 1,  // unsupported in ArangoDB!!
  SSL_V23 = 2,
  SSL_V3 = 3,
  TLS_V1 = 4,
  TLS_V12 = 5,
  TLS_V13 = 6,
  TLS_GENERIC = 9,

  SSL_LAST
};

#define SSL_CONST const

/// @brief returns a set with all available SSL protocols
std::unordered_set<uint64_t> availableSslProtocols();

/// @brief returns a string description the available SSL protocols
std::string availableSslProtocolsDescription();

asio_ns::ssl::context sslContext(SslProtocol, std::string const& keyfile);

std::string protocolName(SslProtocol protocol);

std::string lastSSLError();

}  // namespace arangodb
