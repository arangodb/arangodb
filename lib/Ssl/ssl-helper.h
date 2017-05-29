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

#include "Basics/Common.h"

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <boost/asio/ssl.hpp>

#include "Basics/asio-helper.h"

namespace arangodb {
// SSL protocol methods
enum SslProtocol {
  SSL_UNKNOWN = 0,
  SSL_V2 = 1,
  SSL_V23 = 2,
  SSL_V3 = 3,
  TLS_V1 = 4,
  TLS_V12 = 5,

  SSL_LAST
};

#if (OPENSSL_VERSION_NUMBER < 0x00999999L)
#define SSL_CONST /* */
#else
#define SSL_CONST const
#endif

boost::asio::ssl::context sslContext(
    SslProtocol, std::string const& keyfile);

std::string protocolName(SslProtocol protocol);

std::string lastSSLError();
}

#endif
