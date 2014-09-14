////////////////////////////////////////////////////////////////////////////////
/// @brief general ssl server
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_GENERAL_SERVER_GENERAL_SSL_SERVER_H
#define ARANGODB_GENERAL_SERVER_GENERAL_SSL_SERVER_H 1

#include "Basics/Common.h"

#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/SslAsyncCommTask.h"

#include <openssl/ssl.h>

#include "Basics/ssl-helper.h"
#include "Basics/logging.h"
#include "Scheduler/Scheduler.h"

// -----------------------------------------------------------------------------
// --SECTION--                                            class GeneralSslServer
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {

////////////////////////////////////////////////////////////////////////////////
/// @brief ssl general server
////////////////////////////////////////////////////////////////////////////////

    template<typename S, typename HF, typename CT>
    class GeneralSslServer : virtual public GeneralServer<S, HF, CT> {

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief SSL protocol methods
////////////////////////////////////////////////////////////////////////////////

        enum protocol_e {
          SSL_UNKNOWN = 0,
          SSL_V2      = 1,
          SSL_V23     = 2,
          SSL_V3      = 3,
          TLS_V1      = 4,

          SSL_LAST
        };

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an SSL context
////////////////////////////////////////////////////////////////////////////////

#if (OPENSSL_VERSION_NUMBER < 0x00999999L)
#define SSL_CONST /* */
#else
#define SSL_CONST const
#endif

        static SSL_CTX* sslContext (protocol_e protocol, 
                                    std::string const& keyfile) {
          // create our context
          SSL_METHOD SSL_CONST* meth = nullptr;

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
              LOG_ERROR("unknown SSL protocol method");
              return nullptr;
          }

          SSL_CTX* sslctx = SSL_CTX_new(meth);
            
          // load our keys and certificates
          if (! SSL_CTX_use_certificate_chain_file(sslctx, keyfile.c_str())) {
            LOG_ERROR("cannot read certificate from '%s': %s", keyfile.c_str(), triagens::basics::lastSSLError().c_str());
            return nullptr;
          }

          if (! SSL_CTX_use_PrivateKey_file(sslctx, keyfile.c_str(), SSL_FILETYPE_PEM)) {
            LOG_ERROR("cannot read key from '%s': %s", keyfile.c_str(), triagens::basics::lastSSLError().c_str());
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

        static const std::string protocolName (const protocol_e protocol) {
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

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new general ssl server
////////////////////////////////////////////////////////////////////////////////

        GeneralSslServer (Scheduler* scheduler,
                          Dispatcher* dispatcher,
                          double keepAliveTimeout,
                          HF* handlerFactory,
                          SSL_CTX* ctx)
        : GeneralServer<S, HF, CT>(scheduler, keepAliveTimeout),
          ctx(ctx),
          verificationMode(SSL_VERIFY_NONE),
          verificationCallback(0) {
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        virtual ~GeneralSslServer () {
          SSL_CTX_free(ctx);
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief return encryption to be used
////////////////////////////////////////////////////////////////////////////////

        virtual Endpoint::EncryptionType getEncryption () const {
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

// -----------------------------------------------------------------------------
// --SECTION--                                             GeneralServer methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void handleConnected (TRI_socket_t socket, ConnectionInfo& info) {
          LOG_DEBUG("trying to establish secure connection");

          // create an ssl task
          SocketTask* task = new SslAsyncCommTask<S, HF, CT>(dynamic_cast<S*>(this), socket, info, this->_keepAliveTimeout, ctx, verificationMode, verificationCallback);

          // add the task, otherwise it will not be shut down properly
          GENERAL_SERVER_LOCK(&this->_commTasksLock);
          this->_commTasks.insert(dynamic_cast<GeneralCommTask<S, HF>*>(task));
          GENERAL_SERVER_UNLOCK(&this->_commTasksLock);

          // and register it
          this->_scheduler->registerTask(task);
        }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

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

        int (*verificationCallback)(int, X509_STORE_CTX*);
    };
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
