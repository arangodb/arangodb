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
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#include "AgencyCallback.h"

#include <chrono>
#include <thread>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include "Agency/AgencyComm.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/ClusterFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

using namespace arangodb;
using namespace arangodb::application_features;

AgencyCallback::AgencyCallback(ArangodServer& server, std::string key,
                               CallbackType cb, bool needsValue,
                               bool needsInitialValue)
    : key(std::move(key)),
      _server(server),
      _cb(std::move(cb)),
      _needsValue(needsValue),
      _needsInitialValue(needsInitialValue) {}

AgencyCallback::AgencyCallback(ArangodServer& server, std::string const& key,
                               std::function<bool(VPackSlice const&)> const& cb,
                               bool needsValue, bool needsInitialValue)
    : AgencyCallback(
          server, key,
          [cb](VPackSlice slice, consensus::index_t) { return cb(slice); },
          needsValue, needsInitialValue) {}

void AgencyCallback::local(bool b) {
  _local = b;
  if (!b) {
    _agency = std::make_unique<AgencyComm>(_server);
  }
}

bool AgencyCallback::local() const { return _local; }

void AgencyCallback::refetchAndUpdate(bool needToAcquireMutex,
                                      bool forceCheck) {
  if (!_needsValue) {
    // no need to pass any value to the callback
    if (needToAcquireMutex) {
      std::lock_guard locker{_cv.mutex};
      executeEmpty();
    } else {
      executeEmpty();
    }
    return;
  }

  VPackSlice result;
  std::shared_ptr<VPackBuilder> builder;
  consensus::index_t idx = 0;
  AgencyCommResult tmp;

  LOG_TOPIC("a6344", TRACE, Logger::CLUSTER)
      << "Refetching and update for " << AgencyCommHelper::path(key);

  if (_local) {
    auto& _cache = _server.getFeature<ClusterFeature>().agencyCache();
    std::tie(builder, idx) =
        _cache.read(std::vector<std::string>{AgencyCommHelper::path(key)});
    result = builder->slice();
    if (!result.isArray()) {
      if (!_server.isStopping()) {
        // only log errors if we are not already shutting down...
        // in case of shutdown this error is somewhat expected
        LOG_TOPIC("ec320", ERR, arangodb::Logger::CLUSTER)
            << "Callback to get agency cache was not successful: "
            << result.toJson();
      }
      return;
    }
  } else {
    TRI_ASSERT(_agency.get() != nullptr);
    tmp = _agency->getValues(key);
    if (!tmp.successful()) {
      if (!_server.isStopping()) {
        // only log errors if we are not already shutting down...
        // in case of shutdown this error is somewhat expected
        LOG_TOPIC("fb402", ERR, arangodb::Logger::CLUSTER)
            << "Callback getValues to agency was not successful: "
            << tmp.errorCode() << " " << tmp.errorMessage();
      }
      return;
    }
    result = tmp.slice();
  }

  std::vector<std::string> kv =
      basics::StringUtils::split(AgencyCommHelper::path(key), '/');
  kv.erase(std::remove(kv.begin(), kv.end(), ""), kv.end());

  auto newData = std::make_shared<VPackBuilder>();
  newData->add(result[0].get(kv));

  auto const callCheckValue = [&] {
    if (_lastSeenIndex < idx) {
      _lastSeenIndex = idx;
      checkValue(std::move(newData), idx, forceCheck);
    }
  };

  if (needToAcquireMutex) {
    std::lock_guard locker{_cv.mutex};
    callCheckValue();
  } else {
    callCheckValue();
  }
}

void AgencyCallback::checkValue(std::shared_ptr<VPackBuilder> newData,
                                consensus::index_t raftIndex, bool forceCheck) {
  // Only called from refetchAndUpdate, we always have the mutex when
  // we get here!
  if (!_lastData || forceCheck ||
      !arangodb::basics::VelocyPackHelper::equal(_lastData->slice(),
                                                 newData->slice(), false)) {
    LOG_TOPIC("2bd14", TRACE, Logger::CLUSTER)
        << "AgencyCallback: Got new value " << newData->slice().typeName()
        << " " << newData->toJson() << " forceCheck=" << forceCheck;
    if (execute(newData->slice(), raftIndex)) {
      _lastData = newData;
    } else {
      LOG_TOPIC("337dc", DEBUG, Logger::CLUSTER)
          << "Callback was not successful for " << newData->toJson();
    }
  }
}

bool AgencyCallback::executeEmpty() {
  // only called from refetchAndUpdate, we always have the mutex when
  // we get here! No value is needed, so this is just a notify.
  // No index available in that case.
  return execute(VPackSlice::noneSlice(), 0);
}

bool AgencyCallback::execute(velocypack::Slice newData,
                             consensus::index_t raftIndex) {
  // only called from refetchAndUpdate, we always have the mutex when
  // we get here!
  LOG_TOPIC("add4e", TRACE, Logger::CLUSTER)
      << "Executing" << (newData.isNone() ? " (empty)" : "");
  try {
    bool result = _cb(newData, raftIndex);
    if (result) {
      _wasSignaled = true;
      _cv.cv.notify_one();
    }
    return result;
  } catch (std::exception const& ex) {
    LOG_TOPIC("1de99", WARN, Logger::CLUSTER)
        << "AgencyCallback execution failed: " << ex.what();
    throw;
  }
}

bool AgencyCallback::executeByCallbackOrTimeout(double maxTimeout) {
  // One needs to acquire the mutex of the condition variable
  // before entering this function!
  if (!_server.isStopping()) {
    if (_wasSignaled) {
      // ok, we have been signaled already, so there is no need to wait at all
      // directly refetch the values
      _wasSignaled = false;
      LOG_TOPIC("67690", DEBUG, Logger::CLUSTER) << "We were signaled already";
      return false;
    }

    // we haven't yet been signaled. so let's wait for a signal or
    // the timeout to occur
    std::unique_lock lock{_cv.mutex, std::adopt_lock};
    auto const cv_status = _cv.cv.wait_for(
        lock, std::chrono::microseconds{
                  static_cast<uint64_t>(maxTimeout * 1000000.0)});
    std::ignore = lock.release();
    if (cv_status == std::cv_status::timeout) {
      LOG_TOPIC("1514e", DEBUG, Logger::CLUSTER)
          << "Waiting done and nothing happended. Refetching to be sure";
      // mop: watches have not triggered during our sleep...recheck to be sure
      refetchAndUpdate(false, true);  // Force a check
      return true;
    }
  }
  return false;
}
