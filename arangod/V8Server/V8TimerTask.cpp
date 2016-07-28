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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "V8TimerTask.h"
#include "Dispatcher/Dispatcher.h"
#include "Dispatcher/DispatcherFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "V8/v8-conv.h"
#include "V8Server/V8Job.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::rest;

V8TimerTask::V8TimerTask(std::string const& id, std::string const& name,
                         TRI_vocbase_t* vocbase, double offset,
                         std::string const& command,
                         std::shared_ptr<VPackBuilder> parameters,
                         bool allowUseDatabase)
    : Task(id, name),
      TimerTask(id, (offset <= 0.0 ? 0.00001 : offset)),  // offset must be (at
                                                          // least slightly)
                                                          // greater than zero,
                                                          // otherwise
      // the timertask will not execute the task at all
      _vocbaseGuard(vocbase),
      _command(command),
      _parameters(parameters),
      _created(TRI_microtime()),
      _allowUseDatabase(allowUseDatabase) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a task specific description in JSON format
////////////////////////////////////////////////////////////////////////////////

void V8TimerTask::getDescription(VPackBuilder& builder) const {
  TimerTask::getDescription(builder);
  builder.add("created", VPackValue(_created));
  builder.add("command", VPackValue(_command));
  builder.add("database", VPackValue(_vocbaseGuard.vocbase()->name()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles the timer event
////////////////////////////////////////////////////////////////////////////////

bool V8TimerTask::handleTimeout() {
  TRI_ASSERT(DispatcherFeature::DISPATCHER != nullptr);

  std::unique_ptr<Job> job(
      new V8Job(_vocbaseGuard.vocbase(), "(function (params) { " + _command + " } )(params);",
                _parameters, _allowUseDatabase, nullptr));

  if (DispatcherFeature::DISPATCHER == nullptr) {
    LOG(WARN) << "could not add task " << _command << " to non-existing queue";
    return false;
  }
  
  int res = DispatcherFeature::DISPATCHER->addJob(job, false);

  if (res != TRI_ERROR_NO_ERROR && res != TRI_ERROR_SHUTTING_DOWN) {
    LOG(WARN) << "could not add task " << _command << " to queue";
  }

  // note: this will destroy the task (i.e. ourselves!!)
  SchedulerFeature::SCHEDULER->destroyTask(this);

  return true;
}
