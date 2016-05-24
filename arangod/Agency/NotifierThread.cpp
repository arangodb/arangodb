#include "Basics/ConditionLocker.h"

#include "Agency/NotifierThread.h"
#include "Agency/NotifyCallback.h"
#include "Cluster/ClusterComm.h"
#include "VocBase/server.h"

using namespace arangodb::consensus;

NotifierThread::NotifierThread(const std::string& path,
                               std::shared_ptr<VPackBuilder> body,
                               const std::vector<std::string>& endpoints)
    : Thread("AgencyNotifiaction"),
      _path(path),
      _body(body),
      _endpoints(endpoints) {}

void NotifierThread::scheduleNotification(const std::string& endpoint) {
  LOG(DEBUG) << "Scheduling " << endpoint << _path << " " << _body->toJson()
             << " " << endpoint;

  auto cb = [this, endpoint](bool result) {
    LOG(DEBUG) << "Agencynotification for " << endpoint
               << ". Result: " << result;

    CONDITION_LOCKER(guard, _cv);
    _openResults.emplace_back(NotificationResult({result, endpoint}));
    _cv.signal();
  };

  auto headerFields =
      std::make_unique<std::unordered_map<std::string, std::string>>();

  while (true) {
    auto res = arangodb::ClusterComm::instance()->asyncRequest(
        "", TRI_NewTickServer(), endpoint, GeneralRequest::RequestType::POST,
        _path, std::make_shared<std::string>(_body->toJson()), headerFields,
        std::make_shared<NotifyCallback>(cb), 5.0, true);

    if (res.status == CL_COMM_SUBMITTED) {
      break;
    }
    usleep(500000);
  }
}

bool NotifierThread::start() { return Thread::start(); }

void NotifierThread::run() {
  try {
    LOG(DEBUG) << "Starting Agencynotifications";
    // mop: locker necessary because if scheduledNotifications may return
    // earlier than this thread reaching the while
    CONDITION_LOCKER(locker, _cv);
    size_t numEndpoints = _endpoints.size();
    for (auto& endpoint : _endpoints) {
      scheduleNotification(endpoint);
    }

    while (numEndpoints > 0) {
      LOG(DEBUG) << "WAITING " << numEndpoints;
      locker.wait();
      if (isStopping()) {
        LOG(DEBUG) << "Agencynotifications stopping";
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
    LOG(DEBUG) << "Agencynotifications done";
  } catch (std::exception& e) {
    LOG(ERR) << "Couldn't notify agents: " << e.what();
  } catch (...) {
    LOG(ERR) << "Couldn't notify agents!";
  }
}

void NotifierThread::beginShutdown() {
  Thread::beginShutdown();
  CONDITION_LOCKER(locker, _cv);
  _cv.signal();
}

// Shutdown if not already
NotifierThread::~NotifierThread() { shutdown(); }
