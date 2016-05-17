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

#include "ssl-helper.h"
#include "Logger/Logger.h"

#include <openssl/err.h>

using namespace arangodb;

#ifndef OPENSSL_NO_SSL3_METHOD
extern "C" const SSL_METHOD* SSLv3_method(void);
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an SSL context
////////////////////////////////////////////////////////////////////////////////

SSL_CTX* arangodb::sslContext(protocol_e protocol, std::string const& keyfile) {
  // create our context
  SSL_METHOD SSL_CONST* meth = nullptr;

  switch (protocol) {
#ifndef OPENSSL_NO_SSL2
    case SSL_V2:
      meth = SSLv2_method();
      break;
#endif
#ifndef OPENSSL_NO_SSL3_METHOD
    case SSL_V3:
      meth = SSLv3_method();
      break;
#endif
    case SSL_V23:
      meth = SSLv23_method();
      break;

    case TLS_V1:
      meth = TLSv1_method();
      break;

    case TLS_V12:
      meth = TLSv1_2_method();
      break;

    default:
      LOG(ERR) << "unknown SSL protocol method";
      return nullptr;
  }

  SSL_CTX* sslctx = SSL_CTX_new(meth);

  if (sslctx == nullptr) {
    // could not create SSL context - this is mostly due to the OpenSSL 
    // library not having been initialized
    LOG(FATAL) << "unable to create SSL context";
    FATAL_ERROR_EXIT();
  }

  // load our keys and certificates
  if (!SSL_CTX_use_certificate_chain_file(sslctx, keyfile.c_str())) {
    LOG(ERR) << "cannot read certificate from '" << keyfile
             << "': " << lastSSLError();
    return nullptr;
  }

  if (!SSL_CTX_use_PrivateKey_file(sslctx, keyfile.c_str(), SSL_FILETYPE_PEM)) {
    LOG(ERR) << "cannot read key from '" << keyfile
             << "': " << lastSSLError();
    return nullptr;
  }

#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
  SSL_CTX_set_verify_depth(sslctx, 1);
#endif

  return sslctx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the name of an SSL protocol version
////////////////////////////////////////////////////////////////////////////////

std::string arangodb::protocolName(protocol_e protocol) {
  switch (protocol) {
    case SSL_V2:
      return "SSLv2";

    case SSL_V23:
      return "SSLv23";

    case SSL_V3:
      return "SSLv3";

    case TLS_V1:
      return "TLSv1";

    case TLS_V12:
      return "TLSv12";

    default:
      return "unknown";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get last SSL error
////////////////////////////////////////////////////////////////////////////////

std::string arangodb::lastSSLError() {
  char buf[122];
  memset(buf, 0, sizeof(buf));

  unsigned long err = ERR_get_error();
  ERR_error_string_n(err, buf, sizeof(buf) - 1);

  return std::string(buf);
}
