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

#include "Basics/delete_object.h"
#include "Basics/ssl-helper.h"
#include "Basics/FileUtils.h"
#include "Basics/RandomGenerator.h"
#include "Basics/json.h"
#include "Basics/logging.h"
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
    _backlogSize(10),
    _httpsKeyfile(),
    _cafile(),
    _sslProtocol(HttpsServer::TLS_V1),
    _sslCache(false),
    _sslOptions((long) (SSL_OP_TLS_ROLLBACK_BUG | SSL_OP_CIPHER_SERVER_PREFERENCE)),
    _sslCipherList(""),
    _sslContext(nullptr),
    _rctx() {

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

  EndpointServer* server;

  // unencrypted endpoints
  server = new HttpServer(_applicationScheduler->scheduler(),
                          _applicationDispatcher->dispatcher(),
                          _jobManager,
                          _keepAliveTimeout,
                          _handlerFactory);

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
                             _jobManager,
                             _keepAliveTimeout,
                             _handlerFactory,
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
  options[ApplicationServer::OPTIONS_HIDDEN]
    ("server.http-port", &_httpPort, "http port for client requests (deprecated)")
  ;

  options[ApplicationServer::OPTIONS_SERVER]
    ("server.endpoint", &_endpoints, "endpoint for client requests (e.g. \"tcp://127.0.0.1:8529\", or \"ssl://192.168.1.1:8529\")")
  ;

  options[ApplicationServer::OPTIONS_SERVER + ":help-admin"]
    ("server.allow-method-override", &_allowMethodOverride, "allow HTTP method override using special headers")
    ("server.backlog-size", &_backlogSize, "listen backlog size")
    ("server.default-api-compatibility", &_defaultApiCompatibility, "default API compatibility version")
    ("server.keep-alive-timeout", &_keepAliveTimeout, "keep-alive timeout in seconds")
    ("server.reuse-address", &_reuseAddress, "try to reuse address")
  ;

  options[ApplicationServer::OPTIONS_SSL]
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

bool ApplicationEndpointServer::parsePhase2 (ProgramOptions& options) {
  // create the ssl context (if possible)
  bool ok = createSslContext();

  if (! ok) {
    return false;
  }

  if (_backlogSize <= 0 || _backlogSize > SOMAXCONN) {
    LOG_FATAL_AND_EXIT("invalid value for --server.backlog-size. maximum allowed value is %d", (int) SOMAXCONN);
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

std::map<std::string, std::vector<std::string> > ApplicationEndpointServer::getEndpoints () {
  READ_LOCKER(_endpointsLock);

  return _endpointList.getAll();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new endpoint at runtime, and connects to it
////////////////////////////////////////////////////////////////////////////////

bool ApplicationEndpointServer::addEndpoint (std::string const& newEndpoint,
                                             vector<string> const& dbNames,
                                             bool save) {
  // validate...
  const string unified = Endpoint::getUnifiedForm(newEndpoint);

  if (unified.empty()) {
    // invalid endpoint
    return false;
  }

  Endpoint::EncryptionType encryption;
  if (unified.substr(0, 6) == "ssl://") {
    encryption = Endpoint::ENCRYPTION_SSL;
  }
  else {
    encryption = Endpoint::ENCRYPTION_NONE;
  }

  // find the correct server (HTTP or HTTPS)
  for (size_t i = 0; i < _servers.size(); ++i) {
    if (_servers[i]->getEncryption() == encryption) {
      // found the correct server
      WRITE_LOCKER(_endpointsLock);

      Endpoint* endpoint;
      bool ok = _endpointList.add(newEndpoint, dbNames, _backlogSize, _reuseAddress, &endpoint);

      if (! ok) {
        return false;
      }

      if (endpoint == nullptr) {
        if (save) {
          saveEndpoints();
        }

        LOG_DEBUG("reconfigured endpoint '%s'", newEndpoint.c_str());
        // in this case, we updated an existing endpoint and are done
        return true;
      }

      // this connects the new endpoint
      ok = _servers[i]->addEndpoint(endpoint);

      if (ok) {
        if (save) {
          saveEndpoints();
        }
        LOG_DEBUG("bound to endpoint '%s'", newEndpoint.c_str());
        return true;
      }

      LOG_WARNING("failed to bind to endpoint '%s'", newEndpoint.c_str());
      return false;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an existing endpoint, and disconnects from it
////////////////////////////////////////////////////////////////////////////////

bool ApplicationEndpointServer::removeEndpoint (std::string const& oldEndpoint) {
  // validate...
  const string unified = Endpoint::getUnifiedForm(oldEndpoint);

  if (unified.empty()) {
    // invalid endpoint
    return false;
  }

  Endpoint::EncryptionType encryption;
  if (unified.substr(0, 6) == "ssl://") {
    encryption = Endpoint::ENCRYPTION_SSL;
  }
  else {
    encryption = Endpoint::ENCRYPTION_NONE;
  }

  // find the correct server (HTTP or HTTPS)
  for (size_t i = 0; i < _servers.size(); ++i) {
    if (_servers[i]->getEncryption() == encryption) {
      // found the correct server
      WRITE_LOCKER(_endpointsLock);

      Endpoint* endpoint;
      bool ok = _endpointList.remove(unified, &endpoint);

      if (! ok) {
        LOG_WARNING("could not remove endpoint '%s'", oldEndpoint.c_str());
        return false;
      }

      // this disconnects the new endpoint
      ok = _servers[i]->removeEndpoint(endpoint);
      delete endpoint;

      if (ok) {
        saveEndpoints();
        LOG_DEBUG("removed endpoint '%s'", oldEndpoint.c_str());
      }
      else {
        LOG_WARNING("failed to remove endpoint '%s'", oldEndpoint.c_str());
      }

      return ok;
    }
  }

  return false;
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

  TRI_json_t* json = TRI_JsonFile(TRI_CORE_MEM_ZONE, filename.c_str(), 0);

  if (json == 0) {
    return false;
  }

  std::map<std::string, std::vector<std::string> > endpoints;

  if (! TRI_IsArrayJson(json)) {
    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    return false;
  }

  const size_t n = json->_value._objects._length;
  for (size_t i = 0; i < n; i += 2) {
    TRI_json_t const* e = static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i));
    TRI_json_t const* v = static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i + 1));

    if (! TRI_IsStringJson(e) || ! TRI_IsListJson(v)) {
      TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
      return false;
    }

    const string endpoint = string(e->_value._string.data, e->_value._string.length - 1);

    vector<string> dbNames;
    for (size_t j = 0; j < v->_value._objects._length; ++j) {
      TRI_json_t const* d = (TRI_json_t const*) TRI_AtVector(&v->_value._objects, j);

      if (! TRI_IsStringJson(d)) {
        TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
        return false;
      }

      const string dbName = string(d->_value._string.data, d->_value._string.length - 1);
      dbNames.push_back(dbName);
    }

    endpoints[endpoint] = dbNames;
  }

  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);


  std::map<std::string, std::vector<std::string> >::const_iterator it;
  for (it = endpoints.begin(); it != endpoints.end(); ++it) {

    bool ok = _endpointList.add((*it).first, (*it).second, _backlogSize, _reuseAddress);

    if (! ok) {
      return false;
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief persists the endpoint list
/// this method must be called under the _endpointsLock
////////////////////////////////////////////////////////////////////////////////

bool ApplicationEndpointServer::saveEndpoints () {
  const std::map<std::string, std::vector<std::string> > endpoints = _endpointList.getAll();

  TRI_json_t* json = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE);

  if (json == nullptr) {
    return false;
  }

  std::map<std::string, std::vector<std::string> >::const_iterator it;

  for (it = endpoints.begin(); it != endpoints.end(); ++it) {
    TRI_json_t* list = TRI_CreateListJson(TRI_CORE_MEM_ZONE);

    if (list == nullptr) {
      TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
      return false;
    }

    for (size_t i = 0; i < (*it).second.size(); ++i) {
      const string e = (*it).second.at(i);

      TRI_PushBack3ListJson(TRI_CORE_MEM_ZONE, list, TRI_CreateString2CopyJson(TRI_CORE_MEM_ZONE, e.c_str(), e.size()));
    }

    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, (*it).first.c_str(), list);
  }

  const string filename = getEndpointsFilename();
  LOG_TRACE("saving endpoint list in file '%s'", filename.c_str());
  bool ok = TRI_SaveJson(filename.c_str(), json, true);

  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the list of databases for an endpoint
////////////////////////////////////////////////////////////////////////////////

const std::vector<std::string> ApplicationEndpointServer::getEndpointMapping (std::string const& endpoint) {
  READ_LOCKER(_endpointsLock);

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

bool ApplicationEndpointServer::prepare2 () {
  if (_disabled) {
    return true;
  }

  // scheduler might be created after prepare(), so we need to use prepare2()!!
  Scheduler* scheduler = _applicationScheduler->scheduler();

  if (scheduler == nullptr) {
    LOG_FATAL_AND_EXIT("no scheduler is known, cannot create server");

    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationEndpointServer::open () {
  if (_disabled) {
    return true;
  }

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
  if (_disabled) {
    return;
  }

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
  if (_disabled) {
    return;
  }

  for (vector<EndpointServer*>::iterator i = _servers.begin();  i != _servers.end();  ++i) {
    EndpointServer* server = *i;

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
  if (_sslProtocol <= HttpsServer::SSL_UNKNOWN || _sslProtocol >= HttpsServer::SSL_LAST) {
    LOG_ERROR("invalid SSL protocol version specified. Please use a valid value for --server.ssl-protocol.");
    return false;
  }

  LOG_DEBUG("using SSL protocol version '%s'",
            HttpsServer::protocolName((HttpsServer::protocol_e) _sslProtocol).c_str());

  if (! FileUtils::exists(_httpsKeyfile)) {
    LOG_FATAL_AND_EXIT("unable to find SSL keyfile '%s'", _httpsKeyfile.c_str());
  }

  // create context
  _sslContext = HttpsServer::sslContext(HttpsServer::protocol_e(_sslProtocol), _httpsKeyfile);

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

    if (certNames == 0) {
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
