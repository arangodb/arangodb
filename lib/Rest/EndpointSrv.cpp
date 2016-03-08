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

#ifndef _WIN32

#define BIND_4_COMPAT 1  // LINUX
#define BIND_8_COMPAT 1  // MACOSX

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <resolv.h>

#include "Basics/logging.h"
#include "Rest/EndpointIp.h"

using namespace triagens::basics;
using namespace triagens::rest;

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

std::vector<SrvRecord> srvRecords(std::string specification) {
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
        LOG_WARNING("DNS record for '%s' is corrupt", specification.c_str());
        return {};
      }

      cp += n + QFIXEDSZ;
    }

    // loop through the answer buffer and extract SRV records
    while (0 < ancount-- && cp < eom) {
      n = dn_expand(msg, eom, cp, (char*)hostbuf, 256);

      if (n < 0) {
        LOG_WARNING("DNS record for '%s' is corrupt", specification.c_str());
        return {};
      }

      cp += n;

      int type = _getshort(cp);
      cp += 2;

      int nclass = _getshort(cp);
      cp += 2;

      int ttl = _getlong(cp);
      cp += 4;

      int dlen = _getshort(cp);
      cp += 2;

      int priority = _getshort(cp);
      cp += 2;

      int weight = _getshort(cp);
      cp += 2;

      int port = _getshort(cp);
      cp += 2;

      n = dn_expand(msg, eom, cp, (char*)hostbuf, 256);

      if (n < 0) {
        LOG_WARNING("DNS record for '%s' is corrupt", specification.c_str());
        break;
      }

      cp += n;

      LOG_TRACE("DNS record for '%s': type %d, class %d, ttl %d, len %d, prio %d, "
                " weight %d, port %d, host '%s'", specification.c_str(),
                type, nclass, ttl, dlen, priority, weight, port, hostbuf);

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
    LOG_WARNING("DNS record for '%s' not found", specification.c_str());
  }

  std::sort(services.begin(), services.end(),
            [](SrvRecord const& lhs, SrvRecord const& rhs) {
              if (lhs.priority != rhs.priority) {
                return lhs.priority < rhs.priority;
              }

              return lhs.weight > rhs.weight;
            });

  return services;
}

#else

std::vector<SrvRecord> srvRecords(std::string specification) {
  return {};
}

#endif

EndpointSrv::EndpointSrv(std::string const& specification)
    : Endpoint(ENDPOINT_CLIENT, DOMAIN_SRV, ENCRYPTION_NONE, specification, 0) {
  LOG_ERROR("%s", specification.c_str());
}

EndpointSrv::~EndpointSrv() {}

bool EndpointSrv::isConnected() const {
  if (_endpoint != nullptr) {
    return _endpoint->isConnected();
  }
  
  return false;
}

TRI_socket_t EndpointSrv::connect(double connectTimeout,
                                  double requestTimeout) {
  LOG_ERROR("connecting to ip endpoint '%s'", _specification.c_str());

  auto services = srvRecords(_specification);

  TRI_socket_t res;

  for (auto service : services) {
    std::string spec =
        "tcp://" + service.name + ":" + StringUtils::itoa(service.port);

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

int EndpointSrv::getDomain() const {
  if (_endpoint != nullptr) {
    return _endpoint->getDomain();
  }

  return -1;
}

int EndpointSrv::getPort() const {
  if (_endpoint != nullptr) {
    return _endpoint->getPort();
  }

  return -1;
}

std::string EndpointSrv::getHost() const {
  if (_endpoint != nullptr) {
    return _endpoint->getHost();
  }

  return "";
}

std::string EndpointSrv::getHostString() const {
  if (_endpoint != nullptr) {
    return _endpoint->getHostString();
  }

  return "";
}
