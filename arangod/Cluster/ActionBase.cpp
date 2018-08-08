////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ActionBase.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/MaintenanceFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::maintenance;

const char ActionBase::KEY[]="key";
const char ActionBase::FIELDS[]="fields";
const char ActionBase::TYPE[]="type";
const char ActionBase::INDEXES[]="indexes";
const char ActionBase::INDEX[]="index";
const char ActionBase::SHARDS[]="shards";
const char ActionBase::DATABASE[]="database";
const char ActionBase::COLLECTION[]="collection";
const char ActionBase::EDGE[]="edge";
const char ActionBase::NAME[]="name";
const char ActionBase::ID[]="id";
const char ActionBase::LEADER[]="theLeader";
const char ActionBase::LOCAL_LEADER[]="localLeader";
const char ActionBase::GLOB_UID[]="globallyUniqueId";
const char ActionBase::OBJECT_ID[]="objectId";

ActionBase::ActionBase(MaintenanceFeature& feature,
                       ActionDescription const& desc)
  : _feature(feature), _description(desc), _state(READY),
    _actionCreated(std::chrono::system_clock::now()), _progress(0) {
  _hash = _description.hash();
  _clientId = std::to_string(_hash);
  _id = _feature.nextActionId();
}

ActionBase::~ActionBase() {
  auto cf = ApplicationServer::getFeature<ClusterFeature>("Cluster");
  if (cf != nullptr) {
    cf->syncDBServerStatusQuo();
  }
}

/// @brief execution finished successfully or failed ... and race timer expired
bool ActionBase::done() const {
  bool ret_flag;

  ret_flag = COMPLETE==_state || FAILED==_state;

  // test clock ... avoid race of same task happening again too quickly
  if (ret_flag) {
    std::chrono::seconds secs(_feature.getSecondsActionsBlock());
    std::chrono::system_clock::time_point raceOver = _actionDone + secs;

    ret_flag = (raceOver <= std::chrono::system_clock::now());
  } // if

  return (ret_flag);

} // ActionBase::done

ActionDescription const& ActionBase::describe() const {
  return _description;
}

MaintenanceFeature& ActionBase::feature() const {
  return _feature;
}

VPackSlice const ActionBase::properties() const {
  return _description.properties()->slice();
}


/// @brief Initiate a new action that will start immediately, pausing this action
void ActionBase::createPreAction(std::shared_ptr<ActionDescription> const & description) {

  _preAction = description;
  _feature.preAction(description);

  // shift from EXECUTING to WAITINGPRE ... EXECUTING is set to block other
  //  workers from picking it up
  if (_preAction) {
    setState(WAITINGPRE);
  } else {
    _result.reset(TRI_ERROR_BAD_PARAMETER, "preAction rejected parameters.");
  } // else

  return;

} // ActionBase::createPreAction


/// @brief Retrieve pointer to action that should run before this one
std::shared_ptr<Action> ActionBase::getPreAction() {
  return (_preAction != nullptr) ? _feature.findAction(_preAction) : nullptr;
}


/// @brief Retrieve pointer to action that should run after this one
std::shared_ptr<Action> ActionBase::getPostAction() {
  return (_postAction != nullptr) ? _feature.findAction(_postAction) : nullptr;
}


/// @brief Create a new action that will start after this action successfully completes
void ActionBase::createPostAction(std::shared_ptr<ActionDescription> const& description) {

  // preAction() sets up what we need
  _postAction = description;
  _feature.postAction(description);

  // shift from EXECUTING to WAITINGPOST ... EXECUTING is set to block other
  //  workers from picking it up
  if (_postAction) {
    setState(WAITINGPOST);
  } else {
    _result.reset(TRI_ERROR_BAD_PARAMETER, "preAction rejected parameters for _postAction.");
  } // else

  return;

} // ActionBase::createPostAction


void ActionBase::startStats() {

  _actionStarted = std::chrono::system_clock::now();

  return;

} // ActionBase::startStats


void ActionBase::incStats() {

  ++_progress;
  _actionLastStat = std::chrono::system_clock::now();

  return;

} // ActionBase::incStats


void ActionBase::endStats() {

  _actionDone = std::chrono::system_clock::now();

  return;

} // ActionBase::endStats


Result arangodb::actionError(int errorCode, std::string const& errorMessage) {
  LOG_TOPIC(ERR, Logger::MAINTENANCE) << errorMessage;
  return Result(errorCode, errorMessage);
}

Result arangodb::actionWarn(int errorCode, std::string const& errorMessage) {
  LOG_TOPIC(WARN, Logger::MAINTENANCE) << errorMessage;
  return Result(errorCode, errorMessage);
}

void ActionBase::toVelocyPack(VPackBuilder & builder) const {
  VPackObjectBuilder ob(&builder);

  builder.add("id", VPackValue(_id));
  builder.add("state", VPackValue(_state));
  builder.add("progress", VPackValue(_progress));
#if 0  /// hmm, several should be reported as duration instead of time_point
  builder.add("created", VPackValue(timepointToString(_actionCreated.count())));
  builder.add("started", VPackValue(_actionStarted.count()));
  builder.add("lastStat", VPackValue(_actionLastStat.count()));
  builder.add("done", VPackValue(_actionDone.count()));
#endif
  builder.add("result", VPackValue(_result.errorNumber()));

  builder.add(VPackValue("description"));
  { VPackObjectBuilder desc(&builder);
    _description.toVelocyPack(builder); }

} // MaintanceAction::toVelocityPack

VPackBuilder ActionBase::toVelocyPack() const {
  VPackBuilder builder;
  toVelocyPack(builder);
  return builder;
}


arangodb::Result ActionBase::run(
  std::chrono::duration<double> const& duration, bool& finished) {
  return Result();
}


/**
 * kill() operation is an expected future feature.  Not supported in the
 *  original ActionBase derivatives
 */
arangodb::Result ActionBase::kill(Signal const& signal) {
  return actionError(
    TRI_ERROR_ACTION_OPERATION_UNABORTABLE, "Kill operation not supported on this action.");
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
ostream& operator<< (
  ostream& out, arangodb::maintenance::ActionBase const& d) {
  out << d.toVelocyPack().toJson();
  return out;
}}
