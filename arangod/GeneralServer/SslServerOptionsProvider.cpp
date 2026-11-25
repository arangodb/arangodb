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

void SslServerOptionsProvider::declareOptionsImpl(
    std::shared_ptr<ProgramOptions> options, SslServerOptions& opts) {
  options->addSection("tls", "TLS communication");

  options
      ->addOption("--tls.cafile", "The CA file used for secure connections.",
                  new StringParameter(&opts.cafile))
      .setLongDescription(R"(You can use this option to specify a file with
CA certificates for verifying client certificates (mutual TLS). When the server
requests a client certificate, it sends the names of these CAs to the client
and only accepts client certificates issued by these CAs.

The server only requests client certificates if you also enable the
`--tls.require-peer-certificate` option. Without it, this option has no effect
on which clients can connect.

The certificates in the file must be PEM-formatted. To trust multiple CAs,
concatenate their certificates in one file.

```bash
arangod \
  --server.endpoint https://0.0.0.0:8529 \
  --tls.keyfile server.pem \
  --tls.cafile client-ca.crt \
  --tls.require-peer-certificate \
  ...
```)");

  options
      ->addOption("--tls.keyfile",
                  "The path to a PEM file (server certificate + private key) "
                  "to use for secure connections.",
                  new StringParameter(&opts.keyfile))
      .setLongDescription(R"(If you use TLS encryption by binding the server
to an `https://` endpoint (e.g. `--server.endpoint https://127.0.0.1:8529`),
you must use this option to specify the path to a file with the server's
certificate and private key.

The file must be PEM-formatted and contain the server certificate, followed by
any intermediate CA certificates, and the server's private key. Clients that
only trust the root CA can't verify the server certificate if the intermediate
CA certificates are missing.

Use a private key that is not protected by a passphrase. Otherwise, OpenSSL
prompts for the passphrase on the terminal during startup, possibly multiple
times, and the server fails to start if no terminal is attached, for example,
when running as a service or in a container.

You may use certificates issued by a Certificate Authority (CA) or self-signed
certificates. The certificate needs to list all host names and IP addresses
that clients use to connect to the server in the Subject Alternative Name (SAN)
extension. Many clients, including web browsers and Go-based tools, ignore the
Common Name (CN) of the certificate subject. Clients that still fall back to
the CN only do so for host names, not for IP addresses.

You can generate a self-signed certificate with an ECDSA key (curve P-256) and
create a keyfile using OpenSSL as follows:

```bash
# Create a private key in file "server.key" and a self-signed certificate
# in file "server.crt" that is valid for 365 days
openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:P-256 -noenc \
  -keyout server.key -out server.crt -days 365 \
  -subj "/CN=localhost" \
  -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"

# combine certificate and key into a single PEM file "server.pem"
cat server.crt server.key > server.pem
```

Replace `localhost` and `127.0.0.1` with the host names and IP addresses of
your server. To create an RSA key instead, use `-newkey rsa:2048` (or a larger
key size) in place of `-newkey ec -pkeyopt ec_paramgen_curve:P-256`.

The resulting keyfile has a structure like this:

```
-----BEGIN CERTIFICATE-----
(base64-encoded certificate)
-----END CERTIFICATE-----
-----BEGIN PRIVATE KEY-----
(base64-encoded private key)
-----END PRIVATE KEY-----
```

If you use an ECDSA certificate and restrict the ciphers with
`--tls.cipher-list`, make sure to allow ECDSA cipher suites (`ECDHE-ECDSA-*`)
for TLS 1.2 connections.

For further information, see the manuals of the tools you use to create
the certificate.)");

  options->addOption("--tls.session-cache",
                     "Enable the session cache for connections.",
                     new BooleanParameter(&opts.sessionCache));

  options
      ->addOption("--tls.cipher-list",
                  "The TLS ciphers to use. See the OpenSSL documentation.",
                  new StringParameter(&opts.cipherList))
      .setLongDescription(R"(You can use this option to restrict the server to
certain cipher suites only, and to define the relative usage preference of the
cipher suites.

The option only applies to TLS 1.2 and older protocol versions. The cipher
suites of TLS 1.3 are not affected by this option.

The format of the option's value is documented in the OpenSSL documentation:
<https://docs.openssl.org/3.5/man1/openssl-ciphers/>

To check which cipher suites a value selects, you can use the `openssl` tool.
For example, for the default value:

```bash
> openssl ciphers -v 'HIGH:!EXPORT:!aNULL@STRENGTH'

TLS_AES_256_GCM_SHA384         TLSv1.3 Kx=any      Au=any   Enc=AESGCM(256)            Mac=AEAD
TLS_CHACHA20_POLY1305_SHA256   TLSv1.3 Kx=any      Au=any   Enc=CHACHA20/POLY1305(256) Mac=AEAD
TLS_AES_128_GCM_SHA256         TLSv1.3 Kx=any      Au=any   Enc=AESGCM(128)            Mac=AEAD
ECDHE-ECDSA-AES256-GCM-SHA384  TLSv1.2 Kx=ECDH     Au=ECDSA Enc=AESGCM(256)            Mac=AEAD
ECDHE-RSA-AES256-GCM-SHA384    TLSv1.2 Kx=ECDH     Au=RSA   Enc=AESGCM(256)            Mac=AEAD
DHE-DSS-AES256-GCM-SHA384      TLSv1.2 Kx=DH       Au=DSS   Enc=AESGCM(256)            Mac=AEAD
DHE-RSA-AES256-GCM-SHA384      TLSv1.2 Kx=DH       Au=RSA   Enc=AESGCM(256)            Mac=AEAD
ECDHE-ECDSA-CHACHA20-POLY1305  TLSv1.2 Kx=ECDH     Au=ECDSA Enc=CHACHA20/POLY1305(256) Mac=AEAD
ECDHE-RSA-CHACHA20-POLY1305    TLSv1.2 Kx=ECDH     Au=RSA   Enc=CHACHA20/POLY1305(256) Mac=AEAD
...
```

The output depends on the OpenSSL version of the `openssl` tool, which can
differ from the OpenSSL version that is built into the server. The server logs
its OpenSSL version at startup.

The cipher suites need to match the key type of the server certificate. For
example, an ECDSA certificate requires `ECDHE-ECDSA-*` cipher suites, and an
RSA certificate requires cipher suites with RSA authentication like
`ECDHE-RSA-*`. If no allowed cipher suite matches, TLS 1.2 handshakes fail with
a `handshake_failure` alert.

For example, you can only allow cipher suites with forward secrecy and
authenticated encryption for TLS 1.2 as follows:

```bash
arangod --tls.cipher-list 'ECDHE+AESGCM:ECDHE+CHACHA20' ...
```

You can also set the OpenSSL security level with this option by appending
`@SECLEVEL=<level>` to a cipher selection, for example,
`HIGH:!aNULL:@SECLEVEL=0`. `@SECLEVEL=0` alone selects no cipher suites and
is rejected. The default security level doesn't permit TLS 1.0 and TLS 1.1.
Lowering the security level is insecure and not recommended.

Note that the server treats `@` characters in startup option values specially:
`@NAME@` is replaced by the value of the environment variable `NAME`, and `@@`
stands for a literal `@`. A value with a single `@` is used as is, but if a
value contains multiple `@` characters, you need to escape them as `@@`. For
example, use `HIGH:!aNULL@@STRENGTH:@@SECLEVEL=0` to pass
`HIGH:!aNULL@STRENGTH:@SECLEVEL=0` to OpenSSL. Without the escaping, the server
silently uses `HIGH:!aNULLSECLEVEL=0`. The server logs the cipher list it uses
with `--log.level ssl=info` at startup.)");

  std::unordered_set<uint64_t> const sslProtocols = availableSslProtocols();

  options
      ->addOption("--tls.protocol", availableSslProtocolsDescription(),
                  new DiscreteValuesParameter<UInt64Parameter>(
                      &opts.sslProtocol, sslProtocols))
      .setLongDescription(R"(Use this option to specify the default encryption
protocol to be used. The default value is `9` (generic TLS), which allows the
negotiation of the TLS version between the client and the server, dynamically
choosing the highest mutually supported version of TLS.

The other values restrict the server to exactly one TLS version. For example,
use `--tls.protocol 6` to only accept TLS 1.3 connections.

With the default OpenSSL security level, the server only accepts TLS 1.2 and
TLS 1.3 connections. TLS 1.0 and TLS 1.1 (including `--tls.protocol 4`) are
insecure and require lowering the security level with
`--tls.cipher-list` (e.g. `HIGH:!aNULL:@SECLEVEL=0`). This is not recommended.)");

  options
      ->addOption("--tls.options",
                  "The TLS connection options. See the OpenSSL documentation.",
                  new UInt64Parameter(&opts.sslOptions),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(You can use this option to set various TLS-related
options of OpenSSL. Individual option values must be combined using bitwise OR.

The default value is `2147485776` (`0x80000850`), which corresponds to the
`SSL_OP_ALL` set of bug workarounds of OpenSSL 3: `SSL_OP_TLSEXT_PADDING`,
`SSL_OP_SAFARI_ECDHE_ECDSA_BUG`, `SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS`, and
`SSL_OP_CRYPTOPRO_TLSEXT_BUG`. The server logs the active options with
`--log.level ssl=info` at startup:

```
INFO [afcd3] {ssl} using SSL options: SSL_OP_TLSEXT_PADDING, SSL_OP_SAFARI_ECDHE_ECDSA_BUG, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS, SSL_OP_CRYPTOPRO_TLSEXT_BUG
```

Which options are available is determined by the OpenSSL version. In OpenSSL 3,
each option is a single bit `n` with the value 2<sup>n</sup>, defined via the
`SSL_OP_BIT(n)` macro. If the OpenSSL development headers are installed, you can
list the options with the following shell command:

```bash
> grep -E "# *define SSL_OP_" /usr/include/openssl/ssl.h

#define SSL_OP_BIT(n) ((uint64_t)1 << (uint64_t)n)
#define SSL_OP_NO_EXTENDED_MASTER_SECRET SSL_OP_BIT(0)
#define SSL_OP_CLEANSE_PLAINTEXT SSL_OP_BIT(1)
#define SSL_OP_LEGACY_SERVER_CONNECT SSL_OP_BIT(2)
#define SSL_OP_ENABLE_KTLS SSL_OP_BIT(3)
#define SSL_OP_TLSEXT_PADDING SSL_OP_BIT(4)
#define SSL_OP_SAFARI_ECDHE_ECDSA_BUG SSL_OP_BIT(6)
#define SSL_OP_IGNORE_UNEXPECTED_EOF SSL_OP_BIT(7)
...
```

For example, to make the server prefer its own order of cipher suites over the
client's order, add `SSL_OP_CIPHER_SERVER_PREFERENCE` (bit 22) to the default
value:

```bash
arangod --tls.options $(( 0x80000850 | (1 << 22) )) ...
# equivalent to:
arangod --tls.options 2151680080 ...
```

A description of the options can be found in the OpenSSL documentation:
<https://docs.openssl.org/3.5/man3/SSL_CTX_set_options/>)");

  options
      ->addOption("--tls.ecdh-curve",
                  "The TLS ECDH curve, see the output of \"openssl ecparam "
                  "-list_curves\".",
                  new StringParameter(&opts.ecdhCurve))
      .setLongDescription(R"(The elliptic curve to use for the ephemeral
Elliptic Curve Diffie-Hellman (ECDHE) key exchange of TLS 1.2 and TLS 1.3
connections. It is unrelated to the curve of an ECDSA key in the server
certificate.

The server only accepts this one curve for the key exchange:

- TLS 1.3 clients that don't support the curve cannot connect.
- TLS 1.2 clients that don't support the curve cannot use ECDHE cipher suites.
  Depending on the server certificate and the allowed cipher suites, the
  handshake fails or uses a key exchange without forward secrecy.
- Clients that support the curve but initially offer a key for a different
  curve, like X25519, need an additional round trip to establish the
  connection (HelloRetryRequest).

You can only set this option to the name of an elliptic curve as listed by
`openssl ecparam -list_curves`, like `prime256v1` (P-256, the default),
`secp384r1`, or `secp521r1`. You cannot set it to other key exchange groups
like `X25519`, and doing so prevents the server from starting.

However, the server can still use these groups if you set the option to an
empty string. The key exchange is then not restricted to a single curve, and
OpenSSL negotiates one of its default groups with the client. These include
X25519 and, as of OpenSSL 3.5, the hybrid post-quantum group X25519MLKEM768:

```bash
arangod --tls.ecdh-curve "" ...
```)");

  options->addOption("--tls.prefer-http1-in-alpn",
                     "Allows to let the server prefer HTTP/1.1 over HTTP/2 in "
                     "ALPN protocol negotiations",
                     new BooleanParameter(&opts.preferHttp11InAlpn));

  options->addOldOption("--ssl.cafile", "--tls.cafile");
  options->addOldOption("--ssl.keyfile", "--tls.keyfile");
  options->addOldOption("--ssl.session-cache", "--tls.session-cache");
  options->addOldOption("--ssl.cipher-list", "--tls.cipher-list");
  options->addOldOption("--ssl.protocol", "--tls.protocol");
  options->addOldOption("--ssl.options", "--tls.options");
  options->addOldOption("--ssl.ecdh-curve", "--tls.ecdh-curve");
  options->addOldOption("--ssl.prefer-http1-in-alpn",
                        "--tls.prefer-http1-in-alpn");
}

}  // namespace arangodb
