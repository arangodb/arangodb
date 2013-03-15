////////////////////////////////////////////////////////////////////////////////
/// @brief application endpoint server feature
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_HTTP_SERVER_APPLICATION_ENDPOINT_SERVER_H
#define TRIAGENS_HTTP_SERVER_APPLICATION_ENDPOINT_SERVER_H 1

#include "ApplicationServer/ApplicationFeature.h"

#include <openssl/ssl.h>

#include "GeneralServer/EndpointServer.h"
#include "Rest/EndpointList.h"
#include "HttpServer/HttpHandlerFactory.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {
    class ApplicationDispatcher;
    class ApplicationScheduler;

// -----------------------------------------------------------------------------
// --SECTION--                                   class ApplicationEndpointServer
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief application http server feature
////////////////////////////////////////////////////////////////////////////////

    class ApplicationEndpointServer : public ApplicationFeature {
      private:
        ApplicationEndpointServer (ApplicationEndpointServer const&);
        ApplicationEndpointServer& operator= (ApplicationEndpointServer const&);

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
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        ApplicationEndpointServer (ApplicationServer*,
                                   ApplicationScheduler*,
                                   ApplicationDispatcher*,
                                   std::string const&,
                                   HttpHandlerFactory::auth_fptr);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~ApplicationEndpointServer ();

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
/// @brief builds the servers
////////////////////////////////////////////////////////////////////////////////

        bool buildServers ();

////////////////////////////////////////////////////////////////////////////////
/// @brief get the handler factory
////////////////////////////////////////////////////////////////////////////////

        HttpHandlerFactory* getHandlerFactory () const {
          return _handlerFactory;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether authentication is disabled
////////////////////////////////////////////////////////////////////////////////

        bool isAuthenticationDisabled () const {
          return _disableAuthentication;
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

        void setupOptions (map<string, basics::ProgramOptionsDescription>&);

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool parsePhase2 (basics::ProgramOptions&);

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool prepare ();

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool prepare2 ();

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool open ();

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void close ();

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void stop ();

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

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an ssl context
////////////////////////////////////////////////////////////////////////////////

        bool createSslContext ();

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief application server
////////////////////////////////////////////////////////////////////////////////

        ApplicationServer* _applicationServer;

////////////////////////////////////////////////////////////////////////////////
/// @brief application scheduler
////////////////////////////////////////////////////////////////////////////////

        ApplicationScheduler* _applicationScheduler;

////////////////////////////////////////////////////////////////////////////////
/// @brief application dispatcher or null
////////////////////////////////////////////////////////////////////////////////

        ApplicationDispatcher* _applicationDispatcher;

////////////////////////////////////////////////////////////////////////////////
/// @brief authentication realm
////////////////////////////////////////////////////////////////////////////////

        string _authenticationRealm;

////////////////////////////////////////////////////////////////////////////////
/// @brief authentication callback function
////////////////////////////////////////////////////////////////////////////////

        HttpHandlerFactory::auth_fptr _checkAuthentication;

////////////////////////////////////////////////////////////////////////////////
/// @brief the handler factory
////////////////////////////////////////////////////////////////////////////////

        HttpHandlerFactory* _handlerFactory;

////////////////////////////////////////////////////////////////////////////////
/// @brief all constructed servers
////////////////////////////////////////////////////////////////////////////////

        vector<EndpointServer*> _servers;

////////////////////////////////////////////////////////////////////////////////
/// @brief endpoint list container
////////////////////////////////////////////////////////////////////////////////

        rest::EndpointList _endpointList;

////////////////////////////////////////////////////////////////////////////////
/// @brief deprecated hidden option for downwards compatibility
////////////////////////////////////////////////////////////////////////////////

        string _httpPort;

////////////////////////////////////////////////////////////////////////////////
/// @brief endpoints for client HTTP requests
///
/// @CMDOPT{\--server.endpoint @CA{endpoint}}
///
/// Specifies an @CA{endpoint} for HTTP requests by clients. Endpoints have
/// the following pattern:
/// - tcp://ipv4-address:port - TCP/IP endpoint, using IPv4
/// - tcp://[ipv6-address]:port - TCP/IP endpoint, using IPv6
/// - ssl://ipv4-address:port - TCP/IP endpoint, using IPv4, SSL encryption
/// - ssl://[ipv6-address]:port - TCP/IP endpoint, using IPv6, SSL encryption
/// - unix:///path/to/socket - Unix domain socket endpoint
///
/// If a TCP/IP endpoint is specified without a port number, then the default
/// port (8529) will be used.
/// If multiple endpoints need to be used, the option can be repeated multiple
/// times.
///
/// @EXAMPLES
///
/// @verbinclude option-server-endpoint
///
/// Note that if you are using SSL-encrypted endpoints, you must also supply
/// the path to a server certificate using the \-\-server.keyfile option.
////////////////////////////////////////////////////////////////////////////////

        vector<string> _endpoints;

////////////////////////////////////////////////////////////////////////////////
/// @brief disable authentication for ALL client requests
///
/// @CMDOPT{\--server.disable-authentication @CA{value}}
///
/// Setting @CA{value} to true will turn off authentication on the server side
/// so all clients can execute any action without authorisation and privilege
/// checks.
///
/// The default value is @LIT{false}.
////////////////////////////////////////////////////////////////////////////////

        bool _disableAuthentication;

////////////////////////////////////////////////////////////////////////////////
/// @brief timeout for HTTP keep-alive
///
/// @CMDOPT{\--server.keep-alive-timeout}
///
/// Allows to specify the timeout for HTTP keep-alive connections. The timeout
/// value must be specified in seconds.
/// Idle keep-alive connections will be closed by the server automatically when
/// the timeout is reached. A keep-alive-timeout value 0 will disable the keep
/// alive feature entirely.
////////////////////////////////////////////////////////////////////////////////

        double _keepAliveTimeout;

////////////////////////////////////////////////////////////////////////////////
/// @brief listen backlog size
///
/// @CMDOPT{\--server.backlog-size}
///
/// Allows to specify the size of the backlog for the listen system call
/// The default value is 10. The maximum value is platform-dependent.
////////////////////////////////////////////////////////////////////////////////

        int _backlogSize;

////////////////////////////////////////////////////////////////////////////////
/// @brief keyfile containing server certificate
///
/// @CMDOPT{\--server.keyfile @CA{filename}}
///
/// If SSL encryption is used, this option must be used to specify the filename
/// of the server private key. The file must be PEM formatted and contain both
/// the certificate and the server's private key.
///
/// The file specified by @CA{filename} should have the following structure:
///
/// @verbinclude server-keyfile
///
/// You may use certificates issued by a Certificate Authority or self-signed
/// certificates. Self-signed certificates can be created by a tool of your
/// choice. When using OpenSSL for creating the self-signed certificate, the
/// following commands should create a valid keyfile:
///
/// @verbinclude server-keyfile-openssl
///
/// For further information please check the manuals of the tools you use to
/// create the certificate.
///
/// Note: the \-\-server.keyfile option must be set if the server is started with
/// at least one SSL endpoint.
////////////////////////////////////////////////////////////////////////////////

        string _httpsKeyfile;

////////////////////////////////////////////////////////////////////////////////
/// @brief CA file
///
/// @CMDOPT{\--server.cafile @CA{filename}}
///
/// This option can be used to specify a file with CA certificates that are sent
/// to the client whenever the server requests a client certificate. If the
/// file is specified, The server will only accept client requests with
/// certificates issued by these CAs. Do not specify this option if you want
/// clients to be able to connect without specific certificates.
///
/// The certificates in @CA{filename} must be PEM formatted.
///
/// Note: this option is only relevant if at least one SSL endpoint is used.
////////////////////////////////////////////////////////////////////////////////

        string _cafile;

////////////////////////////////////////////////////////////////////////////////
/// @brief SSL protocol type to use
///
/// @CMDOPT{\--server.ssl-protocol @CA{value}}
///
/// Use this option to specify the default encryption protocol to be used.
/// The following variants are available:
/// - 1: SSLv2
/// - 2: SSLv23
/// - 3: SSLv3
/// - 4: TLSv1
///
/// The default @CA{value} is 4 (i.e. TLSv1).
///
/// Note: this option is only relevant if at least one SSL endpoint is used.
////////////////////////////////////////////////////////////////////////////////

        uint32_t _sslProtocol;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not to use SSL session caching
///
/// @CMDOPT{\--server.ssl-cache @CA{value}}
///
/// Set to true if SSL session caching should be used.
///
/// @CA{value} has a default value of @LIT{false} (i.e. no caching).
///
/// Note: this option is only relevant if at least one SSL endpoint is used, and
/// only if the client supports sending the session id.
////////////////////////////////////////////////////////////////////////////////

        bool _sslCache;

////////////////////////////////////////////////////////////////////////////////
/// @brief ssl options to use
///
/// @CMDOPT{\--server.ssl-options @CA{value}}
///
/// This option can be used to set various SSL-related options. Individual
/// option values must be combined using bitwise OR.
///
/// Which options are available on your platform is determined by the OpenSSL
/// version you use. The list of options available on your platform might be
/// retrieved by the following shell command:
///
/// @verbinclude openssl-options
///
/// A description of the options can be found online in the OpenSSL documentation
/// at: http://www.openssl.org/docs/ssl/SSL_CTX_set_options.html
///
/// Note: this option is only relevant if at least one SSL endpoint is used.
////////////////////////////////////////////////////////////////////////////////

        uint64_t _sslOptions;

////////////////////////////////////////////////////////////////////////////////
/// @brief ssl cipher list to use
///
/// @CMDOPT{\--server.ssl-cipher-list @CA{cipher-list}}
///
/// This option can be used to restrict the server to certain SSL ciphers only,
/// and to define the relative usage preference of SSL ciphers.
///
/// The format of @CA{cipher-list} is documented in the OpenSSL documentation.
///
/// To check which ciphers are available on your platform, you may use the
/// following shell command:
///
/// @verbinclude openssl-ciphers
///
/// The default value for @CA{cipher-list} is "ALL".
///
/// Note: this option is only relevant if at least one SSL endpoint is used.
////////////////////////////////////////////////////////////////////////////////

        string _sslCipherList;

////////////////////////////////////////////////////////////////////////////////
/// @brief ssl context
////////////////////////////////////////////////////////////////////////////////

        SSL_CTX* _sslContext;

////////////////////////////////////////////////////////////////////////////////
/// @brief random string used for initialisation
////////////////////////////////////////////////////////////////////////////////

        string _rctx;


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
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
