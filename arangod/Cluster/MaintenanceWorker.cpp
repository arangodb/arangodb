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
/// @author Kaveh Vahedipour
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#include <thread>

#include "MaintenanceWorker.h"

#include "Cluster/MaintenanceFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

namespace arangodb {

namespace maintenance {

MaintenanceWorker::MaintenanceWorker(
    arangodb::MaintenanceFeature& feature, int minimalPriorityAllowed,
    std::unordered_set<std::string> const& labels)
    : Thread(feature.server(), "MaintenanceWorker"),
      _feature(feature),
      _curAction(nullptr),
      _loopState(eFIND_ACTION),
      _directAction(false),
      _labels(labels),
      _minimalPriorityAllowed(minimalPriorityAllowed) {}

MaintenanceWorker::MaintenanceWorker(arangodb::MaintenanceFeature& feature,
                                     std::shared_ptr<Action>& directAction)
    : Thread(feature.server(), "MaintenanceWorker"),
      _feature(feature),
      _curAction(directAction),
      _loopState(eRUN_FIRST),
      _directAction(true),
      _minimalPriorityAllowed(0) {}

void MaintenanceWorker::run() {
  bool more(false);

  while (eSTOP != _loopState && !_feature.isShuttingDown()) {
    try {
      if (!_feature.isPaused()) {
        try {
          switch (_loopState) {
            case eFIND_ACTION:
              _curAction =
                  _feature.findReadyAction(_minimalPriorityAllowed, _labels);
              more = (bool)_curAction;
              break;

            case eRUN_FIRST:
              if (_curAction->getState() == READY) {
                _curAction->setState(EXECUTING);
              }
              _curAction->startStats();
              LOG_TOPIC("fe241", DEBUG, Logger::MAINTENANCE)
                  << "Maintenance: starting to execute action: " << *_curAction;
              more = _curAction->first();
              break;

            case eRUN_NEXT:
              more = _curAction->next();
              break;

            default:
              _loopState = eSTOP;
              LOG_TOPIC("dc389", ERR, Logger::CLUSTER)
                  << "MaintenanceWorkerRun:  unexpected state (" << _loopState
                  << ")";

          }  // switch

          // determine next loop state
          nextState(more);

        } catch (std::exception const& ex) {
          if (_curAction) {
            LOG_TOPIC("dd8e8", ERR, Logger::CLUSTER)
                << "MaintenanceWorkerRun:  caught exception (" << ex.what()
                << ")"
                << " state:" << _loopState << " action:" << *_curAction;

            try {
              _curAction->endStats();
              _curAction->setState(FAILED);
            } catch (...) {
              // even setState() can fail, e.g. with OOM
              // there is not much we can do in this case
            }
          } else {
            LOG_TOPIC("16d4c", ERR, Logger::CLUSTER)
                << "MaintenanceWorkerRun:  caught exception (" << ex.what()
                << ")"
                << " state:" << _loopState;
          }
        } catch (...) {
          if (_curAction) {
            LOG_TOPIC("ac2a2", ERR, Logger::CLUSTER)
                << "MaintenanceWorkerRun: caught error, state: " << _loopState
                << " state:" << _loopState << " action:" << *_curAction;

            try {
              _curAction->endStats();
              _curAction->setState(FAILED);
            } catch (...) {
              // even setState() can fail, e.g. with OOM
              // there is not much we can do in this case
            }
          } else {
            LOG_TOPIC("88771", ERR, Logger::CLUSTER)
                << "MaintenanceWorkerRun: caught error, state: " << _loopState
                << " state:" << _loopState;
          }
        }

      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC("56021", WARN, Logger::CLUSTER)
          << "caught exception in Maintenance worker thread: " << ex.what();
    } catch (...) {
      LOG_TOPIC("2d64f", WARN, Logger::CLUSTER)
          << "caught unknown exception in Maintenance worker thread";
    }

  }  // while

}  // MaintenanceWorker::run

void MaintenanceWorker::nextState(bool actionMore) {
  // bad result code forces actionMore to false
  if (_curAction &&
      (!_curAction->result().ok() || FAILED == _curAction->getState())) {
    actionMore = false;
  }  // if

  // actionMore means iterate again
  if (actionMore) {
    // There should be an valid _curAction
    if (_curAction) {
      if (eFIND_ACTION == _loopState) {
        _loopState = eRUN_FIRST;
      } else {
        _curAction->incStats();
        _loopState = eRUN_NEXT;
      }  // if

      // move execution to PreAction if it exists
      if (_curAction->getPreAction()) {
        std::shared_ptr<Action> tempPtr;

        _curAction->setState(WAITING);
        tempPtr = _curAction;
        _curAction = _curAction->getPreAction();
        _curAction->setPostAction(
            std::make_shared<ActionDescription>(tempPtr->describe()));
        _loopState = eRUN_FIRST;
      }  // if
    } else {
      // this state should not exist, but deal with it
      _loopState = (_directAction ? eSTOP : eFIND_ACTION);
    }  // else
  } else {
    // finish the current action
    if (_curAction) {
      _lastResult = _curAction->result();

      bool ok = _curAction->result().ok() && FAILED != _curAction->getState();
      recordJobStats(/*failed*/ !ok);

      // if action's state not set, assume it succeeded when result ok
      if (ok) {
        _curAction->endStats();
        _curAction->setState(COMPLETE);
        if (_curAction->requeueRequested()) {
          LOG_TOPIC("a4352", DEBUG, Logger::MAINTENANCE)
              << "Requeueing action " << *_curAction << " with new priority "
              << _curAction->requeuePriority();
          _feature.requeueAction(_curAction, _curAction->requeuePriority());
        }

        // continue execution with "next" action tied to this one
        if (_curAction->getPostAction()) {
          _curAction = _curAction->getPostAction();
          _curAction->clearPreAction();
          _loopState =
              (WAITING == _curAction->getState() ? eRUN_NEXT : eRUN_FIRST);
          _curAction->setState(EXECUTING);
        } else {
          _curAction.reset();
          _loopState = (_directAction ? eSTOP : eFIND_ACTION);
        }  // else
      } else {
        std::shared_ptr<Action> failAction(_curAction);

        // fail all actions that would follow
        do {
          failAction->endStats();
          failAction->setState(FAILED);
          if (failAction->requeueRequested()) {
            LOG_TOPIC("a4353", DEBUG, Logger::MAINTENANCE)
                << "Requeueing action " << *failAction << " with new priority "
                << failAction->requeuePriority();
            _feature.requeueAction(failAction, failAction->requeuePriority());
          }
          failAction = failAction->getPostAction();
        } while (failAction);
        _loopState = (_directAction ? eSTOP : eFIND_ACTION);
      }  // else
    } else {
      // no current action, go back to hunting for one
      _loopState = (_directAction ? eSTOP : eFIND_ACTION);
    }  // else
  }    // else
}

void MaintenanceWorker::recordJobStats(bool failed) {
  auto iter =
      _feature._maintenance_job_metrics_map.find(_curAction->describe().name());
  if (iter != _feature._maintenance_job_metrics_map.end()) {
    MaintenanceFeature::ActionMetrics& metrics = iter->second;
    auto runtime = std::chrono::duration_cast<std::chrono::milliseconds>(
                       _curAction->getRunDuration())
                       .count();
    auto queuetime = std::chrono::duration_cast<std::chrono::milliseconds>(
                         _curAction->getQueueDuration())
                         .count();
    metrics._accum_runtime.count(runtime);
    metrics._runtime_histogram.count(runtime);
    metrics._queue_time_histogram.count(queuetime);
    metrics._accum_queue_time.count(queuetime);
    if (failed) {
      metrics._failure_counter.count();
    }
  }
}
// MaintenanceWorker::nextState

}  // namespace maintenance
}  // namespace arangodb
