////////////////////////////////////////////////////////////////////////////////
/// @brief application endpoint server feature
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
/// @author Copyright 2010-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationEndpointServer.h"

#include <openssl/err.h>

#include "Basics/FileUtils.h"
#include "Basics/RandomGenerator.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Basics/delete_object.h"
#include "Basics/json.h"
#include "Basics/logging.h"
#include "Basics/ssl-helper.h"
#include "Dispatcher/ApplicationDispatcher.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "HttpServer/HttpServer.h"
#include "HttpServer/HttpsServer.h"
#include "Rest/Version.h"
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
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ApplicationEndpointServer::ApplicationEndpointServer (ApplicationServer* applicationServer,
                                                      ApplicationScheduler* applicationScheduler,
                                                      ApplicationDispatcher* applicationDispatcher,
                                                      AsyncJobManager* jobManager,
                                                      std::string const& authenticationRealm,
                                                      HttpHandlerFactory::context_fptr setContext,
                                                      void* contextData)
  : ApplicationFeature("EndpointServer"),
    _applicationServer(applicationServer),
    _applicationScheduler(applicationScheduler),
    _applicationDispatcher(applicationDispatcher),
    _jobManager(jobManager),
    _authenticationRealm(authenticationRealm),
    _setContext(setContext),
    _contextData(contextData),
    _handlerFactory(nullptr),
    _servers(),
    _basePath(),
    _endpointList(),
    _httpPort(),
    _endpoints(),
    _reuseAddress(true),
    _keepAliveTimeout(300.0),
    _defaultApiCompatibility(0),
    _allowMethodOverride(false),
    _backlogSize(64),
    _httpsKeyfile(),
    _cafile(),
    _sslProtocol(TLS_V1),
    _sslCache(false),
    _sslOptions((long) (SSL_OP_TLS_ROLLBACK_BUG | SSL_OP_CIPHER_SERVER_PREFERENCE)),
    _sslCipherList(""),
    _sslContext(nullptr),
    _rctx() {

  // if our default value is too high, we'll use half of the max value provided by the system
  if (_backlogSize > SOMAXCONN) {
    _backlogSize = SOMAXCONN / 2;
  }

  _defaultApiCompatibility = Version::getNumericServerVersion();
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

  if (_handlerFactory != nullptr) {
    delete _handlerFactory;
  }

  if (_sslContext != nullptr) {
    SSL_CTX_free(_sslContext);
    _sslContext = nullptr;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the endpoint servers
////////////////////////////////////////////////////////////////////////////////

bool ApplicationEndpointServer::buildServers () {
  TRI_ASSERT(_handlerFactory != nullptr);
  TRI_ASSERT(_applicationScheduler->scheduler() != nullptr);

  HttpServer* server;

  // unencrypted endpoints
  server = new HttpServer(_applicationScheduler->scheduler(),
                          _applicationDispatcher->dispatcher(),
                          _handlerFactory,
                          _jobManager,
                          _keepAliveTimeout);

  server->setEndpointList(&_endpointList);
  _servers.push_back(server);

  // ssl endpoints
  if (_endpointList.has(Endpoint::ENCRYPTION_SSL)) {
    // check the ssl context
    if (_sslContext == nullptr) {
      LOG_INFO("please use the --server.keyfile option");
      LOG_FATAL_AND_EXIT("no ssl context is known, cannot create https server");
    }

    // https
    server = new HttpsServer(_applicationScheduler->scheduler(),
                             _applicationDispatcher->dispatcher(),
                             _handlerFactory,
                             _jobManager,
                             _keepAliveTimeout,
                             _sslContext);

    server->setEndpointList(&_endpointList);
    _servers.push_back(server);
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                        ApplicationFeature methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationEndpointServer::setupOptions (map<string, ProgramOptionsDescription>& options) {
  // issue #175: add deprecated hidden option for downwards compatibility
  options["Hidden Options"]
    ("server.http-port", &_httpPort, "http port for client requests (deprecated)")
  ;

  options["Server Options:help-default"]
    ("server.endpoint", &_endpoints, "endpoint for client requests (e.g. \"tcp://127.0.0.1:8529\", or \"ssl://192.168.1.1:8529\")")
  ;

  options["Server Options:help-admin"]
    ("server.allow-method-override", &_allowMethodOverride, "allow HTTP method override using special headers")
    ("server.backlog-size", &_backlogSize, "listen backlog size")
    ("server.default-api-compatibility", &_defaultApiCompatibility, "default API compatibility version")
    ("server.keep-alive-timeout", &_keepAliveTimeout, "keep-alive timeout in seconds")
    ("server.reuse-address", &_reuseAddress, "try to reuse address")
  ;

  options["SSL Options:help-ssl"]
    ("server.keyfile", &_httpsKeyfile, "keyfile for SSL connections")
    ("server.cafile", &_cafile, "file containing the CA certificates of clients")
    ("server.ssl-protocol", &_sslProtocol, "1 = SSLv2, 2 = SSLv23, 3 = SSLv3, 4 = TLSv1")
    ("server.ssl-cache", &_sslCache, "use SSL session caching")
    ("server.ssl-options", &_sslOptions, "SSL options, see OpenSSL documentation")
    ("server.ssl-cipher-list", &_sslCipherList, "SSL cipher list, see OpenSSL documentation")
  ;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationEndpointServer::afterOptionParsing (ProgramOptions& options) {
  // create the ssl context (if possible)
  bool ok = createSslContext();

  if (! ok) {
    return false;
  }

  if (_backlogSize <= 0) {
    LOG_FATAL_AND_EXIT("invalid value for --server.backlog-size. expecting a positive value");
  }
  
  if (_backlogSize > SOMAXCONN) {
    LOG_WARNING("value for --server.backlog-size exceeds default system header SOMAXCONN value %d. trying to use %d anyway", (int) SOMAXCONN, (int) SOMAXCONN);
  }

  if (! _httpPort.empty()) {
    // issue #175: add hidden option --server.http-port for downwards-compatibility
    string httpEndpoint("tcp://" + _httpPort);
    _endpoints.push_back(httpEndpoint);
  }

  const vector<string> dbNames;

  // add & validate endpoints
  for (vector<string>::const_iterator i = _endpoints.begin(); i != _endpoints.end(); ++i) {
    bool ok = _endpointList.add((*i), dbNames, _backlogSize, _reuseAddress);

    if (! ok) {
      LOG_FATAL_AND_EXIT("invalid endpoint '%s'", (*i).c_str());
    }
  }

  if (_defaultApiCompatibility < HttpRequest::MinCompatibility) {
    LOG_FATAL_AND_EXIT("invalid value for --server.default-api-compatibility. minimum allowed value is %d",
                       (int) HttpRequest::MinCompatibility);
  }

  // and return
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the name of the endpoints file
////////////////////////////////////////////////////////////////////////////////

std::string ApplicationEndpointServer::getEndpointsFilename () const {
  return _basePath + TRI_DIR_SEPARATOR_CHAR + "ENDPOINTS";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a list of all endpoints
////////////////////////////////////////////////////////////////////////////////

std::map<std::string, std::vector<std::string>> ApplicationEndpointServer::getEndpoints () {
  return _endpointList.getAll();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the endpoint list
////////////////////////////////////////////////////////////////////////////////

bool ApplicationEndpointServer::loadEndpoints () {
  const string filename = getEndpointsFilename();

  if (! FileUtils::exists(filename)) {
    return false;
  }

  LOG_TRACE("loading endpoint list from file '%s'", filename.c_str());

  std::unique_ptr<TRI_json_t> json(TRI_JsonFile(TRI_UNKNOWN_MEM_ZONE, filename.c_str(), nullptr));

  if (! TRI_IsObjectJson(json.get())) {
    return false;
  }

  std::map<std::string, std::vector<std::string>> endpoints;

  size_t const n = TRI_LengthVector(&json->_value._objects);

  for (size_t i = 0; i < n; i += 2) {
    auto const* e = static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i));
    auto const* v = static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i + 1));

    if (! TRI_IsStringJson(e) || ! TRI_IsArrayJson(v)) {
      return false;
    }

    std::string const endpoint = string(e->_value._string.data, e->_value._string.length - 1);

    std::vector<std::string> dbNames;
    for (size_t j = 0; j < TRI_LengthVector(&v->_value._objects); ++j) {
      auto d = static_cast<TRI_json_t const*>(TRI_AtVector(&v->_value._objects, j));

      if (! TRI_IsStringJson(d)) {
        return false;
      }

      std::string const dbName = string(d->_value._string.data, d->_value._string.length - 1);
      dbNames.emplace_back(dbName);
    }

    endpoints[endpoint] = dbNames;
  }

  for (auto& it : endpoints) {
    bool ok = _endpointList.add(it.first, it.second, _backlogSize, _reuseAddress);

    if (! ok) {
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the list of databases for an endpoint
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> const& ApplicationEndpointServer::getEndpointMapping (std::string const& endpoint) {
  return _endpointList.getMapping(endpoint);
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationEndpointServer::prepare () {
  if (_disabled) {
    return true;
  }

  loadEndpoints();

  if (_endpointList.empty()) {
    LOG_INFO("please use the '--server.endpoint' option");
    LOG_FATAL_AND_EXIT("no endpoints have been specified, giving up");
  }

  // dump all endpoints for user information
  _endpointList.dump();

  _handlerFactory = new HttpHandlerFactory(_authenticationRealm,
                                           _defaultApiCompatibility,
                                           _allowMethodOverride,
                                           _setContext,
                                           _contextData);

  LOG_INFO("using default API compatibility: %ld", (long int) _defaultApiCompatibility);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationEndpointServer::open () {
  if (_disabled) {
    return true;
  }

  for (auto& server : _servers) {
    server->startListening();
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationEndpointServer::close () {
  if (_disabled) {
    return;
  }

  // close all listen sockets
  for (auto& server : _servers) {
    server->stopListening();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationEndpointServer::stop () {
  if (_disabled) {
    return;
  }

  for (auto& server : _servers) {
    server->stop();
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an ssl context
////////////////////////////////////////////////////////////////////////////////

bool ApplicationEndpointServer::createSslContext () {
  // check keyfile
  if (_httpsKeyfile.empty()) {
    return true;
  }

  // validate protocol
  if (_sslProtocol <= SSL_UNKNOWN || _sslProtocol >= SSL_LAST) {
    LOG_ERROR("invalid SSL protocol version specified. Please use a valid value for --server.ssl-protocol.");
    return false;
  }

  LOG_DEBUG("using SSL protocol version '%s'", protocolName((protocol_e) _sslProtocol).c_str());

  if (! FileUtils::exists(_httpsKeyfile)) {
    LOG_FATAL_AND_EXIT("unable to find SSL keyfile '%s'", _httpsKeyfile.c_str());
  }

  // create context
  _sslContext = sslContext(protocol_e(_sslProtocol), _httpsKeyfile);

  if (_sslContext == nullptr) {
    LOG_ERROR("failed to create SSL context, cannot create HTTPS server");
    return false;
  }

  // set cache mode
  SSL_CTX_set_session_cache_mode(_sslContext, _sslCache ? SSL_SESS_CACHE_SERVER : SSL_SESS_CACHE_OFF);

  if (_sslCache) {
    LOG_TRACE("using SSL session caching");
  }

  // set options
  SSL_CTX_set_options(_sslContext, (long) _sslOptions);
  LOG_INFO("using SSL options: %ld", (long) _sslOptions);

  if (_sslCipherList.size() > 0) {
    if (SSL_CTX_set_cipher_list(_sslContext, _sslCipherList.c_str()) != 1) {
      LOG_ERROR("SSL error: %s", lastSSLError().c_str());
      LOG_FATAL_AND_EXIT("cannot set SSL cipher list '%s'", _sslCipherList.c_str());
    }
    else {
      LOG_INFO("using SSL cipher-list '%s'", _sslCipherList.c_str());
    }
  }

  // set ssl context
  Random::UniformCharacter r("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
  _rctx = r.random(SSL_MAX_SSL_SESSION_ID_LENGTH);

  int res = SSL_CTX_set_session_id_context(_sslContext, (unsigned char const*) _rctx.c_str(), (int) _rctx.size());

  if (res != 1) {
    LOG_ERROR("SSL error: %s", lastSSLError().c_str());
    LOG_FATAL_AND_EXIT("cannot set SSL session id context '%s'", _rctx.c_str());
  }

  // check CA
  if (! _cafile.empty()) {
    LOG_TRACE("trying to load CA certificates from '%s'", _cafile.c_str());

    int res = SSL_CTX_load_verify_locations(_sslContext, _cafile.c_str(), 0);

    if (res == 0) {
      LOG_ERROR("SSL error: %s", lastSSLError().c_str());
      LOG_FATAL_AND_EXIT("cannot load CA certificates from '%s'", _cafile.c_str());
    }

    STACK_OF(X509_NAME) * certNames;

    certNames = SSL_load_client_CA_file(_cafile.c_str());

    if (certNames == nullptr) {
      LOG_ERROR("ssl error: %s", lastSSLError().c_str());
      LOG_FATAL_AND_EXIT("cannot load CA certificates from '%s'", _cafile.c_str());
    }

    if (TRI_IsTraceLogging(__FILE__)) {
      for (int i = 0;  i < sk_X509_NAME_num(certNames);  ++i) {
        X509_NAME* cert = sk_X509_NAME_value(certNames, i);

        if (cert) {
          BIOGuard bout(BIO_new(BIO_s_mem()));

          X509_NAME_print_ex(bout._bio, cert, 0, (XN_FLAG_SEP_COMMA_PLUS | XN_FLAG_DN_REV | ASN1_STRFLGS_UTF8_CONVERT) & ~ASN1_STRFLGS_ESC_MSB);

#ifdef TRI_ENABLE_LOGGER
          char* r;
          long len = BIO_get_mem_data(bout._bio, &r);

          LOG_TRACE("name: %s", string(r, len).c_str());
#endif
        }
      }
    }

    SSL_CTX_set_client_CA_list(_sslContext, certNames);
  }

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
