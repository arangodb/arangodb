////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#ifndef ARANGOD_CLUSTER_ASYNC_AGENCY_COMM_H
#define ARANGOD_CLUSTER_ASYNC_AGENCY_COMM_H 1

#include <fuerte/message.h>

#include <deque>
#include <memory>
#include <mutex>

#include "Agency/AgencyComm.h"
#include "Cluster/PathComponent.h"
#include "Cluster/ResultT.h"
#include "Futures/Future.h"
#include "Network/Methods.h"

namespace arangodb {

struct AsyncAgencyCommResult {
  arangodb::fuerte::Error error;
  std::unique_ptr<arangodb::fuerte::Response> response;

  [[nodiscard]] bool ok() const {
    return arangodb::fuerte::Error::NoError == this->error;
  }

  [[nodiscard]] bool fail() const { return !ok(); }

  VPackSlice slice() { return response->slice(); }

  arangodb::fuerte::StatusCode statusCode() { return response->statusCode(); }

  Result asResult() {
    if (!ok()) {
      return Result{int(error), arangodb::fuerte::to_string(error)};
    } else if (200 <= statusCode() && statusCode() <= 299) {
      return Result{};
    } else {
      return Result{int(statusCode())};
    }
  }
};

struct AgencyReadResult : public AsyncAgencyCommResult {
  AgencyReadResult(AsyncAgencyCommResult&& result,
                   std::shared_ptr<arangodb::cluster::paths::Path const> valuePath)
      : AsyncAgencyCommResult(std::move(result)),
        _value(nullptr),
        _valuePath(std::move(valuePath)) {}
  VPackSlice value() {
    if (this->_value.start() == nullptr) {
      this->_value = slice().at(0).get(_valuePath->vec());
    }
    return this->_value;
  }

 private:
  VPackSlice _value;
  std::shared_ptr<arangodb::cluster::paths::Path const> _valuePath;
};

class AsyncAgencyComm;

class AsyncAgencyCommManager final {
 public:
  static std::unique_ptr<AsyncAgencyCommManager> INSTANCE;

  static void initialize(application_features::ApplicationServer& server) {
    INSTANCE.reset(new AsyncAgencyCommManager(server));
  }

  static AsyncAgencyCommManager& getInstance();

  explicit AsyncAgencyCommManager(application_features::ApplicationServer&);

  void addEndpoint(std::string const& endpoint);
  void updateEndpoints(std::vector<std::string> const& endpoints);

  std::deque<std::string> endpoints() const {
    std::unique_lock<std::mutex> guard(_lock);
    return _endpoints;
  }

  std::string getCurrentEndpoint();
  void reportError(std::string const& endpoint);
  void reportRedirect(std::string const& endpoint, std::string const& redirectTo);

  network::ConnectionPool* pool() const { return _pool; }
  void pool(network::ConnectionPool* pool) { _pool = pool; }

  application_features::ApplicationServer& server();

 private:
  application_features::ApplicationServer& _server;
  mutable std::mutex _lock;
  std::deque<std::string> _endpoints;
  network::ConnectionPool* _pool = nullptr;
};

class AsyncAgencyComm final {
 public:
  using FutureResult = arangodb::futures::Future<AsyncAgencyCommResult>;
  using FutureReadResult = arangodb::futures::Future<AgencyReadResult>;

  [[nodiscard]] FutureResult getValues(std::string const& path) const;
  [[nodiscard]] FutureReadResult getValues(
      std::shared_ptr<arangodb::cluster::paths::Path const> const& path) const;

  template <typename T>
  [[nodiscard]] FutureResult setValue(network::Timeout timeout,
                                      std::shared_ptr<arangodb::cluster::paths::Path const> const& path,
                                      T const& value, uint64_t ttl = 0) {
    return setValue(timeout, path->str(), value, ttl);
  }

  template <typename T>
  [[nodiscard]] FutureResult setValue(network::Timeout timeout, std::string const& path,
                                      T const& value, uint64_t ttl = 0) {
    VPackBuffer<uint8_t> transaction;
    {
      VPackBuilder trxBuilder(transaction);
      VPackArrayBuilder env(&trxBuilder);
      {
        VPackArrayBuilder trx(&trxBuilder);
        {
          VPackObjectBuilder ops(&trxBuilder);
          {
            VPackObjectBuilder op(&trxBuilder, path);
            trxBuilder.add("op", VPackValue("set"));
            trxBuilder.add("new", value);
            if (ttl > 0) {
              trxBuilder.add("ttl", VPackValue(ttl));
            }
          }
        }
      }
    }

    return sendWriteTransaction(timeout, std::move(transaction));
  }

  [[nodiscard]] FutureResult deleteKey(
      network::Timeout timeout,
      std::shared_ptr<arangodb::cluster::paths::Path const> const& path) const;
  [[nodiscard]] FutureResult deleteKey(network::Timeout timeout, std::string const& path) const;

  [[nodiscard]] FutureResult sendWriteTransaction(network::Timeout timeout,
                                                  velocypack::Buffer<uint8_t>&& body) const;
  [[nodiscard]] FutureResult sendReadTransaction(network::Timeout timeout,
                                                 velocypack::Buffer<uint8_t>&& body) const;

  [[nodiscard]] FutureResult sendTransaction(network::Timeout timeout,
                                             AgencyReadTransaction const&) const;
  [[nodiscard]] FutureResult sendTransaction(network::Timeout timeout,
                                             AgencyWriteTransaction const&) const;

 public:
  enum class RequestType {
    READ,   // send the transaction again in the case of no response
    WRITE,  // does not send the transaction again but instead tries to do inquiry with the given ids
    CUSTOM,  // talk to the leader and return always the result, even on timeout or redirect
  };

  using ClientId = std::string;

  [[nodiscard]] FutureResult sendWithFailover(arangodb::fuerte::RestVerb method,
                                              std::string const& url,
                                              network::Timeout timeout, RequestType type,
                                              std::vector<ClientId> clientIds,
                                              velocypack::Buffer<uint8_t>&& body) const;

  [[nodiscard]] FutureResult sendWithFailover(arangodb::fuerte::RestVerb method,
                                              std::string const& url,
                                              network::Timeout timeout, RequestType type,
                                              std::vector<ClientId> clientIds,
                                              AgencyTransaction const& trx) const;

  [[nodiscard]] FutureResult sendWithFailover(arangodb::fuerte::RestVerb method,
                                              std::string const& url,
                                              network::Timeout timeout, RequestType type,
                                              velocypack::Buffer<uint8_t>&& body) const;

 public:
  AsyncAgencyComm() : _manager(AsyncAgencyCommManager::getInstance()) {}
  explicit AsyncAgencyComm(AsyncAgencyCommManager& manager)
      : _manager(manager) {}

 private:
  AsyncAgencyCommManager& _manager;
};

}  // namespace arangodb
#endif
