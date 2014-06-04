!CHAPTER Command-Line Options for arangod

`--server.endpoint endpoint`

Specifies an endpoint for HTTP requests by clients. Endpoints have the following pattern:

* tcp://ipv4-address:port - TCP/IP endpoint, using IPv4
* tcp://[ipv6-address]:port - TCP/IP endpoint, using IPv6
* ssl://ipv4-address:port - TCP/IP endpoint, using IPv4, SSL encryption
* ssl://[ipv6-address]:port - TCP/IP endpoint, using IPv6, SSL encryption
* unix:///path/to/socket - Unix domain socket endpoint
* If a TCP/IP endpoint is specified without a port number, then the default port (8529) will be used. If multiple endpoints need to be used, the option can be repeated multiple times.

*Examples*

  unix> ./arangod --server.endpoint tcp://127.0.0.1:8529
  --server.endpoint ssl://127.0.0.1:8530
  --server.keyfile server.pem /tmp/vocbase
  2012-07-26T07:07:47Z [8161] INFO using SSL protocol version 'TLSv1'
  2012-07-26T07:07:48Z [8161] INFO using endpoint 'ssl://127.0.0.1:8530' for http ssl requests
  2012-07-26T07:07:48Z [8161] INFO using endpoint 'tcp://127.0.0.1:8529' for http tcp requests
  2012-07-26T07:07:49Z [8161] INFO ArangoDB (version 1.1.alpha) is ready for business
  2012-07-26T07:07:49Z [8161] INFO Have Fun!

Note that if you are using SSL-encrypted endpoints, you must also supply the path to a server certificate using the `--server.keyfile` option.

Endpoints can also be changed at runtime. Please refer to HTTP Interface for Endpoints for more details.


`--server.reuse-address`

If this boolean option is set to true then the socket option SO_REUSEADDR is set on all server endpoints, which is the default. If this option is set to false it is possible that it takes up to a minute after a server has terminated until it is possible for a new server to use the same endpoint again. This is why this is activated by default.

Please note however that under some operating systems this can be a security risk because it might be possible for another process to bind to the same address and port, possibly hijacking network traffic. Under Windows, ArangoDB additionally sets the flag SO_EXCLUSIVEADDRUSE as a measure to alleviate this problem.


`--server.disable-authentication value`

Setting value to true will turn off authentication on the server side so all clients can execute any action without authorisation and privilege checks.

The default value is false.


`--server.disable-authentication-unix-sockets value`
Setting value to true will turn off authentication on the server side for requests coming in via UNIX domain sockets. With this flag enabled, clients located on the same host as the ArangoDB server can use UNIX domain sockets to connect to the server without authentication. Requests coming in by other means (e.g. TCP/IP) are not affected by this option.

The default value is false.

Note: this option is only available on platforms that support UNIX domain sockets.


`--server.authenticate-system-only boolean`

Controls whether incoming requests need authentication only if they are directed to the ArangoDB's internal APIs and features, located at /_api/, /_admin/ etc.

IF the flag is set to true, then HTTP authentication is only required for requests going to URLs starting with /_, but not for other URLs. The flag can thus be used to expose a user-made API without HTTP authentication to the outside world, but to prevent the outside world from using the ArangoDB API and the admin interface without authentication. Note that checking the URL is performed after any database name prefix has been removed. That means when the actual URL called is /_db/_system/myapp/myaction, the URL /myapp/myaction will be used for authenticate-system-only check.

The default is false.

Note that authentication still needs to be enabled for the server regularly in order for HTTP authentication to be forced for the ArangoDB API and the web interface. Setting only this flag is not enough.

You can control ArangoDB's general authentication feature with the --server.disable-authentication flag.


`--server.disable-replication-logger flag`

If true the server will start with the replication logger turned off, even if the replication logger is configured with the autoStart option. Using this option will not change the value of the autoStart option in the logger configuration, but will suppress auto-starting the replication logger just once.

If the option is not used, ArangoDB will read the logger configuration from the file REPLICATION-LOGGER-CONFIG on startup, and use the value of the autoStart attribute from this file.

The default is false.


`--server.disable-replication-applier flag`

If true the server will start with the replication applier turned off, even if the replication applier is configured with the autoStart option. Using the command-line option will not change the value of the autoStart option in the applier configuration, but will suppress auto-starting the replication applier just once.

If the option is not used, ArangoDB will read the applier configuration from the file REPLICATION-APPLIER-CONFIG on startup, and use the value of the autoStart attribute from this file.

The default is false.


`--server.keep-alive-timeout`

Allows to specify the timeout for HTTP keep-alive connections. The timeout value must be specified in seconds. Idle keep-alive connections will be closed by the server automatically when the timeout is reached. A keep-alive-timeout value 0 will disable the keep alive feature entirely.


`--server.default-api-compatibility`

This option can be used to determine the API compatibility of the ArangoDB server. It expects an ArangoDB version number as an integer, calculated as follows:

10000 * major + 100 * minor (example: 10400 for ArangoDB 1.4)

The value of this option will have an influence on some API return values when the HTTP client used does not send any compatibility information.

In most cases it will be sufficient to not set this option explicitly but to keep the default value. However, in case an "old" ArangoDB client is used that does not send any compatibility information and that cannot handle the responses of the current version of ArangoDB, it might be reasonable to set the option to an old version number to improve compatibility with older clients.


`--server.allow-method-override`

When this option is set to true, the HTTP request method will optionally be fetched from one of the following HTTP request headers if present in the request:

* x-http-method
* x-http-method-override
* x-method-override

If the option is set to true and any of these headers is set, the request method will be overriden by the value of the header. For example, this allows issuing an HTTP DELETE request which to the outside world will look like an HTTP GET request. This allows bypassing proxies and tools that will only let certain request types pass.

Setting this option to true may impose a security risk so it should only be used in controlled environments.

The default value for this option is false.


`--server.keyfile filename`

If SSL encryption is used, this option must be used to specify the filename of the server private key. The file must be PEM formatted and contain both the certificate and the server's private key.

The file specified by filename should have the following structure:

  -----BEGIN CERTIFICATE-----

  (base64 encoded certificate)

  -----END CERTIFICATE-----
  -----BEGIN RSA PRIVATE KEY-----

  (base64 encoded private key)

  -----END RSA PRIVATE KEY-----

You may use certificates issued by a Certificate Authority or self-signed certificates. Self-signed certificates can be created by a tool of your choice. When using OpenSSL for creating the self-signed certificate, the following commands should create a valid keyfile:

  # create private key in file "server.key"
  openssl genrsa -des3 -out server.key 1024

  # create certificate signing request (csr) in file "server.csr"
  openssl req -new -key server.key -out server.csr

  # copy away original private key to "server.key.org"
  cp server.key server.key.org

  # remove passphrase from the private key
  openssl rsa -in server.key.org -out server.key

  # sign the csr with the key, creates certificate PEM file "server.crt"
  openssl x509 -req -days 365 -in server.csr -signkey server.key -out server.crt

  # combine certificate and key into single PEM file "server.pem"
  cat server.crt server.key > server.pem
For further information please check the manuals of the tools you use to create the certificate.

Note: the `--server.keyfile` option must be set if the server is started with at least one SSL endpoint.


`--server.cafile filename`

This option can be used to specify a file with CA certificates that are sent to the client whenever the server requests a client certificate. If the file is specified, The server will only accept client requests with certificates issued by these CAs. Do not specify this option if you want clients to be able to connect without specific certificates.

The certificates in filename must be PEM formatted.

Note: this option is only relevant if at least one SSL endpoint is used.


`--server.ssl-protocol value`

Use this option to specify the default encryption protocol to be used. The following variants are available:

  1: SSLv2
  2: SSLv23
  3: SSLv3
  4: TLSv1
The default value is 4 (i.e. TLSv1).

Note: this option is only relevant if at least one SSL endpoint is used.


`--server.ssl-cache value`

Set to true if SSL session caching should be used.

value has a default value of false (i.e. no caching).
Note: this option is only relevant if at least one SSL endpoint is used, and only if the client supports sending the session id.


`--server.ssl-options value`

This option can be used to set various SSL-related options. Individual option values must be combined using bitwise OR.

Which options are available on your platform is determined by the OpenSSL version you use. The list of options available on your platform might be retrieved by the following shell command:

  > grep "#define SSL_OP_.*" /usr/include/openssl/ssl.h

  #define SSL_OP_MICROSOFT_SESS_ID_BUG                    0x00000001L
  #define SSL_OP_NETSCAPE_CHALLENGE_BUG                   0x00000002L
  #define SSL_OP_LEGACY_SERVER_CONNECT                    0x00000004L
  #define SSL_OP_NETSCAPE_REUSE_CIPHER_CHANGE_BUG         0x00000008L
  #define SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG              0x00000010L
  #define SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER               0x00000020L
  ...

A description of the options can be found online in the OpenSSL documentation at: http://www.openssl.org/docs/ssl/SSL_CTX_set_options.html

Note: this option is only relevant if at least one SSL endpoint is used.


`--server.ssl-cipher-list cipher-list`

This option can be used to restrict the server to certain SSL ciphers only, and to define the relative usage preference of SSL ciphers.

The format of cipher-list is documented in the OpenSSL documentation.

To check which ciphers are available on your platform, you may use the following shell command:

  > openssl ciphers -v

  ECDHE-RSA-AES256-SHA    SSLv3 Kx=ECDH     Au=RSA  Enc=AES(256)  Mac=SHA1
  ECDHE-ECDSA-AES256-SHA  SSLv3 Kx=ECDH     Au=ECDSA Enc=AES(256)  Mac=SHA1
  DHE-RSA-AES256-SHA      SSLv3 Kx=DH       Au=RSA  Enc=AES(256)  Mac=SHA1
  DHE-DSS-AES256-SHA      SSLv3 Kx=DH       Au=DSS  Enc=AES(256)  Mac=SHA1
  DHE-RSA-CAMELLIA256-SHA SSLv3 Kx=DH       Au=RSA  Enc=Camellia(256) Mac=SHA1
  ...

The default value for cipher-list is "ALL".

Note: this option is only relevant if at least one SSL endpoint is used.


`--server.backlog-size`

Allows to specify the size of the backlog for the listen system call The default value is 10. The maximum value is platform-dependent.

<!--
@anchor CommandLineArangoEndpoint
@copydetails triagens::rest::ApplicationEndpointServer::_endpoints

@CLEARPAGE
@anchor CommandLineArangoReuseAddress
@copydetails triagens::rest::ApplicationEndpointServer::_reuseAddress

@CLEARPAGE
@anchor CommandLineArangoDisableAuthentication
@copydetails triagens::arango::ArangoServer::_disableAuthentication

@CLEARPAGE
@anchor CommandLineArangoDisableAuthenticationUnixSockets
@copydetails triagens::arango::ArangoServer::_disableAuthenticationUnixSockets

@CLEARPAGE
@anchor CommandLineArangoAuthenticateSystemOnly
@copydetails triagens::arango::ArangoServer::_authenticateSystemOnly

@CLEARPAGE
@anchor CommandLineArangoDisableReplicationLogger
@copydetails triagens::arango::ArangoServer::_disableReplicationLogger

@CLEARPAGE
@anchor CommandLineArangoDisableReplicationApplier
@copydetails triagens::arango::ArangoServer::_disableReplicationApplier

@CLEARPAGE
@anchor CommandLineArangoKeepAliveTimeout
@copydetails triagens::rest::ApplicationEndpointServer::_keepAliveTimeout

@CLEARPAGE
@anchor CommandLineArangoDefaultApiCompatibility
@copydetails triagens::rest::ApplicationEndpointServer::_defaultApiCompatibility

@CLEARPAGE
@anchor CommandLineArangoAllowMethodOverride
@copydetails triagens::rest::ApplicationEndpointServer::_allowMethodOverride

@CLEARPAGE
@anchor CommandLineArangoKeyFile
@copydetails triagens::rest::ApplicationEndpointServer::_httpsKeyfile

@CLEARPAGE
@anchor CommandLineArangoCaFile
@copydetails triagens::rest::ApplicationEndpointServer::_cafile

@CLEARPAGE
@anchor CommandLineArangoSslProtocol
@copydetails triagens::rest::ApplicationEndpointServer::_sslProtocol

@CLEARPAGE
@anchor CommandLineArangoSslCacheMode
@copydetails triagens::rest::ApplicationEndpointServer::_sslCache

@CLEARPAGE
@anchor CommandLineArangoSslOptions
@copydetails triagens::rest::ApplicationEndpointServer::_sslOptions

@CLEARPAGE
@anchor CommandLineArangoSslCipherList
@copydetails triagens::rest::ApplicationEndpointServer::_sslCipherList

@CLEARPAGE
@anchor CommandLineArangoBacklogSize
@copydetails triagens::rest::ApplicationEndpointServer::_backlogSize
-->

If this option is specified and value is `true`, then the HTML
administration interface at URL `http://server:port/` will be disabled and
cannot used by any user at all.

`--disable-statistics value`

If this option is *value* is `true`, then ArangoDB's statistics gathering
is turned off. Statistics gathering causes constant CPU activity so using this
option to turn it off might relieve heavy-loaded instances.
Note: this option is only available when ArangoDB has not been compiled with
the option `--disable-figures`.

`--database.directory directory`

The directory containing the collections and datafiles. Defaults to /var/lib/arango. When specifying the database directory, please make sure the directory is actually writable by the arangod process.

You should further not use a database directory which is provided by a network filesystem such as NFS. The reason is that networked filesystems might cause inconsistencies when there are multiple parallel readers or writers or they lack features required by arangod (e.g. flock()).

`directory`

When using the command line version, you can simply supply the database directory as argument.

*Examples*

  > ./arangod --server.endpoint tcp://127.0.0.1:8529 --database.directory /tmp/vocbase

`--database.maximal-journal-size size`

Maximal size of journal in bytes. Can be overwritten when creating a new collection. Note that this also limits the maximal size of a single document.

The default is 32MB.


`--database.wait-for-sync boolean`

Default wait-for-sync value. Can be overwritten when creating a new collection.

The default is false.


`--database.force-sync-properties boolean`

Force syncing of collection properties to disk after creating a collection or updating its properties.

If turned off, syncing will still happen for collection that have a waitForSync value of true. If turned on, syncing of properties will always happen, regardless of the value of waitForSync.

The default is true.


`--database.remove-on-drop flag`

If true and you drop a collection, then they directory and all associated datafiles will be removed from disk. If false, then they collection directory will be renamed to deleted-..., but remains on hard disk. To restore such a dropped collection, you can rename the directory back to collection-..., but you must also edit the file parameter.json inside the directory.

The default is true.


`--javascript.gc-frequency frequency`

Specifies the frequency (in seconds) for the automatic garbage collection of JavaScript objects. This setting is useful to have the garbage collection still work in periods with no or little numbers of requests.


`--javascript.gc-interval interval`

Specifies the interval (approximately in number of requests) that the garbage collection for JavaScript objects will be run in each thread.


`--javascript.v8-options options`

Optional arguments to pass to the V8 Javascript engine. The V8 engine will run with default settings unless explicit options are specified using this option. The options passed will be forwarded to the V8 engine which will parse them on its own. Passing invalid options may result in an error being printed on stderr and the option being ignored.

Options need to be passed in one string, with V8 option names being prefixed with double dashes. Multiple options need to be separated by whitespace. To get a list of all available V8 options, you can use the value "&ndash;help" as follows:

`--javascript.v8-options "--help"`

Another example of specific V8 options being set at startup:

`--javascript.v8-options "--harmony --log"`

Names and features or usable options depend on the version of V8 being used, and might change in the future if a different version of V8 is being used in ArangoDB. Not all options offered by V8 might be sensible to use in the context of ArangoDB. Use the specific options only if you are sure that they are not harmful for the regular database operation.

<!--
@CLEARPAGE
@anchor CommandLineArangoDirectory
@copydetails triagens::arango::ArangoServer::_databasePath

@CLEARPAGE
@anchor CommandLineArangoMaximalJournalSize
@copydetails triagens::arango::ArangoServer::_defaultMaximalSize

@CLEARPAGE
@anchor CommandLineArangoWaitForSync
@copydetails triagens::arango::ArangoServer::_defaultWaitForSync

@CLEARPAGE
@anchor CommandLineArangoForceSyncProperties
@copydetails triagens::arango::ArangoServer::_forceSyncProperties

@CLEARPAGE
@anchor CommandLineArangoRemoveOnDrop
@copydetails triagens::arango::ArangoServer::_removeOnDrop

@CLEARPAGE
@anchor CommandLineArangoJsGcFrequency
@copydetails triagens::arango::ApplicationV8::_gcFrequency

@CLEARPAGE
@anchor CommandLineArangoJsGcInterval
@copydetails triagens::arango::ApplicationV8::_gcInterval

@CLEARPAGE
@anchor CommandLineArangoJsV8Options
@copydetails triagens::arango::ApplicationV8::_v8Options
-->