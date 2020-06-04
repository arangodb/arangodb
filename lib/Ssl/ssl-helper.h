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

#ifndef ARANGODB_BASICS_SSL__HELPER_H
#define ARANGODB_BASICS_SSL__HELPER_H 1

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
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
  TLS_V13 = 6,
#endif
  TLS_GENERIC = 9,

  SSL_LAST
};

#if (OPENSSL_VERSION_NUMBER < 0x00999999L)
#define SSL_CONST /* */
#else
#define SSL_CONST const
#endif

/// @brief returns a set with all available SSL protocols
std::unordered_set<uint64_t> availableSslProtocols();

/// @brief returns a string description the available SSL protocols
std::string availableSslProtocolsDescription();

asio_ns::ssl::context sslContext(SslProtocol, std::string const& keyfile);

std::string protocolName(SslProtocol protocol);

std::string lastSSLError();

}  // namespace arangodb

#endif
