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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Metrics/Fwd.h"
#include "Network/ConnectionPool.h"
#include "RestServer/arangod.h"
#include "Scheduler/Scheduler.h"

#include <fuerte/requests.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace arangodb {
class Thread;

namespace network {
struct RequestOptions;

struct RetryableRequest {
  virtual ~RetryableRequest() = default;
  virtual bool isDone() const = 0;
  virtual void retry() = 0;
  virtual void cancel() = 0;
};
}  // namespace network

class NetworkFeature final : public ArangodFeature {
 public:
  using RequestCallback = std::function<void(
      fuerte::Error err, std::unique_ptr<fuerte::Request> req,
      std::unique_ptr<fuerte::Response> res, bool isFromPool)>;

  static constexpr std::string_view name() noexcept { return "Network"; }

  NetworkFeature(Server& server, metrics::MetricsFeature& metrics,
                 network::ConnectionPool::Config);
  ~NetworkFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;
  void prepare() override;
  void start() override;
  void beginShutdown() override;
  void stop() override;
  void unprepare() override;

  bool prepared() const noexcept;

  /// @brief global connection pool
  network::ConnectionPool* pool() const noexcept;

#ifdef ARANGODB_USE_GOOGLE_TESTS
  void setPoolTesting(network::ConnectionPool* pool);
#endif

  /// @brief increase the counter for forwarded requests
  void trackForwardedRequest() noexcept;

  std::size_t requestsInFlight() const noexcept;

  bool isCongested() const noexcept;  // in-flight above low-water mark
  bool isSaturated() const noexcept;  // in-flight above high-water mark
  void sendRequest(network::ConnectionPool& pool,
                   network::RequestOptions const& options,
                   std::string const& endpoint,
                   std::unique_ptr<fuerte::Request>&& req,
                   RequestCallback&& cb);

  void retryRequest(std::shared_ptr<network::RetryableRequest>, RequestLane,
                    std::chrono::steady_clock::duration);

  static uint64_t defaultIOThreads();

 protected:
  void prepareRequest(network::ConnectionPool const& pool,
                      std::unique_ptr<fuerte::Request>& req);
  void finishRequest(network::ConnectionPool const& pool, fuerte::Error err,
                     std::unique_ptr<fuerte::Request> const& req,
                     std::unique_ptr<fuerte::Response>& res);

 private:
  void cancelGarbageCollection() noexcept;
  void injectAcceptEncodingHeader(fuerte::Request& req) const;
  bool compressRequestBody(network::RequestOptions const& opts,
                           fuerte::Request& req) const;

  // configuration
  std::string _protocol;
  uint64_t _maxOpenConnections;
  uint64_t _idleTtlMilli;
  uint32_t _numIOThreads;
  bool _verifyHosts;

  std::atomic<bool> _prepared;

  /// @brief where rhythm is life, and life is rhythm :)
  std::function<void(bool)> _gcfunc;

  std::unique_ptr<network::ConnectionPool> _pool;
  std::atomic<network::ConnectionPool*> _poolPtr;

  // protects _workItem and _retryRequests
  std::mutex _workItemMutex;
  Scheduler::WorkHandle _workItem;

  std::unique_ptr<Thread> _retryThread;

  /// @brief number of cluster-internal forwarded requests
  /// (from one coordinator to another, in case load-balancing
  /// is used)
  metrics::Counter& _forwardedRequests;

  std::uint64_t _maxInFlight;
  metrics::Gauge<std::uint64_t>& _requestsInFlight;

  metrics::Counter& _requestTimeouts;
  metrics::Histogram<metrics::FixScale<double>>& _requestDurations;

  metrics::Counter& _unfinishedSends;
  metrics::Histogram<metrics::FixScale<double>>& _dequeueDurations;
  metrics::Histogram<metrics::FixScale<double>>& _sendDurations;
  metrics::Histogram<metrics::FixScale<double>>& _responseDurations;

  uint64_t _compressRequestThreshold;

  enum class CompressionType { kNone, kDeflate, kGzip, kLz4, kAuto };
  CompressionType _compressionType;
  std::string _compressionTypeLabel;
  metrics::MetricsFeature& _metrics;
};

}  // namespace arangodb
