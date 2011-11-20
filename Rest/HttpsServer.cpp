////////////////////////////////////////////////////////////////////////////////
/// @brief https server
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2010-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "HttpsServer.h"

#include <openssl/err.h>

#include <Basics/Logger.h>
#include <Basics/Mutex.h>
#include <Basics/MutexLocker.h>
#include <Rest/Scheduler.h>
#include <Rest/HttpHandler.h>

#include "HttpServer/HttpsAsyncCommTask.h"

using namespace triagens::basics;

namespace triagens {
  namespace rest {

    // -----------------------------------------------------------------------------
    // SSL helper functions
    // -----------------------------------------------------------------------------

    namespace {
      Mutex SslLock;

      string lastSSLError () {
        char buf[122];
        memset(buf, 0, sizeof(buf));

        unsigned long err = ERR_get_error();
        ERR_error_string_n(err, buf, sizeof(buf) - 1);

        return string(buf);
      }
    }

    // -----------------------------------------------------------------------------
    // static methods
    // -----------------------------------------------------------------------------

#if (OPENSSL_VERSION_NUMBER < 0x00999999L)
#define SSL_CONST /* */
#else
#define SSL_CONST const
#endif

    SSL_CTX* HttpsServer::sslContext (protocol_e protocol, string const& keyfile) {

      // create our context
      SSL_METHOD SSL_CONST* meth = 0;

      switch (protocol) {
#ifndef OPENSSL_NO_SSL2
        case SSL_V2:
          meth = SSLv2_method();
          break;
#endif
        case SSL_V3:
          meth = SSLv3_method();
          break;

        case SSL_V23:
          meth = SSLv23_method();
          break;

        case TLS_V1:
          meth = TLSv1_method();
          break;

        default:
          LOGGER_ERROR << "unknown SSL protocol method";
          return 0;
      }

      SSL_CTX* sslctx = SSL_CTX_new(meth);

      // load our keys and certificates
      if (! SSL_CTX_use_certificate_chain_file(sslctx, keyfile.c_str())) {
        LOGGER_ERROR << "cannot read certificate from '" << keyfile << "'";
        LOGGER_ERROR << lastSSLError();
        return 0;
      }

      if (! SSL_CTX_use_PrivateKey_file(sslctx, keyfile.c_str(), SSL_FILETYPE_PEM)) {
        LOGGER_ERROR << "cannot read key from '" << keyfile << "'";
        LOGGER_ERROR << lastSSLError();
        return 0;
      }

#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
      SSL_CTX_set_verify_depth(sslctx, 1);
#endif

      return sslctx;
    }
  }
}




