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

#include <errno.h>
#include <string.h>
#include <string>

#include "Basics/Common.h"
#include "Basics/operating-system.h"

#ifdef TRI_HAVE_WINSOCK2_H
#include <WS2tcpip.h>
#include <WinSock2.h>
#endif

#include <openssl/opensslv.h>
#include <openssl/ssl.h>
#ifndef OPENSSL_VERSION_NUMBER
#error expecting OPENSSL_VERSION_NUMBER to be defined
#endif

#include <openssl/err.h>
#include <openssl/opensslconf.h>
#include <openssl/ssl3.h>
#include <openssl/x509.h>

#include "SslClientConnection.h"

#include "Basics/Exceptions.h"
#include "Basics/StringBuffer.h"
#include "Basics/debugging.h"
#include "Basics/error.h"
#include "Basics/socket-utils.h"
#include "Basics/voc-errors.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Ssl/ssl-helper.h"

#undef TRACE_SSL_CONNECTIONS

#ifdef _WIN32
#define STR_ERROR()                                                  \
  windowsErrorBuf;                                                   \
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0, \
                windowsErrorBuf, sizeof(windowsErrorBuf), NULL);     \
  errno = GetLastError();
#else
#define STR_ERROR() strerror(errno)
#endif

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;

namespace {

#ifdef TRACE_SSL_CONNECTIONS
static char const* tlsTypeName(int type) {
  switch (type) {
#ifdef SSL3_RT_HEADER
    case SSL3_RT_HEADER:
      return "TLS header";
#endif
    case SSL3_RT_CHANGE_CIPHER_SPEC:
      return "TLS change cipher";
    case SSL3_RT_ALERT:
      return "TLS alert";
    case SSL3_RT_HANDSHAKE:
      return "TLS handshake";
    case SSL3_RT_APPLICATION_DATA:
      return "TLS app data";
    default:
      return "TLS Unknown";
  }
}

static char const* sslMessageType(int sslVersion, int msg) {
#ifdef SSL2_VERSION_MAJOR
  if (sslVersion == SSL2_VERSION_MAJOR) {
    switch (msg) {
      case SSL2_MT_ERROR:
        return "Error";
      case SSL2_MT_CLIENT_HELLO:
        return "Client hello";
      case SSL2_MT_CLIENT_MASTER_KEY:
        return "Client key";
      case SSL2_MT_CLIENT_FINISHED:
        return "Client finished";
      case SSL2_MT_SERVER_HELLO:
        return "Server hello";
      case SSL2_MT_SERVER_VERIFY:
        return "Server verify";
      case SSL2_MT_SERVER_FINISHED:
        return "Server finished";
      case SSL2_MT_REQUEST_CERTIFICATE:
        return "Request CERT";
      case SSL2_MT_CLIENT_CERTIFICATE:
        return "Client CERT";
    }
  } else
#endif
      if (sslVersion == SSL3_VERSION_MAJOR) {
    switch (msg) {
      case SSL3_MT_HELLO_REQUEST:
        return "Hello request";
      case SSL3_MT_CLIENT_HELLO:
        return "Client hello";
      case SSL3_MT_SERVER_HELLO:
        return "Server hello";
#ifdef SSL3_MT_NEWSESSION_TICKET
      case SSL3_MT_NEWSESSION_TICKET:
        return "Newsession Ticket";
#endif
      case SSL3_MT_CERTIFICATE:
        return "Certificate";
      case SSL3_MT_SERVER_KEY_EXCHANGE:
        return "Server key exchange";
      case SSL3_MT_CLIENT_KEY_EXCHANGE:
        return "Client key exchange";
      case SSL3_MT_CERTIFICATE_REQUEST:
        return "Request CERT";
      case SSL3_MT_SERVER_DONE:
        return "Server finished";
      case SSL3_MT_CERTIFICATE_VERIFY:
        return "CERT verify";
      case SSL3_MT_FINISHED:
        return "Finished";
#ifdef SSL3_MT_CERTIFICATE_STATUS
      case SSL3_MT_CERTIFICATE_STATUS:
        return "Certificate Status";
#endif
    }
  }
  return "Unknown";
}

static void sslTlsTrace(int direction, int sslVersion, int contentType,
                        void const* buf, size_t, SSL*, void*) {
  // enable this for tracing SSL connections
  if (sslVersion) {
    sslVersion >>= 8; /* check the upper 8 bits only below */
    char const* tlsRtName;
    if (sslVersion == SSL3_VERSION_MAJOR && contentType)
      tlsRtName = tlsTypeName(contentType);
    else
      tlsRtName = "";

    LOG_TOPIC("5e087", TRACE, arangodb::Logger::FIXME)
        << "SSL connection trace: " << (direction ? "out" : "in") << ", " << tlsRtName
        << ", " << sslMessageType(sslVersion, *static_cast<char const*>(buf));
  }
}
#endif

}  // namespace

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new client connection
////////////////////////////////////////////////////////////////////////////////

SslClientConnection::SslClientConnection(application_features::ApplicationServer& server,
                                         Endpoint* endpoint, double requestTimeout,
                                         double connectTimeout,
                                         size_t connectRetries, uint64_t sslProtocol)
    : GeneralClientConnection(server, endpoint, requestTimeout, connectTimeout, connectRetries),
      _ssl(nullptr),
      _ctx(nullptr),
      _sslProtocol(sslProtocol) {
  init(sslProtocol);
}

SslClientConnection::SslClientConnection(application_features::ApplicationServer& server,
                                         std::unique_ptr<Endpoint>& endpoint,
                                         double requestTimeout, double connectTimeout,
                                         size_t connectRetries, uint64_t sslProtocol)
    : GeneralClientConnection(server, endpoint, requestTimeout, connectTimeout, connectRetries),
      _ssl(nullptr),
      _ctx(nullptr),
      _sslProtocol(sslProtocol) {
  init(sslProtocol);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a client connection
////////////////////////////////////////////////////////////////////////////////

SslClientConnection::~SslClientConnection() {
  disconnect();

  if (_ssl != nullptr) {
    SSL_free(_ssl);
  }

  if (_ctx != nullptr) {
    SSL_CTX_free(_ctx);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief internal initialization method, called from ctor
////////////////////////////////////////////////////////////////////////////////

void SslClientConnection::init(uint64_t sslProtocol) {
  TRI_invalidatesocket(&_socket);

  SSL_METHOD SSL_CONST* meth = nullptr;

  switch (SslProtocol(sslProtocol)) {
    case SSL_V2:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                     "support for SSLv2 has been dropped");

#ifndef OPENSSL_NO_SSL3_METHOD
    case SSL_V3:
      meth = SSLv3_method();
      break;
#endif

    case SSL_V23:
      meth = SSLv23_method();
      break;

    case TLS_V1:
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
      meth = TLS_client_method();
#else
      meth = TLSv1_method();
#endif
      break;

    case TLS_V12:
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
      meth = TLS_client_method();
#else
      meth = TLSv1_2_method();
#endif
      break;

      // TLS 1.3, only supported from OpenSSL 1.1.1 onwards

      // openssl version number format is
      // MNNFFPPS: major minor fix patch status
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
    case TLS_V13:
      meth = TLS_client_method();
      break;
#endif

    case TLS_GENERIC:
      meth = TLS_client_method();
      break;

    case SSL_UNKNOWN:
    default:
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
      // The actual protocol version used will be negotiated to the highest version mutually supported by the client and the server. The supported protocols are SSLv3, TLSv1, TLSv1.1 and TLSv1.2. Applications should use these methods, and avoid the version-specific methods described below.
      meth = TLS_method();
#else
      // default to TLS 1.2
      meth = TLSv1_2_method();
#endif
      break;
  }

  _ctx = SSL_CTX_new(meth);

  if (_ctx != nullptr) {
#ifdef TRACE_SSL_CONNECTIONS
    SSL_CTX_set_msg_callback(_ctx, sslTlsTrace);
#endif
    SSL_CTX_set_cipher_list(_ctx, "ALL");
    // TODO: better use the following ciphers...
    // SSL_CTX_set_cipher_list(_ctx,
    // "ALL:!EXPORT:!EXPORT40:!EXPORT56:!aNULL:!LOW:!RC4:@STRENGTH");

    bool sslCache = true;
    SSL_CTX_set_session_cache_mode(_ctx, sslCache ? SSL_SESS_CACHE_SERVER : SSL_SESS_CACHE_OFF);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief connect
////////////////////////////////////////////////////////////////////////////////

bool SslClientConnection::connectSocket() {
  TRI_ASSERT(_endpoint != nullptr);

  if (_endpoint->isConnected()) {
    disconnectSocket();
    _isConnected = false;
  }

  _errorDetails.clear();
  _socket = _endpoint->connect(_connectTimeout, _requestTimeout);

  if (!TRI_isvalidsocket(_socket) || _ctx == nullptr) {
    _errorDetails = _endpoint->_errorMessage;
    _isConnected = false;
    return false;
  }

  _isConnected = true;

  _ssl = SSL_new(_ctx);

  if (_ssl == nullptr) {
    _errorDetails = std::string("failed to create ssl context");
    disconnectSocket();
    _isConnected = false;
    return false;
  }

  switch (SslProtocol(_sslProtocol)) {
    case TLS_V1:
    case TLS_V12:
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
    case TLS_V13:
#endif
    case TLS_GENERIC:
    default:
      SSL_set_tlsext_host_name(_ssl, _endpoint->host().c_str());
  }

  SSL_set_connect_state(_ssl);

  if (SSL_set_fd(_ssl, (int)TRI_get_fd_or_handle_of_socket(_socket)) != 1) {
    _errorDetails = std::string("SSL: failed to create context ") +
                    ERR_error_string(ERR_get_error(), nullptr);
    disconnectSocket();
    _isConnected = false;
    return false;
  }

  SSL_set_verify(_ssl, SSL_VERIFY_NONE, nullptr);

  ERR_clear_error();

  int ret = SSL_connect(_ssl);

  if (ret != 1) {
    _errorDetails.append("SSL: during SSL_connect: ");

    int errorDetail;
    long certError;

    errorDetail = SSL_get_error(_ssl, ret);
    if ((errorDetail == SSL_ERROR_WANT_READ) || (errorDetail == SSL_ERROR_WANT_WRITE)) {
      return true;
    }

    /* Gets the earliest error code from the
       thread's error queue and removes the entry. */
    unsigned long lastError = ERR_get_error();

    if (errorDetail == SSL_ERROR_SYSCALL && lastError == 0) {
      if (ret == 0) {
        _errorDetails +=
            "an EOF was observed that violates the protocol. this may happen "
            "when the other side has closed the connection";
      } else if (ret == -1) {
        _errorDetails += "I/O reported by BIO";
      }
    }

    switch (errorDetail) {
      case 0x1407E086:
      /* 1407E086:
        SSL routines:
        SSL2_SET_CERTIFICATE:
        certificate verify failed */
      /* fall-through */
      case 0x14090086:
        /* 14090086:
          SSL routines:
          SSL3_GET_SERVER_CERTIFICATE:
          certificate verify failed */

        certError = SSL_get_verify_result(_ssl);

        if (certError != X509_V_OK) {
          _errorDetails += std::string("certificate problem: ") +
                           X509_verify_cert_error_string(certError);
        } else {
          _errorDetails =
              std::string("certificate problem, verify that the CA cert is OK");
        }
        break;

      default:
        char errorBuffer[256];
        ERR_error_string_n(errorDetail, errorBuffer, sizeof(errorBuffer));
        _errorDetails += std::string(" - details: ") + errorBuffer;
        break;
    }

    disconnectSocket();
    _isConnected = false;
    return false;
  }

  LOG_TOPIC("b3d52", TRACE, arangodb::Logger::FIXME)
      << "SSL connection opened: " << SSL_get_cipher(_ssl) << ", "
      << SSL_get_cipher_version(_ssl) << " ("
      << SSL_get_cipher_bits(_ssl, nullptr) << " bits)";

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief disconnect
////////////////////////////////////////////////////////////////////////////////

void SslClientConnection::disconnectSocket() {
  _endpoint->disconnect();
  TRI_invalidatesocket(&_socket);

  if (_ssl != nullptr) {
    SSL_free(_ssl);
    _ssl = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief write data to the connection
////////////////////////////////////////////////////////////////////////////////

bool SslClientConnection::writeClientConnection(void const* buffer, size_t length,
                                                size_t* bytesWritten) {
  TRI_ASSERT(bytesWritten != nullptr);

#ifdef _WIN32
  char windowsErrorBuf[256];
#endif
  *bytesWritten = 0;

  if (_ssl == nullptr) {
    return false;
  }

  int written = SSL_write(_ssl, buffer, (int)length);
  int err = SSL_get_error(_ssl, written);
  switch (err) {
    case SSL_ERROR_NONE:
      *bytesWritten = written;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      _written += written;
#endif
      return true;

    case SSL_ERROR_ZERO_RETURN:
      SSL_shutdown(_ssl);
      break;

    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
    case SSL_ERROR_WANT_CONNECT:
      break;

    case SSL_ERROR_SYSCALL: {
      char const* pErr = STR_ERROR();
      _errorDetails =
          std::string("SSL: while writing: SYSCALL returned errno = ") +
          std::to_string(errno) + std::string(" - ") + pErr;
      break;
    }

    case SSL_ERROR_SSL: {
      /*  A failure in the SSL library occurred, usually a protocol error.
          The OpenSSL error queue contains more information on the error. */
      unsigned long errorDetail = ERR_get_error();
      char errorBuffer[256];
      ERR_error_string_n(errorDetail, errorBuffer, sizeof(errorBuffer));
      _errorDetails = std::string("SSL: while writing: ") + errorBuffer;
      break;
    }

    default:
      /* a true error */
      _errorDetails = std::string("SSL: while writing: error ") + std::to_string(err);
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read data from the connection
////////////////////////////////////////////////////////////////////////////////

bool SslClientConnection::readClientConnection(StringBuffer& stringBuffer, bool& connectionClosed) {
#ifdef _WIN32
  char windowsErrorBuf[256];
#endif

  connectionClosed = true;
  if (_ssl == nullptr) {
    return false;
  }
  if (!_isConnected) {
    return true;
  }

  connectionClosed = false;

  do {
  again:
    // reserve some memory for reading
    if (stringBuffer.reserve(READBUFFER_SIZE) == TRI_ERROR_OUT_OF_MEMORY) {
      // out of memory
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return false;
    }

    ERR_clear_error();

    int lenRead = SSL_read(_ssl, stringBuffer.end(), READBUFFER_SIZE - 1);

    switch (SSL_get_error(_ssl, lenRead)) {
      case SSL_ERROR_NONE:
        stringBuffer.increaseLength(lenRead);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
        _read += lenRead;
#endif
        break;

      case SSL_ERROR_ZERO_RETURN:
        connectionClosed = true;
        SSL_shutdown(_ssl);
        _isConnected = false;
        return true;

      case SSL_ERROR_WANT_READ:
        goto again;

      case SSL_ERROR_WANT_WRITE:
      case SSL_ERROR_WANT_CONNECT:
      case SSL_ERROR_SYSCALL:
      default: {
        char const* pErr = STR_ERROR();
        unsigned long errorDetail = ERR_get_error();
        char errorBuffer[256];
        ERR_error_string_n(errorDetail, errorBuffer, sizeof(errorBuffer));
        _errorDetails = std::string("SSL: while reading: error '") +
                        std::to_string(errno) + std::string("' - ") +
                        errorBuffer + std::string("' - ") + pErr;

        /* unexpected */
        connectionClosed = true;
        return false;
      }
    }
  } while (readable());

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether the connection is readable
////////////////////////////////////////////////////////////////////////////////

bool SslClientConnection::readable() {
  // must use SSL_pending() and not select() as SSL_read() might read more
  // bytes from the socket into the _ssl structure than we actually requested
  // via SSL_read().
  // if we used select() to check whether there is more data available, select()
  // might return 0 already but we might not have consumed all bytes yet

  // ...........................................................................
  // SSL_pending(...) is an OpenSSL function which returns the number of bytes
  // which are available inside ssl for reading.
  // ...........................................................................

  if (SSL_pending(_ssl) > 0) {
    return true;
  }

  if (prepare(_socket, 0.0, false)) {
    return checkSocket();
  }

  return false;
}
