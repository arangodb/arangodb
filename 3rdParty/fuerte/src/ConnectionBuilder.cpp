////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
/// @author Ewout Prangsma
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include <fuerte/FuerteLogger.h>
#include <fuerte/connection.h>

#include <boost/algorithm/string.hpp>

#include "H1Connection.h"
#include "H2Connection.h"
#include "VstConnection.h"

namespace arangodb { namespace fuerte { inline namespace v1 {
// Create an connection and start opening it.
std::shared_ptr<Connection> ConnectionBuilder::connect(EventLoopService& loop) {
  std::shared_ptr<Connection> result;

  if (_conf._protocolType == ProtocolType::Http) {
    // throw std::logic_error("http in vst test");
    FUERTE_LOG_DEBUG << "fuerte - creating http 1.1 connection\n";
    if (_conf._socketType == SocketType::Tcp) {
      result =
          std::make_shared<http::H1Connection<SocketType::Tcp>>(loop, _conf);
    } else if (_conf._socketType == SocketType::Ssl) {
      result =
          std::make_shared<http::H1Connection<SocketType::Ssl>>(loop, _conf);
    }
#ifdef ASIO_HAS_LOCAL_SOCKETS
    else if (_conf._socketType == SocketType::Unix) {
      result =
          std::make_shared<http::H1Connection<SocketType::Unix>>(loop, _conf);
    }
#endif
  } else if (_conf._protocolType == ProtocolType::Http2) {
    FUERTE_LOG_DEBUG << "fuerte - creating http 2 connection\n";
    if (_conf._socketType == SocketType::Tcp) {
      result =
          std::make_shared<http::H2Connection<SocketType::Tcp>>(loop, _conf);
    } else if (_conf._socketType == SocketType::Ssl) {
      result =
          std::make_shared<http::H2Connection<SocketType::Ssl>>(loop, _conf);
    }
#ifdef ASIO_HAS_LOCAL_SOCKETS
    else if (_conf._socketType == SocketType::Unix) {
      result =
          std::make_shared<http::H2Connection<SocketType::Unix>>(loop, _conf);
    }
#endif
  } else if (_conf._protocolType == ProtocolType::Vst) {
    FUERTE_LOG_DEBUG << "fuerte - creating velocystream connection\n";
    if (_conf._socketType == SocketType::Tcp) {
      result =
          std::make_shared<vst::VstConnection<SocketType::Tcp>>(loop, _conf);
    } else if (_conf._socketType == SocketType::Ssl) {
      result =
          std::make_shared<vst::VstConnection<SocketType::Ssl>>(loop, _conf);
    }
#ifdef ASIO_HAS_LOCAL_SOCKETS
    else if (_conf._socketType == SocketType::Unix) {
      result =
          std::make_shared<vst::VstConnection<SocketType::Unix>>(loop, _conf);
    }
#endif
  }
  if (!result) {
    throw std::logic_error("unsupported socket or protocol type");
  }

  return result;
}

void parseSchema(std::string const& schema,
                 detail::ConnectionConfiguration& conf) {
  // non exthausive list of supported url schemas
  // "http+tcp://", "http+ssl://", "tcp://", "ssl://", "unix://", "http+unix://"
  // "vsts://", "vst://", "http://", "https://", "vst+unix://", "vst+tcp://"
  velocypack::StringRef proto(schema.data(), schema.length());
  std::string::size_type pos = schema.find('+');
  if (pos != std::string::npos && pos + 1 < proto.length()) {
    // got something like "http+tcp://"
    velocypack::StringRef socket = proto.substr(pos + 1);
    proto = proto.substr(0, pos);
    if (socket == "tcp" || socket == "srv") {
      conf._socketType = SocketType::Tcp;
    } else if (socket == "ssl") {
      conf._socketType = SocketType::Ssl;
    } else if (socket == "unix") {
      conf._socketType = SocketType::Unix;
    } else if (conf._socketType == SocketType::Undefined) {
      throw std::runtime_error(std::string("invalid socket type: ") +
                               proto.toString());
    }

    if (proto == "vst") {
      conf._protocolType = ProtocolType::Vst;
    } else if (proto == "http") {
      conf._protocolType = ProtocolType::Http;
    } else if (proto == "h2" || proto == "http2") {
      conf._protocolType = ProtocolType::Http2;
    }

  } else {  // got only protocol
    if (proto == "vst") {
      conf._socketType = SocketType::Tcp;
      conf._protocolType = ProtocolType::Vst;
    } else if (proto == "vsts") {
      conf._socketType = SocketType::Ssl;
      conf._protocolType = ProtocolType::Vst;
    } else if (proto == "http" || proto == "tcp") {
      conf._socketType = SocketType::Tcp;
      conf._protocolType = ProtocolType::Http;
    } else if (proto == "https" || proto == "ssl") {
      conf._socketType = SocketType::Ssl;
      conf._protocolType = ProtocolType::Http;
    } else if (proto == "unix") {
      conf._socketType = SocketType::Unix;
      conf._protocolType = ProtocolType::Http;
    } else if (proto == "h2" || proto == "http2") {
      conf._socketType = SocketType::Tcp;
      conf._protocolType = ProtocolType::Http2;
    } else if (proto == "h2s" || proto == "https2") {
      conf._socketType = SocketType::Ssl;
      conf._protocolType = ProtocolType::Http2;
    }
  }
  
  if (conf._socketType == SocketType::Undefined ||
             conf._protocolType == ProtocolType::Undefined) {
    throw std::runtime_error(std::string("invalid schema: ") +
                             proto.toString());
  }
}

namespace {
bool is_invalid(char c) { return c == ' ' || c == '\r' || c == '\n'; }
char lower(char c) { return (unsigned char)(c | 0x20); }
bool is_alpha(char c) { return (lower(c) >= 'a' && lower(c) <= 'z'); }
bool is_num(char c) { return ((c) >= '0' && (c) <= '9'); }
bool is_alphanum(char c) { return is_num(c) || is_alpha(c); }
bool is_host_char(char c) {
  return (is_alphanum(c) || (c) == '.' || (c) == '-');
}
}  // namespace

ConnectionBuilder& ConnectionBuilder::endpoint(std::string const& spec) {
  if (spec.empty()) {
    throw std::runtime_error("invalid empty endpoint spec");
  }

  // we need to handle unix:// urls seperately
  std::string::size_type pos = spec.find("://");
  if (std::string::npos != pos) {
    std::string schema = spec.substr(0, pos);
    toLowerInPlace(schema);  // in-place
    parseSchema(schema, _conf);
  }

  if (_conf._socketType == SocketType::Unix) {
    if (std::string::npos != pos) {
      // unix:///a/b/c does not contain a port
      _conf._host = spec.substr(pos + 3);
    }
    return *this;
  }

  pos = (pos != std::string::npos) ? pos + 3 : 0;
  std::string::size_type x = pos + 1;
  if (spec[pos] == '[') {  // ipv6 addresses contain colons
    pos++;                 // do not include '[' in actual address
    while (spec[x] != '\0' && spec[x] != ']') {
      if (is_invalid(spec[x])) {  // we could validate this better
        throw std::runtime_error(std::string("invalid ipv6 address: ") + spec);
      }
      x++;
    }
    if (spec[x] != ']') {
      throw std::runtime_error(std::string("invalid ipv6 address: ") + spec);
    }
    _conf._host = spec.substr(pos, ++x - pos - 1);  // do not include ']'
  } else {
    while (spec[x] != '\0' && spec[x] != '/' && spec[x] != ':') {
      if (!is_host_char(spec[x])) {
        throw std::runtime_error(std::string("invalid host in spec: ") + spec);
      }
      x++;
    }
    _conf._host = spec.substr(pos, x - pos);
  }
  if (_conf._host.empty()) {
    throw std::runtime_error(std::string("invalid host: ") + spec);
  }

  if (spec[x] == ':') {
    pos = ++x;
    while (spec[x] != '\0' && spec[x] != '/' && spec[x] != '?') {
      if (!is_num(spec[x])) {
        throw std::runtime_error(std::string("invalid port in spec: ") + spec);
      }
      x++;
    }
    _conf._port = spec.substr(pos, x - pos);
    if (_conf._port.empty()) {
      throw std::runtime_error(std::string("invalid port in spec: ") + spec);
    }
  }

  return *this;
}

/// @brief get the normalized endpoint
std::string ConnectionBuilder::normalizedEndpoint() const {
  std::string endpoint;
  if (ProtocolType::Http == _conf._protocolType) {
    endpoint.append("http+");
  } else if (ProtocolType::Vst == _conf._protocolType) {
    endpoint.append("vst+");
  }

  if (SocketType::Tcp == _conf._socketType) {
    endpoint.append("tcp://");
  } else if (SocketType::Ssl == _conf._socketType) {
    endpoint.append("ssl://");
  } else if (SocketType::Unix == _conf._socketType) {
    endpoint.append("unix://");
  }

  endpoint.append(_conf._host);
  endpoint.push_back(':');
  endpoint.append(_conf._port);

  return endpoint;
}

}}}  // namespace arangodb::fuerte::v1
