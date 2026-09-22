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
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "Basics/EncodingUtils.h"
#include "Basics/StringBuffer.h"
#include "Basics/socket-utils.h"
#include "Endpoint/Endpoint.h"
#include "Rest/CommonDefines.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

#include <sys/socket.h>

#include <memory>
#include <string>

using namespace arangodb;
using namespace arangodb::httpclient;

namespace {

// Serves a canned server response over one end of a socketpair. A real socket
// is needed because the base class select()s on it and asserts it is valid,
// so the read path below mirrors ClientConnection's.
class FakeConnection final : public GeneralClientConnection {
 public:
  FakeConnection(application_features::CommunicationFeaturePhase& comm,
                 Endpoint* endpoint, std::string response)
      : GeneralClientConnection(comm, endpoint, 60.0, 60.0, 1),
        _response(std::move(response)) {}

  ~FakeConnection() { closeSockets(); }

  bool test_idle_connection() override { return true; }

 protected:
  bool connectSocket() override {
    int fds[2];
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
      return false;
    }
    _socket.fileDescriptor = fds[0];
    _peer = fds[1];

    // hand over the whole response, then close the write end so the reader
    // sees EOF once it has drained everything
    ssize_t written = ::write(_peer, _response.data(), _response.size());
    EXPECT_EQ(static_cast<ssize_t>(_response.size()), written);
    ::shutdown(_peer, SHUT_WR);

    _isConnected = true;
    return true;
  }

  void disconnectSocket() override {
    closeSockets();
    _isConnected = false;
  }

  // the request itself is irrelevant here, so drop it
  bool writeClientConnection(void const*, size_t length,
                             size_t* bytesWritten) override {
    *bytesWritten = length;
    return true;
  }

  bool readClientConnection(basics::StringBuffer& buffer,
                            bool& connectionClosed) override {
    connectionClosed = false;

    do {
      if (buffer.reserve(READBUFFER_SIZE) == TRI_ERROR_OUT_OF_MEMORY) {
        return false;
      }
      TRI_read_return_t lenRead =
          TRI_READ_SOCKET(_socket, buffer.end(), READBUFFER_SIZE - 1, 0);
      if (lenRead == -1) {
        connectionClosed = true;
        return false;
      }
      if (lenRead == 0) {
        connectionClosed = true;
        disconnect();
        return true;
      }
      buffer.increaseLength(lenRead);
    } while (readable());

    return true;
  }

  bool readable() override { return prepare(_socket, 0.0, false); }

 private:
  void closeSockets() {
    if (TRI_isvalidsocket(_socket)) {
      TRI_closesocket(_socket);
      TRI_invalidatesocket(&_socket);
    }
    if (_peer != -1) {
      ::close(_peer);
      _peer = -1;
    }
  }

  std::string _response;
  int _peer = -1;
};

std::string const& payload() {
  static std::string const p = [] {
    std::string s;
    for (int i = 0; i < 300; ++i) {
      s += "the quick brown fox jumps over the lazy dog ";
    }
    return s;
  }();
  return p;
}

// compress with the same framing the server uses
std::string lz4Body(std::string const& input) {
  basics::StringBuffer compressed;
  auto res = encoding::lz4Compress(
      reinterpret_cast<uint8_t const*>(input.data()), input.size(), compressed);
  EXPECT_EQ(TRI_ERROR_NO_ERROR, res);
  return std::string(compressed.data(), compressed.size());
}

// wrap a body in HTTP chunked transfer framing
std::string chunkify(std::string const& body, size_t chunkSize) {
  std::string out;
  for (size_t pos = 0; pos < body.size(); pos += chunkSize) {
    size_t n = std::min(chunkSize, body.size() - pos);
    char header[32];
    snprintf(header, sizeof(header), "%zx\r\n", n);
    out += header;
    out.append(body, pos, n);
    out += "\r\n";
  }
  out += "0\r\n\r\n";
  return out;
}

// drive the real SimpleHttpClient over a canned response, return the body
std::string runRequest(std::string const& response) {
  application_features::ApplicationServer server(nullptr, nullptr);
  server.addFeature<application_features::CommunicationFeaturePhase>();

  std::unique_ptr<Endpoint> endpoint(
      Endpoint::clientFactory("tcp://127.0.0.1:9999"));
  EXPECT_NE(nullptr, endpoint);

  std::unique_ptr<GeneralClientConnection> connection(new FakeConnection(
      server.getFeature<application_features::CommunicationFeaturePhase>(),
      endpoint.get(), response));

  SimpleHttpClientParams params(60.0, false);
  SimpleHttpClient client(connection, params);

  std::unique_ptr<SimpleHttpResult> result(
      client.request(rest::RequestType::GET, "/_api/version", nullptr, 0));
  EXPECT_NE(nullptr, result);
  if (result == nullptr) {
    return {};
  }
  return std::string(result->getBody().data(), result->getBody().size());
}

}  // namespace

// regression test for the outbound-client LZ4 path: a response body larger
// than the body buffer's initial allocation must not overflow it
TEST(Lz4ResponseTest, testContentLengthLz4Response) {
  std::string body = lz4Body(::payload());

  std::string response =
      "HTTP/1.1 200 OK\r\n"
      "Content-Encoding: x-arango-lz4\r\n"
      "Content-Length: " +
      std::to_string(body.size()) + "\r\n\r\n" + body;

  EXPECT_EQ(::payload(), runRequest(response));
}

// same response, but delivered with Transfer-Encoding: chunked.
// expected to FAIL today: processChunkedBody() decompresses once per chunk,
// but a chunk is a fragment of one compressed stream, not a payload of its own.
TEST(Lz4ResponseTest, testChunkedLz4Response) {
  std::string body = lz4Body(::payload());
  std::string chunked = chunkify(body, 40);

  std::string response =
      "HTTP/1.1 200 OK\r\n"
      "Content-Encoding: x-arango-lz4\r\n"
      "Transfer-Encoding: chunked\r\n\r\n" +
      chunked;

  std::string result = runRequest(response);

  printf("payload:     %zu bytes: %.60s...\n", ::payload().size(),
         ::payload().c_str());
  printf("compressed:  %zu bytes, sent as %zu bytes of chunked framing\n",
         body.size(), chunked.size());
  printf("decompressed: %zu bytes: %.60s\n", result.size(), result.c_str());

  EXPECT_EQ(::payload(), result);
}

// chunked framing, but the whole body arrives in a single chunk.
// still fails: lz4Uncompress is handed everything from _readBufferOffset to
// the end of the read buffer, so the trailing "\r\n0\r\n\r\n" is counted as
// part of the compressed stream.
TEST(Lz4ResponseTest, testSingleChunkLz4Response) {
  std::string body = lz4Body(::payload());
  std::string chunked = chunkify(body, body.size());

  std::string response =
      "HTTP/1.1 200 OK\r\n"
      "Content-Encoding: x-arango-lz4\r\n"
      "Transfer-Encoding: chunked\r\n\r\n" +
      chunked;

  std::string result = runRequest(response);

  printf("payload:      %zu bytes\n", ::payload().size());
  printf("compressed:   %zu bytes, sent as one chunk (%zu bytes framed)\n",
         body.size(), chunked.size());
  printf("decompressed: %zu bytes\n", result.size());

  EXPECT_EQ(::payload(), result);
}
