////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Mocks/LogLevels.h"

#include "Basics/Common.h"
#include "Endpoint/Endpoint.h"
#include "Endpoint/EndpointIp.h"
#include "Endpoint/EndpointIpV4.h"
#include "Endpoint/EndpointIpV6.h"
#include "Endpoint/EndpointUnixDomain.h"
#include "Logger/LogTopic.h"
#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                                            macros
// -----------------------------------------------------------------------------

#define FACTORY_NAME(name) name ## Factory  

#define FACTORY(name, specification) arangodb::Endpoint::FACTORY_NAME(name)(specification)

#define CHECK_ENDPOINT_FEATURE(type, specification, feature, expected) \
  e = FACTORY(type, specification); \
  EXPECT_EQ((expected), (e->feature())); \
  delete e;

#define CHECK_ENDPOINT_SERVER_FEATURE(type, specification, feature, expected) \
  e = arangodb::Endpoint::serverFactory(specification, 1, true); \
  EXPECT_EQ((expected), (e->feature())); \
  delete e;

TEST(EndpointTest, EndpointInvalid) {
  tests::LogSuppressor<Logger::FIXME, LogLevel::FATAL> suppressor;

  Endpoint* e = nullptr;

  EXPECT_TRUE(e == arangodb::Endpoint::clientFactory(""));
  EXPECT_TRUE(e == arangodb::Endpoint::clientFactory("@"));

  EXPECT_TRUE(e == arangodb::Endpoint::clientFactory("http://"));
  EXPECT_TRUE(e == arangodb::Endpoint::clientFactory("ssl://"));
  EXPECT_TRUE(e == arangodb::Endpoint::clientFactory("unix://"));

  EXPECT_TRUE(e == arangodb::Endpoint::clientFactory("fish://127.0.0.1:8529"));
  EXPECT_TRUE(e == arangodb::Endpoint::clientFactory("http://127.0.0.1:8529"));
  EXPECT_TRUE(e == arangodb::Endpoint::clientFactory("https://127.0.0.1:8529"));
  
  EXPECT_TRUE(e == arangodb::Endpoint::clientFactory("tcp//127.0.0.1:8529"));
  EXPECT_TRUE(e == arangodb::Endpoint::clientFactory("tcp:127.0.0.1:8529"));
  EXPECT_TRUE(e == arangodb::Endpoint::clientFactory("ssl:localhost"));
  EXPECT_TRUE(e == arangodb::Endpoint::clientFactory("ssl//:localhost"));
  EXPECT_TRUE(e == arangodb::Endpoint::clientFactory("unix///tmp/socket"));
  EXPECT_TRUE(e == arangodb::Endpoint::clientFactory("unix:tmp/socket"));
  
  EXPECT_TRUE(e == arangodb::Endpoint::clientFactory("fish@tcp://127.0.0.1:8529"));
  EXPECT_TRUE(e == arangodb::Endpoint::clientFactory("ssl@tcp://127.0.0.1:8529"));
  EXPECT_TRUE(e == arangodb::Endpoint::clientFactory("https@tcp://127.0.0.1:8529"));
  EXPECT_TRUE(e == arangodb::Endpoint::clientFactory("https@tcp://127.0.0.1:"));
  
  EXPECT_TRUE(e == arangodb::Endpoint::clientFactory("tcp://127.0.0.1:65536"));
  EXPECT_TRUE(e == arangodb::Endpoint::clientFactory("tcp://127.0.0.1:65537"));
  EXPECT_TRUE(e == arangodb::Endpoint::clientFactory("tcp://127.0.0.1:-1"));
  EXPECT_TRUE(e == arangodb::Endpoint::clientFactory("tcp://127.0.0.1:6555555555"));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test specification
////////////////////////////////////////////////////////////////////////////////

TEST(EndpointTest, EndpointSpecification) {
  Endpoint* e;

  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1", specification, "http+tcp://127.0.0.1:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost", specification, "http+tcp://127.0.0.1:8529");
  CHECK_ENDPOINT_FEATURE(client, "SSL://127.0.0.5", specification, "http+ssl://127.0.0.5:8529");
  CHECK_ENDPOINT_FEATURE(client, "httP@ssl://localhost:4635", specification, "http+ssl://127.0.0.1:4635");

#ifndef _WIN32
  CHECK_ENDPOINT_SERVER_FEATURE(server, "unix:///path/to/socket", specification, "http+unix:///path/to/socket");
  CHECK_ENDPOINT_SERVER_FEATURE(server, "htTp@UNIx:///a/b/c/d/e/f.s", specification, "http+unix:///a/b/c/d/e/f.s");
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test types
////////////////////////////////////////////////////////////////////////////////

TEST(EndpointTest, EndpointTypes) {
  Endpoint* e;

  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1", type, arangodb::Endpoint::EndpointType::CLIENT);
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost", type, arangodb::Endpoint::EndpointType::CLIENT);
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1", type, arangodb::Endpoint::EndpointType::CLIENT);
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost", type, arangodb::Endpoint::EndpointType::CLIENT);
#ifndef _WIN32
  CHECK_ENDPOINT_FEATURE(client, "unix:///path/to/socket", type, arangodb::Endpoint::EndpointType::CLIENT);
#endif

  CHECK_ENDPOINT_SERVER_FEATURE(server, "tcp://127.0.0.1", type, arangodb::Endpoint::EndpointType::SERVER);
  CHECK_ENDPOINT_SERVER_FEATURE(server, "tcp://localhost", type, arangodb::Endpoint::EndpointType::SERVER);
  CHECK_ENDPOINT_SERVER_FEATURE(server, "ssl://127.0.0.1", type, arangodb::Endpoint::EndpointType::SERVER);
  CHECK_ENDPOINT_SERVER_FEATURE(server, "ssl://localhost", type, arangodb::Endpoint::EndpointType::SERVER);
#ifndef _WIN32
  CHECK_ENDPOINT_SERVER_FEATURE(server, "unix:///path/to/socket", type, arangodb::Endpoint::EndpointType::SERVER);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test domains
////////////////////////////////////////////////////////////////////////////////

TEST(EndpointTest, EndpointDomains) {
  Endpoint* e;

  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1", domain, AF_INET);
  CHECK_ENDPOINT_FEATURE(client, "tcp://192.168.173.13", domain, AF_INET);
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost", domain, AF_INET);
  CHECK_ENDPOINT_FEATURE(client, "tcp://www.arangodb.org", domain, AF_INET);
  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1:8529", domain, AF_INET);
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost:8529", domain, AF_INET);
  CHECK_ENDPOINT_FEATURE(client, "tcp://www.arangodb.org:8529", domain, AF_INET);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]", domain, AF_INET6);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]", domain, AF_INET6);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]:8529", domain, AF_INET6);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]:8529", domain, AF_INET6);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[2001:0db8:0000:0000:0000:ff00:0042:8329]:8529", domain, AF_INET6);
  CHECK_ENDPOINT_FEATURE(client, "http@tcp://[2001:0db8:0000:0000:0000:ff00:0042:8329]:8529", domain, AF_INET6);
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1", domain, AF_INET);
  CHECK_ENDPOINT_FEATURE(client, "ssl://192.168.173.13", domain, AF_INET);
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost", domain, AF_INET);
  CHECK_ENDPOINT_FEATURE(client, "ssl://www.arangodb.org", domain, AF_INET);
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1:8529", domain, AF_INET);
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost:8529", domain, AF_INET);
  CHECK_ENDPOINT_FEATURE(client, "ssl://www.arangodb.org:8529", domain, AF_INET);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]", domain, AF_INET6);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]", domain, AF_INET6);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]:8529", domain, AF_INET6);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]:8529", domain, AF_INET6);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[2001:0db8:0000:0000:0000:ff00:0042:8329]:8529", domain, AF_INET6);
  CHECK_ENDPOINT_FEATURE(client, "http@ssl://[2001:0db8:0000:0000:0000:ff00:0042:8329]:8529", domain, AF_INET6);
  
#ifndef _WIN32
  CHECK_ENDPOINT_FEATURE(client, "unix:///tmp/socket", domain, AF_UNIX);
  CHECK_ENDPOINT_FEATURE(client, "unix:///tmp/socket/arango.sock", domain, AF_UNIX);
  CHECK_ENDPOINT_FEATURE(client, "http@unix:///tmp/socket/arango.sock", domain, AF_UNIX);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test domain types
////////////////////////////////////////////////////////////////////////////////

TEST(EndpointTest, EndpointDomainTypes) {
  Endpoint* e;

  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1", domainType, arangodb::Endpoint::DomainType::IPV4);
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost", domainType, arangodb::Endpoint::DomainType::IPV4);
  CHECK_ENDPOINT_FEATURE(client, "tcp://www.arangodb.org", domainType, arangodb::Endpoint::DomainType::IPV4);
  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1:8529", domainType, arangodb::Endpoint::DomainType::IPV4);
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost:8529", domainType, arangodb::Endpoint::DomainType::IPV4);
  CHECK_ENDPOINT_FEATURE(client, "tcp://www.arangodb.org:8529", domainType, arangodb::Endpoint::DomainType::IPV4);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]", domainType, arangodb::Endpoint::DomainType::IPV6);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]", domainType, arangodb::Endpoint::DomainType::IPV6);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]:8529", domainType, arangodb::Endpoint::DomainType::IPV6);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]:8529", domainType, arangodb::Endpoint::DomainType::IPV6);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[2001:0db8:0000:0000:0000:ff00:0042:8329]:8529", domainType, arangodb::Endpoint::DomainType::IPV6);
  CHECK_ENDPOINT_FEATURE(client, "TCP://127.0.0.1", domainType, arangodb::Endpoint::DomainType::IPV4);
  CHECK_ENDPOINT_FEATURE(client, "Tcp://127.0.0.1", domainType, arangodb::Endpoint::DomainType::IPV4);
  CHECK_ENDPOINT_FEATURE(client, "tCP://127.0.0.1", domainType, arangodb::Endpoint::DomainType::IPV4);
  CHECK_ENDPOINT_FEATURE(client, "http@tcp://127.0.0.1", domainType, arangodb::Endpoint::DomainType::IPV4);
  
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1", domainType, arangodb::Endpoint::DomainType::IPV4);
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost", domainType, arangodb::Endpoint::DomainType::IPV4);
  CHECK_ENDPOINT_FEATURE(client, "ssl://www.arangodb.org", domainType, arangodb::Endpoint::DomainType::IPV4);
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1:8529", domainType, arangodb::Endpoint::DomainType::IPV4);
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost:8529", domainType, arangodb::Endpoint::DomainType::IPV4);
  CHECK_ENDPOINT_FEATURE(client, "ssl://www.arangodb.org:8529", domainType, arangodb::Endpoint::DomainType::IPV4);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]", domainType, arangodb::Endpoint::DomainType::IPV6);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]", domainType, arangodb::Endpoint::DomainType::IPV6);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]:8529", domainType, arangodb::Endpoint::DomainType::IPV6);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]:8529", domainType, arangodb::Endpoint::DomainType::IPV6);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[2001:0db8:0000:0000:0000:ff00:0042:8329]:8529", domainType, arangodb::Endpoint::DomainType::IPV6);
  CHECK_ENDPOINT_FEATURE(client, "SSL://127.0.0.1", domainType, arangodb::Endpoint::DomainType::IPV4);
  CHECK_ENDPOINT_FEATURE(client, "Ssl://127.0.0.1", domainType, arangodb::Endpoint::DomainType::IPV4);
  CHECK_ENDPOINT_FEATURE(client, "sSL://127.0.0.1", domainType, arangodb::Endpoint::DomainType::IPV4);
  CHECK_ENDPOINT_FEATURE(client, "http@ssl://127.0.0.1", domainType, arangodb::Endpoint::DomainType::IPV4);
  
#ifndef _WIN32
  CHECK_ENDPOINT_FEATURE(client, "unix:///tmp/socket", domainType, arangodb::Endpoint::DomainType::UNIX);
  CHECK_ENDPOINT_FEATURE(client, "unix:///tmp/socket/arango.sock", domainType, arangodb::Endpoint::DomainType::UNIX);
  CHECK_ENDPOINT_FEATURE(client, "UNIX:///tmp/socket", domainType, arangodb::Endpoint::DomainType::UNIX);
  CHECK_ENDPOINT_FEATURE(client, "Unix:///tmp/socket", domainType, arangodb::Endpoint::DomainType::UNIX);
  CHECK_ENDPOINT_FEATURE(client, "uNIX:///tmp/socket", domainType, arangodb::Endpoint::DomainType::UNIX);
  CHECK_ENDPOINT_FEATURE(client, "http@unix:///tmp/socket", domainType, arangodb::Endpoint::DomainType::UNIX);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test ports
////////////////////////////////////////////////////////////////////////////////

TEST(EndpointTest, EndpointPorts) {
  Endpoint* e;

  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1", port, EndpointIp::_defaultPortHttp);
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost", port, EndpointIp::_defaultPortHttp);
  CHECK_ENDPOINT_FEATURE(client, "tcp://www.arangodb.org", port, EndpointIp::_defaultPortHttp); 
  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1:8529", port, 8529);
  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1:8532", port, 8532);
  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1:80", port, 80);
  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1:443", port, 443);
  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1:65535", port, 65535);
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost:8529", port, 8529);
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost:8532", port, 8532);
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost:80", port, 80);
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost:443", port, 443);
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost:65535", port, 65535);
  CHECK_ENDPOINT_FEATURE(client, "tcp://www.arangodb.org:8529", port, 8529);
  CHECK_ENDPOINT_FEATURE(client, "http@tcp://www.arangodb.org:8529", port, 8529);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]", port, EndpointIp::_defaultPortHttp);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]", port, EndpointIp::_defaultPortHttp);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]:8529", port, 8529);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]:8532", port, 8532);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]:80", port, 80);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]:443", port, 443);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]:65535", port, 65535);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]:8529", port, 8529);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]:8532", port, 8532);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]:80", port, 80);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]:443", port, 443);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]:65535", port, 65535);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[2001:0db8:0000:0000:0000:ff00:0042:8329]:666", port, 666);
  CHECK_ENDPOINT_FEATURE(client, "http@tcp://[2001:0db8:0000:0000:0000:ff00:0042:8329]:666", port, 666);
  
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1", port, EndpointIp::_defaultPortHttp);
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost", port, EndpointIp::_defaultPortHttp);
  CHECK_ENDPOINT_FEATURE(client, "ssl://www.arangodb.org", port, EndpointIp::_defaultPortHttp);
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1:8529", port, 8529);
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1:8532", port, 8532);
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1:80", port, 80);
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1:443", port, 443);
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1:65535", port, 65535);
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost:8529", port, 8529);
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost:8532", port, 8532);
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost:80", port, 80);
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost:443", port, 443);
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost:65535", port, 65535);
  CHECK_ENDPOINT_FEATURE(client, "ssl://www.arangodb.org:8529", port, 8529);
  CHECK_ENDPOINT_FEATURE(client, "http@ssl://www.arangodb.org:8529", port, 8529);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]", port, EndpointIp::_defaultPortHttp);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]", port, EndpointIp::_defaultPortHttp);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]:8529", port, 8529);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]:8532", port, 8532);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]:80", port, 80);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]:443", port, 443);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]:65535", port, 65535);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]:8529", port, 8529);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]:8532", port, 8532);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]:80", port, 80);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]:443", port, 443);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]:65535", port, 65535);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[2001:0db8:0000:0000:0000:ff00:0042:8329]:666", port, 666);
  CHECK_ENDPOINT_FEATURE(client, "http@ssl://[2001:0db8:0000:0000:0000:ff00:0042:8329]:666", port, 666);
  
#ifndef _WIN32
  CHECK_ENDPOINT_FEATURE(client, "unix:///tmp/socket", port, 0);
  CHECK_ENDPOINT_FEATURE(client, "unix:///tmp/socket/arango.sock", port, 0);
  CHECK_ENDPOINT_FEATURE(client, "http@unix:///tmp/socket/arango.sock", port, 0);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test encryption
////////////////////////////////////////////////////////////////////////////////

TEST(EndpointTest, EndpointEncryption) {
  Endpoint* e;

  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1", encryption, arangodb::Endpoint::EncryptionType::NONE);
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost", encryption, arangodb::Endpoint::EncryptionType::NONE);
  CHECK_ENDPOINT_FEATURE(client, "tcp://www.arangodb.org", encryption, arangodb::Endpoint::EncryptionType::NONE);
  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1:8529", encryption, arangodb::Endpoint::EncryptionType::NONE);
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost:8529", encryption, arangodb::Endpoint::EncryptionType::NONE);
  CHECK_ENDPOINT_FEATURE(client, "tcp://www.arangodb.org:8529", encryption, arangodb::Endpoint::EncryptionType::NONE);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]", encryption, arangodb::Endpoint::EncryptionType::NONE);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]", encryption, arangodb::Endpoint::EncryptionType::NONE);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]:8529", encryption, arangodb::Endpoint::EncryptionType::NONE);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]:8529", encryption, arangodb::Endpoint::EncryptionType::NONE);
  CHECK_ENDPOINT_FEATURE(client, "tcp://[2001:0db8:0000:0000:0000:ff00:0042:8329]:666", encryption, arangodb::Endpoint::EncryptionType::NONE);
  CHECK_ENDPOINT_FEATURE(client, "http@tcp://[2001:0db8:0000:0000:0000:ff00:0042:8329]:666", encryption, arangodb::Endpoint::EncryptionType::NONE);
  
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1", encryption, arangodb::Endpoint::EncryptionType::SSL);
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost", encryption, arangodb::Endpoint::EncryptionType::SSL);
  CHECK_ENDPOINT_FEATURE(client, "ssl://www.arangodb.org", encryption, arangodb::Endpoint::EncryptionType::SSL);
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1:8529", encryption, arangodb::Endpoint::EncryptionType::SSL);
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost:8529", encryption, arangodb::Endpoint::EncryptionType::SSL);
  CHECK_ENDPOINT_FEATURE(client, "ssl://www.arangodb.org:8529", encryption, arangodb::Endpoint::EncryptionType::SSL);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]", encryption, arangodb::Endpoint::EncryptionType::SSL);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]", encryption, arangodb::Endpoint::EncryptionType::SSL);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]:8529", encryption, arangodb::Endpoint::EncryptionType::SSL);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[2001:0db8:0000:0000:0000:ff00:0042:8329]:666", encryption, arangodb::Endpoint::EncryptionType::SSL);
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]:8529", encryption, arangodb::Endpoint::EncryptionType::SSL);
  CHECK_ENDPOINT_FEATURE(client, "SSL://[::]:8529", encryption, arangodb::Endpoint::EncryptionType::SSL);
  CHECK_ENDPOINT_FEATURE(client, "Ssl://[::]:8529", encryption, arangodb::Endpoint::EncryptionType::SSL);
  CHECK_ENDPOINT_FEATURE(client, "sSL://[::]:8529", encryption, arangodb::Endpoint::EncryptionType::SSL);
  CHECK_ENDPOINT_FEATURE(client, "http@ssl://[::]:8529", encryption, arangodb::Endpoint::EncryptionType::SSL);
  
#ifndef _WIN32
  CHECK_ENDPOINT_FEATURE(client, "unix:///tmp/socket", encryption, arangodb::Endpoint::EncryptionType::NONE);
  CHECK_ENDPOINT_FEATURE(client, "unix:///tmp/socket/arango.sock", encryption, arangodb::Endpoint::EncryptionType::NONE);
  CHECK_ENDPOINT_FEATURE(client, "UNIX:///tmp/socket/arango.sock", encryption, arangodb::Endpoint::EncryptionType::NONE);
  CHECK_ENDPOINT_FEATURE(client, "Unix:///tmp/socket/arango.sock", encryption, arangodb::Endpoint::EncryptionType::NONE);
  CHECK_ENDPOINT_FEATURE(client, "uNIX:///tmp/socket/arango.sock", encryption, arangodb::Endpoint::EncryptionType::NONE);
  CHECK_ENDPOINT_FEATURE(client, "http@unix:///tmp/socket/arango.sock", encryption, arangodb::Endpoint::EncryptionType::NONE);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test host
////////////////////////////////////////////////////////////////////////////////

TEST(EndpointTest, EndpointHost) {
  Endpoint* e;

  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1", host, "127.0.0.1");
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost", host, "127.0.0.1");
  CHECK_ENDPOINT_FEATURE(client, "tcp://www.arangodb.org", host, "www.arangodb.org");
  CHECK_ENDPOINT_FEATURE(client, "tcp://arangodb.org", host, "arangodb.org");
  CHECK_ENDPOINT_FEATURE(client, "tcp://DE.triagens.ArangoDB.org", host, "de.triagens.arangodb.org");
  CHECK_ENDPOINT_FEATURE(client, "tcp://192.168.173.13:8529", host, "192.168.173.13");
  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1:8529", host, "127.0.0.1");
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost:8529", host, "127.0.0.1");
  CHECK_ENDPOINT_FEATURE(client, "tcp://www.arangodb.org:8529", host, "www.arangodb.org");
  CHECK_ENDPOINT_FEATURE(client, "tcp://arangodb.org:8529", host, "arangodb.org");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]", host, "127.0.0.1");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]", host, "::");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]:8529", host, "127.0.0.1");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]:8529", host, "::");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[2001:0db8:0000:0000:0000:ff00:0042:8329]", host, "2001:0db8:0000:0000:0000:ff00:0042:8329");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[2001:0db8:0000:0000:0000:ff00:0042:8329]:8529", host, "2001:0db8:0000:0000:0000:ff00:0042:8329");
  CHECK_ENDPOINT_FEATURE(client, "http@tcp://[::]:8529", host, "::");
  
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1", host, "127.0.0.1");
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost", host, "127.0.0.1");
  CHECK_ENDPOINT_FEATURE(client, "ssl://www.arangodb.org", host, "www.arangodb.org");
  CHECK_ENDPOINT_FEATURE(client, "ssl://arangodb.org", host, "arangodb.org");
  CHECK_ENDPOINT_FEATURE(client, "ssl://DE.triagens.ArangoDB.org", host, "de.triagens.arangodb.org");
  CHECK_ENDPOINT_FEATURE(client, "ssl://192.168.173.13:8529", host, "192.168.173.13");
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost:8529", host, "127.0.0.1");
  CHECK_ENDPOINT_FEATURE(client, "ssl://www.arangodb.org:8529", host, "www.arangodb.org");
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]", host, "127.0.0.1");
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]", host, "::");
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]:8529", host, "127.0.0.1");
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]:8529", host, "::");
  CHECK_ENDPOINT_FEATURE(client, "ssl://[2001:0db8:0000:0000:0000:ff00:0042:8329]", host, "2001:0db8:0000:0000:0000:ff00:0042:8329");
  CHECK_ENDPOINT_FEATURE(client, "ssl://[2001:0db8:0000:0000:0000:ff00:0042:8329]:8529", host, "2001:0db8:0000:0000:0000:ff00:0042:8329");
  CHECK_ENDPOINT_FEATURE(client, "http@ssl://[::]:8529", host, "::");
  
#ifndef _WIN32
  CHECK_ENDPOINT_FEATURE(client, "unix:///tmp/socket", host, "localhost");
  CHECK_ENDPOINT_FEATURE(client, "unix:///tmp/socket/arango.sock", host, "localhost");
  CHECK_ENDPOINT_FEATURE(client, "http@unix:///tmp/socket/arango.sock", host, "localhost");
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test hoststring
////////////////////////////////////////////////////////////////////////////////

TEST(EndpointTest, EndpointHostString) {
  Endpoint* e;

  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1", hostAndPort, "127.0.0.1:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost", hostAndPort, "127.0.0.1:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://www.arangodb.org", hostAndPort, "www.arangodb.org:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://arangodb.org", hostAndPort, "arangodb.org:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://DE.triagens.ArangoDB.org", hostAndPort, "de.triagens.arangodb.org:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://192.168.173.13:8529", hostAndPort, "192.168.173.13:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://192.168.173.13:678", hostAndPort, "192.168.173.13:678");
  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1:8529", hostAndPort, "127.0.0.1:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://127.0.0.1:44", hostAndPort, "127.0.0.1:44");
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost:8529", hostAndPort, "127.0.0.1:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://localhost:65535", hostAndPort, "127.0.0.1:65535");
  CHECK_ENDPOINT_FEATURE(client, "tcp://www.arangodb.org:8529", hostAndPort, "www.arangodb.org:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://arangodb.org:8529", hostAndPort, "arangodb.org:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]", hostAndPort, "[127.0.0.1]:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]", hostAndPort, "[::]:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]:8529", hostAndPort, "[127.0.0.1]:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]:80", hostAndPort, "[127.0.0.1]:80");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]:555", hostAndPort, "[127.0.0.1]:555");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[127.0.0.1]:65535", hostAndPort, "[127.0.0.1]:65535");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]:8529", hostAndPort, "[::]:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]:80", hostAndPort, "[::]:80");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[::]:8080", hostAndPort, "[::]:8080");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[2001:0db8:0000:0000:0000:ff00:0042:8329]", hostAndPort, "[2001:0db8:0000:0000:0000:ff00:0042:8329]:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[2001:0db8:0000:0000:0000:ff00:0042:8329]:8529", hostAndPort, "[2001:0db8:0000:0000:0000:ff00:0042:8329]:8529");
  CHECK_ENDPOINT_FEATURE(client, "tcp://[2001:0db8:0000:0000:0000:ff00:0042:8329]:777", hostAndPort, "[2001:0db8:0000:0000:0000:ff00:0042:8329]:777");
  CHECK_ENDPOINT_FEATURE(client, "http@tcp://[2001:0db8:0000:0000:0000:ff00:0042:8329]:777", hostAndPort, "[2001:0db8:0000:0000:0000:ff00:0042:8329]:777");
  
  CHECK_ENDPOINT_FEATURE(client, "ssl://127.0.0.1", hostAndPort, "127.0.0.1:8529");
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost", hostAndPort, "127.0.0.1:8529");
  CHECK_ENDPOINT_FEATURE(client, "ssl://www.arangodb.org", hostAndPort, "www.arangodb.org:8529");
  CHECK_ENDPOINT_FEATURE(client, "ssl://arangodb.org", hostAndPort, "arangodb.org:8529");
  CHECK_ENDPOINT_FEATURE(client, "ssl://DE.triagens.ArangoDB.org", hostAndPort, "de.triagens.arangodb.org:8529");
  CHECK_ENDPOINT_FEATURE(client, "ssl://192.168.173.13:8529", hostAndPort, "192.168.173.13:8529");
  CHECK_ENDPOINT_FEATURE(client, "ssl://192.168.173.13:1234", hostAndPort, "192.168.173.13:1234");
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost:8529", hostAndPort, "127.0.0.1:8529");
  CHECK_ENDPOINT_FEATURE(client, "ssl://localhost:5", hostAndPort, "127.0.0.1:5");
  CHECK_ENDPOINT_FEATURE(client, "ssl://www.arangodb.org:8529", hostAndPort, "www.arangodb.org:8529");
  CHECK_ENDPOINT_FEATURE(client, "ssl://www.arangodb.org:12345", hostAndPort, "www.arangodb.org:12345");
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]", hostAndPort, "[127.0.0.1]:8529");
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]", hostAndPort, "[::]:8529");
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]:8529", hostAndPort, "[127.0.0.1]:8529");
  CHECK_ENDPOINT_FEATURE(client, "ssl://[127.0.0.1]:32768", hostAndPort, "[127.0.0.1]:32768");
  CHECK_ENDPOINT_FEATURE(client, "ssl://[::]:8529", hostAndPort, "[::]:8529");
  CHECK_ENDPOINT_FEATURE(client, "ssl://[2001:0db8:0000:0000:0000:ff00:0042:8329]", hostAndPort, "[2001:0db8:0000:0000:0000:ff00:0042:8329]:8529");
  CHECK_ENDPOINT_FEATURE(client, "ssl://[2001:0db8:0000:0000:0000:ff00:0042:8329]:8529", hostAndPort, "[2001:0db8:0000:0000:0000:ff00:0042:8329]:8529");
  CHECK_ENDPOINT_FEATURE(client, "ssl://[2001:0db8:0000:0000:0000:ff00:0042:8329]:994", hostAndPort, "[2001:0db8:0000:0000:0000:ff00:0042:8329]:994");
  CHECK_ENDPOINT_FEATURE(client, "http@ssl://[2001:0db8:0000:0000:0000:ff00:0042:8329]:994", hostAndPort, "[2001:0db8:0000:0000:0000:ff00:0042:8329]:994");
  
#ifndef _WIN32
  CHECK_ENDPOINT_FEATURE(client, "unix:///tmp/socket", hostAndPort, "localhost");
  CHECK_ENDPOINT_FEATURE(client, "unix:///tmp/socket/arango.sock", hostAndPort, "localhost");
  CHECK_ENDPOINT_FEATURE(client, "http@unix:///tmp/socket/arango.sock", hostAndPort, "localhost");
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test isconnected
////////////////////////////////////////////////////////////////////////////////

TEST(EndpointTest, EndpointIsConnectedServer1) {
  Endpoint* e;

  e = arangodb::Endpoint::serverFactory("tcp://127.0.0.1", 1, true);
  EXPECT_TRUE(false == e->isConnected());
  delete e;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test isconnected
////////////////////////////////////////////////////////////////////////////////

TEST(EndpointTest, EndpointIsConnectedServer2) {
  Endpoint* e;

  e = arangodb::Endpoint::serverFactory("ssl://127.0.0.1", 1, true);
  EXPECT_TRUE(false == e->isConnected());
  delete e;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test isconnected
////////////////////////////////////////////////////////////////////////////////

#ifndef _WIN32
TEST(EndpointTest, EndpointIsConnectedServer3) {
  Endpoint* e;

  e = arangodb::Endpoint::serverFactory("unix:///tmp/socket", 1, true);
  EXPECT_TRUE(false == e->isConnected());
  delete e;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief test isconnected
////////////////////////////////////////////////////////////////////////////////

TEST(EndpointTest, EndpointIsConnectedClient1) {
  Endpoint* e;

  e = arangodb::Endpoint::clientFactory("tcp://127.0.0.1");
  EXPECT_TRUE(false == e->isConnected());
  delete e;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test isconnected
////////////////////////////////////////////////////////////////////////////////

TEST(EndpointTest, EndpointIsConnectedClient2) {
  Endpoint* e;

  e = arangodb::Endpoint::clientFactory("ssl://127.0.0.1");
  EXPECT_TRUE(false == e->isConnected());
  delete e;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test isconnected
////////////////////////////////////////////////////////////////////////////////

#ifndef _WIN32
TEST(EndpointTest, EndpointIsConnectedClient3) {
  Endpoint* e;

  e = arangodb::Endpoint::clientFactory("unix:///tmp/socket");
  EXPECT_TRUE(false == e->isConnected());
  delete e;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief test server endpoint
////////////////////////////////////////////////////////////////////////////////

TEST(EndpointTest, EndpointServerTcpIpv4WithPort) {
  Endpoint* e;

  e = arangodb::Endpoint::serverFactory("tcp://127.0.0.1:667", 1, true);
  EXPECT_TRUE("http+tcp://127.0.0.1:667" == e->specification());
  EXPECT_TRUE(arangodb::Endpoint::EndpointType::SERVER == e->type());
  EXPECT_TRUE(arangodb::Endpoint::DomainType::IPV4 == e->domainType());
  EXPECT_TRUE(arangodb::Endpoint::EncryptionType::NONE == e->encryption());
  EXPECT_TRUE(AF_INET == e->domain());
  EXPECT_TRUE("127.0.0.1" == e->host());
  EXPECT_TRUE(667 == e->port());
  EXPECT_TRUE("127.0.0.1:667" == e->hostAndPort());
  EXPECT_TRUE(false == e->isConnected());
  delete e;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test server endpoint
////////////////////////////////////////////////////////////////////////////////

#ifndef _WIN32
TEST(EndpointTest, EndpointServerUnix) {
  Endpoint* e;

  e = arangodb::Endpoint::serverFactory("unix:///path/to/arango.sock", 1, true);
  EXPECT_TRUE("http+unix:///path/to/arango.sock" == e->specification());
  EXPECT_TRUE(arangodb::Endpoint::EndpointType::SERVER == e->type());
  EXPECT_TRUE(arangodb::Endpoint::DomainType::UNIX == e->domainType());
  EXPECT_TRUE(arangodb::Endpoint::EncryptionType::NONE == e->encryption());
  EXPECT_TRUE(AF_UNIX == e->domain());
  EXPECT_TRUE("localhost" == e->host());
  EXPECT_TRUE(0 == e->port());
  EXPECT_TRUE("localhost" == e->hostAndPort());
  EXPECT_TRUE(false == e->isConnected());
  delete e;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief test client endpoint
////////////////////////////////////////////////////////////////////////////////

TEST(EndpointTest, EndpointClientSslIpV6WithPortHttp) {
  Endpoint* e;

  e = arangodb::Endpoint::clientFactory("http+ssl://[0001:0002:0003:0004:0005:0006:0007:0008]:43425");
  EXPECT_TRUE("http+ssl://[0001:0002:0003:0004:0005:0006:0007:0008]:43425" == e->specification());
  EXPECT_TRUE(arangodb::Endpoint::EndpointType::CLIENT == e->type());
  EXPECT_TRUE(arangodb::Endpoint::DomainType::IPV6 == e->domainType());
  EXPECT_TRUE(arangodb::Endpoint::EncryptionType::SSL == e->encryption());
  EXPECT_TRUE(AF_INET6 == e->domain());
  EXPECT_TRUE("0001:0002:0003:0004:0005:0006:0007:0008" == e->host());
  EXPECT_TRUE(43425 == e->port());
  EXPECT_TRUE("[0001:0002:0003:0004:0005:0006:0007:0008]:43425" == e->hostAndPort());
  EXPECT_TRUE(false == e->isConnected());
  delete e;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test client endpoint
////////////////////////////////////////////////////////////////////////////////

TEST(EndpointTest, EndpointClientTcpIpv6WithoutPort) {
  Endpoint* e;

  e = arangodb::Endpoint::clientFactory("tcp://[::]");
  EXPECT_TRUE("http+tcp://[::]:8529" == e->specification());
  EXPECT_TRUE(arangodb::Endpoint::EndpointType::CLIENT == e->type());
  EXPECT_TRUE(arangodb::Endpoint::DomainType::IPV6 == e->domainType());
  EXPECT_TRUE(arangodb::Endpoint::EncryptionType::NONE == e->encryption());
  EXPECT_TRUE(AF_INET6 == e->domain());
  EXPECT_TRUE("::" == e->host());
  EXPECT_TRUE(8529 == e->port());
  EXPECT_TRUE("[::]:8529" == e->hostAndPort());
  EXPECT_TRUE(false == e->isConnected());
  delete e;
}
