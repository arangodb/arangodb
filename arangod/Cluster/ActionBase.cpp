////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "Cluster/ActionBase.h"

#include "Agency/TimeString.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/HeartbeatThread.h"
#include "Cluster/MaintenanceFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::maintenance;

std::string const ActionBase::FAST_TRACK = "fastTrack";

inline static std::chrono::system_clock::duration secs_since_epoch() {
  return std::chrono::system_clock::now().time_since_epoch();
}

ActionBase::ActionBase(MaintenanceFeature& feature, ActionDescription const& desc)
    : _feature(feature),
      _description(desc),
      _state(READY),
      _progress(0),
      _priority(desc.priority()) {
  init();
}

ActionBase::ActionBase(MaintenanceFeature& feature, ActionDescription&& desc)
    : _feature(feature),
      _description(std::move(desc)),
      _state(READY),
      _progress(0),
      _priority(desc.priority()) {
  init();
}

void ActionBase::init() {
  _hash = _description.hash();
  _clientId = std::to_string(_hash);
  _id = _feature.nextActionId();

  // initialization of duration struct is not guaranteed in atomic form
  _actionCreated = secs_since_epoch();
  _actionStarted = std::chrono::system_clock::duration::zero();
  _actionLastStat = std::chrono::system_clock::duration::zero();
  _actionDone = std::chrono::system_clock::duration::zero();
}

ActionBase::~ActionBase() = default;

void ActionBase::notify() {
  LOG_TOPIC("df020", DEBUG, Logger::MAINTENANCE)
      << "Job " << _description << " notifing maintenance";
  auto& server = _feature.server();
  if (server.hasFeature<ClusterFeature>()) {
    server.getFeature<ClusterFeature>().notify();
  }
}

bool ActionBase::matches(std::unordered_set<std::string> const& labels) const {
  for (auto const& label : labels) {
    if (_labels.find(label) == _labels.end()) {
      LOG_TOPIC("e29f1", TRACE, Logger::MAINTENANCE)
          << "Must not run in worker with " << label << ": " << *this;
      return false;
    }
  }
  return true;
}

bool ActionBase::fastTrack() const {
  return _labels.find(FAST_TRACK) != _labels.end();
}

/// @brief execution finished successfully or failed ... and race timer expired
bool ActionBase::done() const {
  return (COMPLETE == _state || FAILED == _state) &&
         _actionDone.load() + std::chrono::seconds(_feature.getSecondsActionsBlock()) <=
             secs_since_epoch();

}  // ActionBase::done

ActionDescription const& ActionBase::describe() const { return _description; }

MaintenanceFeature& ActionBase::feature() const { return _feature; }

VPackSlice const ActionBase::properties() const {
  return _description.properties()->slice();
}

/// @brief Initiate a new action that will start immediately, pausing this
/// action
void ActionBase::createPreAction(std::shared_ptr<ActionDescription> const& description) {
  _preAction = description;
  std::shared_ptr<Action> new_action = _feature.preAction(description);

  // shift from EXECUTING to WAITINGPRE ... EXECUTING is set to block other
  //  workers from picking it up
  if (_preAction && new_action->ok()) {
    setState(WAITINGPRE);
  } else {
    _result.reset(TRI_ERROR_BAD_PARAMETER, "preAction rejected parameters.");
  }  // else

}  // ActionBase::createPreAction

/// @brief Retrieve pointer to action that should run before this one
std::shared_ptr<Action> ActionBase::getPreAction() {
  return (_preAction != nullptr) ? _feature.findFirstNotDoneAction(_preAction) : nullptr;
}

/// @brief Retrieve pointer to action that should run after this one
std::shared_ptr<Action> ActionBase::getPostAction() {
  return (_postAction != nullptr) ? _feature.findFirstNotDoneAction(_postAction) : nullptr;
}

// FIXMEMAINTENANCE: Code path could corrupt registry object because
//   this does not hold lock.

/// @brief Create a new action that will start after this action successfully
/// completes
void ActionBase::createPostAction(std::shared_ptr<ActionDescription> const& description) {
  // postAction() sets up what we need
  _postAction = description;
  if (_postAction) {
    _feature.postAction(description);
  } else {
    _result.reset(TRI_ERROR_BAD_PARAMETER,
                  "postAction rejected parameters for _postAction.");
  }
}  // ActionBase::createPostAction

void ActionBase::startStats() {
  _actionStarted = secs_since_epoch();

}  // ActionBase::startStats

/// @brief show progress on Action, and when that progress occurred
void ActionBase::incStats() {
  ++_progress;
  _actionLastStat = secs_since_epoch();

}  // ActionBase::incStats

void ActionBase::endStats() {
  _actionDone = secs_since_epoch();

}  // ActionBase::endStats

Result arangodb::actionError(int errorCode, std::string const& errorMessage) {
  LOG_TOPIC("c889d", ERR, Logger::MAINTENANCE) << errorMessage;
  return Result(errorCode, errorMessage);
}

Result arangodb::actionWarn(int errorCode, std::string const& errorMessage) {
  LOG_TOPIC("abe54", WARN, Logger::MAINTENANCE) << errorMessage;
  return Result(errorCode, errorMessage);
}

void ActionBase::toVelocyPack(VPackBuilder& builder) const {
  VPackObjectBuilder ob(&builder);

  builder.add("id", VPackValue(_id));
  builder.add("state", VPackValue(_state));
  builder.add("progress", VPackValue(_progress));

  builder.add("created", VPackValue(timepointToString(
                             std::chrono::duration_cast<std::chrono::system_clock::duration>(
                                 _actionCreated.load()))));
  builder.add("started", VPackValue(timepointToString(
                             std::chrono::duration_cast<std::chrono::system_clock::duration>(
                                 _actionStarted.load()))));
  builder.add("lastStat", VPackValue(timepointToString(
                              std::chrono::duration_cast<std::chrono::system_clock::duration>(
                                  _actionLastStat.load()))));
  builder.add("done", VPackValue(timepointToString(
                          std::chrono::duration_cast<std::chrono::system_clock::duration>(
                              _actionDone.load()))));

  builder.add("result", VPackValue(_result.errorNumber()));

  builder.add(VPackValue("description"));
  {
    VPackObjectBuilder desc(&builder);
    _description.toVelocyPack(builder);
  }

}  // MaintanceAction::toVelocityPack

VPackBuilder ActionBase::toVelocyPack() const {
  VPackBuilder builder;
  toVelocyPack(builder);
  return builder;
}

ActionState ActionBase::getState() const { return _state; }

void ActionBase::setState(ActionState state) {
  // We want to make sure that we get another maintenance run
  // when we shift from any state to complete or failed 
  if ((COMPLETE == state || FAILED == state) && _state != state && _description.has(DATABASE)) {
    _feature.addDirty(_description.get(DATABASE));
    TRI_ASSERT(!_description.get(DATABASE).empty());
  }
  _state = state;
}

/**
 * progress() operation is an expected future feature.  Not supported in the
 *  original ActionBase derivatives
 */
arangodb::Result ActionBase::progress(double& progress) {
  progress = 0.5;
  return arangodb::Result(TRI_ERROR_NO_ERROR);
}

namespace std {
ostream& operator<<(ostream& out, arangodb::maintenance::ActionBase const& d) {
  out << d.toVelocyPack().toJson();
  return out;
}
}  // namespace std
