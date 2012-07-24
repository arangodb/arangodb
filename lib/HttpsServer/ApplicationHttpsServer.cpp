////////////////////////////////////////////////////////////////////////////////
/// @brief application https server feature
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

#include "ApplicationHttpsServer.h"

#include <openssl/err.h>

#include "Basics/delete_object.h"
#include "Basics/ssl-helper.h"
#include "Basics/Random.h"
#include "Dispatcher/ApplicationDispatcher.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "HttpServer/HttpHandler.h"
#include "HttpsServer/HttpsServer.h"
#include "Logger/Logger.h"
#include "Scheduler/ApplicationScheduler.h"

using namespace triagens::basics;
using namespace triagens::rest;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private classes
// -----------------------------------------------------------------------------

namespace {
  class BIOGuard {
    public:
      BIOGuard(BIO *bio)
        : _bio(bio) {
      }

      ~BIOGuard() {
        BIO_free(_bio);
      }

    public:
      BIO *_bio;
  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ApplicationHttpsServer::ApplicationHttpsServer (ApplicationServer* applicationServer,
                                                ApplicationScheduler* applicationScheduler,
                                                ApplicationDispatcher* applicationDispatcher,
                                                std::string const& authenticationRealm,
                                                HttpHandlerFactory::auth_fptr checkAuthentication)
  : ApplicationFeature("HttpsServer"),
    _applicationServer(applicationServer),
    _applicationScheduler(applicationScheduler),
    _applicationDispatcher(applicationDispatcher),
    _authenticationRealm(authenticationRealm),
    _checkAuthentication(checkAuthentication),
    _sslProtocol(HttpsServer::TLS_V1),
    _sslCacheMode(0),
    _sslOptions(SSL_OP_TLS_ROLLBACK_BUG | SSL_OP_CIPHER_SERVER_PREFERENCE),
    _sslCipherList(""),
    _sslContext(0) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ApplicationHttpsServer::~ApplicationHttpsServer () {
  for_each(_httpsServers.begin(), _httpsServers.end(), DeleteObject());
  _httpsServers.clear();
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

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the https server
////////////////////////////////////////////////////////////////////////////////

HttpsServer* ApplicationHttpsServer::buildServer (const EndpointList* endpointList) {
  return buildHttpsServer(endpointList);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        ApplicationFeature methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ApplicationServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationHttpsServer::setupOptions (map<string, ProgramOptionsDescription>& options) {
  options[ApplicationServer::OPTIONS_SERVER + ":help-ssl"]
    ("server.keyfile", &_httpsKeyfile, "keyfile for SSL connections")
    ("server.cafile", &_cafile, "file containing the CA certificates of clients")
    ("server.ssl-protocol", &_sslProtocol, "1 = SSLv2, 2 = SSLv23, 3 = SSLv3, 4 = TLSv1")
    ("server.ssl-cache-mode", &_sslCacheMode, "0 = off, 1 = client, 2 = server")
    ("server.ssl-options", &_sslOptions, "ssl options, see OpenSSL documentation")
    ("server.ssl-cipher-list", &_sslCipherList, "ssl cipher list, see OpenSSL documentation")
  ;

  options[ApplicationServer::OPTIONS_SERVER + ":help-extended"]
    ("server.https-auth", &_httpsAuth, "use basic authentication")
  ;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationHttpsServer::parsePhase2 (ProgramOptions& options) {
  // create the ssl context (if possible)
  bool ok = createSslContext();

  if (! ok) {
    return false;
  }

  // and return
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationHttpsServer::open () {
  for (vector<HttpsServer*>::iterator i = _httpsServers.begin();  i != _httpsServers.end();  ++i) {
    HttpsServer* server = *i;

    server->startListening();
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationHttpsServer::close () {

  // close all open connections
  for (vector<HttpsServer*>::iterator i = _httpsServers.begin();  i != _httpsServers.end();  ++i) {
    HttpsServer* server = *i;

    server->shutdownHandlers();
  }

  // close all listen sockets
  for (vector<HttpsServer*>::iterator i = _httpsServers.begin();  i != _httpsServers.end();  ++i) {
    HttpsServer* server = *i;

    server->stopListening();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationHttpsServer::stop () {
  for (vector<HttpsServer*>::iterator i = _httpsServers.begin();  i != _httpsServers.end();  ++i) {
    HttpsServer* server = *i;

    server->stop();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

HttpsServer* ApplicationHttpsServer::buildHttpsServer (const EndpointList* endpointList) {
  Scheduler* scheduler = _applicationScheduler->scheduler();

  if (scheduler == 0) {
    LOGGER_FATAL << "no scheduler is known, cannot create https server";
    TRI_ShutdownLogging();
    exit(EXIT_FAILURE);
  }

  Dispatcher* dispatcher = 0;
  HttpHandlerFactory::auth_fptr auth = 0;

  if (_applicationDispatcher != 0) {
    dispatcher = _applicationDispatcher->dispatcher();
  }

  if (_httpsAuth) {
    auth = _checkAuthentication;
  }

  // check the ssl context
  if (_sslContext == 0) {
    LOGGER_FATAL << "no ssl context is known, cannot create https server";
    TRI_ShutdownLogging();
    exit(EXIT_FAILURE);
  }

  // create new server
  HttpsServer* httpsServer = new HttpsServer(scheduler, dispatcher, _authenticationRealm, auth, _sslContext);

  // keep a list of active server
  _httpsServers.push_back(httpsServer);

  httpsServer->addEndpointList(endpointList);

  return httpsServer;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an ssl context
////////////////////////////////////////////////////////////////////////////////

bool ApplicationHttpsServer::createSslContext () {

  // check keyfile
  if (_httpsKeyfile.empty()) {
    return true;
  }

  // validate protocol
  if (_sslProtocol <= HttpsServer::SSL_UNKNOWN || _sslProtocol >= HttpsServer::SSL_LAST) {
    LOGGER_ERROR << "invalid SSL protocol version specified.";
    LOGGER_INFO << "please use a valid value for --server.ssl-protocol.";
    return false;
  }
    
  LOGGER_INFO << "using SSL protocol version '" << HttpsServer::protocolName((HttpsServer::protocol_e) _sslProtocol) << "'";
  
  // create context
  _sslContext = HttpsServer::sslContext(HttpsServer::protocol_e(_sslProtocol), _httpsKeyfile);

  if (_sslContext == 0) {
    LOGGER_ERROR << "failed to create SSL context, cannot create a HTTPS server";
    return false;
  }

  // set cache mode
  SSL_CTX_set_session_cache_mode(_sslContext, _sslCacheMode);
  LOGGER_INFO << "using SSL session cache mode: " << _sslCacheMode;

  // set options
  SSL_CTX_set_options(_sslContext, _sslOptions);
  LOGGER_INFO << "using SSL options: " << _sslOptions;

  if (_sslCipherList.size() > 0) {
    LOGGER_INFO << "using SSL cipher-list '" << _sslCipherList << "'";
    if (SSL_CTX_set_cipher_list(_sslContext, _sslCipherList.c_str()) != 1) {
      LOGGER_FATAL << "cannot set SSL cipher list '" << _sslCipherList << "'";
      LOGGER_ERROR << lastSSLError();
      return false;
    }
  }

  // set ssl context
  Random::UniformCharacter r("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
  string rctx = r.random(SSL_MAX_SSL_SESSION_ID_LENGTH);

  int res = SSL_CTX_set_session_id_context(_sslContext, (unsigned char const*) StringUtils::duplicate(rctx), rctx.size());

  if (res != 1) {
    LOGGER_FATAL << "cannot set SSL session id context '" << rctx << "'";
    LOGGER_ERROR << lastSSLError();
    return false;
  }

  // check CA
  if (! _cafile.empty()) {
    LOGGER_TRACE << "trying to load CA certificates from '" << _cafile << "'";

    int res = SSL_CTX_load_verify_locations(_sslContext, _cafile.c_str(), 0);

    if (res == 0) {
      LOGGER_FATAL << "cannot load CA certificates from '" << _cafile << "'";
      LOGGER_ERROR << lastSSLError();
      return false;
    }

    STACK_OF(X509_NAME) * certNames;

    certNames = SSL_load_client_CA_file(_cafile.c_str());

    if (certNames == 0) {
      LOGGER_FATAL << "cannot load CA certificates from '" << _cafile << "'";
      LOGGER_ERROR << lastSSLError();
      return false;
    }

    if (TRI_IsTraceLogging(__FILE__)) {
      for (int i = 0;  i < sk_X509_NAME_num(certNames);  ++i) {
        X509_NAME* cert = sk_X509_NAME_value(certNames, i);

        if (cert) {
          BIOGuard bout(BIO_new(BIO_s_mem()));

          X509_NAME_print_ex(bout._bio, cert, 0, (XN_FLAG_SEP_COMMA_PLUS | XN_FLAG_DN_REV | ASN1_STRFLGS_UTF8_CONVERT) & ~ASN1_STRFLGS_ESC_MSB);

          char* r;
          long len = BIO_get_mem_data(bout._bio, &r);

          LOGGER_TRACE << "name: " << string(r, len);
        }
      }
    }

    SSL_CTX_set_client_CA_list(_sslContext, certNames);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
