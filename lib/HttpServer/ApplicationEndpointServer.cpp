////////////////////////////////////////////////////////////////////////////////
/// @brief application endpoint server feature
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

#include "ApplicationEndpointServer.h"

#include <openssl/err.h>

#include "Basics/delete_object.h"
#include "Basics/ssl-helper.h"
#include "Basics/RandomGenerator.h"
#include "Dispatcher/ApplicationDispatcher.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "HttpServer/HttpServer.h"
#include "HttpServer/HttpsServer.h"
#include "Logger/Logger.h"
#include "Rest/OperationMode.h"
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
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ApplicationEndpointServer::ApplicationEndpointServer (ApplicationServer* applicationServer,
                                                      ApplicationScheduler* applicationScheduler,
                                                      ApplicationDispatcher* applicationDispatcher,
                                                      std::string const& authenticationRealm,
                                                      HttpHandlerFactory::auth_fptr checkAuthentication)
  : ApplicationFeature("EndpointServer"),
    _applicationServer(applicationServer),
    _applicationScheduler(applicationScheduler),
    _applicationDispatcher(applicationDispatcher),
    _authenticationRealm(authenticationRealm),
    _checkAuthentication(checkAuthentication),
    _handlerFactory(0), 
    _servers(),
    _endpointList(),
    _httpPort(),
    _endpoints(),
    _disableAuthentication(false),
    _keepAliveTimeout(300.0),
    _backlogSize(10),
    _httpsKeyfile(),
    _cafile(),
    _sslProtocol(HttpsServer::TLS_V1),
    _sslCache(false),
    _sslOptions((uint64_t) (SSL_OP_TLS_ROLLBACK_BUG | SSL_OP_CIPHER_SERVER_PREFERENCE)),
    _sslCipherList(""),
    _sslContext(0),
    _rctx() {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ApplicationEndpointServer::~ApplicationEndpointServer () {

  // ..........................................................................
  // Where ever possible we should EXPLICITLY write down the type used in 
  // a templated class/method etc. This makes it a lot easier to debug the 
  // code. Granted however, that explicitly writing down the type for an
  // overloaded class operator is a little unwieldy.
  // ..........................................................................
  for_each(_servers.begin(), _servers.end(), triagens::basics::DeleteObjectAny());
  _servers.clear();

  if (_handlerFactory != 0) {
    delete _handlerFactory;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the endpoint servers
////////////////////////////////////////////////////////////////////////////////

bool ApplicationEndpointServer::buildServers () {
  assert(_handlerFactory != 0);
  assert(_applicationScheduler->scheduler() != 0);
  
  EndpointServer* server;

  // turn off authentication
  if (_disableAuthentication) {
    _handlerFactory->setRequireAuthentication(false);
    LOGGER_INFO << "Authentication is turned off";
  }

  
  // unencrypted endpoints
  if (_endpointList.count(Endpoint::PROTOCOL_HTTP, Endpoint::ENCRYPTION_NONE) > 0) {
    // http endpoints 
    server = new HttpServer(_applicationScheduler->scheduler(), 
                            _applicationDispatcher->dispatcher(), 
                            _keepAliveTimeout,
                            _handlerFactory); 
 
    server->setEndpointList(&_endpointList);
    _servers.push_back(server);
  }
  
  // ssl endpoints
  if (_endpointList.count(Endpoint::PROTOCOL_HTTP, Endpoint::ENCRYPTION_SSL) > 0) {
    // check the ssl context
    if (_sslContext == 0) {
      LOGGER_FATAL << "no ssl context is known, cannot create https server";
      LOGGER_INFO << "please use the --server.keyfile option";
      TRI_FlushLogging();

      TRI_EXIT_FUNCTION(EXIT_FAILURE,0);
    }

    // https 
    server = new HttpsServer(_applicationScheduler->scheduler(),
                             _applicationDispatcher->dispatcher(),
                             _keepAliveTimeout,
                             _handlerFactory,
                             _sslContext);

    server->setEndpointList(&_endpointList);
    _servers.push_back(server);
  }

  return true;
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

void ApplicationEndpointServer::setupOptions (map<string, ProgramOptionsDescription>& options) {
  // issue #175: add deprecated hidden option for downwards compatibility
  options[ApplicationServer::OPTIONS_HIDDEN]
    ("server.http-port", &_httpPort, "http port for client requests (deprecated)")
  ;

  options[ApplicationServer::OPTIONS_SERVER]
    ("server.endpoint", &_endpoints, "endpoint for client requests")
  ;
  
  options[ApplicationServer::OPTIONS_SERVER + ":help-admin"]
    ("server.disable-authentication", &_disableAuthentication, "disable authentication for ALL client requests")
    ("server.keep-alive-timeout", &_keepAliveTimeout, "keep-alive timeout in seconds")
    ("server.backlog-size", &_backlogSize, "listen backlog size")
  ;

  options[ApplicationServer::OPTIONS_SERVER + ":help-ssl"]
    ("server.keyfile", &_httpsKeyfile, "keyfile for SSL connections")
    ("server.cafile", &_cafile, "file containing the CA certificates of clients")
    ("server.ssl-protocol", &_sslProtocol, "1 = SSLv2, 2 = SSLv23, 3 = SSLv3, 4 = TLSv1")
    ("server.ssl-cache", &_sslCache, "use SSL session caching")
    ("server.ssl-options", &_sslOptions, "ssl options, see OpenSSL documentation")
    ("server.ssl-cipher-list", &_sslCipherList, "ssl cipher list, see OpenSSL documentation")
  ;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationEndpointServer::parsePhase2 (ProgramOptions& options) {
  // create the ssl context (if possible)
  bool ok = createSslContext();

  if (! ok) {
    return false;
  }

  if (_backlogSize <= 0 || _backlogSize > SOMAXCONN) {
    LOGGER_FATAL << "invalid value for --server.backlog-size. maximum allowed value is " << SOMAXCONN;
    cerr << "invalid value for --server.backlog-size. maximum allowed value is " << SOMAXCONN << "\n";
    TRI_FlushLogging();
    TRI_EXIT_FUNCTION(EXIT_FAILURE,0);
  }

  if (_httpPort != "") {
    // issue #175: add hidden option --server.http-port for downwards-compatibility
    string httpEndpoint("tcp://" + _httpPort);
    _endpoints.push_back(httpEndpoint);
  }

  OperationMode::server_operation_mode_e mode = OperationMode::determineMode(options);
  if (_endpoints.empty() && mode == OperationMode::MODE_SERVER) {
    LOGGER_FATAL << "no endpoint has been specified, giving up";
    cerr << "no endpoint has been specified, giving up\n";
    LOGGER_INFO << "please use the '--server.endpoint' option";
    TRI_FlushLogging();
    TRI_EXIT_FUNCTION(EXIT_FAILURE,0);
  }
  
  // add & validate endpoints
  for (vector<string>::const_iterator i = _endpoints.begin(); i != _endpoints.end(); ++i) {
    Endpoint* endpoint = Endpoint::serverFactory(*i, _backlogSize);
  
    if (endpoint == 0) {
      LOGGER_FATAL << "invalid endpoint '" << *i << "'";
      cerr << "invalid endpoint '" << *i << "'\n";
      TRI_FlushLogging();
      TRI_EXIT_FUNCTION(EXIT_FAILURE,0);
    }

    assert(endpoint);

    bool ok = _endpointList.addEndpoint(endpoint->getProtocol(), endpoint->getEncryption(), endpoint);
    if (! ok) {
      LOGGER_FATAL << "invalid endpoint '" << *i << "'";
      cerr << "invalid endpoint '" << *i << "'\n";
      TRI_FlushLogging();
      TRI_EXIT_FUNCTION(EXIT_FAILURE,0);
    }
  }

  // dump used endpoints for user information
  _endpointList.dump();

  // and return
  return true;
}
  
////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationEndpointServer::prepare () {
  _handlerFactory = new HttpHandlerFactory(_authenticationRealm, _checkAuthentication);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationEndpointServer::prepare2 () {
  // scheduler might be created after prepare(), so we need to use prepare2()!!
  Scheduler* scheduler = _applicationScheduler->scheduler();
 
  if (scheduler == 0) {
    LOGGER_FATAL << "no scheduler is known, cannot create http server";

    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationEndpointServer::open () {
  for (vector<EndpointServer*>::iterator i = _servers.begin();  i != _servers.end();  ++i) {
    EndpointServer* server = *i;

    server->startListening();
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationEndpointServer::close () {
  // close all open connections
  for (vector<EndpointServer*>::iterator i = _servers.begin();  i != _servers.end();  ++i) {
    EndpointServer* server = *i;

    server->shutdownHandlers();
  }

  // close all listen sockets
  for (vector<EndpointServer*>::iterator i = _servers.begin();  i != _servers.end();  ++i) {
    EndpointServer* server = *i;

    server->stopListening();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationEndpointServer::stop () {
  for (vector<EndpointServer*>::iterator i = _servers.begin();  i != _servers.end();  ++i) {
    EndpointServer* server = *i;

    server->stop();
  }
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

bool ApplicationEndpointServer::createSslContext () {
  LOGGER_INFO << "using " << OPENSSL_VERSION_TEXT;

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
  SSL_CTX_set_session_cache_mode(_sslContext, _sslCache ? SSL_SESS_CACHE_SERVER : SSL_SESS_CACHE_OFF);
  if (_sslCache) {
    LOGGER_INFO << "using SSL session caching"; 
  }

  // set options
  SSL_CTX_set_options(_sslContext, _sslOptions);
  LOGGER_INFO << "using SSL options: " << _sslOptions;

  if (_sslCipherList.size() > 0) {
    LOGGER_INFO << "using SSL cipher-list '" << _sslCipherList << "'";
    if (SSL_CTX_set_cipher_list(_sslContext, _sslCipherList.c_str()) != 1) {
      LOGGER_FATAL << "cannot set SSL cipher list '" << _sslCipherList << "'";
      LOGGER_ERROR << lastSSLError();
      TRI_FlushLogging();
      
      TRI_EXIT_FUNCTION(EXIT_FAILURE,0);
    }
  }

  // set ssl context
  Random::UniformCharacter r("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
  _rctx = r.random(SSL_MAX_SSL_SESSION_ID_LENGTH);

  int res = SSL_CTX_set_session_id_context(_sslContext, (unsigned char const*) _rctx.c_str(), _rctx.size());

  if (res != 1) {
    LOGGER_FATAL << "cannot set SSL session id context '" << _rctx << "'";
    LOGGER_ERROR << lastSSLError();
    TRI_FlushLogging();
    
    TRI_EXIT_FUNCTION(EXIT_FAILURE,0);
  }

  // check CA
  if (! _cafile.empty()) {
    LOGGER_TRACE << "trying to load CA certificates from '" << _cafile << "'";

    int res = SSL_CTX_load_verify_locations(_sslContext, _cafile.c_str(), 0);

    if (res == 0) {
      LOGGER_FATAL << "cannot load CA certificates from '" << _cafile << "'";
      LOGGER_ERROR << lastSSLError();
      TRI_FlushLogging();
    
      TRI_EXIT_FUNCTION(EXIT_FAILURE,0);
    }

    STACK_OF(X509_NAME) * certNames;

    certNames = SSL_load_client_CA_file(_cafile.c_str());

    if (certNames == 0) {
      LOGGER_FATAL << "cannot load CA certificates from '" << _cafile << "'";
      LOGGER_ERROR << lastSSLError();
      TRI_FlushLogging();
      
      TRI_EXIT_FUNCTION(EXIT_FAILURE,0);
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
