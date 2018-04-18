////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "AgencyCommManagerMock.h"

AgencyCommManagerMock::AgencyCommManagerMock(): AgencyCommManager("") {
}

void AgencyCommManagerMock::addConnection(
    std::unique_ptr<arangodb::httpclient::GeneralClientConnection>&& connection
) {
  std::string endpoint(""); // must be empty string to match addEndpoint(...) normalization

  addEndpoint(endpoint);
  release(std::move(connection), endpoint);
}

EndpointMock::EndpointMock()
  : arangodb::Endpoint(
      arangodb::Endpoint::DomainType:: UNKNOWN,
      arangodb::Endpoint::EndpointType::CLIENT,
      arangodb::Endpoint::TransportType::HTTP,
      arangodb::Endpoint::EncryptionType::NONE,
      "",
      0
    ) {
}

TRI_socket_t EndpointMock::connect(
    double connectTimeout, double requestTimeout
) {
  TRI_ASSERT(false);
  return TRI_socket_t();
}

void EndpointMock::disconnect() {
}

int EndpointMock::domain() const {
  TRI_ASSERT(false);
  return 0;
}

std::string EndpointMock::host() const {
  return ""; // empty host for testing
}

std::string EndpointMock::hostAndPort() const {
  TRI_ASSERT(false);
  return "";
}

bool EndpointMock::initIncoming(TRI_socket_t socket) {
  TRI_ASSERT(false);
  return 0;
}

int EndpointMock::port() const {
  TRI_ASSERT(false);
  return 0;
}

GeneralClientConnectionMock::GeneralClientConnectionMock()
  : GeneralClientConnection(&endpoint, 0, 0, 0),
    nil(file_open((file_path_t)nullptr, "rw")) {
  _socket.fileDescriptor = file_no(nil.get()); // must be a readable/writable descriptor
}

bool GeneralClientConnectionMock::connectSocket() {
  _isConnected = true;
  return true;
}

void GeneralClientConnectionMock::disconnectSocket() {
}

void GeneralClientConnectionMock::getValue(
    arangodb::basics::StringBuffer& buffer
) {
  buffer.appendChar('\n');
}

bool GeneralClientConnectionMock::readable() {
  TRI_ASSERT(false);
  return false;
}

bool GeneralClientConnectionMock::readClientConnection(
    arangodb::basics::StringBuffer& buffer, bool& connectionClosed
) {
  getValue(buffer);
  connectionClosed = true;

  return true;
}

void GeneralClientConnectionMock::setKey(char const* data, size_t length) {
  // NOOP
}

bool GeneralClientConnectionMock::writeClientConnection(
    void const* buffer, size_t length, size_t* bytesWritten
) {
  setKey(static_cast<char const*>(buffer), length);
  *bytesWritten = length; // assume wrote the entire buffer

  return true;
}

void GeneralClientConnectionListMock::getValue(
    arangodb::basics::StringBuffer& buffer
) {
  if (responses.empty()) {
    GeneralClientConnectionMock::getValue(buffer);

    return;
  }

  buffer.appendText(responses.front());
  responses.pop_front();
}

void GeneralClientConnectionMapMock::getValue(
    arangodb::basics::StringBuffer& buffer
) {
  auto itr = responses.find(lastKey);

  // try to search by just the header
  if (itr == responses.end()) {
    auto pos = lastKey.find("\r\n");

    if (pos != std::string::npos) {
      itr = responses.find(lastKey.substr(0, pos));
    }
  }

  if (itr == responses.end()) {
    GeneralClientConnectionMock::getValue(buffer);

    return;
  }

  buffer.appendText(itr->second);
}

void GeneralClientConnectionMapMock::setKey(char const* data, size_t length) {
  lastKey.assign(data, length);

  auto pos = lastKey.find("\r\n");

  if (pos == std::string::npos) {
    return; // use full string as key
  }

  auto head = lastKey.substr(0, pos);

  pos = lastKey.find("\r\n\r\n", pos);
  lastKey = pos == std::string::npos
          ? head // first line of header (no body in request)
          : head.append(lastKey.c_str() + pos); // first line of header with body
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------