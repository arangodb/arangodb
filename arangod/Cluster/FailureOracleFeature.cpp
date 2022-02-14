////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
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

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/ApplicationFeature.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/AgencyCallback.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/FailureOracle.h"
#include "Scheduler/SchedulerFeature.h"
#include "Logger/LogMacros.h"

#include <chrono>
#include <memory>
#include <shared_mutex>
#include <unordered_map>
// TODO remove LOG_DEVEL

namespace arangodb::cluster {

namespace {
constexpr auto kSupervisionHealthPath = "Supervision/Health";
constexpr auto kHealthyServerKey = "Status";
constexpr auto HealthyServerValue = "GOOD";

using FailureMap = std::unordered_map<std::string, bool>;
}  // namespace

class FailureOracleImpl final
    : public IFailureOracle,
      public std::enable_shared_from_this<FailureOracleImpl> {
 public:
  FailureOracleImpl() = default;
  ~FailureOracleImpl() override = default;

  auto isServerFailed(std::string_view const serverId) const noexcept
      -> bool override {
    std::shared_lock readLock(_mutex);
    if (auto status = _isFailed.find(std::string(serverId));
        status != std::end(_isFailed)) [[likely]] {
      return status->second;
    }
    return true;
  }

  void setAgencyCallback(std::shared_ptr<AgencyCallback> callback) {
    TRI_ASSERT(_agencyCallback == nullptr);
    _agencyCallback = std::move(callback);
  }

  void start(AgencyCallbackRegistry* agencyCallbackRegistry);
  void stop(AgencyCallbackRegistry* agencyCallbackRegistry);
  auto getIsFailed() -> FailureMap;
  void reset(FailureMap newMap);

  template<typename Server>
  void createAgencyCallback(Server& server);

 private:
  mutable std::shared_mutex _mutex;
  FailureMap _isFailed;
  std::shared_ptr<AgencyCallback> _agencyCallback;
};

void FailureOracleImpl::start(AgencyCallbackRegistry* agencyCallbackRegistry) {
  Result res = agencyCallbackRegistry->registerCallback(_agencyCallback, true);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

void FailureOracleImpl::stop(AgencyCallbackRegistry* agencyCallbackRegistry) {
  try {
    agencyCallbackRegistry->unregisterCallback(_agencyCallback);
  } catch (std::exception const& ex) {
    LOG_TOPIC("42bf2", WARN, Logger::REPLICATION2)
        << "Caught unexpected exception while unregistering agency callback "
           "for FailureOracleImpl: "
        << ex.what();
  }
}

auto FailureOracleImpl::getIsFailed() -> FailureMap {
  FailureMap status;
  std::shared_lock readLock(_mutex);
  for (auto const& [pid, value] : _isFailed) {
    status[pid] = value;
  }
  return status;
}

void FailureOracleImpl::reset(FailureMap newMap) {
  std::unique_lock writeLock(_mutex);
  _isFailed = std::move(newMap);
}

template<typename Server>
void FailureOracleImpl::createAgencyCallback(Server& server) {
  setAgencyCallback(std::make_shared<AgencyCallback>(
      server, kSupervisionHealthPath,
      [weak = weak_from_this()](VPackSlice const& result) {
        LOG_DEVEL << "FailureOracleFeature agencyCallback called";
        auto self = weak.lock();
        if (self && !result.isNone()) {
          TRI_ASSERT(result.isObject())
              << " expected object in agency at " << kSupervisionHealthPath
              << " but got " << result.toString();
          std::unique_lock writeLock(self->_mutex);
          for (auto [key, value] : VPackObjectIterator(result)) {
            auto serverId = key.copyString();
            auto isServerGood =
                value.get(kHealthyServerKey).isEqualString(HealthyServerValue);
            self->_isFailed[serverId] = !isServerGood;
            LOG_DEVEL << "Setting " << serverId << " to "
                      << self->_isFailed[serverId];
          }
        }
        return true;
      },
      true, true));
}

FailureOracleFeature::FailureOracleFeature(Server& server)
    : ArangodFeature{server, *this} {
  setOptional(true);
  startsAfter<SchedulerFeature>();
  startsAfter<ClusterFeature>();
}

void FailureOracleFeature::prepare() {
  if (ServerState::instance()->isAgent()) {
    disable();
  } else {
    enable();
  }
}

void FailureOracleFeature::start() {
  LOG_DEVEL << "FailureOracleFeature started";
  auto agencyCallbackRegistry =
      server().getEnabledFeature<ClusterFeature>().agencyCallbackRegistry();
  if (agencyCallbackRegistry == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "Expected non-null AgencyCallbackRegistry "
                                   "when starting FailureOracleFeature.");
  }

  initHealthCache();
  _cache->start(agencyCallbackRegistry);

  scheduleFlush();
  LOG_TOPIC("42af3", DEBUG, Logger::REPLICATION2)
      << "FailureOracleFeature is ready";
}

void FailureOracleFeature::stop() {
  LOG_DEVEL << "FailureOracleFeature stopped";
  try {
    auto agencyCallbackRegistry =
        server().getEnabledFeature<ClusterFeature>().agencyCallbackRegistry();
    _cache->stop(agencyCallbackRegistry);
  } catch (std::exception const& ex) {
    LOG_TOPIC("42af2", WARN, Logger::REPLICATION2)
        << "caught unexpected exception while unregistering agency callback in "
           "FailureOracleFeature: "
        << ex.what();
  }
  if (_flushJob) {
    _flushJob->cancel();
  }
}

auto FailureOracleFeature::status() -> std::unordered_map<std::string, bool> {
  return _cache->getIsFailed();
}

void FailureOracleFeature::flush() {
  LOG_DEVEL << "FailureOracleFeature flushed";
  AgencyCache& agencyCache =
      server().getEnabledFeature<ClusterFeature>().agencyCache();
  std::shared_ptr<VPackBuilder> builder;
  std::tie(builder, std::ignore) = agencyCache.get(kSupervisionHealthPath);
  if (auto result = builder->slice(); !result.isNone()) {
    TRI_ASSERT(result.isObject())
        << " expected object in agency at " << kSupervisionHealthPath
        << " but got " << result.toString();
    FailureMap newMap;
    for (auto const& [key, value] : VPackObjectIterator(result)) {
      auto serverId = key.copyString();
      auto isServerGood =
          value.get(kHealthyServerKey).isEqualString(HealthyServerValue);
      newMap[serverId] = !isServerGood;
    }
    _cache->reset(std::move(newMap));
  }
}

auto FailureOracleFeature::getFailureOracle()
    -> std::shared_ptr<IFailureOracle> {
  return std::static_pointer_cast<IFailureOracle>(_cache);
}

void FailureOracleFeature::initHealthCache() {
  TRI_ASSERT(_cache == nullptr);

  _cache = std::make_shared<FailureOracleImpl>();
  _cache->createAgencyCallback(server());
}

void FailureOracleFeature::scheduleFlush() {
  using namespace std::chrono_literals;

  auto scheduler = SchedulerFeature::SCHEDULER;
  if (!scheduler) {
    return;
  }

  _flushJob = scheduler->queueDelayed(RequestLane::AGENCY_CLUSTER, 50s,
                                      [this](bool canceled) {
                                        if (!canceled) {
                                          this->flush();
                                          this->scheduleFlush();
                                        }
                                      });
}

}  // namespace arangodb::cluster
