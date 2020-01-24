////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2016, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "EndpointSrv.h"

#include <algorithm>
#include <vector>

#ifndef _WIN32

#define BIND_4_COMPAT 1  // LINUX
#define BIND_8_COMPAT 1  // MACOSX

#include <arpa/nameser.h>
#include <netinet/in.h>
#include <resolv.h>

#include "Basics/StringUtils.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

using namespace arangodb;
using namespace arangodb::basics;

#if PACKETSZ > 1024
#define MAXPACKET PACKETSZ
#else
#define MAXPACKET 1024
#endif

union QueryBuffer {
  ::HEADER header;
  unsigned char buffer[MAXPACKET];
};

struct SrvRecord {
  int priority;
  int weight;
  int port;
  std::string name;
};

static std::vector<SrvRecord> srvRecords(std::string const& specification) {
  res_init();

  char const* dname = specification.c_str();
  int nclass = ns_c_in;
  int type = ns_t_srv;

  QueryBuffer answer;
  int anslen = sizeof(answer);

  int n = res_search(dname, nclass, type, answer.buffer, anslen);

  std::vector<SrvRecord> services;

  if (n != -1) {
    HEADER* hp = &answer.header;

    int qdcount = ntohs(hp->qdcount);
    int ancount = ntohs(hp->ancount);

    unsigned char* msg = answer.buffer;
    unsigned char* eom = msg + n;
    unsigned char* cp = msg + sizeof(HEADER);

    unsigned char hostbuf[256];

    while (0 < qdcount-- && cp < eom) {
      n = dn_expand(msg, eom, cp, (char*)hostbuf, 256);

      if (n < 0) {
        LOG_TOPIC("c39cc", WARN, arangodb::Logger::FIXME)
            << "DNS record for '" << specification << "' is corrupt";
        return {};
      }

      cp += n + QFIXEDSZ;
    }

    // loop through the answer buffer and extract SRV records
    while (0 < ancount-- && cp < eom) {
      n = dn_expand(msg, eom, cp, (char*)hostbuf, 256);

      if (n < 0) {
        LOG_TOPIC("352d9", WARN, arangodb::Logger::FIXME)
            << "DNS record for '" << specification << "' is corrupt";
        return {};
      }

      cp += n;

      int type;
      GETSHORT(type, cp);

      int nclass;
      GETSHORT(nclass, cp);

      int ttl;
      GETLONG(ttl, cp);

      int dlen;
      GETSHORT(dlen, cp);

      int priority;
      GETSHORT(priority, cp);

      int weight;
      GETSHORT(weight, cp);

      int port;
      GETSHORT(port, cp);

      n = dn_expand(msg, eom, cp, (char*)hostbuf, 256);

      if (n < 0) {
        LOG_TOPIC("4c4db", WARN, arangodb::Logger::FIXME)
            << "DNS record for '" << specification << "' is corrupt";
        break;
      }

      cp += n;

      LOG_TOPIC("b1488", TRACE, arangodb::Logger::FIXME)
          << "DNS record for '" << specification << "': type " << type
          << ", class " << nclass << ", ttl " << ttl << ", len " << dlen
          << ", prio " << priority << ", weight " << weight << ", port " << port
          << ", host '" << hostbuf << "'";

      if (type != T_SRV) {
        continue;
      }

      SrvRecord srv;

      srv.weight = weight;
      srv.priority = priority;
      srv.port = port;
      srv.name = (char*)hostbuf;

      services.push_back(srv);
    }
  } else {
    LOG_TOPIC("b804a", WARN, arangodb::Logger::FIXME)
        << "DNS record for '" << specification << "' not found";
  }

  std::sort(services.begin(), services.end(), [](SrvRecord const& lhs, SrvRecord const& rhs) {
    if (lhs.priority != rhs.priority) {
      return lhs.priority < rhs.priority;
    }

    return lhs.weight > rhs.weight;
  });

  return services;
}

#else

static std::vector<SrvRecord> srvRecords(std::string const& specification) {
  return {};
}

#endif

EndpointSrv::EndpointSrv(std::string const& specification)
    : Endpoint(DomainType::SRV, EndpointType::CLIENT, TransportType::HTTP,
               EncryptionType::NONE, specification, 0) {}

EndpointSrv::~EndpointSrv() = default;

bool EndpointSrv::isConnected() const {
  if (_endpoint != nullptr) {
    return _endpoint->isConnected();
  }

  return false;
}

TRI_socket_t EndpointSrv::connect(double connectTimeout, double requestTimeout) {
  auto services = srvRecords(_specification);

  TRI_socket_t res;

  for (auto service : services) {
    std::string spec = "tcp://" + service.name + ":" + StringUtils::itoa(service.port);

    _endpoint.reset(Endpoint::clientFactory(spec));

    res = _endpoint->connect(connectTimeout, requestTimeout);

    if (_endpoint->isConnected()) {
      return res;
    }
  }

  TRI_invalidatesocket(&res);
  return res;
}

void EndpointSrv::disconnect() {
  if (_endpoint != nullptr) {
    _endpoint->disconnect();
  }
}

bool EndpointSrv::initIncoming(TRI_socket_t) { return false; }

int EndpointSrv::domain() const {
  if (_endpoint != nullptr) {
    return _endpoint->domain();
  }

  return -1;
}

int EndpointSrv::port() const {
  if (_endpoint != nullptr) {
    return _endpoint->port();
  }

  return -1;
}

std::string EndpointSrv::host() const {
  if (_endpoint != nullptr) {
    return _endpoint->host();
  }

  return "";
}

std::string EndpointSrv::hostAndPort() const {
  if (_endpoint != nullptr) {
    return _endpoint->hostAndPort();
  }

  return "";
}
