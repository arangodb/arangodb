////////////////////////////////////////////////////////////////////////////////
/// @brief https server implementation
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

#ifndef TRIAGENS_FYN_HTTP_SERVER_HTTPS_SERVER_IMPL_H
#define TRIAGENS_FYN_HTTP_SERVER_HTTPS_SERVER_IMPL_H 1

#include <Rest/HttpsServer.h>

#include "HttpServer/HttpServerImpl.h"

#include <openssl/ssl.h>

namespace triagens {
  namespace rest {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief http server implementation
    ////////////////////////////////////////////////////////////////////////////////

    class HttpsServerImpl : public HttpServerImpl, public HttpsServer {
      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructs a new http server
        ////////////////////////////////////////////////////////////////////////////////

        HttpsServerImpl (Scheduler*, SSL_CTX*);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief constructs a new http server
        ////////////////////////////////////////////////////////////////////////////////

        HttpsServerImpl (Scheduler*, Dispatcher*, SSL_CTX*);

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief destructs a http server
        ////////////////////////////////////////////////////////////////////////////////

        ~HttpsServerImpl ();

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        void setVerificationMode (int mode) {
          verificationMode = mode;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        void setVerificationCallback (int (*func)(int, X509_STORE_CTX *)) {
          verificationCallback = func;
        }

      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// {@inheritDoc}
        ////////////////////////////////////////////////////////////////////////////////

        void handleConnected (socket_t, ConnectionInfo&);

      private:
        SSL_CTX* ctx;
        int verificationMode;
        int (*verificationCallback)(int, X509_STORE_CTX *);
    };
  }
}

#endif
