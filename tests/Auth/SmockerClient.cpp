////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "SmockerClient.h"

#include "Basics/StringUtils.h"
#include "Basics/process-utils.h"
#include "Inspection/JsonPrintInspector.h"
#include "Logger/LogMacros.h"

#include <fuerte/connection.h>
#include <fuerte/requests.h>
#include <gtest/gtest.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/String.h>

#include <chrono>
#include <map>
#include <sstream>
#include <thread>
#include <vector>

namespace {

struct SmockerRequestDef {
  std::string method;
  std::string path;

  template<class Inspector>
  friend auto inspect(Inspector& f, SmockerRequestDef& x) {
    return f.object(x).fields(f.field("method", x.method),
                              f.field("path", x.path));
  }
};

struct SmockerResponseDef {
  int status;
  std::map<std::string, std::vector<std::string>> headers;
  std::string body;

  template<class Inspector>
  friend auto inspect(Inspector& f, SmockerResponseDef& x) {
    return f.object(x).fields(f.field("status", x.status),
                              f.field("headers", x.headers),
                              f.field("body", x.body));
  }
};

struct SmockerMockDef {
  SmockerRequestDef request;
  SmockerResponseDef response;

  template<class Inspector>
  friend auto inspect(Inspector& f, SmockerMockDef& x) {
    return f.object(x).fields(f.field("request", x.request),
                              f.field("response", x.response));
  }
};

template<typename T>
auto toJson(T& value) -> std::string {
  std::ostringstream stream;
  arangodb::inspection::JsonPrintInspector<> inspector{
      stream, arangodb::inspection::JsonPrintFormat::kMinimal};
  auto res = inspector.apply(value);
  EXPECT_TRUE(res.ok()) << (res.ok() ? "" : res.error());
  return std::move(stream).str();
}

auto runDocker(std::vector<std::string> const& args) -> ExternalProcessStatus {
  ExternalId pid{};
  TRI_CreateExternalProcess("docker", args, {}, false, &pid);
  if (pid._pid == TRI_INVALID_PROCESS_ID) {
    return ExternalProcessStatus{._status = TRI_EXT_FORK_FAILED,
                                 ._errorMessage = "Failed to start docker"};
  }
  return TRI_CheckExternalProcess(pid, true, 0);
}

}  // namespace

namespace arangodb::test {

SmockerClient::SmockerClient(std::string containerName, std::string mockUrl,
                             std::string adminUrl)
    : _containerName(std::move(containerName)),
      _mockUrl(std::move(mockUrl)),
      _adminUrl(std::move(adminUrl)) {}

void SmockerClient::start() {
  // Remove any leftover container (ignore errors).
  runDocker({"rm", "-f", _containerName});

  auto status =
      runDocker({"run", "-d", "-p", "8080:8080", "-p", "8081:8081", "--name",
                 _containerName, "ghcr.io/smocker-dev/smocker"});
  if (status._status != TRI_EXT_TERMINATED) {
    _startError = "docker run failed: " + status._errorMessage;
    return;
  }
  if (status._exitStatus != 0) {
    _startError = "docker run exited with status " +
                  std::to_string(status._exitStatus);
    return;
  }

  auto config = network::ConnectionPool::Config();
  config.metrics =
      network::ConnectionPool::Metrics::createStub("SmockerClient admin");
  _adminPool = std::make_unique<network::ConnectionPool>(config);

  for (auto t0 = std::chrono::steady_clock::now();
       std::chrono::steady_clock::now() - t0 < std::chrono::seconds(120);
       std::this_thread::sleep_for(std::chrono::milliseconds(10))) {
    try {
      auto res = sendToAdmin(fuerte::RestVerb::Get, "/version");
      if (res && res->statusCode() == fuerte::StatusOK) {
        _startError.reset();
        return;
      }
    } catch (...) {
    }
  }
  _startError = "Smocker did not become ready within 120 seconds";
}

void SmockerClient::stop() {
  _adminPool.reset();
  runDocker({"rm", "-f", _containerName});
}

void SmockerClient::resetMocks() {
  auto res = sendToAdmin(fuerte::RestVerb::Post, "/reset");
  ASSERT_TRUE(res);
  ASSERT_EQ(res->statusCode(), fuerte::StatusOK)
      << "Failed to reset smocker mocks";
}

void SmockerClient::addMock(std::string const& path, int status,
                            std::string const& responseBody) {
  auto mocks = std::vector<SmockerMockDef>{{
      .request = {.method = "POST", .path = path},
      .response =
          {
              .status = status,
              .headers = {{"Content-Type", {"application/json"}}},
              .body = responseBody,
          },
  }};
  auto mockJson = toJson(mocks);
  auto res = sendToAdmin(fuerte::RestVerb::Post, "/mocks", mockJson);
  ASSERT_TRUE(res);
  ASSERT_EQ(res->statusCode(), fuerte::StatusOK)
      << "Failed to add smocker mock: " << res->payloadAsString();
}

auto SmockerClient::getHistory() -> std::vector<SmockerHistoryEntry> {
  auto res = sendToAdmin(fuerte::RestVerb::Get, "/history");
  EXPECT_TRUE(res);
  EXPECT_EQ(res->statusCode(), fuerte::StatusOK)
      << "Failed to get smocker history";
  if (!res || res->statusCode() != fuerte::StatusOK) {
    return {};
  }

  auto builder =
      velocypack::Parser::fromJson(res->payloadAsStringView());
  auto slice = builder->slice();

  std::vector<SmockerHistoryEntry> entries;
  for (auto const& entry : velocypack::ArrayIterator(slice)) {
    auto req = entry.get("request");
    entries.push_back({
        .method = req.get("method").copyString(),
        .path = req.get("path").copyString(),
        .body = velocypack::String{req.get("body")},
    });
  }
  return entries;
}

auto SmockerClient::sendToAdmin(fuerte::RestVerb verb, std::string const& path,
                                std::string const& body)
    -> std::unique_ptr<fuerte::Response> {
  bool isFromPool = false;
  auto conn = _adminPool->leaseConnection(_adminUrl, isFromPool);
  auto req = fuerte::createRequest(verb, path);
  if (!body.empty()) {
    req->header.contentType(fuerte::ContentType::Json);
    auto& buf = req->payloadForModification();
    buf.append(reinterpret_cast<uint8_t const*>(body.data()), body.size());
  }
  return conn->sendRequest(std::move(req));
}

}  // namespace arangodb::test
