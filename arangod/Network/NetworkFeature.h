////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_NETWORK_NETWORK_FEATURE_H
#define ARANGOD_NETWORK_NETWORK_FEATURE_H 1

#include <atomic>
#include <mutex>

#include <fuerte/requests.h>

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Network/ConnectionPool.h"
#include "RestServer/Metrics.h"
#include "Scheduler/Scheduler.h"

namespace arangodb {
namespace network {
struct RequestOptions;
}

class NetworkFeature final : public application_features::ApplicationFeature {
 public:
  using RequestCallback =
      std::function<void(fuerte::Error err, std::unique_ptr<fuerte::Request> req,
                         std::unique_ptr<fuerte::Response> res, bool isFromPool)>;

  explicit NetworkFeature(application_features::ApplicationServer& server);
  explicit NetworkFeature(application_features::ApplicationServer& server,
                          network::ConnectionPool::Config);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;
  void prepare() override;
  void start() override;
  void beginShutdown() override;
  void stop() override;
  void unprepare() override;
  bool prepared() const;

  /// @brief global connection pool
  arangodb::network::ConnectionPool* pool() const;

#ifdef ARANGODB_USE_GOOGLE_TESTS
  void setPoolTesting(arangodb::network::ConnectionPool* pool);
#endif

  /// @brief increase the counter for forwarded requests
  void trackForwardedRequest();

  std::size_t requestsInFlight() const;

  bool isCongested() const;  // in-flight above low-water mark
  bool isSaturated() const;  // in-flight above high-water mark
  void sendRequest(network::ConnectionPool& pool,
                   network::RequestOptions const& options, std::string const& endpoint,
                   std::unique_ptr<fuerte::Request>&& req, RequestCallback&& cb);

 protected:
  void prepareRequest(network::ConnectionPool const& pool,
                      std::unique_ptr<fuerte::Request>& req);
  void finishRequest(network::ConnectionPool const& pool, fuerte::Error err,
                     std::unique_ptr<fuerte::Request> const& req,
                     std::unique_ptr<fuerte::Response>& res);

 private:
  std::string _protocol;
  uint64_t _maxOpenConnections;
  uint64_t _idleTtlMilli;
  uint32_t _numIOThreads;
  bool _verifyHosts;
  bool _prepared;

  std::mutex _workItemMutex;
  Scheduler::WorkHandle _workItem;
  /// @brief where rhythm is life, and life is rhythm :)
  std::function<void(bool)> _gcfunc;

  std::unique_ptr<network::ConnectionPool> _pool;
  std::atomic<network::ConnectionPool*> _poolPtr;

  /// @brief number of cluster-internal forwarded requests
  /// (from one coordinator to another, in case load-balancing
  /// is used)
  Counter& _forwardedRequests;

  std::uint64_t _maxInFlight;
  Gauge<std::uint64_t>& _requestsInFlight;

  Counter& _requestTimeouts;
  Histogram<fixed_scale_t<double>>& _requestDurations;
};

}  // namespace arangodb

#endif
