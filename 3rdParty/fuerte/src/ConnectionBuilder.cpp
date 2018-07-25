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
/// @author Jan Christoph Uhde
/// @author Ewout Prangsma
////////////////////////////////////////////////////////////////////////////////

#include <boost/algorithm/string.hpp>

#include <fuerte/connection.h>
#include <fuerte/waitgroup.h>

#include "HttpConnection.h"
#include "VstConnection.h"

namespace arangodb { namespace fuerte { inline namespace v1 {
// Create an connection and start opening it.
std::shared_ptr<Connection> ConnectionBuilder::connect(EventLoopService& loop) {
  std::shared_ptr<Connection> result;

  if (_conf._connType == TransportType::Vst) {
    FUERTE_LOG_DEBUG << "fuerte - creating velocystream connection"
                     << std::endl;
    result = std::make_shared<vst::VstConnection>(loop.nextIOContext(), _conf);
  } else {
    // throw std::logic_error("http in vst test");
    FUERTE_LOG_DEBUG << "fuerte - creating http connection" << std::endl;
    result =
        std::make_shared<http::HttpConnection>(loop.nextIOContext(), _conf);
  }
  // Start the connection implementation
  result->start();

  return result;
}

ConnectionBuilder& ConnectionBuilder::host(std::string const& str) {
  std::vector<std::string> strings;
  boost::split(strings, str, boost::is_any_of(":"));

  // get protocol
  std::string const& proto = strings[0];
  if (proto == "vst") {
    _conf._connType = TransportType::Vst;
    _conf._ssl = false;
  } else if (proto == "vsts") {
    _conf._connType = TransportType::Vst;
    _conf._ssl = true;
  } else if (proto == "http") {
    _conf._connType = TransportType::Http;
    _conf._ssl = false;
  } else if (proto == "https") {
    _conf._connType = TransportType::Http;
    _conf._ssl = true;
  } else {
    throw std::runtime_error(std::string("invalid protocol: ") + proto);
  }

  // TODO
  // do more checking?
  _conf._host = strings[1].erase(0, 2);  // remove '//'
  _conf._port = strings[2];

  return *this;
}
}}}  // namespace arangodb::fuerte::v1
