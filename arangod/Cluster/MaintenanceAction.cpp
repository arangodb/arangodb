////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#include "MaintenanceAction.h"

#include "Cluster/ActionDescription.h"
#include "Cluster/MaintenanceFeature.h"

namespace arangodb {

namespace maintenance {

const char MaintenanceAction::KEY[]="key";
const char MaintenanceAction::FIELDS[]="fields";
const char MaintenanceAction::TYPE[]="type";
const char MaintenanceAction::INDEXES[]="indexes";
const char MaintenanceAction::SHARDS[]="shards";
const char MaintenanceAction::DATABASE[]="database";
const char MaintenanceAction::COLLECTION[]="collection";
const char MaintenanceAction::EDGE[]="edge";
const char MaintenanceAction::NAME[]="name";
const char MaintenanceAction::ID[]="id";
const char MaintenanceAction::LEADER[]="leader";
const char MaintenanceAction::LOCAL_LEADER[]="localLeader";
const char MaintenanceAction::GLOB_UID[]="globallyUniqueId";
const char MaintenanceAction::OBJECT_ID[]="objectId";

MaintenanceAction::MaintenanceAction(arangodb::MaintenanceFeature & feature,
                                     std::shared_ptr<ActionDescription_t> const & description,
                                     std::shared_ptr<VPackBuilder> const & properties)
  : _feature(feature), _description(description), _properties(properties),
    _state(READY),
    _actionCreated(std::chrono::system_clock::now()),
    _actionStarted(std::chrono::system_clock::now()),
    _actionLastStat(std::chrono::system_clock::now()),
    _actionDone(std::chrono::system_clock::now()),
    _progress(0) {

  _hash = ActionDescription::hash(*description);
  _id = feature.nextActionId();
  return;

} // MaintenanceAction::MaintenanceAction


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


} // namespace maintenance

} // namespace arangodb
