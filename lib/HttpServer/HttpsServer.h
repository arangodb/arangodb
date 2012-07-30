////////////////////////////////////////////////////////////////////////////////
/// @brief https server
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2010-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_HTTP_SERVER_HTTPS_SERVER_H
#define TRIAGENS_HTTP_SERVER_HTTPS_SERVER_H 1

#include "HttpServer/GeneralHttpServer.h"

#include <openssl/ssl.h>

#include "Basics/ssl-helper.h"
#include "Logger/Logger.h"

#include "HttpServer/HttpHandler.h"
#include "HttpServer/SslAsyncCommTask.h"
#include "Scheduler/Scheduler.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 class HttpsServer
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

namespace triagens {
  namespace rest {
    class HttpServer;

////////////////////////////////////////////////////////////////////////////////
/// @brief http server
////////////////////////////////////////////////////////////////////////////////

    class HttpsServer : virtual public GeneralHttpServer<HttpsServer, HttpCommTask<HttpsServer> > {

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief SSL protocol methods
////////////////////////////////////////////////////////////////////////////////

        enum protocol_e {
          SSL_UNKNOWN = 0,
          SSL_V2  = 1,
          SSL_V23 = 2,
          SSL_V3  = 3,
          TLS_V1  = 4,

          SSL_LAST
        };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an SSL context
////////////////////////////////////////////////////////////////////////////////

#if (OPENSSL_VERSION_NUMBER < 0x00999999L)
#define SSL_CONST /* */
#else
#define SSL_CONST const
#endif

        static SSL_CTX* sslContext (protocol_e protocol, string const& keyfile) {
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
            LOGGER_ERROR << triagens::basics::lastSSLError();
            return 0;
          }

          if (! SSL_CTX_use_PrivateKey_file(sslctx, keyfile.c_str(), SSL_FILETYPE_PEM)) {
            LOGGER_ERROR << "cannot read key from '" << keyfile << "'";
            LOGGER_ERROR << triagens::basics::lastSSLError();
            return 0;
          }

#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
          SSL_CTX_set_verify_depth(sslctx, 1);
#endif

          return sslctx;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the name of an SSL protocol version
////////////////////////////////////////////////////////////////////////////////

        static const string protocolName (const protocol_e protocol) {
          switch (protocol) {
            case SSL_V2:
              return "SSLv2";
            case SSL_V23:
              return "SSLv23";
            case SSL_V3:
              return "SSLv3";
            case TLS_V1:
              return "TLSv1";
            default:
              return "unknown";
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new http server
////////////////////////////////////////////////////////////////////////////////

        HttpsServer (Scheduler* scheduler,
                     Dispatcher* dispatcher,
                     HttpHandlerFactory* handlerFactory,
                     SSL_CTX* ctx)
        : GeneralHttpServer<HttpsServer, HttpCommTask< HttpsServer> >(scheduler, dispatcher, handlerFactory),
          ctx(ctx),
          verificationMode(SSL_VERIFY_NONE),
          verificationCallback(0) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~HttpsServer () {
          SSL_CTX_free(ctx);
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief return encryption to be used
////////////////////////////////////////////////////////////////////////////////
          
        virtual Endpoint::Encryption getEncryption () const {
          return Endpoint::ENCRYPTION_SSL;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the verification mode
////////////////////////////////////////////////////////////////////////////////

        void setVerificationMode (int mode) {
          verificationMode = mode;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the verification callback
////////////////////////////////////////////////////////////////////////////////

        void setVerificationCallback (int (*func)(int, X509_STORE_CTX *)) {
          verificationCallback = func;
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                             GeneralServer methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void handleConnected (socket_t socket, ConnectionInfo& info) {
          LOGGER_DEBUG << "trying to establish secure connection";

          // convert in a SSL BIO structure
          BIO * sbio = BIO_new_socket((int) socket, BIO_NOCLOSE);

          if (sbio == 0) {
            LOGGER_WARNING << "cannot build new SSL BIO: " << triagens::basics::lastSSLError();
            ::close(socket);
            return;
          }

          // build a new connection
          SSL * ssl = SSL_new(ctx);

          info.sslContext = ssl;

          if (ssl == 0) {
            BIO_free_all(sbio);
            LOGGER_WARNING << "cannot build new SSL connection: " << triagens::basics::lastSSLError();
            ::close(socket);
            return;
          }

          // enforce verification
          SSL_set_verify(ssl, verificationMode, verificationCallback);

          // with the above bio
          SSL_set_bio(ssl, sbio, sbio);

          // create a https task
          SocketTask* task = new SslAsyncCommTask<HttpsServer, HttpCommTask<HttpsServer> >(this, socket, info, sbio);

          // add the task, otherwise it will not be shut down properly          
          GENERAL_SERVER_LOCK(&this->_commTasksLock);
          this->_commTasks.insert(dynamic_cast<GeneralCommTask<HttpsServer, HttpHandlerFactory>*>(task));
          GENERAL_SERVER_UNLOCK(&this->_commTasksLock);

          // and register it
          _scheduler->registerTask(task);
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief ssl context
////////////////////////////////////////////////////////////////////////////////

        SSL_CTX* ctx;

////////////////////////////////////////////////////////////////////////////////
/// @brief verification mode
////////////////////////////////////////////////////////////////////////////////

        int verificationMode;

////////////////////////////////////////////////////////////////////////////////
/// @brief verification callback
////////////////////////////////////////////////////////////////////////////////

        int (*verificationCallback)(int, X509_STORE_CTX *);
    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
