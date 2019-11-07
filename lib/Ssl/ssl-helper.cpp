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

#include <string.h>
#include <algorithm>
#include <cstdint>

#include <boost/asio/ssl/context_base.hpp>
#include <boost/asio/ssl/impl/context.ipp>
#include <boost/system/error_code.hpp>

#include <openssl/err.h>
#include <openssl/opensslconf.h>

#include "ssl-helper.h"

#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

using namespace arangodb;

#ifndef OPENSSL_NO_SSL3_METHOD
extern "C" const SSL_METHOD* SSLv3_method(void);
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an SSL context
////////////////////////////////////////////////////////////////////////////////

asio_ns::ssl::context arangodb::sslContext(SslProtocol protocol, std::string const& keyfile) {
  // create our context

  asio_ns::ssl::context::method meth;

  switch (protocol) {
    case SSL_V2:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                     "support for SSLv2 has been dropped");

#ifndef OPENSSL_NO_SSL3_METHOD
    case SSL_V3:
      meth = asio_ns::ssl::context::method::sslv3;
      break;
#endif
    case SSL_V23:
      meth = asio_ns::ssl::context::method::sslv23;
      break;

    case TLS_V1:
      meth = asio_ns::ssl::context::method::tlsv1_server;
      break;

    case TLS_V12:
      meth = asio_ns::ssl::context::method::tlsv12_server;
      break;
    
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
    case TLS_V13: 
      // TLS 1.3, only supported from OpenSSL 1.1.1 onwards
      // openssl version number format is
      // MNNFFPPS: major minor fix patch status
      meth = asio_ns::ssl::context::method::tlsv13_server;
      break;
#endif

    case TLS_GENERIC:
      meth = asio_ns::ssl::context::method::tls_server;
      break;

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
  boost::system::error_code ec;
  sslctx.use_certificate_chain_file(keyfile, ec);
  if (ec) {
    LOG_TOPIC("c6a00", ERR, arangodb::Logger::SSL)
    << "cannot read certificate from '" << keyfile << "': " << ec;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "unable to read certificate from file");
  }
  
  sslctx.use_private_key_file(keyfile, asio_ns::ssl::context::file_format::pem, ec);
  if (ec) {
    LOG_TOPIC("98712", ERR, arangodb::Logger::FIXME)
    << "cannot read key from '" << keyfile << "': " << ec;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "unable to read key from keyfile");
  }
#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
  sslctx.set_verify_depth(1);
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
    
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
    case TLS_V13:
      return "TLSv13";
#endif
    
    case TLS_GENERIC:
      return "TLS";

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
                                      SslProtocol::TLS_V13, SslProtocol::TLS_GENERIC};
#else
  // no support for TLS 1.3                                      
  return std::unordered_set<uint64_t>{SslProtocol::SSL_V2,  // unsupported!
                                      SslProtocol::SSL_V23, SslProtocol::SSL_V3,
                                      SslProtocol::TLS_V1, SslProtocol::TLS_V12,
                                      SslProtocol::TLS_GENERIC};
#endif
}

std::string arangodb::availableSslProtocolsDescription() {
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
  return "ssl protocol (1 = SSLv2 (unsupported), 2 = SSLv2 or SSLv3 "
         "(negotiated), 3 = SSLv3, 4 = "
         "TLSv1, 5 = TLSv1.2, 6 = TLSv1.3, 9 = generic TLS)";
#else
  return "ssl protocol (1 = SSLv2 (unsupported), 2 = SSLv2 or SSLv3 "
         "(negotiated), 3 = SSLv3, 4 = "
         "TLSv1, 5 = TLSv1.2, 9 = generic TLS)";
#endif
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
