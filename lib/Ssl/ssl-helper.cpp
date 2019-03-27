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

#include <openssl/err.h>

#include "Basics/Exceptions.h"
#include "Basics/asio_ns.h"
#include "Logger/Logger.h"

using namespace arangodb;

#ifndef OPENSSL_NO_SSL3_METHOD
extern "C" const SSL_METHOD* SSLv3_method(void);
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an SSL context
////////////////////////////////////////////////////////////////////////////////

asio_ns::ssl::context arangodb::sslContext(SslProtocol protocol, std::string const& keyfile) {
  // create our context

  using asio_ns::ssl::context;
  context::method meth;

  switch (protocol) {
    case SSL_V2:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                     "support for SSLv2 has been dropped");

#ifndef OPENSSL_NO_SSL3_METHOD
    case SSL_V3:
      meth = context::method::sslv3;
      break;
#endif
    case SSL_V23:
      meth = context::method::sslv23;
      break;

    case TLS_V1:
      meth = context::method::tlsv1_server;
      break;

    case TLS_V12:
      meth = context::method::tlsv12_server;
      break;
    
    case TLS_V13: 
      // TLS 1.3, only supported from OpenSSL 1.1.1 onwards
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
      // openssl version number format is
      // MNNFFPPS: major minor fix patch status
      meth = context::method::tlsv13_server;
      break;
#else
      // no TLS 1.3 support
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                     "TLS 1.3 is not supported in this build");
#endif

    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                     "unknown SSL protocol method");
  }

  asio_ns::ssl::context sslctx(meth);

  if (sslctx.native_handle() == nullptr) {
    // could not create SSL context - this is mostly due to the OpenSSL
    // library not having been initialized
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "unable to create SSL context");
  }

  // load our keys and certificates
  if (!SSL_CTX_use_certificate_chain_file(sslctx.native_handle(), keyfile.c_str())) {
    LOG_TOPIC("c6a00", ERR, arangodb::Logger::FIXME)
        << "cannot read certificate from '" << keyfile << "': " << lastSSLError();
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "unable to read certificate from file");
  }

  if (!SSL_CTX_use_PrivateKey_file(sslctx.native_handle(), keyfile.c_str(), SSL_FILETYPE_PEM)) {
    LOG_TOPIC("98712", ERR, arangodb::Logger::FIXME)
        << "cannot read key from '" << keyfile << "': " << lastSSLError();
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "unable to read key from keyfile");
  }

#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
  SSL_CTX_set_verify_depth(sslctx.native_handle(), 1);
#endif

  return sslctx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the name of an SSL protocol version
////////////////////////////////////////////////////////////////////////////////

std::string arangodb::protocolName(SslProtocol protocol) {
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
    
    case TLS_V13:
      return "TLSv13";

    default:
      return "unknown";
  }
}

std::unordered_set<uint64_t> arangodb::availableSslProtocols() {
  // openssl version number format is
  // MNNFFPPS: major minor fix patch status
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
  // TLS 1.3, only support from OpenSSL 1.1.1 onwards
  return std::unordered_set<uint64_t>{SslProtocol::SSL_V2,  // unsupported!
                                      SslProtocol::SSL_V23, SslProtocol::SSL_V3,
                                      SslProtocol::TLS_V1, SslProtocol::TLS_V12,
                                      SslProtocol::TLS_V13};
#else
  // no support for TLS 1.3                                      
  return std::unordered_set<uint64_t>{SslProtocol::SSL_V2,  // unsupported!
                                      SslProtocol::SSL_V23, SslProtocol::SSL_V3,
                                      SslProtocol::TLS_V1, SslProtocol::TLS_V12};
#endif
}

std::string arangodb::availableSslProtocolsDescription() {
  return "ssl protocol (1 = SSLv2 (unsupported), 2 = SSLv2 or SSLv3 "
         "(negotiated), 3 = SSLv3, 4 = "
         "TLSv1, 5 = TLSv1.2)";
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
