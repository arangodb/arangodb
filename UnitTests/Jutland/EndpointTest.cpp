////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for Endpoint classes
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <boost/test/unit_test.hpp>

#include "Rest/Endpoint.h"
#include "Rest/EndpointUnixDomain.h"
#include "Rest/EndpointIp.h"
#include "Rest/EndpointIpV4.h"
#include "Rest/EndpointIpV6.h"

using namespace triagens;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                                            macros
// -----------------------------------------------------------------------------

#define DELETE_ENDPOINT(e) if (e != 0) delete e;

#define FEATURE_NAME(name) get ## name
  
#define FACTORY_NAME(name) name ## Factory  

#define FACTORY(name, specification) Endpoint::FACTORY_NAME(name)(specification)

#define CHECK_ENDPOINT_FEATURE(type, specification, feature, expected) \
  e = FACTORY(type, specification); \
  BOOST_CHECK_EQUAL(expected, e->FEATURE_NAME(feature)()); \
  DELETE_ENDPOINT(e);

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct EndpointSetup {
  EndpointSetup () {
    BOOST_TEST_MESSAGE("setup Endpoint");
  }

  ~EndpointSetup () {
    BOOST_TEST_MESSAGE("tear-down Endpoint");
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE (EndpointTest, EndpointSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalid
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (EndpointInvalid) {
  Endpoint* e = 0;

  BOOST_CHECK_EQUAL(e, Endpoint::clientFactory(""));
  BOOST_CHECK_EQUAL(e, Endpoint::clientFactory("@"));

  BOOST_CHECK_EQUAL(e, Endpoint::clientFactory("http://"));
  BOOST_CHECK_EQUAL(e, Endpoint::clientFactory("ssl://"));
  BOOST_CHECK_EQUAL(e, Endpoint::clientFactory("unix://"));

  BOOST_CHECK_EQUAL(e, Endpoint::clientFactory("fish://127.0.0.1:8529"));
  BOOST_CHECK_EQUAL(e, Endpoint::clientFactory("http://127.0.0.1:8529"));
  BOOST_CHECK_EQUAL(e, Endpoint::clientFactory("https://127.0.0.1:8529"));
  
  BOOST_CHECK_EQUAL(e, Endpoint::clientFactory("tcp//127.0.0.1:8529"));
  BOOST_CHECK_EQUAL(e, Endpoint::clientFactory("tcp:127.0.0.1:8529"));
  BOOST_CHECK_EQUAL(e, Endpoint::clientFactory("ssl:localhost"));
  BOOST_CHECK_EQUAL(e, Endpoint::clientFactory("ssl//:localhost"));
  BOOST_CHECK_EQUAL(e, Endpoint::clientFactory("unix///tmp/socket"));
  BOOST_CHECK_EQUAL(e, Endpoint::clientFactory("unix:tmp/socket"));
  
  BOOST_CHECK_EQUAL(e, Endpoint::clientFactory("fish@tcp://127.0.0.1:8529"));
  BOOST_CHECK_EQUAL(e, Endpoint::clientFactory("ssl@tcp://127.0.0.1:8529"));
  BOOST_CHECK_EQUAL(e, Endpoint::clientFactory("https@tcp://127.0.0.1:8529"));
  BOOST_CHECK_EQUAL(e, Endpoint::clientFactory("https@tcp://127.0.0.1:"));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test specification
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (EndpointSpecification) {
  Endpoint* e;

  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1", Specification, "tcp://127.0.0.1");
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost", Specification, "tcp://localhost");
  CHECK_ENDPOINT_FEATURE(client, "SSL://127.0.0.5", Specification, "SSL://127.0.0.5");
  CHECK_ENDPOINT_FEATURE(client, "httP@ssl://localhost:4635", Specification, "httP@ssl://localhost:4635");
  CHECK_ENDPOINT_FEATURE(server, "unix:///path/to/socket", Specification, "unix:///path/to/socket");
  CHECK_ENDPOINT_FEATURE(server, "htTp@UNIx:///a/b/c/d/e/f.s", Specification, "htTp@UNIx:///a/b/c/d/e/f.s");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test types
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (EndpointTypes) {
  Endpoint* e;

  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1", Type, Endpoint::ENDPOINT_CLIENT);
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost", Type, Endpoint::ENDPOINT_CLIENT);
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1", Type, Endpoint::ENDPOINT_CLIENT);
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost", Type, Endpoint::ENDPOINT_CLIENT);
  CHECK_ENDPOINT_FEATURE(client, "unix:///path/to/socket", Type, Endpoint::ENDPOINT_CLIENT);

  CHECK_ENDPOINT_FEATURE(server, "tcp://127.0.0.1", Type, Endpoint::ENDPOINT_SERVER);
  CHECK_ENDPOINT_FEATURE(server, "tcp://localhost", Type, Endpoint::ENDPOINT_SERVER);
  CHECK_ENDPOINT_FEATURE(server, "ssl://127.0.0.1", Type, Endpoint::ENDPOINT_SERVER);
  CHECK_ENDPOINT_FEATURE(server, "ssl://localhost", Type, Endpoint::ENDPOINT_SERVER);
  CHECK_ENDPOINT_FEATURE(server, "unix:///path/to/socket", Type, Endpoint::ENDPOINT_SERVER);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test domains
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (EndpointDomains) {
  Endpoint* e;

  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1", Domain, AF_INET);
  CHECK_ENDPOINT_FEATURE(client, "tcp://192.168.173.13", Domain, AF_INET);
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost", Domain, AF_INET);
  CHECK_ENDPOINT_FEATURE(client, "tcp://www.arangodb.org", Domain, AF_INET);
  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1:8529", Domain, AF_INET);
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost:8529", Domain, AF_INET);
  CHECK_ENDPOINT_FEATURE(client, "tcp://www.arangodb.org:8529", Domain, AF_INET);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]", Domain, AF_INET6);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]", Domain, AF_INET6);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]:8529", Domain, AF_INET6);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]:8529", Domain, AF_INET6);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[2001:0db8:0000:0000:0000:ff00:0042:8329]:8529", Domain, AF_INET6);
  CHECK_ENDPOINT_FEATURE(client, "http@tcp://[2001:0db8:0000:0000:0000:ff00:0042:8329]:8529", Domain, AF_INET6);
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1", Domain, AF_INET);
  CHECK_ENDPOINT_FEATURE(client, "ssl://192.168.173.13", Domain, AF_INET);
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost", Domain, AF_INET);
  CHECK_ENDPOINT_FEATURE(client, "ssl://www.arangodb.org", Domain, AF_INET);
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1:8529", Domain, AF_INET);
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost:8529", Domain, AF_INET);
  CHECK_ENDPOINT_FEATURE(client, "ssl://www.arangodb.org:8529", Domain, AF_INET);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]", Domain, AF_INET6);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]", Domain, AF_INET6);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]:8529", Domain, AF_INET6);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]:8529", Domain, AF_INET6);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[2001:0db8:0000:0000:0000:ff00:0042:8329]:8529", Domain, AF_INET6);
  CHECK_ENDPOINT_FEATURE(client, "http@ssl://[2001:0db8:0000:0000:0000:ff00:0042:8329]:8529", Domain, AF_INET6);
  
  CHECK_ENDPOINT_FEATURE(client, "unix:///tmp/socket", Domain, AF_UNIX);
  CHECK_ENDPOINT_FEATURE(client, "unix:///tmp/socket/arango.sock", Domain, AF_UNIX);
  CHECK_ENDPOINT_FEATURE(client, "http@unix:///tmp/socket/arango.sock", Domain, AF_UNIX);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test domain types
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (EndpointDomainTypes) {
  Endpoint* e;

  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1", DomainType, Endpoint::DOMAIN_IPV4);
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost", DomainType, Endpoint::DOMAIN_IPV4);
  CHECK_ENDPOINT_FEATURE(client, "tcp://www.arangodb.org", DomainType, Endpoint::DOMAIN_IPV4);
  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1:8529", DomainType, Endpoint::DOMAIN_IPV4);
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost:8529", DomainType, Endpoint::DOMAIN_IPV4);
  CHECK_ENDPOINT_FEATURE(client, "tcp://www.arangodb.org:8529", DomainType, Endpoint::DOMAIN_IPV4);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]", DomainType, Endpoint::DOMAIN_IPV6);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]", DomainType, Endpoint::DOMAIN_IPV6);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]:8529", DomainType, Endpoint::DOMAIN_IPV6);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]:8529", DomainType, Endpoint::DOMAIN_IPV6);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[2001:0db8:0000:0000:0000:ff00:0042:8329]:8529", DomainType, Endpoint::DOMAIN_IPV6);
  CHECK_ENDPOINT_FEATURE(client, "TCP://127.0.0.1", DomainType, Endpoint::DOMAIN_IPV4);
  CHECK_ENDPOINT_FEATURE(client, "Tcp://127.0.0.1", DomainType, Endpoint::DOMAIN_IPV4);
  CHECK_ENDPOINT_FEATURE(client, "tCP://127.0.0.1", DomainType, Endpoint::DOMAIN_IPV4);
  CHECK_ENDPOINT_FEATURE(client, "http@tcp://127.0.0.1", DomainType, Endpoint::DOMAIN_IPV4);
  
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1", DomainType, Endpoint::DOMAIN_IPV4);
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost", DomainType, Endpoint::DOMAIN_IPV4);
  CHECK_ENDPOINT_FEATURE(client, "ssl://www.arangodb.org", DomainType, Endpoint::DOMAIN_IPV4);
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1:8529", DomainType, Endpoint::DOMAIN_IPV4);
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost:8529", DomainType, Endpoint::DOMAIN_IPV4);
  CHECK_ENDPOINT_FEATURE(client, "ssl://www.arangodb.org:8529", DomainType, Endpoint::DOMAIN_IPV4);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]", DomainType, Endpoint::DOMAIN_IPV6);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]", DomainType, Endpoint::DOMAIN_IPV6);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]:8529", DomainType, Endpoint::DOMAIN_IPV6);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]:8529", DomainType, Endpoint::DOMAIN_IPV6);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[2001:0db8:0000:0000:0000:ff00:0042:8329]:8529", DomainType, Endpoint::DOMAIN_IPV6);
  CHECK_ENDPOINT_FEATURE(client, "SSL://127.0.0.1", DomainType, Endpoint::DOMAIN_IPV4);
  CHECK_ENDPOINT_FEATURE(client, "Ssl://127.0.0.1", DomainType, Endpoint::DOMAIN_IPV4);
  CHECK_ENDPOINT_FEATURE(client, "sSL://127.0.0.1", DomainType, Endpoint::DOMAIN_IPV4);
  CHECK_ENDPOINT_FEATURE(client, "http@ssl://127.0.0.1", DomainType, Endpoint::DOMAIN_IPV4);
  
  CHECK_ENDPOINT_FEATURE(client, "unix:///tmp/socket", DomainType, Endpoint::DOMAIN_UNIX);
  CHECK_ENDPOINT_FEATURE(client, "unix:///tmp/socket/arango.sock", DomainType, Endpoint::DOMAIN_UNIX);
  CHECK_ENDPOINT_FEATURE(client, "UNIX:///tmp/socket", DomainType, Endpoint::DOMAIN_UNIX);
  CHECK_ENDPOINT_FEATURE(client, "Unix:///tmp/socket", DomainType, Endpoint::DOMAIN_UNIX);
  CHECK_ENDPOINT_FEATURE(client, "uNIX:///tmp/socket", DomainType, Endpoint::DOMAIN_UNIX);
  CHECK_ENDPOINT_FEATURE(client, "http@unix:///tmp/socket", DomainType, Endpoint::DOMAIN_UNIX);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test ports
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (EndpointPorts) {
  Endpoint* e;

  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1", Port, EndpointIp::_defaultPort);
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost", Port, EndpointIp::_defaultPort);
  CHECK_ENDPOINT_FEATURE(client, "tcp://www.arangodb.org", Port, EndpointIp::_defaultPort); 
  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1:8529", Port, 8529);
  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1:8532", Port, 8532);
  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1:80", Port, 80);
  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1:443", Port, 443);
  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1:65535", Port, 65535);
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost:8529", Port, 8529);
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost:8532", Port, 8532);
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost:80", Port, 80);
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost:443", Port, 443);
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost:65535", Port, 65535);
  CHECK_ENDPOINT_FEATURE(client, "tcp://www.arangodb.org:8529", Port, 8529);
  CHECK_ENDPOINT_FEATURE(client, "http@tcp://www.arangodb.org:8529", Port, 8529);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]", Port, EndpointIp::_defaultPort);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]", Port, EndpointIp::_defaultPort);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]:8529", Port, 8529);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]:8532", Port, 8532);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]:80", Port, 80);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]:443", Port, 443);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]:65535", Port, 65535);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]:8529", Port, 8529);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]:8532", Port, 8532);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]:80", Port, 80);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]:443", Port, 443);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]:65535", Port, 65535);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[2001:0db8:0000:0000:0000:ff00:0042:8329]:666", Port, 666);
  CHECK_ENDPOINT_FEATURE(client, "http@tcp://[2001:0db8:0000:0000:0000:ff00:0042:8329]:666", Port, 666);
  
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1", Port, EndpointIp::_defaultPort);
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost", Port, EndpointIp::_defaultPort);
  CHECK_ENDPOINT_FEATURE(client, "ssl://www.arangodb.org", Port, EndpointIp::_defaultPort);
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1:8529", Port, 8529);
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1:8532", Port, 8532);
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1:80", Port, 80);
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1:443", Port, 443);
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1:65535", Port, 65535);
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost:8529", Port, 8529);
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost:8532", Port, 8532);
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost:80", Port, 80);
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost:443", Port, 443);
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost:65535", Port, 65535);
  CHECK_ENDPOINT_FEATURE(client, "ssl://www.arangodb.org:8529", Port, 8529);
  CHECK_ENDPOINT_FEATURE(client, "http@ssl://www.arangodb.org:8529", Port, 8529);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]", Port, EndpointIp::_defaultPort);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]", Port, EndpointIp::_defaultPort);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]:8529", Port, 8529);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]:8532", Port, 8532);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]:80", Port, 80);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]:443", Port, 443);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]:65535", Port, 65535);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]:8529", Port, 8529);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]:8532", Port, 8532);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]:80", Port, 80);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]:443", Port, 443);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]:65535", Port, 65535);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[2001:0db8:0000:0000:0000:ff00:0042:8329]:666", Port, 666);
  CHECK_ENDPOINT_FEATURE(client, "http@ssl://[2001:0db8:0000:0000:0000:ff00:0042:8329]:666", Port, 666);
  
  CHECK_ENDPOINT_FEATURE(client, "unix:///tmp/socket", Port, 0);
  CHECK_ENDPOINT_FEATURE(client, "unix:///tmp/socket/arango.sock", Port, 0);
  CHECK_ENDPOINT_FEATURE(client, "http@unix:///tmp/socket/arango.sock", Port, 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test encryption
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (EndpointEncryption) {
  Endpoint* e;

  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1", Encryption, Endpoint::ENCRYPTION_NONE);
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost", Encryption, Endpoint::ENCRYPTION_NONE);
  CHECK_ENDPOINT_FEATURE(client, "tcp://www.arangodb.org", Encryption, Endpoint::ENCRYPTION_NONE);
  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1:8529", Encryption, Endpoint::ENCRYPTION_NONE);
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost:8529", Encryption, Endpoint::ENCRYPTION_NONE);
  CHECK_ENDPOINT_FEATURE(client, "tcp://www.arangodb.org:8529", Encryption, Endpoint::ENCRYPTION_NONE);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]", Encryption, Endpoint::ENCRYPTION_NONE);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]", Encryption, Endpoint::ENCRYPTION_NONE);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]:8529", Encryption, Endpoint::ENCRYPTION_NONE);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]:8529", Encryption, Endpoint::ENCRYPTION_NONE);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[2001:0db8:0000:0000:0000:ff00:0042:8329]:666", Encryption, Endpoint::ENCRYPTION_NONE);
  CHECK_ENDPOINT_FEATURE(client, "http@tcp://[2001:0db8:0000:0000:0000:ff00:0042:8329]:666", Encryption, Endpoint::ENCRYPTION_NONE);
  
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1", Encryption, Endpoint::ENCRYPTION_SSL);
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost", Encryption, Endpoint::ENCRYPTION_SSL);
  CHECK_ENDPOINT_FEATURE(client, "ssl://www.arangodb.org", Encryption, Endpoint::ENCRYPTION_SSL);
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1:8529", Encryption, Endpoint::ENCRYPTION_SSL);
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost:8529", Encryption, Endpoint::ENCRYPTION_SSL);
  CHECK_ENDPOINT_FEATURE(client, "ssl://www.arangodb.org:8529", Encryption, Endpoint::ENCRYPTION_SSL);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]", Encryption, Endpoint::ENCRYPTION_SSL);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]", Encryption, Endpoint::ENCRYPTION_SSL);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]:8529", Encryption, Endpoint::ENCRYPTION_SSL);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[2001:0db8:0000:0000:0000:ff00:0042:8329]:666", Encryption, Endpoint::ENCRYPTION_SSL);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]:8529", Encryption, Endpoint::ENCRYPTION_SSL);
  CHECK_ENDPOINT_FEATURE(client, "SSL://[::]:8529", Encryption, Endpoint::ENCRYPTION_SSL);
  CHECK_ENDPOINT_FEATURE(client, "Ssl://[::]:8529", Encryption, Endpoint::ENCRYPTION_SSL);
  CHECK_ENDPOINT_FEATURE(client, "sSL://[::]:8529", Encryption, Endpoint::ENCRYPTION_SSL);
  CHECK_ENDPOINT_FEATURE(client, "http@ssl://[::]:8529", Encryption, Endpoint::ENCRYPTION_SSL);
  
  CHECK_ENDPOINT_FEATURE(client, "unix:///tmp/socket", Encryption, Endpoint::ENCRYPTION_NONE);
  CHECK_ENDPOINT_FEATURE(client, "unix:///tmp/socket/arango.sock", Encryption, Endpoint::ENCRYPTION_NONE);
  CHECK_ENDPOINT_FEATURE(client, "UNIX:///tmp/socket/arango.sock", Encryption, Endpoint::ENCRYPTION_NONE);
  CHECK_ENDPOINT_FEATURE(client, "Unix:///tmp/socket/arango.sock", Encryption, Endpoint::ENCRYPTION_NONE);
  CHECK_ENDPOINT_FEATURE(client, "uNIX:///tmp/socket/arango.sock", Encryption, Endpoint::ENCRYPTION_NONE);
  CHECK_ENDPOINT_FEATURE(client, "http@unix:///tmp/socket/arango.sock", Encryption, Endpoint::ENCRYPTION_NONE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test host
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (EndpointHost) {
  Endpoint* e;

  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1", Host, "127.0.0.1");
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost", Host, "localhost");
  CHECK_ENDPOINT_FEATURE(client, "tcp://www.arangodb.org", Host, "www.arangodb.org");
  CHECK_ENDPOINT_FEATURE(client, "tcp://arangodb.org", Host, "arangodb.org");
  CHECK_ENDPOINT_FEATURE(client, "tcp://DE.triagens.ArangoDB.org", Host, "DE.triagens.ArangoDB.org");
  CHECK_ENDPOINT_FEATURE(client, "tcp://192.168.173.13:8529", Host, "192.168.173.13");
  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1:8529", Host, "127.0.0.1");
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost:8529", Host, "localhost");
  CHECK_ENDPOINT_FEATURE(client, "tcp://www.arangodb.org:8529", Host, "www.arangodb.org");
  CHECK_ENDPOINT_FEATURE(client, "tcp://arangodb.org:8529", Host, "arangodb.org");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]", Host, "127.0.0.1");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]", Host, "::");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]:8529", Host, "127.0.0.1");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]:8529", Host, "::");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[2001:0db8:0000:0000:0000:ff00:0042:8329]", Host, "2001:0db8:0000:0000:0000:ff00:0042:8329");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[2001:0db8:0000:0000:0000:ff00:0042:8329]:8529", Host, "2001:0db8:0000:0000:0000:ff00:0042:8329");
  CHECK_ENDPOINT_FEATURE(client, "http@tcp://[::]:8529", Host, "::");
  
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1", Host, "127.0.0.1");
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost", Host, "localhost");
  CHECK_ENDPOINT_FEATURE(client, "ssl://www.arangodb.org", Host, "www.arangodb.org");
  CHECK_ENDPOINT_FEATURE(client, "ssl://arangodb.org", Host, "arangodb.org");
  CHECK_ENDPOINT_FEATURE(client, "ssl://DE.triagens.ArangoDB.org", Host, "DE.triagens.ArangoDB.org");
  CHECK_ENDPOINT_FEATURE(client, "ssl://192.168.173.13:8529", Host, "192.168.173.13");
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost:8529", Host, "localhost");
  CHECK_ENDPOINT_FEATURE(client, "ssl://www.arangodb.org:8529", Host, "www.arangodb.org");
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]", Host, "127.0.0.1");
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]", Host, "::");
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]:8529", Host, "127.0.0.1");
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]:8529", Host, "::");
  CHECK_ENDPOINT_FEATURE(client, "ssl://[2001:0db8:0000:0000:0000:ff00:0042:8329]", Host, "2001:0db8:0000:0000:0000:ff00:0042:8329");
  CHECK_ENDPOINT_FEATURE(client, "ssl://[2001:0db8:0000:0000:0000:ff00:0042:8329]:8529", Host, "2001:0db8:0000:0000:0000:ff00:0042:8329");
  CHECK_ENDPOINT_FEATURE(client, "http@ssl://[::]:8529", Host, "::");
  
  CHECK_ENDPOINT_FEATURE(client, "unix:///tmp/socket", Host, "localhost");
  CHECK_ENDPOINT_FEATURE(client, "unix:///tmp/socket/arango.sock", Host, "localhost");
  CHECK_ENDPOINT_FEATURE(client, "http@unix:///tmp/socket/arango.sock", Host, "localhost");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test hoststring
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (EndpointHostString) {
  Endpoint* e;

  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1", HostString, "127.0.0.1:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost", HostString, "localhost:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://www.arangodb.org", HostString, "www.arangodb.org:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://arangodb.org", HostString, "arangodb.org:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://DE.triagens.ArangoDB.org", HostString, "DE.triagens.ArangoDB.org:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://192.168.173.13:8529", HostString, "192.168.173.13:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://192.168.173.13:678", HostString, "192.168.173.13:678");
  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1:8529", HostString, "127.0.0.1:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1:44", HostString, "127.0.0.1:44");
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost:8529", HostString, "localhost:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost:65535", HostString, "localhost:65535");
  CHECK_ENDPOINT_FEATURE(client, "tcp://www.arangodb.org:8529", HostString, "www.arangodb.org:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://arangodb.org:8529", HostString, "arangodb.org:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]", HostString, "[127.0.0.1]:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]", HostString, "[::]:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]:8529", HostString, "[127.0.0.1]:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]:80", HostString, "[127.0.0.1]:80");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]:555", HostString, "[127.0.0.1]:555");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]:65535", HostString, "[127.0.0.1]:65535");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]:8529", HostString, "[::]:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]:80", HostString, "[::]:80");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]:8080", HostString, "[::]:8080");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[2001:0db8:0000:0000:0000:ff00:0042:8329]", HostString, "[2001:0db8:0000:0000:0000:ff00:0042:8329]:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[2001:0db8:0000:0000:0000:ff00:0042:8329]:8529", HostString, "[2001:0db8:0000:0000:0000:ff00:0042:8329]:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[2001:0db8:0000:0000:0000:ff00:0042:8329]:777", HostString, "[2001:0db8:0000:0000:0000:ff00:0042:8329]:777");
  CHECK_ENDPOINT_FEATURE(client, "http@tcp://[2001:0db8:0000:0000:0000:ff00:0042:8329]:777", HostString, "[2001:0db8:0000:0000:0000:ff00:0042:8329]:777");
  
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1", HostString, "127.0.0.1:8529");
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost", HostString, "localhost:8529");
  CHECK_ENDPOINT_FEATURE(client, "ssl://www.arangodb.org", HostString, "www.arangodb.org:8529");
  CHECK_ENDPOINT_FEATURE(client, "ssl://arangodb.org", HostString, "arangodb.org:8529");
  CHECK_ENDPOINT_FEATURE(client, "ssl://DE.triagens.ArangoDB.org", HostString, "DE.triagens.ArangoDB.org:8529");
  CHECK_ENDPOINT_FEATURE(client, "ssl://192.168.173.13:8529", HostString, "192.168.173.13:8529");
  CHECK_ENDPOINT_FEATURE(client, "ssl://192.168.173.13:1234", HostString, "192.168.173.13:1234");
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost:8529", HostString, "localhost:8529");
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost:5", HostString, "localhost:5");
  CHECK_ENDPOINT_FEATURE(client, "ssl://www.arangodb.org:8529", HostString, "www.arangodb.org:8529");
  CHECK_ENDPOINT_FEATURE(client, "ssl://www.arangodb.org:12345", HostString, "www.arangodb.org:12345");
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]", HostString, "[127.0.0.1]:8529");
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]", HostString, "[::]:8529");
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]:8529", HostString, "[127.0.0.1]:8529");
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]:32768", HostString, "[127.0.0.1]:32768");
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]:8529", HostString, "[::]:8529");
  CHECK_ENDPOINT_FEATURE(client, "ssl://[2001:0db8:0000:0000:0000:ff00:0042:8329]", HostString, "[2001:0db8:0000:0000:0000:ff00:0042:8329]:8529");
  CHECK_ENDPOINT_FEATURE(client, "ssl://[2001:0db8:0000:0000:0000:ff00:0042:8329]:8529", HostString, "[2001:0db8:0000:0000:0000:ff00:0042:8329]:8529");
  CHECK_ENDPOINT_FEATURE(client, "ssl://[2001:0db8:0000:0000:0000:ff00:0042:8329]:994", HostString, "[2001:0db8:0000:0000:0000:ff00:0042:8329]:994");
  CHECK_ENDPOINT_FEATURE(client, "http@ssl://[2001:0db8:0000:0000:0000:ff00:0042:8329]:994", HostString, "[2001:0db8:0000:0000:0000:ff00:0042:8329]:994");
  
  CHECK_ENDPOINT_FEATURE(client, "unix:///tmp/socket", HostString, "localhost");
  CHECK_ENDPOINT_FEATURE(client, "unix:///tmp/socket/arango.sock", HostString, "localhost");
  CHECK_ENDPOINT_FEATURE(client, "http@unix:///tmp/socket/arango.sock", HostString, "localhost");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test isconnected
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (EndpointIsConnectedServer1) {
  Endpoint* e;

  e = Endpoint::serverFactory("tcp://127.0.0.1");
  BOOST_CHECK_EQUAL(false, e->isConnected());
  DELETE_ENDPOINT(e);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test isconnected
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (EndpointIsConnectedServer2) {
  Endpoint* e;

  e = Endpoint::serverFactory("ssl://127.0.0.1");
  BOOST_CHECK_EQUAL(false, e->isConnected());
  DELETE_ENDPOINT(e);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test isconnected
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (EndpointIsConnectedServer3) {
  Endpoint* e;

  e = Endpoint::serverFactory("unix:///tmp/socket");
  BOOST_CHECK_EQUAL(false, e->isConnected());
  DELETE_ENDPOINT(e);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test isconnected
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (EndpointIsConnectedClient1) {
  Endpoint* e;

  e = Endpoint::clientFactory("tcp://127.0.0.1");
  BOOST_CHECK_EQUAL(false, e->isConnected());
  DELETE_ENDPOINT(e);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test isconnected
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (EndpointIsConnectedClient2) {
  Endpoint* e;

  e = Endpoint::clientFactory("ssl://127.0.0.1");
  BOOST_CHECK_EQUAL(false, e->isConnected());
  DELETE_ENDPOINT(e);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test isconnected
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (EndpointIsConnectedClient3) {
  Endpoint* e;

  e = Endpoint::clientFactory("unix:///tmp/socket");
  BOOST_CHECK_EQUAL(false, e->isConnected());
  DELETE_ENDPOINT(e);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test server endpoint
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (EndpointServerTcpIpv4WithPort) {
  Endpoint* e;

  e = Endpoint::serverFactory("tcp://127.0.0.1:667");
  BOOST_CHECK_EQUAL("tcp://127.0.0.1:667", e->getSpecification());
  BOOST_CHECK_EQUAL(Endpoint::ENDPOINT_SERVER, e->getType());
  BOOST_CHECK_EQUAL(Endpoint::DOMAIN_IPV4, e->getDomainType());
  BOOST_CHECK_EQUAL(Endpoint::ENCRYPTION_NONE, e->getEncryption());
  BOOST_CHECK_EQUAL(AF_INET, e->getDomain());
  BOOST_CHECK_EQUAL("127.0.0.1", e->getHost());
  BOOST_CHECK_EQUAL(667, e->getPort());
  BOOST_CHECK_EQUAL("127.0.0.1:667", e->getHostString());
  BOOST_CHECK_EQUAL(false, e->isConnected());
  DELETE_ENDPOINT(e);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test server endpoint
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (EndpointServerUnix) {
  Endpoint* e;

  e = Endpoint::serverFactory("unix:///path/to/arango.sock");
  BOOST_CHECK_EQUAL("unix:///path/to/arango.sock", e->getSpecification());
  BOOST_CHECK_EQUAL(Endpoint::ENDPOINT_SERVER, e->getType());
  BOOST_CHECK_EQUAL(Endpoint::DOMAIN_UNIX, e->getDomainType());
  BOOST_CHECK_EQUAL(Endpoint::ENCRYPTION_NONE, e->getEncryption());
  BOOST_CHECK_EQUAL(AF_UNIX, e->getDomain());
  BOOST_CHECK_EQUAL("localhost", e->getHost());
  BOOST_CHECK_EQUAL(0, e->getPort());
  BOOST_CHECK_EQUAL("localhost", e->getHostString());
  BOOST_CHECK_EQUAL(false, e->isConnected());
  DELETE_ENDPOINT(e);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test client endpoint
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (EndpointClientSslIpV6WithPortHttp) {
  Endpoint* e;

  e = Endpoint::clientFactory("http@SSL://[0001:0002:0003:0004:0005:0006:0007:0008]:43425");
  BOOST_CHECK_EQUAL("http@SSL://[0001:0002:0003:0004:0005:0006:0007:0008]:43425", e->getSpecification());
  BOOST_CHECK_EQUAL(Endpoint::ENDPOINT_CLIENT, e->getType());
  BOOST_CHECK_EQUAL(Endpoint::DOMAIN_IPV6, e->getDomainType());
  BOOST_CHECK_EQUAL(Endpoint::ENCRYPTION_SSL, e->getEncryption());
  BOOST_CHECK_EQUAL(AF_INET6, e->getDomain());
  BOOST_CHECK_EQUAL("0001:0002:0003:0004:0005:0006:0007:0008", e->getHost());
  BOOST_CHECK_EQUAL(43425, e->getPort());
  BOOST_CHECK_EQUAL("[0001:0002:0003:0004:0005:0006:0007:0008]:43425", e->getHostString());
  BOOST_CHECK_EQUAL(false, e->isConnected());
  DELETE_ENDPOINT(e);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test client endpoint
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (EndpointClientTcpIpv6WithoutPort) {
  Endpoint* e;

  e = Endpoint::clientFactory("tcp://[::]");
  BOOST_CHECK_EQUAL("tcp://[::]", e->getSpecification());
  BOOST_CHECK_EQUAL(Endpoint::ENDPOINT_CLIENT, e->getType());
  BOOST_CHECK_EQUAL(Endpoint::DOMAIN_IPV6, e->getDomainType());
  BOOST_CHECK_EQUAL(Endpoint::ENCRYPTION_NONE, e->getEncryption());
  BOOST_CHECK_EQUAL(AF_INET6, e->getDomain());
  BOOST_CHECK_EQUAL("::", e->getHost());
  BOOST_CHECK_EQUAL(8529, e->getPort());
  BOOST_CHECK_EQUAL("[::]:8529", e->getHostString());
  BOOST_CHECK_EQUAL(false, e->isConnected());
  DELETE_ENDPOINT(e);
}

BOOST_AUTO_TEST_SUITE_END()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
