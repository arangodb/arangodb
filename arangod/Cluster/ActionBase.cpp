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
/// @author Kaveh Vahedipour
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#include "ActionBase.h"
#include "MaintenanceFeature.h"

using namespace arangodb;
using namespace arangodb::maintenance;

ActionBase::ActionBase(
  std::shared_ptr<MaintenanceFeature> feature, ActionDescription const& d)
  : _feature(feature), _description(d), _state(READY),
    _actionCreated(std::chrono::system_clock::now()), _progress(0) {
  _hash = _description.hash();
  _id = _feature->nextActionId();
}

  /// @brief execution finished successfully or failed ... and race timer expired
bool ActionBase::done() const {
  bool ret_flag;

  ret_flag = COMPLETE==_state || FAILED==_state;

  // test clock ... avoid race of same task happening again too quickly
  if (ret_flag) {
    std::chrono::seconds secs(_feature->getSecondsActionsBlock());
    std::chrono::system_clock::time_point raceOver = _actionDone + secs;

    ret_flag = (raceOver <= std::chrono::system_clock::now());
  } // if

  return (ret_flag);

} // ActionBase::done

ActionBase::~ActionBase() {}

ActionDescription const& ActionBase::describe() const {
  return _description;
}


VPackSlice const ActionBase::properties() const {
  return _description.properties()->slice();
}


/// @brief Initiate a new action that will start immediately, pausing this action
void ActionBase::createPreAction(std::shared_ptr<ActionDescription> const & description) {

  _preAction = _feature->preAction(description);

  // shift from EXECUTING to WAITINGPRE ... EXECUTING is set to block other
  //  workers from picking it up
  if (_preAction) {
    setState(WAITINGPRE);
  } else {
    _result.reset(TRI_ERROR_BAD_PARAMETER, "preAction rejected parameters.");
  } // else

  return;

} // ActionBase::createPreAction


/// @brief Create a new action that will start after this action successfully completes
void ActionBase::createPostAction(std::shared_ptr<ActionDescription> const& description) {

  // preAction() sets up what we need
  _postAction = _feature->preAction(description);
  
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
