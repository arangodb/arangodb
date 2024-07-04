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

#include "NetworkFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/EncodingUtils.h"
#include "Basics/FunctionUtils.h"
#include "Basics/NumberOfCores.h"
#include "Basics/ScopeGuard.h"
#include "Basics/Thread.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/FixScale.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/HistogramBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "Network/Methods.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Scheduler/SchedulerFeature.h"

#include <fuerte/connection.h>

#include <chrono>
#include <condition_variable>
#include <queue>
#include <vector>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace {
// executes network request retry operations in a separate thread,
// so that they do not have be executed via the scheduler.
// the reason to execute them from a dedicated thread is that a
// dedicated thread will always have capacity to execute, whereas
// pushing retry operations to the scheduler needs to use the correct
// priority lanes and also could be blocked by scheduler threads
// not pulling any more new tasks due to overload/overwhelm.
class RetryThread : public ServerThread<ArangodServer> {
  static constexpr auto kDefaultSleepTime = std::chrono::seconds(10);

 public:
  explicit RetryThread(ArangodServer& server)
      : ServerThread<ArangodServer>(server, "NetworkRetry"),
        _nextRetryTime(std::chrono::steady_clock::now() + kDefaultSleepTime) {}

  ~RetryThread() {
    shutdown();
    cancelAll();
  }

  void beginShutdown() override {
    Thread::beginShutdown();

    _mutex.lock();
    _mutex.unlock();
    // notify retry thread
    _cv.notify_one();
  }

  void cancelAll() noexcept {
    std::unique_lock<std::mutex> guard(_mutex);

    // pop everything from the queue until it is empty.
    while (!_retryRequests.empty()) {
      auto& top = _retryRequests.top();
      auto req = std::move(top.req);
      TRI_ASSERT(req);
      _retryRequests.pop();
      try {
        // canceling a request can throw in case a concurrent
        // thread has already resolved or canceled the request.
        // in this case, simply ignore the exception.
        req->cancel();
      } catch (...) {
        // does not matter
      }
    }
  }

  void push(std::shared_ptr<network::RetryableRequest> req,
            std::chrono::steady_clock::time_point retryTime) {
    TRI_ASSERT(req);

    auto cancelGuard = scopeGuard([req]() noexcept {
      try {
        // canceling a request can throw in case a concurrent
        // thread has already resolved or canceled the request.
        // in this case, simply ignore the exception.
        req->cancel();
      } catch (...) {
        // does not matter
      }
    });

    TRI_IF_FAILURE("NetworkFeature::retryRequestFail") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    std::unique_lock<std::mutex> guard(_mutex);

    if (isStopping()) {
      return;
    }

    bool mustNotify = (retryTime < _nextRetryTime);
    if (mustNotify) {
      _nextRetryTime = retryTime;
    }

    _retryRequests.push(RetryItem{retryTime, std::move(req)});
    guard.unlock();

    cancelGuard.cancel();

    if (!mustNotify) {
      // retry time is already in the past?
      mustNotify = retryTime <= std::chrono::steady_clock::now();
    }

    // notify retry thread about the new item
    if (mustNotify) {
      _cv.notify_one();
    }
  }

 protected:
  void run() override {
    while (!isStopping()) {
      try {
        std::unique_lock<std::mutex> guard(_mutex);

        // by default, sleep an arbitrary amount of 10 seconds.
        // note that this value may be reduced if we have an
        // element in the queue that is due earlier.
        _nextRetryTime = std::chrono::steady_clock::now() + kDefaultSleepTime;

        while (!_retryRequests.empty()) {
          auto& top = _retryRequests.top();
          auto now = std::chrono::steady_clock::now();
          if (top.retryTime > now) {
            // next retry operation is in the future...
            _nextRetryTime = top.retryTime;
            break;
          }

          auto req = std::move(top.req);
          TRI_ASSERT(req);
          _retryRequests.pop();

          if (!_retryRequests.empty()) {
            _nextRetryTime = _retryRequests.top().retryTime;
          } else {
            _nextRetryTime = now + kDefaultSleepTime;
          }

          if (isStopping()) {
            try {
              req->cancel();
            } catch (...) {
              // ignore error during cancelation
            }
            break;
          }

          guard.unlock();

          try {
            // the actual retry action is carried out here.
            // note: there is a small opportunity of a race here, if
            // a concurrent request sets resolves the request's
            // promise. this will lead to an exception here, which
            // we can ignore.
            if (!req->isDone()) {
              req->retry();
            }
          } catch (std::exception const& ex) {
            LOG_TOPIC("aa476", WARN, Logger::COMMUNICATION)
                << "network retry thread caught exception while "
                   "retrying/canceling request: "
                << ex.what();
          }

          guard.lock();
        }

        // nothing (more) to do
        TRI_ASSERT(guard.owns_lock());

        if (!isStopping()) {
          _cv.wait_until(guard, _nextRetryTime);
        }
      } catch (std::exception const& ex) {
        LOG_TOPIC("2b2e9", WARN, Logger::COMMUNICATION)
            << "network retry thread caught exception: " << ex.what();
      }
    }

    // cancel all outstanding requests
    cancelAll();
  }

 private:
  struct RetryItem {
    std::chrono::steady_clock::time_point retryTime;
    std::shared_ptr<network::RetryableRequest> req;
  };

  // comparator for _retryRequests priority queue:
  // the item with the lowest retryTime timestamp sits at the top and
  // will be pulled from the queue first
  struct RetryItemComparator {
    bool operator()(RetryItem const& lhs, RetryItem const& rhs) const noexcept {
      if (lhs.retryTime > rhs.retryTime) {
        // priority_queue: elements with higher times need to return true for
        // the comparator.
        return true;
      }
      if (lhs.retryTime == rhs.retryTime) {
        // equal retry time. use pointer values to define order.
        return lhs.req.get() < rhs.req.get();
      }
      return false;
    }
  };

  std::mutex _mutex;
  std::condition_variable _cv;
  std::priority_queue<RetryItem, std::vector<RetryItem>, RetryItemComparator>
      _retryRequests;
  std::chrono::steady_clock::time_point _nextRetryTime{};
};

void queueGarbageCollection(std::mutex& mutex,
                            arangodb::Scheduler::WorkHandle& workItem,
                            std::function<void(bool)>& gcfunc,
                            std::chrono::seconds offset) {
  std::lock_guard<std::mutex> guard(mutex);
  workItem = arangodb::SchedulerFeature::SCHEDULER->queueDelayed(
      "networkfeature-gc", arangodb::RequestLane::INTERNAL_LOW, offset, gcfunc);
}

constexpr double CongestionRatio = 0.5;
constexpr std::uint64_t MaxAllowedInFlight = 65536;
constexpr std::uint64_t MinAllowedInFlight = 64;
}  // namespace

namespace arangodb {
struct NetworkFeatureScale {
  static metrics::FixScale<double> scale() {
    return {0.0, 100.0, {1.0, 5.0, 15.0, 50.0}};
  }
};

struct NetworkFeatureSendScaleSmall {
  static metrics::FixScale<double> scale() {
    return {0.0, 10.0, {0.000001, 0.00001, 0.0001, 0.001, 0.01, 0.1, 1.0}};
  }
};

struct NetworkFeatureSendScaleLarge {
  static metrics::FixScale<double> scale() {
    return {0.0, 10000.0, {0.01, 0.1, 1.0, 10.0, 100.0, 1000.0}};
  }
};

DECLARE_COUNTER(arangodb_network_forwarded_requests_total,
                "Number of requests forwarded to another coordinator");
DECLARE_COUNTER(arangodb_network_request_timeouts_total,
                "Number of internal requests that have timed out");
DECLARE_HISTOGRAM(
    arangodb_network_request_duration_as_percentage_of_timeout,
    NetworkFeatureScale,
    "Internal request round-trip time as a percentage of timeout [%]");
DECLARE_COUNTER(arangodb_network_unfinished_sends_total,
                "Number of times the sending of a request remained unfinished");
DECLARE_HISTOGRAM(
    arangodb_network_dequeue_duration, NetworkFeatureSendScaleSmall,
    "Time to dequeue a queued network request in fuerte in seconds");
DECLARE_HISTOGRAM(arangodb_network_send_duration, NetworkFeatureSendScaleLarge,
                  "Time to send out internal requests in seconds");
DECLARE_HISTOGRAM(
    arangodb_network_response_duration, NetworkFeatureSendScaleLarge,
    "Time to wait for network response after it was sent out in seconds");
DECLARE_GAUGE(arangodb_network_requests_in_flight, uint64_t,
              "Number of outgoing internal requests in flight");

NetworkFeature::NetworkFeature(Server& server, metrics::MetricsFeature& metrics,
                               network::ConnectionPool::Config config)
    : ArangodFeature{server, *this},
      _protocol(),
      _maxOpenConnections(config.maxOpenConnections),
      _idleTtlMilli(config.idleConnectionMilli),
      _numIOThreads(defaultIOThreads()),
      _verifyHosts(config.verifyHosts),
      _prepared(false),
      _forwardedRequests(
          metrics.add(arangodb_network_forwarded_requests_total{})),
      _maxInFlight(::MaxAllowedInFlight),
      _requestsInFlight(metrics.add(arangodb_network_requests_in_flight{})),
      _requestTimeouts(metrics.add(arangodb_network_request_timeouts_total{})),
      _requestDurations(metrics.add(
          arangodb_network_request_duration_as_percentage_of_timeout{})),
      _unfinishedSends(metrics.add(arangodb_network_unfinished_sends_total{})),
      _dequeueDurations(metrics.add(arangodb_network_dequeue_duration{})),
      _sendDurations(metrics.add(arangodb_network_send_duration{})),
      _responseDurations(metrics.add(arangodb_network_response_duration{})),
      _compressRequestThreshold(200),
      // note: we cannot use any compression method by default here for the
      // 3.12 release because that could cause upgrades from 3.11 to 3.12
      // to break. for example, if we enable compression here and during the
      // upgrade the 3.12 servers could pick it up and send compressed
      // requests to 3.11 servers which cannot handle them.
      // we should set the compression type "auto" for future releases though
      // to save some traffic.
      _compressionType(CompressionType::kNone),
      _compressionTypeLabel("none"),
      _metrics(metrics) {
  setOptional(true);
  startsAfter<ClusterFeature>();
  startsAfter<SchedulerFeature>();
  startsAfter<ServerFeature>();
  startsAfter<EngineSelectorFeature>();
}

NetworkFeature::~NetworkFeature() {
  if (_pool) {
    _pool->stop();
  }
}

void NetworkFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options->addSection("network", "cluster-internal networking");

  options->addOption(
      "--network.io-threads",
      "The number of network I/O threads for cluster-internal "
      "communication.",
      new UInt32Parameter(&_numIOThreads, /*base*/ 1, /*minValue*/ 1));
  options->addOption(
      "--network.max-open-connections",
      "The maximum number of open TCP connections for "
      "cluster-internal communication per endpoint",
      new UInt64Parameter(&_maxOpenConnections, /*base*/ 1, /*minValue*/ 8));
  options->addOption("--network.idle-connection-ttl",
                     "The default time-to-live of idle connections for "
                     "cluster-internal communication (in milliseconds).",
                     new UInt64Parameter(&_idleTtlMilli));
  options->addOption(
      "--network.verify-hosts",
      "Verify peer certificates when using TLS in cluster-internal "
      "communication.",
      new BooleanParameter(&_verifyHosts));

  std::unordered_set<std::string> protos = {"", "http", "http2", "h2"};

  // starting with 3.9, we will hard-code the protocol for cluster-internal
  // communication
  options
      ->addOption(
          "--network.protocol",
          "The network protocol to use for cluster-internal communication.",
          new DiscreteValuesParameter<StringParameter>(&_protocol, protos),
          options::makeDefaultFlags(options::Flags::Uncommon))
      .setDeprecatedIn(30900);

  options
      ->addOption("--network.max-requests-in-flight",
                  "The number of internal requests that can be in "
                  "flight at a given point in time.",
                  new options::UInt64Parameter(&_maxInFlight),
                  options::makeDefaultFlags(options::Flags::Uncommon))
      .setIntroducedIn(30800);

  options
      ->addOption("--network.compress-request-threshold",
                  "The HTTP request body size from which on cluster-internal "
                  "requests are transparently compressed.",
                  new UInt64Parameter(&_compressRequestThreshold),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnCoordinator))
      .setIntroducedIn(31200)
      .setLongDescription(
          R"(Automatically compress outgoing HTTP requests in cluster-internal
traffic with the deflate, gzip or lz4 compression format.
Compression will only happen if the size of the uncompressed request body exceeds 
the threshold value controlled by this startup option,
and if the request body size after compression is less than the original 
request body size.
Using the value 0 disables the automatic compression.")");

  std::unordered_set<std::string> types = {
      StaticStrings::EncodingGzip, StaticStrings::EncodingDeflate,
      StaticStrings::EncodingLz4, "auto", "none"};
  options
      ->addOption("--network.compression-method",
                  "The compression method used for cluster-internal requests.",
                  new DiscreteValuesParameter<StringParameter>(
                      &_compressionTypeLabel, types),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnCoordinator))
      .setIntroducedIn(31200)
      .setLongDescription(
          R"(Setting this option to 'none' will disable compression for
cluster-internal requests.
To enable compression for cluster-internal requests, set this option to either
'deflate', 'gzip', 'lz4' or 'auto'.
The 'deflate' and 'gzip' compression methods are general purpose, 
but have significant CPU overhead for performing the compression work. 
The 'lz4' compression method compresses slightly worse, but has a lot lower 
CPU overhead for performing the compression.
The 'auto' compression method will use 'deflate' by default, and 'lz4' for
requests which have a size that is at least 3 times the configured threshold
size.
The compression method only matters if `--network.compress-request-threshold`
is set to value greater than zero. If the threshold is set to value of 0,
then no compression will be performed.)");
}

void NetworkFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> opts) {
  if (!opts->processingResult().touched("--network.idle-connection-ttl")) {
    auto& gs = server().getFeature<GeneralServerFeature>();
    _idleTtlMilli = uint64_t(gs.keepAliveTimeout() * 1000 / 2);
  }
  if (_idleTtlMilli < 10000) {
    _idleTtlMilli = 10000;
  }

  uint64_t clamped =
      std::clamp(_maxInFlight, ::MinAllowedInFlight, ::MaxAllowedInFlight);
  if (clamped != _maxInFlight) {
    LOG_TOPIC("38cd1", WARN, Logger::CONFIG)
        << "Must set --network.max-requests-in-flight between "
        << ::MinAllowedInFlight << " and " << ::MaxAllowedInFlight
        << ", clamping value";
    _maxInFlight = clamped;
  }

  if (_compressionTypeLabel == StaticStrings::EncodingGzip) {
    _compressionType = CompressionType::kGzip;
  } else if (_compressionTypeLabel == StaticStrings::EncodingDeflate) {
    _compressionType = CompressionType::kDeflate;
  } else if (_compressionTypeLabel == StaticStrings::EncodingLz4) {
    _compressionType = CompressionType::kLz4;
  } else if (_compressionTypeLabel == "auto") {
    _compressionType = CompressionType::kAuto;
  } else if (_compressionTypeLabel == "none") {
    _compressionType = CompressionType::kNone;
  } else {
    LOG_TOPIC("339d5", FATAL, Logger::CONFIG)
        << "invalid value for `--network.compression-method` ('"
        << _compressionTypeLabel << "')";
    FATAL_ERROR_EXIT();
  }
}

void NetworkFeature::prepare() {
  ClusterInfo* ci = nullptr;
  if (server().hasFeature<ClusterFeature>() &&
      server().isEnabled<ClusterFeature>()) {
    // in unit tests the ClusterInfo may not be enabled.
    ci = &server().getFeature<ClusterFeature>().clusterInfo();
  }

  network::ConnectionPool::Config config;
  config.numIOThreads = static_cast<unsigned>(_numIOThreads);
  config.maxOpenConnections = _maxOpenConnections;
  config.idleConnectionMilli = _idleTtlMilli;
  config.verifyHosts = _verifyHosts;
  config.clusterInfo = ci;
  config.name = "ClusterComm";
  config.metrics = network::ConnectionPool::Metrics::fromMetricsFeature(
      _metrics, config.name);

  // using an internal network protocol other than HTTP/1 is
  // not supported since 3.9. the protocol is always hard-coded
  // to HTTP/1 from now on.
  // note: we plan to upgrade the internal protocol to HTTP/2 at
  // some point in the future
  config.protocol = fuerte::ProtocolType::Http;
  if (_protocol == "http" || _protocol == "h1") {
    config.protocol = fuerte::ProtocolType::Http;
  } else if (_protocol == "http2" || _protocol == "h2") {
    config.protocol = fuerte::ProtocolType::Http2;
  }

  if (config.protocol != fuerte::ProtocolType::Http) {
    LOG_TOPIC("6d221", WARN, Logger::CONFIG)
        << "using `--network.protocol` is deprecated. "
        << "the network protocol for cluster-internal requests is hard-coded "
           "to HTTP/1 in this version";
    config.protocol = fuerte::ProtocolType::Http;
  }

  _pool = std::make_unique<network::ConnectionPool>(config);
  _poolPtr.store(_pool.get(), std::memory_order_relaxed);

  _gcfunc = [this, ci](bool canceled) {
    if (canceled) {
      return;
    }

    _pool->pruneConnections();

    if (ci != nullptr) {
      auto failed = ci->getFailedServers();
      for (ServerID const& srvId : failed) {
        std::string endpoint = ci->getServerEndpoint(srvId);
        size_t n = _pool->cancelConnections(endpoint);
        LOG_TOPIC_IF("15d94", INFO, Logger::COMMUNICATION, n > 0)
            << "canceling " << n << " connection(s) to failed server '" << srvId
            << "' on endpoint '" << endpoint << "'";
      }
    }

    if (!server().isStopping() && !canceled) {
      std::chrono::seconds off(12);
      ::queueGarbageCollection(_workItemMutex, _workItem, _gcfunc, off);
    }
  };

  _prepared = true;
}

void NetworkFeature::start() {
  _retryThread = std::make_unique<RetryThread>(server());
  if (!_retryThread->start()) {
    LOG_TOPIC("9b1a2", FATAL, arangodb::Logger::COMMUNICATION)
        << "unable to start network request retry thread";
    FATAL_ERROR_EXIT();
  }

  Scheduler* scheduler = SchedulerFeature::SCHEDULER;
  if (scheduler != nullptr) {  // is nullptr in unit tests
    auto off = std::chrono::seconds(1);
    ::queueGarbageCollection(_workItemMutex, _workItem, _gcfunc, off);
  }
}

void NetworkFeature::beginShutdown() {
  cancelGarbageCollection();
  if (_retryThread) {
    _retryThread->beginShutdown();
  }
  _poolPtr.store(nullptr, std::memory_order_relaxed);
  if (_pool) {  // first cancel all connections
    _pool->shutdownConnections();
  }
}

void NetworkFeature::stop() {
  cancelGarbageCollection();
  if (_pool) {
    _pool->shutdownConnections();
    _pool->drainConnections();
    _pool->stop();
  }
  _retryThread.reset();
}

void NetworkFeature::unprepare() { cancelGarbageCollection(); }

void NetworkFeature::cancelGarbageCollection() noexcept try {
  std::lock_guard<std::mutex> guard(_workItemMutex);
  _workItem.reset();
} catch (std::exception const& ex) {
  LOG_TOPIC("2b843", WARN, Logger::COMMUNICATION)
      << "caught exception while canceling retry requests: " << ex.what();
}

network::ConnectionPool* NetworkFeature::pool() const noexcept {
  return _poolPtr.load(std::memory_order_relaxed);
}

#ifdef ARANGODB_USE_GOOGLE_TESTS
void NetworkFeature::setPoolTesting(network::ConnectionPool* pool) {
  _poolPtr.store(pool, std::memory_order_release);
}
#endif

bool NetworkFeature::prepared() const noexcept { return _prepared; }

void NetworkFeature::trackForwardedRequest() noexcept { ++_forwardedRequests; }

std::size_t NetworkFeature::requestsInFlight() const noexcept {
  return _requestsInFlight.load();
}

bool NetworkFeature::isCongested() const noexcept {
  return _requestsInFlight.load() >= (_maxInFlight * ::CongestionRatio);
}

bool NetworkFeature::isSaturated() const noexcept {
  return _requestsInFlight.load() >= _maxInFlight;
}

uint64_t NetworkFeature::defaultIOThreads() {
  return std::max(uint64_t(1), uint64_t(NumberOfCores::getValue() / 4));
}

void NetworkFeature::sendRequest(network::ConnectionPool& pool,
                                 network::RequestOptions const& options,
                                 std::string const& endpoint,
                                 std::unique_ptr<fuerte::Request>&& req,
                                 RequestCallback&& cb) {
  TRI_ASSERT(req != nullptr);

  injectAcceptEncodingHeader(*req);

  bool didCompress = compressRequestBody(options, *req);

  prepareRequest(pool, req);

  bool isFromPool = false;
  auto now = std::chrono::steady_clock::now();
  auto conn = pool.leaseConnection(endpoint, isFromPool);
  auto dur = std::chrono::steady_clock::now() - now;
  if (dur > std::chrono::seconds(1)) {
    LOG_TOPIC("52418", WARN, Logger::COMMUNICATION)
        << "have leased connection to '" << endpoint
        << "' came from pool: " << isFromPool << " leasing took "
        << std::chrono::duration_cast<std::chrono::duration<double>>(dur)
               .count()
        << " seconds, url: " << to_string(req->header.restVerb) << " "
        << req->header.path << ", request ptr: " << (void*)req.get();
  } else {
    LOG_TOPIC("52417", TRACE, Logger::COMMUNICATION)
        << "have leased connection to '" << endpoint
        << "' came from pool: " << isFromPool
        << ", url: " << to_string(req->header.restVerb) << " "
        << req->header.path << ", request ptr: " << (void*)req.get();
  }
  conn->sendRequest(
      std::move(req),
      [this, &pool, isFromPool,
       handleContentEncoding = options.handleContentEncoding || didCompress,
       cb = std::move(cb), endpoint = std::move(endpoint)](
          fuerte::Error err, std::unique_ptr<fuerte::Request> req,
          std::unique_ptr<fuerte::Response> res) {
        TRI_ASSERT(req != nullptr);
        if (req->timeQueued().time_since_epoch().count() != 0 &&
            req->timeAsyncWrite().time_since_epoch().count() != 0) {
          // In the 0 cases fuerte did not even accept or start to send
          // the request, so there is nothing to report.
          auto dur = std::chrono::duration_cast<std::chrono::duration<double>>(
              req->timeAsyncWrite() - req->timeQueued());
          _dequeueDurations.count(dur.count());
          if (req->timeSent().time_since_epoch().count() == 0) {
            // The request sending was never finished. This could be a timeout
            // during the sending phase.
            LOG_TOPIC("effc3", DEBUG, Logger::COMMUNICATION)
                << "Time to dequeue request to " << endpoint << ": "
                << dur.count()
                << " seconds, however, the sending has not yet finished so far"
                << ", endpoint: " << endpoint
                << ", request ptr: " << (void*)req.get()
                << ", response ptr: " << (void*)res.get()
                << ", error: " << uint16_t(err);
            _unfinishedSends.count();
          } else {
            // The request was fully sent off, we have received the callback
            // from asio.
            dur = std::chrono::duration_cast<std::chrono::duration<double>>(
                req->timeSent() - req->timeAsyncWrite());
            _sendDurations.count(dur.count());
            // If you suspect network delays in your infrastructure, you
            // can use the following log message to track them down and
            // to associate them with particular requests:
            LOG_TOPIC_IF("effc4", DEBUG, Logger::COMMUNICATION,
                         dur > std::chrono::seconds(3))
                << "Time to send request to " << endpoint << ": " << dur.count()
                << " seconds, endpoint: " << endpoint
                << ", request ptr: " << (void*)req.get()
                << ", response ptr: " << (void*)res.get()
                << ", error: " << uint16_t(err);

            dur = std::chrono::duration_cast<std::chrono::duration<double>>(
                std::chrono::steady_clock::now() - req->timeSent());
            // If you suspect network delays in your infrastructure, you
            // can use the following log message to track them down and
            // to associate them with particular requests:
            LOG_TOPIC_IF("effc5", DEBUG, Logger::COMMUNICATION,
                         dur > std::chrono::seconds(61))
                << "Time since request was sent out to " << endpoint
                << " until now was " << dur.count()
                << " seconds, endpoint: " << endpoint
                << ", request ptr: " << (void*)req.get()
                << ", response ptr: " << (void*)res.get()
                << ", error: " << uint16_t(err);
            _responseDurations.count(dur.count());
          }
        }
        finishRequest(pool, err, req, res);
        if (res != nullptr && handleContentEncoding) {
          // transparently handle decompression
          auto const& encoding =
              res->header.metaByKey(StaticStrings::ContentEncoding);
          if (encoding == StaticStrings::EncodingGzip) {
            velocypack::Buffer<uint8_t> uncompressed;
            auto r = encoding::gzipUncompress(
                reinterpret_cast<uint8_t const*>(res->payload().data()),
                res->payload().size(), uncompressed);
            if (r != TRI_ERROR_NO_ERROR) {
              THROW_ARANGO_EXCEPTION(r);
            }
            // replace response body and remove "content-encoding"
            // handler to prevent duplicate uncompression
            res->setPayload(std::move(uncompressed), 0);
            res->header.contentEncoding(fuerte::ContentEncoding::Identity);
            res->header.removeMeta(StaticStrings::ContentEncoding);
          } else if (encoding == StaticStrings::EncodingDeflate) {
            velocypack::Buffer<uint8_t> uncompressed;
            auto r = encoding::zlibInflate(
                reinterpret_cast<uint8_t const*>(res->payload().data()),
                res->payload().size(), uncompressed);
            if (r != TRI_ERROR_NO_ERROR) {
              THROW_ARANGO_EXCEPTION(r);
            }
            // replace response body and remove "content-encoding"
            // handler to prevent duplicate uncompression
            res->setPayload(std::move(uncompressed), 0);
            res->header.contentEncoding(fuerte::ContentEncoding::Identity);
            res->header.removeMeta(StaticStrings::ContentEncoding);
          } else if (encoding == StaticStrings::EncodingArangoLz4) {
            velocypack::Buffer<uint8_t> uncompressed;
            auto r = encoding::lz4Uncompress(
                reinterpret_cast<uint8_t const*>(res->payload().data()),
                res->payload().size(), uncompressed);
            if (r != TRI_ERROR_NO_ERROR) {
              THROW_ARANGO_EXCEPTION(r);
            }
            // replace response body and remove "content-encoding"
            // handler to prevent duplicate uncompression
            res->setPayload(std::move(uncompressed), 0);
            res->header.contentEncoding(fuerte::ContentEncoding::Identity);
            res->header.removeMeta(StaticStrings::ContentEncoding);
          }
        }
        cb(err, std::move(req), std::move(res), isFromPool);
      });
}

void NetworkFeature::prepareRequest(network::ConnectionPool const& pool,
                                    std::unique_ptr<fuerte::Request>& req) {
  _requestsInFlight += 1;
  if (req) {
    req->timestamp(std::chrono::steady_clock::now());
  }
}

void NetworkFeature::finishRequest(network::ConnectionPool const& pool,
                                   fuerte::Error err,
                                   std::unique_ptr<fuerte::Request> const& req,
                                   std::unique_ptr<fuerte::Response>& res) {
  _requestsInFlight -= 1;
  if (err == fuerte::Error::RequestTimeout) {
    _requestTimeouts.count();
  } else if (req && res) {
    res->timestamp(std::chrono::steady_clock::now());
    std::chrono::milliseconds duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(res->timestamp() -
                                                              req->timestamp());
    std::chrono::milliseconds timeout = req->timeout();
    TRI_ASSERT(timeout.count() > 0);
    if (timeout.count() > 0) {
      // only go in here if we are sure to not divide by zero
      double percentage =
          std::clamp(100.0 * static_cast<double>(duration.count()) /
                         static_cast<double>(timeout.count()),
                     0.0, 100.0);
      _requestDurations.count(percentage);
    } else {
      // the timeout value was 0, for whatever reason. this is unexpected,
      // but we must not make the program crash here.
      // so instead log a warning and interpret this as a request that took
      // 100% of the timeout duration.
      _requestDurations.count(100.0);
      LOG_TOPIC("1688c", WARN, Logger::FIXME)
          << "encountered invalid 0s timeout for internal request to path "
          << req->header.path;
    }
  }
}

void NetworkFeature::retryRequest(
    std::shared_ptr<network::RetryableRequest> req, RequestLane /*lane*/,
    std::chrono::steady_clock::duration duration) {
  if (!req) {
    return;
  }

  if (server().isStopping()) {
    try {
      req->cancel();
    } catch (...) {
      // does not matter if we are stopping anyway
    }
  } else {
    TRI_ASSERT(_retryThread);
    static_cast<RetryThread&>(*_retryThread)
        .push(std::move(req), std::chrono::steady_clock::now() + duration);
  }
}

void NetworkFeature::injectAcceptEncodingHeader(fuerte::Request& req) const {
  // inject "Accept-Encoding" header into the request if it was not
  // already set.
  if (req.header.meta().contains(StaticStrings::AcceptEncoding)) {
    // header already set in original request
    return;
  }

  if (_compressionType == CompressionType::kNone) {
    return;
  }

  if (_compressionType == CompressionType::kDeflate) {
    // if cluster-internal compression type is set to "deflate", add
    // "accept-encoding: deflate" header
    req.header.addMeta(StaticStrings::AcceptEncoding,
                       StaticStrings::EncodingDeflate);
  } else if (_compressionType == CompressionType::kGzip) {
    // if cluster-internal compression type is set to "gzip", add
    // "accept-encoding: gzip, deflate" header. we leave "deflate" in
    // as a general fallback
    req.header.addMeta(StaticStrings::AcceptEncoding,
                       absl::StrCat(StaticStrings::EncodingGzip, ",",
                                    StaticStrings::EncodingDeflate));
  } else if (_compressionType == CompressionType::kLz4 ||
             _compressionType == CompressionType::kAuto) {
    // if cluster-internal compression type is set to "lz4", add
    // "accept-encoding: lz4, deflate" header. we leave "deflate" in
    // as a general fallback
    req.header.addMeta(StaticStrings::AcceptEncoding,
                       absl::StrCat(StaticStrings::EncodingArangoLz4, ",",
                                    StaticStrings::EncodingDeflate));
  } else {
    TRI_ASSERT(false);
  }
}

bool NetworkFeature::compressRequestBody(network::RequestOptions const& opts,
                                         fuerte::Request& req) const {
  if (!opts.allowCompression) {
    // compression explicitly disallowed
    return false;
  }

  uint64_t threshold = _compressRequestThreshold;
  if (threshold == 0) {
    // opted out of compression by configuration
    return false;
  }

  if (_compressionType == CompressionType::kNone) {
    return false;
  }

  if (req.header.meta().contains(StaticStrings::ContentEncoding)) {
    // Content-Encoding already set. better not overwrite it
    return false;
  }

  auto& pfm = req.payloadForModification();
  size_t bodySize = pfm.size();
  if (bodySize < threshold) {
    // request body size too small for compression
    return false;
  }

  auto compressionType = _compressionType;

  if (compressionType == CompressionType::kAuto) {
    // "auto" compression means that we will pick deflate for all
    // requests that exceed the threshold size, and lz4 for all
    // requests with substantially larger ones
    compressionType = CompressionType::kDeflate;
    if (bodySize >= threshold * 3) {
      compressionType = CompressionType::kLz4;
    }
  }

  TRI_ASSERT(compressionType != CompressionType::kNone);
  TRI_ASSERT(compressionType != CompressionType::kAuto);

  velocypack::Buffer<uint8_t> compressed;
  if (compressionType == CompressionType::kDeflate) {
    if (encoding::zlibDeflate(pfm.data(), pfm.size(), compressed) !=
        TRI_ERROR_NO_ERROR) {
      return false;
    }

    if (compressed.size() >= bodySize) {
      // compression did not provide any benefit. better leave it
      return false;
    }

    pfm = std::move(compressed);
    req.header.addMeta(StaticStrings::ContentEncoding,
                       StaticStrings::EncodingDeflate);
  } else if (compressionType == CompressionType::kGzip) {
    if (encoding::gzipCompress(pfm.data(), pfm.size(), compressed) !=
        TRI_ERROR_NO_ERROR) {
      return false;
    }

    if (compressed.size() >= bodySize) {
      // compression did not provide any benefit. better leave it
      return false;
    }

    pfm = std::move(compressed);
    req.header.addMeta(StaticStrings::ContentEncoding,
                       StaticStrings::EncodingGzip);
  } else if (compressionType == CompressionType::kLz4) {
    if (encoding::lz4Compress(pfm.data(), pfm.size(), compressed) !=
        TRI_ERROR_NO_ERROR) {
      return false;
    }

    if (compressed.size() >= bodySize) {
      // compression did not provide any benefit. better leave it
      return false;
    }

    pfm = std::move(compressed);
    req.header.addMeta(StaticStrings::ContentEncoding,
                       StaticStrings::EncodingArangoLz4);
  } else {
    // unknown compression type
    return false;
  }

  return true;
}

}  // namespace arangodb
