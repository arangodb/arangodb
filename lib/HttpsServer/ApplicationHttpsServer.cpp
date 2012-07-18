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
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

namespace {
  string lastSSLError () {
    char buf[122];
    memset(buf, 0, sizeof(buf));

    unsigned long err = ERR_get_error();
    ERR_error_string_n(err, buf, sizeof(buf) - 1);

    return string(buf);
  }
}

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

ApplicationHttpsServer::ApplicationHttpsServer (ApplicationScheduler* applicationScheduler,
                                                ApplicationDispatcher* applicationDispatcher)
  : ApplicationFeature("HttpsServer"),
    _applicationScheduler(applicationScheduler),
    _applicationDispatcher(applicationDispatcher),
    _showPort(true),
    _sslProtocol(3),
    _sslCacheMode(0),
    _sslOptions(SSL_OP_TLS_ROLLBACK_BUG),
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
/// @brief shows the port options
////////////////////////////////////////////////////////////////////////////////

void ApplicationHttpsServer::showPortOptions (bool value) {
  _showPort = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a https address:port
////////////////////////////////////////////////////////////////////////////////

AddressPort ApplicationHttpsServer::addPort (string const& name) {
  AddressPort ap;

  if (! ap.split(name)) {
    LOGGER_ERROR << "unknown server:port definition '" << name << "'";
  }
  else {
    _httpsAddressPorts.push_back(ap);
  }

  return ap;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the https server
////////////////////////////////////////////////////////////////////////////////

HttpsServer* ApplicationHttpsServer::buildServer () {
  return buildServer(_httpsAddressPorts);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the https server
////////////////////////////////////////////////////////////////////////////////

HttpsServer* ApplicationHttpsServer::buildServer (vector<AddressPort> const& ports) {
  if (ports.empty()) {
    return 0;
  }
  else {
    return buildHttpsServer(ports);
  }
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
  if (_showPort) {
    options[ApplicationServer::OPTIONS_SERVER + ":help-ssl"]
      ("server.secure", &_httpsPorts, "listen port or address:port")
    ;
  }

  options[ApplicationServer::OPTIONS_SERVER + ":help-ssl"]
    ("server.keyfile", &_httpsKeyfile, "keyfile for SSL connections")
    ("server.cafile", &_cafile, "file containing the CA certificates of clients")
    ("server.ssl-protocol", &_sslProtocol, "1 = SSLv2, 2 = SSLv3, 3 = SSLv23, 4 = TLSv1")
    ("server.ssl-cache-mode", &_sslCacheMode, "0 = off, 1 = client, 2 = server")
    ("server.ssl-options", &_sslOptions, "ssl options, see OpenSSL documentation")
  ;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationHttpsServer::parsePhase2 (ProgramOptions& options) {

  // add ports
  for (vector<string>::const_iterator i = _httpsPorts.begin();  i != _httpsPorts.end();  ++i) {
    addPort(*i);
  }


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

HttpsServer* ApplicationHttpsServer::buildHttpsServer (vector<AddressPort> const& ports) {
  Scheduler* scheduler = _applicationScheduler->scheduler();

  if (scheduler == 0) {
    LOGGER_FATAL << "no scheduler is known, cannot create https server";
    TRI_ShutdownLogging();
    exit(EXIT_FAILURE);
  }

  Dispatcher* dispatcher = 0;

  if (_applicationDispatcher != 0) {
    dispatcher = _applicationDispatcher->dispatcher();
  }

  // check the ssl context
  if (_sslContext == 0) {
    LOGGER_FATAL << "no ssl context is known, cannot create https server";
    TRI_ShutdownLogging();
    exit(EXIT_FAILURE);
  }

  // create new server
  HttpsServer* httpsServer = new HttpsServer(scheduler, dispatcher, _sslContext);

  // keep a list of active server
  _httpsServers.push_back(httpsServer);

  // open http ports
  for (vector<AddressPort>::const_iterator i = ports.begin();  i != ports.end();  ++i) {
    httpsServer->addPort(i->_address, i->_port, _applicationScheduler->addressReuseAllowed());
  }

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
