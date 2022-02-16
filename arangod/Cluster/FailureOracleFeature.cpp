///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Alex Petenchea
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include "FailureOracleFeature.h"

#include "Agency/TimeString.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/application-exit.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/AgencyCallback.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/FailureOracle.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <shared_mutex>

namespace arangodb::cluster {

namespace {
constexpr auto kSupervisionHealthPath = "Supervision/Health";
constexpr auto kHealthyServerKey = "Status";
constexpr auto HealthyServerValue = "GOOD";
}  // namespace

class FailureOracleImpl final
    : public IFailureOracle,
      public std::enable_shared_from_this<FailureOracleImpl> {
 public:
  explicit FailureOracleImpl(ClusterFeature&);

  auto isServerFailed(std::string_view const serverId) const noexcept
      -> bool override {
    std::shared_lock readLock(_mutex);
    if (auto status = _isFailed.find(std::string(serverId));
        status != std::end(_isFailed)) {
      return status->second;
    }
    return true;
  }

  void start();
  void stop();
  auto getStatus() -> FailureOracleFeature::Status;
  void reload(VPackSlice const& result);
  void flush();
  void scheduleFlush() noexcept;

  template<typename Server>
  void createAgencyCallback(Server& server);

 private:
  mutable std::shared_mutex _mutex;
  FailureOracleFeature::FailureMap _isFailed;
  std::shared_ptr<AgencyCallback> _agencyCallback;
  Scheduler::WorkHandle _flushJob;
  ClusterFeature& _clusterFeature;
  std::atomic_bool _is_running;
  std::chrono::system_clock::time_point _lastUpdated;
};

FailureOracleImpl::FailureOracleImpl(ClusterFeature& _clusterFeature)
    : _clusterFeature{_clusterFeature} {};

void FailureOracleImpl::start() {
  _is_running.store(true);

  auto agencyCallbackRegistry = _clusterFeature.agencyCallbackRegistry();
  if (!agencyCallbackRegistry) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "Expected non-null AgencyCallbackRegistry "
                                   "while starting FailureOracle.");
  }
  Result res = agencyCallbackRegistry->registerCallback(_agencyCallback, true);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  scheduleFlush();
}

void FailureOracleImpl::stop() {
  _is_running.store(false);

  try {
    auto agencyCallbackRegistry = _clusterFeature.agencyCallbackRegistry();
    if (agencyCallbackRegistry) {
      agencyCallbackRegistry->unregisterCallback(_agencyCallback);
    }
  } catch (std::exception const& ex) {
    LOG_TOPIC("42bf2", WARN, Logger::CLUSTER)
        << "Caught unexpected exception while unregistering agency callback "
           "for FailureOracleImpl: "
        << ex.what();
  }

  _flushJob->cancel();
}

auto FailureOracleImpl::getStatus() -> FailureOracleFeature::Status {
  std::shared_lock readLock(_mutex);
  return FailureOracleFeature::Status{.isFailed = _isFailed,
                                      .lastUpdated = _lastUpdated};
}

void FailureOracleImpl::reload(const VPackSlice& result) {
  FailureOracleFeature::FailureMap isFailed;
  for (auto [key, value] : VPackObjectIterator(result)) {
    auto serverId = key.copyString();
    auto isServerGood =
        value.get(kHealthyServerKey).isEqualString(HealthyServerValue);
    isFailed[serverId] = !isServerGood;
  }
  std::unique_lock writeLock(_mutex);
  _isFailed = std::move(isFailed);
  _lastUpdated = std::chrono::system_clock::now();
}

void FailureOracleImpl::flush() {
  if (!_is_running.load()) {
    return;
  }
  std::shared_ptr<VPackBuilder> builder;
  AgencyCache& agencyCache = _clusterFeature.agencyCache();
  std::tie(builder, std::ignore) = agencyCache.get(kSupervisionHealthPath);
  if (auto result = builder->slice(); !result.isNone()) {
    TRI_ASSERT(result.isObject())
        << " expected object in agency at " << kSupervisionHealthPath
        << " but got " << result.toString();
    reload(result);
  }
}

void FailureOracleImpl::scheduleFlush() noexcept {
  using namespace std::chrono_literals;

  auto scheduler = SchedulerFeature::SCHEDULER;
  if (!scheduler) {
    return;
  }

  _flushJob = scheduler->queueDelayed(
      RequestLane::AGENCY_CLUSTER, 50s,
      [weak = weak_from_this()](bool canceled) {
        auto self = weak.lock();
        if (self && !canceled && self->_is_running.load()) {
          try {
            self->flush();
          } catch (std::exception& ex) {
            LOG_TOPIC("42bf3", WARN, Logger::CLUSTER)
                << "Exception while flushing the failure oracle " << ex.what();
            FATAL_ERROR_EXIT();
          }
          self->scheduleFlush();
        }
      });
}

template<typename Server>
void FailureOracleImpl::createAgencyCallback(Server& server) {
  TRI_ASSERT(_agencyCallback == nullptr);
  _agencyCallback = std::make_shared<AgencyCallback>(
      server, kSupervisionHealthPath,
      [weak = weak_from_this()](VPackSlice const& result) {
        auto self = weak.lock();
        if (self && !result.isNone()) {
          TRI_ASSERT(result.isObject())
              << " expected object in agency at " << kSupervisionHealthPath
              << " but got " << result.toString();
          self->reload(result);
        }
        return true;
      },
      true, true);
}

FailureOracleFeature::FailureOracleFeature(Server& server)
    : ArangodFeature{server, *this} {
  setOptional(true);
  startsAfter<SchedulerFeature>();
  startsAfter<ClusterFeature>();
}

void FailureOracleFeature::prepare() {
  bool const enabled = ServerState::instance()->isCoordinator() ||
                       ServerState::instance()->isDBServer();
  setEnabled(enabled);
}

void FailureOracleFeature::start() {
  TRI_ASSERT(_cache == nullptr);
  _cache = std::make_shared<FailureOracleImpl>(
      server().getEnabledFeature<ClusterFeature>());
  _cache->createAgencyCallback(server());
  _cache->start();

  LOG_TOPIC("42af3", DEBUG, Logger::CLUSTER) << "FailureOracleFeature is ready";
}

void FailureOracleFeature::stop() { _cache->stop(); }

auto FailureOracleFeature::status() -> Status { return _cache->getStatus(); }

void FailureOracleFeature::flush() { _cache->flush(); }

auto FailureOracleFeature::getFailureOracle()
    -> std::shared_ptr<IFailureOracle> {
  return std::static_pointer_cast<IFailureOracle>(_cache);
}

void FailureOracleFeature::Status::toVelocyPack(
    velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add("lastUpdated", VPackValue(timepointToString(lastUpdated)));
  {
    VPackObjectBuilder ob2(&builder, "isFailed");
    for (auto const& [pid, status] : isFailed) {
      builder.add(pid, VPackValue(status));
    }
  }
}

}  // namespace arangodb::cluster
