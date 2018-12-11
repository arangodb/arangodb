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

#include <fuerte/connection.h>
#include <fuerte/FuerteLogger.h>

#include <boost/algorithm/string.hpp>

#include "HttpConnection.h"
#include "VstConnection.h"
#include "http_parser/http_parser.h"

namespace arangodb { namespace fuerte { inline namespace v1 {
// Create an connection and start opening it.
std::shared_ptr<Connection> ConnectionBuilder::connect(EventLoopService& loop) {
  std::shared_ptr<Connection> result;

  if (_conf._protocolType == ProtocolType::Vst) {
    FUERTE_LOG_DEBUG << "fuerte - creating velocystream connection\n";
    if (_conf._socketType == SocketType::Tcp) {
      result = std::make_shared<vst::VstConnection<SocketType::Tcp>>(loop, _conf);
    } else if (_conf._socketType == SocketType::Ssl) {
      result = std::make_shared<vst::VstConnection<SocketType::Ssl>>(loop, _conf);
    }
#ifdef ASIO_HAS_LOCAL_SOCKETS
     else if (_conf._socketType == SocketType::Unix) {
      result = std::make_shared<vst::VstConnection<SocketType::Unix>>(loop, _conf);
    }
#endif
  } else {
    // throw std::logic_error("http in vst test");
    FUERTE_LOG_DEBUG << "fuerte - creating http connection\n";
    if (_conf._socketType == SocketType::Tcp) {
      result = std::make_shared<http::HttpConnection<SocketType::Tcp>>(loop, _conf);
    } else if (_conf._socketType == SocketType::Ssl) {
      result = std::make_shared<http::HttpConnection<SocketType::Ssl>>(loop, _conf);
    }
#ifdef ASIO_HAS_LOCAL_SOCKETS
     else if (_conf._socketType == SocketType::Unix) {
      result = std::make_shared<http::HttpConnection<SocketType::Unix>>(loop, _conf);
    }
#endif
  }
  if (!result) {
    throw std::logic_error("unsupported socket or protocol type");
  }
  
  // Start the connection implementation
  result->startConnection();

  return result;
}
  
void parseSchema( std::string const& schema,
                  detail::ConnectionConfiguration& conf) {
  // non exthausive list of supported url schemas
  // "http+tcp://", "http+ssl://", "tcp://", "ssl://", "unix://", "http+unix://"
  // "vsts://", "vst://", "http://", "https://", "vst+unix://", "vst+tcp://"
  std::string proto = schema;
  std::string::size_type pos = schema.find('+');
  if (pos != std::string::npos && pos + 1 < schema.length()) {
    // got something like "http+tcp://"
    proto = schema.substr(0, pos);
    std::string socket = schema.substr(pos + 1);
    if (socket == "tcp" || socket == "srv") {
      conf._socketType = SocketType::Tcp;
    } else if (socket == "ssl") {
      conf._socketType = SocketType::Ssl;
    } else if (socket == "unix") {
      conf._socketType = SocketType::Unix;
    } else {
      throw std::runtime_error(std::string("invalid socket type: ") + proto);
    }
    
    if (proto == "vst") {
      conf._protocolType = ProtocolType::Vst;
    } else if (proto == "http") {
      conf._protocolType = ProtocolType::Http;
    } else {
      throw std::runtime_error(std::string("invalid protocol: ") + proto);
    }
    
  } else { // got only protocol
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
    } else {
      throw std::runtime_error(std::string("invalid protocol: ") + proto);
    }
  }
}

ConnectionBuilder& ConnectionBuilder::endpoint(std::string const& host) {
  // we need to handle unix:// urls seperately
  size_t pos = host.find("://");
  if (pos == std::string::npos) {
    throw std::runtime_error(std::string("invalid endpoint spec: ") + host);
  }
  std::string schema = host.substr(0, pos);
  boost::algorithm::to_lower(schema); // in-place
  parseSchema(schema, _conf);
  
  if (_conf._socketType == SocketType::Unix) {
    // unix:///a/b/c does not contain a port
    _conf._host = host.substr(pos + 3);
    return *this;
  }
  
  // now lets perform proper URL parsing
  struct http_parser_url parsed;
  http_parser_url_init(&parsed);
  int error = http_parser_parse_url(host.c_str(), host.length(), 0, &parsed);
  if (error != 0) {
    throw std::runtime_error(std::string("invalid endpoint spec: ") + host);
  }
  
  // put hostname, port and path in seperate strings
  if (!(parsed.field_set & (1 << UF_HOST))) {
    throw std::runtime_error(std::string("invalid host: ") + host);
  }
  _conf._host = host.substr(parsed.field_data[UF_HOST].off,
                            parsed.field_data[UF_HOST].len);
  
  if (!(parsed.field_set & (1 << UF_PORT))) {
    throw std::runtime_error(std::string("invalid port: ") + host);
  }
  _conf._port = host.substr(parsed.field_data[UF_PORT].off,
                            parsed.field_data[UF_PORT].len);

  return *this;
}
}}}  // namespace arangodb::fuerte::v1
