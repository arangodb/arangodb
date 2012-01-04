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

#include "HttpsServerImpl.h"

#include <openssl/err.h>

#include <Logger/Logger.h>
#include <Basics/Mutex.h>
#include <Basics/MutexLocker.h>

#include "HttpServer/HttpHandler.h"
#include "HttpsServer/HttpsAsyncCommTask.h"
#include "Scheduler/Scheduler.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;

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

namespace triagens {
  namespace rest {

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    HttpsServerImpl::HttpsServerImpl (Scheduler* scheduler, Dispatcher* dispatcher, SSL_CTX* ctx)
      : HttpServerImpl(scheduler, dispatcher), ctx(ctx), verificationMode(SSL_VERIFY_NONE), verificationCallback(0) {
    }



    HttpsServerImpl::~HttpsServerImpl () {
      SSL_CTX_free(ctx);
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    void HttpsServerImpl::handleConnected (socket_t socket, ConnectionInfo& info) {
      LOGGER_INFO << "trying to establish secure connection";

      // convert in a SSL BIO structure
      BIO * sbio = BIO_new_socket((int) socket, BIO_NOCLOSE);

      if (sbio == 0) {
        LOGGER_WARNING << "cannot build new SSL BIO: " << lastSSLError();
        ::close(socket);
        return;
      }

      // build a new connection
      SSL * ssl = SSL_new(ctx);

      info.sslContext = ssl;

      if (ssl == 0) {
        BIO_free_all(sbio);
        LOGGER_WARNING << "cannot build new SSL connection: " << lastSSLError();
        ::close(socket);
        return;
      }

      // enforce verification
      SSL_set_verify(ssl, verificationMode, verificationCallback);

      // with the above bio
      SSL_set_bio(ssl, sbio, sbio);

      // create a https task
      SocketTask* task = new HttpsAsyncCommTask(this, socket, info, sbio);

      // and register it
      _scheduler->registerTask(task);
    }
  }
}
