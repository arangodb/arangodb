////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#include "Basics/ConditionLocker.h"

#include "Agency/NotifierThread.h"
#include "Agency/NotifyCallback.h"
#include "Cluster/ClusterComm.h"
#include "VocBase/server.h"

using namespace arangodb::consensus;

NotifierThread::NotifierThread(const std::string& path,
                               std::shared_ptr<VPackBuilder> body,
                               const std::vector<std::string>& endpoints)
    : Thread("AgencyNotification"),
      _path(path),
      _body(body),
      _endpoints(endpoints) {}

void NotifierThread::scheduleNotification(const std::string& endpoint) {
  LOG_TOPIC(DEBUG, Logger::AGENCY)
    << "Scheduling " << endpoint << _path << " " << _body->toJson()
    << " " << endpoint;

  auto cb = [this, endpoint](bool result) {
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "Agencynotification for " << endpoint
               << ". Result: " << result;

    CONDITION_LOCKER(guard, _cv);
    _openResults.emplace_back(NotificationResult({result, endpoint}));
    _cv.signal();
  };

  auto headerFields =
      std::make_unique<std::unordered_map<std::string, std::string>>();

  // This is best effort: We do not guarantee at least once delivery!
  arangodb::ClusterComm::instance()->asyncRequest(
      "", TRI_NewTickServer(), endpoint, GeneralRequest::RequestType::POST,
      _path, std::make_shared<std::string>(_body->toJson()), headerFields,
      std::make_shared<NotifyCallback>(cb), 5.0, true);
}

bool NotifierThread::start() { return Thread::start(); }

void NotifierThread::run() {
  try {
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "Starting Agencynotifications";
    // mop: locker necessary because if scheduledNotifications may return
    // earlier than this thread reaching the while
    CONDITION_LOCKER(locker, _cv);
    size_t numEndpoints = _endpoints.size();
    for (auto& endpoint : _endpoints) {
      scheduleNotification(endpoint);
    }

    while (numEndpoints > 0) {
      LOG_TOPIC(DEBUG, Logger::AGENCY) << "WAITING " << numEndpoints;
      locker.wait();
      if (isStopping()) {
        LOG_TOPIC(DEBUG, Logger::AGENCY) << "Agencynotifications stopping";
        break;
      }

      for (auto& result : _openResults) {
        if (result.success) {
          numEndpoints--;
        } else {
          // mop: this is totally suboptimal under the lock but
          // we don't want to commence a http bombardment
          usleep(50000);
          scheduleNotification(result.endpoint);
        }
      }
      _openResults.clear();
    }
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "Agencynotifications done";
  } catch (std::exception& e) {
    LOG_TOPIC(ERR, Logger::AGENCY) << "Couldn't notify agents: " << e.what();
  } catch (...) {
    LOG_TOPIC(ERR, Logger::AGENCY) << "Couldn't notify agents!";
  }

}

void NotifierThread::beginShutdown() {
  Thread::beginShutdown();
  CONDITION_LOCKER(locker, _cv);
  _cv.signal();
}

// Shutdown if not already
NotifierThread::~NotifierThread() { shutdown(); }
