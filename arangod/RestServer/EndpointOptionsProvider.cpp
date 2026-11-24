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

#include "EndpointOptionsProvider.h"

#include <sys/socket.h>

#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

using namespace arangodb::options;

void EndpointOptionsProvider::declareOptionsImpl(
    std::shared_ptr<ProgramOptions> options, EndpointFeatureOptions& opts) {
  options
      ->addOption("--server.endpoint",
                  "Endpoint for client requests (e.g. "
                  "`http://127.0.0.1:8529`, or "
                  "`https://192.168.1.1:8529`)",
                  new VectorParameter<StringParameter>(&opts.endpoints))
      .setLongDescription(R"(You can specify this option multiple times to let
the ArangoDB server listen for incoming requests on multiple endpoints.

The endpoints are normally specified either in ArangoDB's configuration file or
on the command-line with `--server.endpoint`. ArangoDB supports the following
types of endpoints:

| Endpoint               | Encryption |
|:-----------------------|:-----------|
| `http://address:port`  | None       |
| `https://address:port` | TLS        |

The address can be one of the following:

- An IPv4 address, like `http://192.168.1.1:8529`
- An IPv6 address in square brackets, like `https://[fd12:3456::789a]:8529`
- A host name, like `https://db.example.com:8529`. The server resolves the name
  once on startup and binds to the first IPv4 address it resolves to. To
  resolve the name to an IPv6 address, put it in square brackets, like
  `https://[db.example.com]:8529`.

If you don't specify a port number, the default port 8529 is used.

If you use TLS-encrypted endpoints, you must also supply the path to a server
certificate using the `--tls.keyfile` option.

```bash
arangod \
  --server.endpoint http://127.0.0.1:8529 \
  --server.endpoint https://127.0.0.1:8530 \
  --tls.keyfile server.pem \
  /tmp/data-dir

...
2026-10-28T13:12:19Z [1] INFO [6ea38] {general} using endpoint 'http+ssl://127.0.0.1:8530' for TLS-encrypted requests
2026-10-28T13:12:19Z [1] INFO [6ea38] {general} using endpoint 'http+tcp://127.0.0.1:8529' for non-encrypted requests
2026-10-28T13:12:20Z [1] INFO [cf3f4] {general} ArangoDB (version 3.12.13 enterprise [linux]) is ready for business. Have fun!
```

The server logs the endpoints in a normalized form, with `http+tcp://` for
`http://` and `http+ssl://` for `https://`. The `ssl` in these names is a
historical term and refers to TLS. ArangoDB doesn't support the outdated SSL
protocols. For backward compatibility, you can also specify endpoints in this
form, or use `tcp://` and `ssl://` as aliases for `http://` and `https://`.

On a network interface, each port can only be bound **exactly once**. You can
look up the available interfaces and their addresses with the `ip address`
command on Linux. Interface names differ between operating systems and the
hardware they run on, like `eth0`, `enp3s0`, or `wlan0`. Every host typically
has a loopback interface, which is a virtual interface. By convention, it has
the address `127.0.0.1` (IPv4) or `::1` (IPv6), and can only be reached from the
same host.

To find out which services already use ports so that ArangoDB can't bind them
anymore, you can use `ss -lntp` on Linux, or `netstat -lntp` on systems without
`ss`.

To make ArangoDB reachable on all network interfaces of the host, bind it to
the unspecified address `0.0.0.0` (IPv4) or `[::]` (IPv6), like
`http://0.0.0.0:8529`. This is required when running ArangoDB in a container
and you want to reach it from outside the container. It may also be useful on
development systems that frequently change their network setup, like laptops.

ArangoDB can also listen on IPv6 link-local addresses if you add the zone ID
to the IPv6 address in the form `[ipv6-link-local-address%zone-id]`. However,
what you probably want instead is to bind to a local IPv6 address. Local IPv6
addresses start with `fd`. If you only see an IPv6 address starting with `fe80:`
in your interface configuration but none starting with `fd`, your interface
has no local IPv6 address assigned. You can read more about IPv6 link-local
addresses here: https://en.wikipedia.org/wiki/Link-local_address#IPv6.

For example, `ip address` may list the link-local address
`fe80::6257:18ff:fe82:3ec6` for the interface `eth0`, and the local IPv6 address
`fd12:3456::789a`. To bind ArangoDB to the link-local address, start `arangod`
with `--server.endpoint http://[fe80::6257:18ff:fe82:3ec6%eth0]:8529`. To bind
it to the local address, use `--server.endpoint http://[fd12:3456::789a]:8529`.
You can use `curl` to test the connection, like
`curl http://[fd12:3456::789a]:8529/_api/version`.)");

  options->addSection("tcp", "TCP features");

  options
      ->addOption("--tcp.reuse-address", "Try to reuse TCP port(s).",
                  new BooleanParameter(&opts.reuseAddress),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(If you set this option to `true`, the socket
option `SO_REUSEADDR` is set on all server endpoints, which is the default.
If you set this option to `false`, it is possible that it takes up to a minute
after a server has terminated until it is possible for a new server to use the
same endpoint again.

**Note**: This can be a security risk because it might be possible for another
process to bind to the same address and port, possibly hijacking network
traffic.)");

  options
      ->addOption("--tcp.backlog-size",
                  "Specify the size of the backlog for the `listen` "
                  "system call.",
                  new UInt64Parameter(&opts.backlogSize),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(The maximum value is platform-dependent.
If you specify a value higher than defined in the system header's `SOMAXCONN`
may result in a warning on server start. The actual value used by `listen`
may also be silently truncated on some platforms (this happens inside the
`listen` system call).)");
}

void EndpointOptionsProvider::validateOptionsImpl(
    std::shared_ptr<ProgramOptions> /*options*/, EndpointFeatureOptions& opts) {
  if (opts.backlogSize > SOMAXCONN) {
    LOG_TOPIC("b4d44", WARN, arangodb::Logger::FIXME)
        << "value for --tcp.backlog-size exceeds default system "
           "header SOMAXCONN value "
        << SOMAXCONN << ". trying to use " << SOMAXCONN << " anyway";
  }
}

}  // namespace arangodb
