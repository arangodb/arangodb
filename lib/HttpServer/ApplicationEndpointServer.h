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

#ifndef ARANGODB_HTTP_SERVER_APPLICATION_ENDPOINT_SERVER_H
#define ARANGODB_HTTP_SERVER_APPLICATION_ENDPOINT_SERVER_H 1

#include "Basics/Common.h"

#include "ApplicationServer/ApplicationFeature.h"

#include <openssl/ssl.h>

#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
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
    class AsyncJobManager;

// -----------------------------------------------------------------------------
// --SECTION--                                   class ApplicationEndpointServer
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief application http server feature
////////////////////////////////////////////////////////////////////////////////

    class ApplicationEndpointServer : public ApplicationFeature {
      private:
        ApplicationEndpointServer (ApplicationEndpointServer const&);
        ApplicationEndpointServer& operator= (ApplicationEndpointServer const&);

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        ApplicationEndpointServer (ApplicationServer*,
                                   ApplicationScheduler*,
                                   ApplicationDispatcher*,
                                   AsyncJobManager*,
                                   std::string const&,
                                   HttpHandlerFactory::context_fptr,
                                   void*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~ApplicationEndpointServer ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

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
/// @brief set the server basepath
////////////////////////////////////////////////////////////////////////////////

        void setBasePath (std::string const& basePath) {
          _basePath = basePath;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                        ApplicationFeature methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void setupOptions (std::map<std::string, basics::ProgramOptionsDescription>&);

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool parsePhase2 (basics::ProgramOptions&);

////////////////////////////////////////////////////////////////////////////////
/// @brief return a list of all endpoints
////////////////////////////////////////////////////////////////////////////////

        std::map<std::string, std::vector<std::string> > getEndpoints ();

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new endpoint at runtime, and connects to it
////////////////////////////////////////////////////////////////////////////////

        bool addEndpoint (std::string const&,
                          std::vector<std::string> const&,
                          bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an existing endpoint and disconnects from it
////////////////////////////////////////////////////////////////////////////////

        bool removeEndpoint (std::string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief restores the endpoint list
////////////////////////////////////////////////////////////////////////////////

        bool loadEndpoints ();

////////////////////////////////////////////////////////////////////////////////
/// @brief persists the endpoint list
/// this method must be called under the _endpointsLock
////////////////////////////////////////////////////////////////////////////////

        bool saveEndpoints ();

////////////////////////////////////////////////////////////////////////////////
/// @brief get the list of databases for an endpoint
////////////////////////////////////////////////////////////////////////////////

        const std::vector<std::string> getEndpointMapping (std::string const&);

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

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief return the filename for the endpoints file
////////////////////////////////////////////////////////////////////////////////

        std::string getEndpointsFilename () const;

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an ssl context
////////////////////////////////////////////////////////////////////////////////

        bool createSslContext ();

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

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
/// @brief application job manager
////////////////////////////////////////////////////////////////////////////////

        AsyncJobManager* _jobManager;

////////////////////////////////////////////////////////////////////////////////
/// @brief authentication realm
////////////////////////////////////////////////////////////////////////////////

        std::string _authenticationRealm;

////////////////////////////////////////////////////////////////////////////////
/// @brief set context callback function
////////////////////////////////////////////////////////////////////////////////

        HttpHandlerFactory::context_fptr _setContext;

////////////////////////////////////////////////////////////////////////////////
/// @brief context data passed to callback functions
////////////////////////////////////////////////////////////////////////////////

        void* _contextData;

////////////////////////////////////////////////////////////////////////////////
/// @brief the handler factory
////////////////////////////////////////////////////////////////////////////////

        HttpHandlerFactory* _handlerFactory;

////////////////////////////////////////////////////////////////////////////////
/// @brief all constructed servers
////////////////////////////////////////////////////////////////////////////////

        std::vector<EndpointServer*> _servers;

////////////////////////////////////////////////////////////////////////////////
/// @brief server basepath
////////////////////////////////////////////////////////////////////////////////

        std::string _basePath;

////////////////////////////////////////////////////////////////////////////////
/// @brief endpoint list container
////////////////////////////////////////////////////////////////////////////////

        rest::EndpointList _endpointList;

////////////////////////////////////////////////////////////////////////////////
/// @brief mutex to protect _endpointList
////////////////////////////////////////////////////////////////////////////////

        basics::ReadWriteLock _endpointsLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief deprecated hidden option for downwards compatibility
////////////////////////////////////////////////////////////////////////////////

        std::string _httpPort;

////////////////////////////////////////////////////////////////////////////////
/// @brief endpoints for client HTTP requests
/// @startDocuBlock serverEndpoint
/// `--server.endpoint endpoint`
///
/// Specifies an *endpoint* for HTTP requests by clients. Endpoints have
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
/// ```
/// unix> ./arangod --server.endpoint tcp://127.0.0.1:8529
///                 --server.endpoint ssl://127.0.0.1:8530
///                 --server.keyfile server.pem /tmp/vocbase
/// 2012-07-26T07:07:47Z [8161] INFO using SSL protocol version 'TLSv1'
/// 2012-07-26T07:07:48Z [8161] INFO using endpoint 'ssl://127.0.0.1:8530' for http ssl requests
/// 2012-07-26T07:07:48Z [8161] INFO using endpoint 'tcp://127.0.0.1:8529' for http tcp requests
/// 2012-07-26T07:07:49Z [8161] INFO ArangoDB (version 1.1.alpha) is ready for business
/// 2012-07-26T07:07:49Z [8161] INFO Have Fun!
/// ```
///
/// **Note**: If you are using SSL-encrypted endpoints, you must also supply
/// the path to a server certificate using the \-\-server.keyfile option.
///
/// Endpoints can also be changed at runtime.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

        std::vector<std::string> _endpoints;

////////////////////////////////////////////////////////////////////////////////
/// @brief try to reuse address
/// @startDocuBlock serverReuseAddress
/// `--server.reuse-address`
///
/// If this boolean option is set to *true* then the socket option
/// SO_REUSEADDR is set on all server endpoints, which is the default.
/// If this option is set to *false* it is possible that it takes up
/// to a minute after a server has terminated until it is possible for
/// a new server to use the same endpoint again. This is why this is
/// activated by default.
///
/// Please note however that under some operating systems this can be
/// a security risk because it might be possible for another process
/// to bind to the same address and port, possibly hijacking network
/// traffic. Under Windows, ArangoDB additionally sets the flag
/// SO_EXCLUSIVEADDRUSE as a measure to alleviate this problem.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

        bool _reuseAddress;

////////////////////////////////////////////////////////////////////////////////
/// @brief timeout for HTTP keep-alive
/// @startDocuBlock keep_alive_timeout
/// `--server.keep-alive-timeout`
///
/// Allows to specify the timeout for HTTP keep-alive connections. The timeout
/// value must be specified in seconds.
/// Idle keep-alive connections will be closed by the server automatically when
/// the timeout is reached. A keep-alive-timeout value 0 will disable the keep
/// alive feature entirely.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

        double _keepAliveTimeout;

////////////////////////////////////////////////////////////////////////////////
/// @brief default API compatibility
/// @startDocuBlock serverDefaultApi
/// `--server.default-api-compatibility`
///
/// This option can be used to determine the API compatibility of the ArangoDB
/// server. It expects an ArangoDB version number as an integer, calculated as
/// follows:
///
/// *10000 \* major + 100 \* minor (example: *10400* for ArangoDB 1.4)*
///
/// The value of this option will have an influence on some API return values
/// when the HTTP client used does not send any compatibility information.
///
/// In most cases it will be sufficient to not set this option explicitly but to
/// keep the default value. However, in case an "old" ArangoDB client is used
/// that does not send any compatibility information and that cannot handle the
/// responses of the current version of ArangoDB, it might be reasonable to set
/// the option to an old version number to improve compatibility with older
/// clients.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

        int32_t _defaultApiCompatibility;

////////////////////////////////////////////////////////////////////////////////
/// @brief allow HTTP method override via custom headers?
/// @startDocuBlock serverAllowMethod
/// `--server.allow-method-override`
///
/// When this option is set to *true*, the HTTP request method will optionally
/// be fetched from one of the following HTTP request headers if present in
/// the request:
///
/// - *x-http-method*
/// - *x-http-method-override*
/// - *x-method-override*
///
/// If the option is set to *true* and any of these headers is set, the
/// request method will be overriden by the value of the header. For example,
/// this allows issuing an HTTP DELETE request which to the outside world will
/// look like an HTTP GET request. This allows bypassing proxies and tools that
/// will only let certain request types pass.
///
/// Setting this option to *true* may impose a security risk so it should only
/// be used in controlled environments.
///
/// The default value for this option is *false*.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

        bool _allowMethodOverride;

////////////////////////////////////////////////////////////////////////////////
/// @brief listen backlog size
/// @startDocuBlock serverBacklog
/// `--server.backlog-size`
///
/// Allows to specify the size of the backlog for the *listen* system call
/// The default value is 10. The maximum value is platform-dependent.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

        int _backlogSize;

////////////////////////////////////////////////////////////////////////////////
/// @brief keyfile containing server certificate
/// @startDocuBlock serverKeyfile
/// `--server.keyfile filename`
///
/// If SSL encryption is used, this option must be used to specify the filename
/// of the server private key. The file must be PEM formatted and contain both
/// the certificate and the server's private key.
///
/// The file specified by *filename* should have the following structure:
///
/// ```
/// # create private key in file "server.key"
/// openssl genrsa -des3 -out server.key 1024
/// 
/// # create certificate signing request (csr) in file "server.csr"
/// openssl req -new -key server.key -out server.csr
/// 
/// # copy away original private key to "server.key.org"
/// cp server.key server.key.org
/// 
/// # remove passphrase from the private key
/// openssl rsa -in server.key.org -out server.key
/// 
/// # sign the csr with the key, creates certificate PEM file "server.crt"
/// openssl x509 -req -days 365 -in server.csr -signkey server.key -out server.crt
/// 
/// # combine certificate and key into single PEM file "server.pem"
/// cat server.crt server.key > server.pem
/// ```
///
/// You may use certificates issued by a Certificate Authority or self-signed
/// certificates. Self-signed certificates can be created by a tool of your
/// choice. When using OpenSSL for creating the self-signed certificate, the
/// following commands should create a valid keyfile:
///
/// ```
/// -----BEGIN CERTIFICATE-----
/// 
/// (base64 encoded certificate)
/// 
/// -----END CERTIFICATE-----
/// -----BEGIN RSA PRIVATE KEY-----
/// 
/// (base64 encoded private key)
/// 
/// -----END RSA PRIVATE KEY-----
/// ```
///
/// For further information please check the manuals of the tools you use to
/// create the certificate.
///
/// **Note**: the \-\-server.keyfile option must be set if the server is started with
/// at least one SSL endpoint.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

        std::string _httpsKeyfile;

////////////////////////////////////////////////////////////////////////////////
/// @brief CA file
/// @startDocuBlock serverCafile
/// `--server.cafile filename`
///
/// This option can be used to specify a file with CA certificates that are sent
/// to the client whenever the server requests a client certificate. If the
/// file is specified, The server will only accept client requests with
/// certificates issued by these CAs. Do not specify this option if you want
/// clients to be able to connect without specific certificates.
///
/// The certificates in *filename* must be PEM formatted.
///
/// **Note**: this option is only relevant if at least one SSL endpoint is used.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

        std::string _cafile;

////////////////////////////////////////////////////////////////////////////////
/// @brief SSL protocol type to use
/// @startDocuBlock serverSSLProtocol
/// `--server.ssl-protocolvalue`
///
/// Use this option to specify the default encryption protocol to be used.
/// The following variants are available:
/// - 1: SSLv2
/// - 2: SSLv23
/// - 3: SSLv3
/// - 4: TLSv1
///
/// The default *value* is 4 (i.e. TLSv1).
///
/// **Note**: this option is only relevant if at least one SSL endpoint is used.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

        uint32_t _sslProtocol;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not to use SSL session caching
/// @startDocuBlock serverSSLCache
/// `--server.ssl-cache value`
///
/// Set to true if SSL session caching should be used.
///
/// *value* has a default value of *false* (i.e. no caching).
///
/// **Note**: this option is only relevant if at least one SSL endpoint is used, and
/// only if the client supports sending the session id.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

        bool _sslCache;

////////////////////////////////////////////////////////////////////////////////
/// @brief ssl options to use
/// @startDocuBlock serverSSLOptions
/// `--server.ssl-options value`
///
/// This option can be used to set various SSL-related options. Individual
/// option values must be combined using bitwise OR.
///
/// Which options are available on your platform is determined by the OpenSSL
/// version you use. The list of options available on your platform might be
/// retrieved by the following shell command:
///
/// ```
///  > grep "#define SSL_OP_.*" /usr/include/openssl/ssl.h
/// 
///  #define SSL_OP_MICROSOFT_SESS_ID_BUG                    0x00000001L
///  #define SSL_OP_NETSCAPE_CHALLENGE_BUG                   0x00000002L
///  #define SSL_OP_LEGACY_SERVER_CONNECT                    0x00000004L
///  #define SSL_OP_NETSCAPE_REUSE_CIPHER_CHANGE_BUG         0x00000008L
///  #define SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG              0x00000010L
///  #define SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER               0x00000020L
///  ...
/// ```
///
/// A description of the options can be found online in the 
/// [OpenSSL documentation](http://www.openssl.org/docs/ssl/SSL_CTX_set_options.html)
///
/// **Note**: this option is only relevant if at least one SSL endpoint is used.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

        uint64_t _sslOptions;

////////////////////////////////////////////////////////////////////////////////
/// @brief ssl cipher list to use
/// @startDocuBlock serverSSLCipher
/// `--server.ssl-cipher-list cipher-list`
///
/// This option can be used to restrict the server to certain SSL ciphers only,
/// and to define the relative usage preference of SSL ciphers.
///
/// The format of *cipher-list* is documented in the OpenSSL documentation.
///
/// To check which ciphers are available on your platform, you may use the
/// following shell command:
///
/// ```
/// > openssl ciphers -v
///
/// ECDHE-RSA-AES256-SHA    SSLv3 Kx=ECDH     Au=RSA  Enc=AES(256)  Mac=SHA1
/// ECDHE-ECDSA-AES256-SHA  SSLv3 Kx=ECDH     Au=ECDSA Enc=AES(256)  Mac=SHA1
/// DHE-RSA-AES256-SHA      SSLv3 Kx=DH       Au=RSA  Enc=AES(256)  Mac=SHA1
/// DHE-DSS-AES256-SHA      SSLv3 Kx=DH       Au=DSS  Enc=AES(256)  Mac=SHA1
/// DHE-RSA-CAMELLIA256-SHA SSLv3 Kx=DH       Au=RSA  Enc=Camellia(256) Mac=SHA1
/// ...
/// ```
///
/// The default value for *cipher-list* is "ALL".
///
/// **Note**: this option is only relevant if at least one SSL endpoint is used.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

        std::string _sslCipherList;

////////////////////////////////////////////////////////////////////////////////
/// @brief ssl context
////////////////////////////////////////////////////////////////////////////////

        SSL_CTX* _sslContext;

////////////////////////////////////////////////////////////////////////////////
/// @brief random string used for initialisation
////////////////////////////////////////////////////////////////////////////////

        std::string _rctx;

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
