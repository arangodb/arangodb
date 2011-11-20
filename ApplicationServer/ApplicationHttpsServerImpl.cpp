////////////////////////////////////////////////////////////////////////////////
/// @brief application server https server implementation
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
/// @author Copyright 2009-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationHttpsServerImpl.h"

#include <openssl/err.h>

#include <Basics/DeleteObject.h>
#include <Basics/Logger.h>
#include <Basics/ProgramOptions.h>
#include <Basics/ProgramOptionsDescription.h>
#include <Basics/Random.h>
#include <Basics/StringUtils.h>
#include <Rest/ApplicationServerDispatcher.h>
#include <Rest/HttpHandler.h>
#include <Rest/HttpsServer.h>

#include "HttpServer/HttpsServerImpl.h"

using namespace std;
using namespace triagens::basics;

// -----------------------------------------------------------------------------
// helper classes and methods
// -----------------------------------------------------------------------------

namespace {

  string lastSSLError () {
    char buf[122];
    memset(buf, 0, sizeof(buf));

    unsigned long err = ERR_get_error();
    ERR_error_string_n(err, buf, sizeof(buf) - 1);

    return string(buf);
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief OpenSSL BIO guard
  ////////////////////////////////////////////////////////////////////////////////

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

namespace triagens {
  namespace rest {

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    ApplicationHttpsServerImpl::ApplicationHttpsServerImpl (ApplicationServer* applicationServer)
      : applicationServer(applicationServer),
        showPort(true),
        requireKeepAlive(false),
        sslProtocol(3),
        sslCacheMode(0),
        sslOptions(SSL_OP_TLS_ROLLBACK_BUG),
        sslContext(0) {
    }



    ApplicationHttpsServerImpl::~ApplicationHttpsServerImpl () {
      for_each(httpsServers.begin(), httpsServers.end(), DeleteObject());
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    void ApplicationHttpsServerImpl::setupOptions (map<string, ProgramOptionsDescription>& options) {
      if (showPort) {
        options[ApplicationServer::OPTIONS_SERVER + ":help-ssl"]
          ("server.secure", &httpsPorts, "listen port or address:port")
        ;
      }

      options[ApplicationServer::OPTIONS_SERVER + ":help-ssl"]
        ("server.secure-require-keep-alive", "close connection, if keep-alive is missing")
        ("server.keyfile", &httpsKeyfile, "keyfile for SSL connections")
        ("server.cafile", &cafile, "file containing the CA certificates of clients")
        ("server.ssl-protocol", &sslProtocol, "1 = SSLv2, 2 = SSLv3, 3 = SSLv23, 4 = TLSv1")
        ("server.ssl-cache-mode", &sslCacheMode, "0 = off, 1 = client, 2 = server")
        ("server.ssl-options", &sslOptions, "ssl options, see OpenSSL documentation")
      ;
    }



    bool ApplicationHttpsServerImpl::parsePhase2 (ProgramOptions& options) {

      // check keep alive
      if (options.has("server.secure-require-keep-alive")) {
        requireKeepAlive= true;
      }


      // add ports
      for (vector<string>::const_iterator i = httpsPorts.begin();  i != httpsPorts.end();  ++i) {
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



    AddressPort ApplicationHttpsServerImpl::addPort (string const& name) {
      AddressPort ap;

      if (! ap.split(name)) {
        LOGGER_ERROR << "unknown server:port definition '" << name << "'";
      }
      else {
        httpsAddressPorts.push_back(ap);
      }

      return ap;
    }



    HttpsServer* ApplicationHttpsServerImpl::buildServer (HttpHandlerFactory* httpHandlerFactory) {
      return buildServer(httpHandlerFactory, httpsAddressPorts);
    }



    HttpsServer* ApplicationHttpsServerImpl::buildServer (HttpHandlerFactory* httpHandlerFactory, vector<AddressPort> const& ports) {
      if (ports.empty()) {
        delete httpHandlerFactory;
        return 0;
      }
      else {
        return buildHttpsServer(httpHandlerFactory, ports);
      }
    }

    // -----------------------------------------------------------------------------
    // private methods
    // -----------------------------------------------------------------------------

    bool ApplicationHttpsServerImpl::createSslContext () {

      // check keyfile
      if (httpsKeyfile.empty()) {
        return true;
      }


      // create context
      sslContext = HttpsServer::sslContext(HttpsServer::protocol_e(sslProtocol), httpsKeyfile);

      if (sslContext == 0) {
        LOGGER_ERROR << "failed to create SSL context, cannot create a HTTPS server";
        return false;
      }

      // set cache mode
      SSL_CTX_set_session_cache_mode(sslContext, sslCacheMode);
      LOGGER_INFO << "using SSL session cache mode: " << sslCacheMode;

      // set options
      SSL_CTX_set_options(sslContext, sslOptions);
      LOGGER_INFO << "using SSL options: " << sslOptions;


      // set ssl context
      Random::UniformCharacter r("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
      string rctx = r.random(SSL_MAX_SSL_SESSION_ID_LENGTH);

      int res = SSL_CTX_set_session_id_context(sslContext, (unsigned char const*) StringUtils::duplicate(rctx), rctx.size());

      if (res != 1) {
        LOGGER_FATAL << "cannot set SSL session id context '" << rctx << "'";
        LOGGER_ERROR << lastSSLError();
        return false;
      }

      // check CA
      if (! cafile.empty()) {
        LOGGER_TRACE << "trying to load CA certificates from '" << cafile << "'";

        int res = SSL_CTX_load_verify_locations(sslContext, cafile.c_str(), 0);

        if (res == 0) {
          LOGGER_FATAL << "cannot load CA certificates from '" << cafile << "'";
          LOGGER_ERROR << lastSSLError();
          return false;
        }

        STACK_OF(X509_NAME) * certNames;

        certNames = SSL_load_client_CA_file(cafile.c_str());

        if (certNames == 0) {
          LOGGER_FATAL << "cannot load CA certificates from '" << cafile << "'";
          LOGGER_ERROR << lastSSLError();
          return false;
        }

        if (TRI_IsTraceLogging()) {
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

        SSL_CTX_set_client_CA_list(sslContext, certNames);
      }

      return true;
    }


    HttpsServerImpl* ApplicationHttpsServerImpl::buildHttpsServer (HttpHandlerFactory* httpHandlerFactory,
                                                                   vector<AddressPort> const& ports) {
      Scheduler* scheduler = applicationServer->scheduler();

      if (scheduler == 0) {
        LOGGER_FATAL << "no scheduler is known, cannot create https server";
        exit(EXIT_FAILURE);
      }

      Dispatcher* dispatcher = 0;
      ApplicationServerDispatcher* asd = dynamic_cast<ApplicationServerDispatcher*>(applicationServer);

      if (asd != 0) {
        dispatcher = asd->dispatcher();
      }

      // check the ssl context
      if (sslContext == 0) {
        LOGGER_FATAL << "no ssl context is known, cannot create https server";
        exit(EXIT_FAILURE);
      }

      // create new server
      HttpsServerImpl* httpsServer = new HttpsServerImpl(scheduler, dispatcher, sslContext);

      httpsServer->setHandlerFactory(httpHandlerFactory);

      if (requireKeepAlive) {
        httpsServer->setCloseWithoutKeepAlive(true);
      }

      // keep a list of active server
      httpsServers.push_back(httpsServer);

      // open http ports
      deque<AddressPort> addresses;
      addresses.insert(addresses.begin(), ports.begin(), ports.end());

      while (! addresses.empty()) {
        AddressPort ap = addresses[0];
        addresses.pop_front();

        string bindAddress = ap.address;
        int port = ap.port;

        bool result;

        if (bindAddress.empty()) {
          LOGGER_TRACE << "trying to open port " << port << " for https requests";

          result = httpsServer->addPort(port, applicationServer->addressReuseAllowed());
        }
        else {
          LOGGER_TRACE << "trying to open address " << bindAddress
                       << " on port " << port
                       << " for https requests";

          result = httpsServer->addPort(bindAddress, port, applicationServer->addressReuseAllowed());
        }

        if (result) {
          LOGGER_DEBUG << "opened port " << port << " for " << (bindAddress.empty() ? "any" : bindAddress);
        }
        else {
          LOGGER_TRACE << "failed to open port " << port << " for " << (bindAddress.empty() ? "any" : bindAddress);
          addresses.push_back(ap);

          if (scheduler->isShutdownInProgress()) {
            addresses.clear();
          }
          else {
            sleep(1);
          }
        }
      }

      return httpsServer;
    }
  }
}
