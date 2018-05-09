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
bool MaintenanceAction::done() const {
  bool ret_flag;

  ret_flag = COMPLETE==_state || FAILED==_state;

  // test clock ... avoid race of same task happening again too quickly
  if (ret_flag) {
    std::chrono::seconds secs(_feature.getSecondsActionsBlock());
    std::chrono::system_clock::time_point raceOver = _actionDone + secs;

    ret_flag = (raceOver <= std::chrono::system_clock::now());
  } // if

  return (ret_flag);

} // MaintenanceAction::done

ActionBase::~ActionBase() {}

ActionDescription const& ActionBase::describe() const {
  return _description;
}


VPackSlice const ActionBase::properties() const {
  return _description.properties()->slice();
}


/// @brief Initiate a new action that will start immediately, pausing this action
void MaintenanceAction::createPreAction(std::shared_ptr<ActionDescription_t> const & description,
                                       std::shared_ptr<VPackBuilder> const & properties) {

  _preAction = _feature.preAction(description, properties);

  // shift from EXECUTING to WAITINGPRE ... EXECUTING is set to block other
  //  workers from picking it up
  if (_preAction) {
    setState(WAITINGPRE);
  } else {
    _result.reset(TRI_ERROR_BAD_PARAMETER, "preAction rejected parameters.");
  } // else

  return;

} // MaintenanceAction::createPreAction


/// @brief Create a new action that will start after this action successfully completes
void MaintenanceAction::createPostAction(std::shared_ptr<ActionDescription_t> const & description,
                                       std::shared_ptr<VPackBuilder> const & properties) {

  // preAction() sets up what we need
  _postAction = _feature.preAction(description, properties);

  // shift from EXECUTING to WAITINGPOST ... EXECUTING is set to block other
  //  workers from picking it up
  if (_postAction) {
    setState(WAITINGPOST);
  } else {
    _result.reset(TRI_ERROR_BAD_PARAMETER, "preAction rejected parameters for _postAction.");
  } // else

  return;

} // MaintenanceAction::createPostAction


void MaintenanceAction::startStats() {

  _actionStarted = std::chrono::system_clock::now();

  return;

} // MaintenanceAction::startStats


void MaintenanceAction::incStats() {

  ++_progress;
  _actionLastStat = std::chrono::system_clock::now();

  return;

} // MaintenanceAction::incStats


void MaintenanceAction::endStats() {

  _actionDone = std::chrono::system_clock::now();

  return;

} // MaintenanceAction::endStats


Result arangodb::actionError(int errorCode, std::string const& errorMessage) {	
  LOG_TOPIC(ERR, Logger::MAINTENANCE) << errorMessage;	
  return Result(errorCode, errorMessage);	
}	
 	 
Result arangodb::actionWarn(int errorCode, std::string const& errorMessage) { 
  LOG_TOPIC(WARN, Logger::MAINTENANCE) << errorMessage;
  return Result(errorCode, errorMessage);
}

