////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "GeneralServer/SslServerOptionsProvider.h"

#include "Basics/application-exit.h"
#include "GeneralServer/SslServerOptions.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Ssl/ssl-helper.h"

namespace arangodb {

using namespace arangodb::options;

void SslServerOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> options, SslServerOptions& opts) {
  options->addOldOption("server.cafile", "ssl.cafile");
  options->addOldOption("server.keyfile", "ssl.keyfile");
  options->addOldOption("server.ssl-cache", "ssl.session-cache");
  options->addOldOption("server.ssl-cipher-list", "ssl.cipher-list");
  options->addOldOption("server.ssl-options", "ssl.options");
  options->addOldOption("server.ssl-protocol", "ssl.protocol");

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // make the future version known to the old binaries
  options->addSection("tls", "TLS communication");
  options->addOldOption("--tls.cafile", "--ssl.cafile");
  options->addOldOption("--tls.keyfile", "--ssl.keyfile");
  options->addOldOption("--tls.session-cache", "--ssl.session-cache");
  options->addOldOption("--tls.cipher-list", "--ssl.cipher-list");
  options->addOldOption("--tls.protocol", "--ssl.protocol");
  options->addOldOption("--tls.options", "--ssl.options");
  options->addOldOption("--tls.ecdh-curve", "--ssl.ecdh-curve");
  options->addOldOption("--tls.prefer-http1-in-alpn",
                        "--ssl.prefer-http1-in-alpn");
#endif
  options->addSection("ssl", "SSL communication");

  options
      ->addOption("--ssl.cafile", "The CA file used for secure connections.",
                  new StringParameter(&opts.cafile))
      .setLongDescription(R"(You can use this option to specify a file with
CA certificates that are sent to the client whenever the server requests a
client certificate. If you specify a file, the server only accepts client
requests with certificates issued by these CAs. Do not specify this option if
you want clients to be able to connect without specific certificates.

The certificates in the file must be PEM-formatted.)");

  options
      ->addOption("--ssl.keyfile",
                  "The path to a PEM file (server certificate + private key) "
                  "to use for secure connections.",
                  new StringParameter(&opts.keyfile))
      .setLongDescription(R"(If you use TLS/SSL encryption by binding the
server to an `ssl://` endpoint (e.g. `--server.endpoint ssl://127.0.0.1:8529`),
you must use this option to specify the filename of the server's private key.
The file must be PEM-formatted and contain both, the certificate and the
server's private key.

You can generate a keyfile using OpenSSL as follows:

```bash
# create private key in file "server.key"
openssl genpkey -out server.key -algorithm RSA -pkeyopt rsa_keygen_bits:2048 -aes-128-cbc

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
```

You may use certificates issued by a Certificate Authority or self-signed
certificates. Self-signed certificates can be created by a tool of your
choice. When using OpenSSL for creating the self-signed certificate, the
above commands should create a valid keyfile with a structure like this:

```
-----BEGIN CERTIFICATE-----

(base64 encoded certificate)

-----END CERTIFICATE-----
-----BEGIN RSA PRIVATE KEY-----

(base64 encoded private key)

-----END RSA PRIVATE KEY-----
```

For further information please check the manuals of the tools you use to create
the certificate.)");

  options->addOption("--ssl.session-cache",
                     "Enable the session cache for connections.",
                     new BooleanParameter(&opts.sessionCache));

  options
      ->addOption("--ssl.cipher-list",
                  "The SSL ciphers to use. See the OpenSSL documentation.",
                  new StringParameter(&opts.cipherList))
      .setLongDescription(R"(You can use this option to restrict the server to
certain SSL ciphers only, and to define the relative usage preference of SSL
ciphers.

The format of the option's value is documented in the OpenSSL documentation.

To check which ciphers are available on your platform, you may use the
following shell command:

```bash
> openssl ciphers -v

ECDHE-RSA-AES256-SHA    SSLv3 Kx=ECDH     Au=RSA  Enc=AES(256)  Mac=SHA1
ECDHE-ECDSA-AES256-SHA  SSLv3 Kx=ECDH     Au=ECDSA Enc=AES(256)  Mac=SHA1
DHE-RSA-AES256-SHA      SSLv3 Kx=DH       Au=RSA  Enc=AES(256)  Mac=SHA1
DHE-DSS-AES256-SHA      SSLv3 Kx=DH       Au=DSS  Enc=AES(256)  Mac=SHA1
DHE-RSA-CAMELLIA256-SHA SSLv3 Kx=DH       Au=RSA  Enc=Camellia(256)
Mac=SHA1
...
```)");

  std::unordered_set<uint64_t> const sslProtocols = availableSslProtocols();

  options
      ->addOption("--ssl.protocol", availableSslProtocolsDescription(),
                  new DiscreteValuesParameter<UInt64Parameter>(
                      &opts.sslProtocol, sslProtocols))
      .setLongDescription(R"(Use this option to specify the default encryption
protocol to be used. The default value is 9 (generic TLS), which allows the
negotiation of the TLS version between the client and the server, dynamically
choosing the highest mutually supported version of TLS.

Note that SSLv2 is unsupported as of version 3.4, because of the inherent
security vulnerabilities in this protocol. Selecting SSLv2 as protocol aborts
the startup.)");

  options
      ->addOption("--ssl.options",
                  "The SSL connection options. See the OpenSSL documentation.",
                  new UInt64Parameter(&opts.sslOptions),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(You can use this option to set various SSL-related
options. Individual option values must be combined using bitwise OR.

Which options are available on your platform is determined by the OpenSSL
version you use. The list of options available on your platform might be
retrieved by the following shell command:

```bash
 > grep "#define SSL_OP_.*" /usr/include/openssl/ssl.h

 #define SSL_OP_MICROSOFT_SESS_ID_BUG                    0x00000001L
 #define SSL_OP_NETSCAPE_CHALLENGE_BUG                   0x00000002L
 #define SSL_OP_LEGACY_SERVER_CONNECT                    0x00000004L
 #define SSL_OP_NETSCAPE_REUSE_CIPHER_CHANGE_BUG         0x00000008L
 #define SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG              0x00000010L
 #define SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER               0x00000020L
 ...
```

A description of the options can be found online in the OpenSSL documentation:
http://www.openssl.org/docs/ssl/SSL_CTX_set_options.html))");

  options->addOption(
      "--ssl.ecdh-curve",
      "The SSL ECDH curve, see the output of \"openssl ecparam -list_curves\".",
      new StringParameter(&opts.ecdhCurve));

  options->addOption("--ssl.prefer-http1-in-alpn",
                     "Allows to let the server prefer HTTP/1.1 over HTTP/2 in "
                     "ALPN protocol negotiations",
                     new BooleanParameter(&opts.preferHttp11InAlpn));
}

void SslServerOptionsProvider::validateOptions(
    std::shared_ptr<ProgramOptions> options, SslServerOptions& opts) {
  // check for SSLv2
  if (opts.sslProtocol == SslProtocol::SSL_V2) {
    LOG_TOPIC("b7890", FATAL, arangodb::Logger::SSL)
        << "SSLv2 is not supported any longer because of security "
           "vulnerabilities in this protocol";
    FATAL_ERROR_EXIT();
  }
}

}  // namespace arangodb
