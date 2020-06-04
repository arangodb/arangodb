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

#include <fuerte/FuerteLogger.h>
#include <fuerte/connection.h>
#include <fuerte/waitgroup.h>

namespace arangodb { namespace fuerte { inline namespace v1 {
// Deconstructor
Connection::~Connection() {
  FUERTE_LOG_DEBUG << "Destroying Connection" << "\n";
}

// sendRequest and wait for it to finished.
std::unique_ptr<Response> Connection::sendRequest(std::unique_ptr<Request> request) {
  FUERTE_LOG_TRACE << "sendRequest (sync): before send" << "\n";

  WaitGroup wg;
  std::unique_ptr<Response> rv;
  ::arangodb::fuerte::v1::Error error = Error::NoError;

  auto cb = [&](::arangodb::fuerte::v1::Error e,
                std::unique_ptr<Request> request,
                std::unique_ptr<Response> response) {
    WaitGroupDone done(wg);
    rv = std::move(response);
    error = e;
  };

  {
    // Start asynchronous request
    wg.add();
    sendRequest(std::move(request), cb);

    // Wait for request to finish.
    FUERTE_LOG_TRACE << "sendRequest (sync): before wait" << "\n";
    wg.wait();
  }

  FUERTE_LOG_TRACE << "sendRequest (sync): done" << "\n";

  if (error != Error::NoError) {
    throw error;
  }

  return rv;
}

std::string Connection::endpoint() const {
  std::string endpoint;
  endpoint.reserve(32);
  // http/vst
  endpoint.append(fuerte::to_string(_config._protocolType));
  endpoint.push_back('+');
  // tcp/ssl/unix
  endpoint.append(fuerte::to_string(_config._socketType));
  endpoint.append("://");
  // domain name or IP (xxx.xxx.xxx.xxx)
  endpoint.append(_config._host);
  if (_config._socketType != SocketType::Unix) {
    endpoint.push_back(':');
    endpoint.append(_config._port);
  }
  return endpoint;
}
}}}  // namespace arangodb::fuerte::v1
