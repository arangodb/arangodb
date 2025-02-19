////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "EndpointFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/application-exit.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

EndpointFeature::EndpointFeature(ArangodServer& server)
    : HttpEndpointProvider{server, *this},
      _reuseAddress(true),
      _backlogSize(64) {
  setOptional(true);
  startsAfter<application_features::AqlFeaturePhase, ArangodServer>();

  startsAfter<ServerFeature, ArangodServer>();

  // if our default value is too high, we'll use half of the max value provided
  // by the system
  if (_backlogSize > SOMAXCONN) {
    _backlogSize = SOMAXCONN / 2;
  }
}

void EndpointFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOldOption("server.backlog-size", "tcp.backlog-size");
  options->addOldOption("server.reuse-address", "tcp.reuse-address");

  options
      ->addOption("--server.endpoint",
                  "Endpoint for client requests (e.g. "
                  "`http://127.0.0.1:8529`, or "
                  "`https://192.168.1.1:8529`)",
                  new VectorParameter<StringParameter>(&_endpoints))
      .setLongDescription(R"(You can specify this option multiple times to let
the ArangoDB server listen for incoming requests on multiple endpoints.

The endpoints are normally specified either in ArangoDB's configuration file or
on the command-line with `--server.endpoint`. ArangoDB supports different types
of endpoints:

- `tcp://ipv4-address:port` - TCP/IP endpoint, using IPv4
- `tcp://[ipv6-address]:port` - TCP/IP endpoint, using IPv6
- `ssl://ipv4-address:port` - TCP/IP endpoint, using IPv4, SSL encryption
- `ssl://[ipv6-address]:port` - TCP/IP endpoint, using IPv6, SSL encryption
- `unix:///path/to/socket` - Unix domain socket endpoint

You can use `http://` as an alias for `tcp://`, and `https://` as an alias for
`ssl://`.

If a TCP/IP endpoint is specified without a port number, then the default port
(8529) is used.

If you use SSL-encrypted endpoints, you must also supply the path to a server
certificate using the `--ssl.keyfile` option.

```bash
arangod --server.endpoint tcp://127.0.0.1:8529 \
        --server.endpoint ssl://127.0.0.1:8530 \
        --ssl.keyfile server.pem /tmp/data-dir

...
2022-11-07T10:39:30Z [1] INFO [6ea38] {general} using endpoint 'http+ssl://0.0.0.0:8530' for ssl-encrypted requests
2022-11-07T10:39:30Z [1] INFO [6ea38] {general} using endpoint 'http+tcp://0.0.0.0:8529' for non-encrypted requests
2022-11-07T10:39:31Z [1] INFO [cf3f4] {general} ArangoDB (version 3.10.0 [linux]) is ready for business. Have fun!
```

On one specific ethernet interface, each port can only be bound
**exactly once**. You can look up your available interfaces using the `ifconfig`
command on Linux. The general names of the
interfaces differ between operating systems and the hardware they run on.
However, every host has typically a so called loopback interface, which is a
virtual interface. By convention, it always has the address `127.0.0.1` (IPv4)
or `::1` (IPv6), and can only be reached from the very same host. Ethernet
interfaces usually have names like `eth0`, `wlan0`, `eth1:17`, `le0`.

To find out which services already use ports (so ArangoDB can't bind them
anymore), you can use the `netstat` command. It behaves a little different on
each platform; run it with `-lnpt` on Linux for valuable information.

ArangoDB can also do a so called *broadcast bind* using `tcp://0.0.0.0:8529`.
This way, it is reachable on all interfaces of the host. This may be useful on
development systems that frequently change their network setup, like laptops.

ArangoDB can also listen to IPv6 link-local addresses via adding the zone ID
to the IPv6 address in the form `[ipv6-link-local-address%zone-id]`. However,
what you probably want instead is to bind to a local IPv6 address. Local IPv6
addresses start with `fd`. If you only see a `fe80:` IPv6 address in your
interface configuration but no IPv6 address starting with `fd`, your interface
has no local IPv6 address assigned. You can read more about IPv6 link-local
addresses here: https://en.wikipedia.org/wiki/Link-local_address#IPv6.

To bind to a link-local and local IPv6 address, run `ifconfig` or equivalent
command. The command lists all interfaces and assigned IP addresses. The
link-local address may be `fe80::6257:18ff:fe82:3ec6%eth0` (IPv6 address plus
interface name). A local IPv6 address may be `fd12:3456::789a`.
To bind ArangoDB to it, start `arangod` with
`--server.endpoint tcp://[fe80::6257:18ff:fe82:3ec6%eth0]:8529`.
You can use `telnet` to test the connection.)");

  options->addSection("tcp", "TCP features");

  options
      ->addOption("--tcp.reuse-address", "Try to reuse TCP port(s).",
                  new BooleanParameter(&_reuseAddress),
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
                  new UInt64Parameter(&_backlogSize),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(The maximum value is platform-dependent.
If you specify a value higher than defined in the system header's `SOMAXCONN`
may result in a warning on server start. The actual value used by `listen`
may also be silently truncated on some platforms (this happens inside the
`listen` system call).)");
}

void EndpointFeature::validateOptions(std::shared_ptr<ProgramOptions>) {
  if (_backlogSize > SOMAXCONN) {
    LOG_TOPIC("b4d44", WARN, arangodb::Logger::FIXME)
        << "value for --tcp.backlog-size exceeds default system "
           "header SOMAXCONN value "
        << SOMAXCONN << ". trying to use " << SOMAXCONN << " anyway";
  }
}

void EndpointFeature::prepare() {
  buildEndpointLists();

  if (_endpointList.empty()) {
    LOG_TOPIC("2c5f0", FATAL, arangodb::Logger::FIXME)
        << "no endpoints have been specified, giving up, please use the "
           "'--server.endpoint' option";
    FATAL_ERROR_EXIT();
  }
}

std::vector<std::string> EndpointFeature::httpEndpoints() {
  auto httpEntries = _endpointList.all();
  std::vector<std::string> result;

  for (auto http : httpEntries) {
    auto uri = Endpoint::uriForm(http);

    if (!uri.empty()) {
      result.emplace_back(uri);
    }
  }

  return result;
}

void EndpointFeature::buildEndpointLists() {
  for (auto const& it : _endpoints) {
    bool ok =
        _endpointList.add(it, static_cast<int>(_backlogSize), _reuseAddress);

    if (!ok) {
      LOG_TOPIC("1ddc1", FATAL, arangodb::Logger::FIXME)
          << "invalid endpoint '" << it << "'";
      FATAL_ERROR_EXIT();
    }
  }
}

}  // namespace arangodb
