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
#include "Basics/json.h"
#include "Dispatcher/Dispatcher.h"
#include "Scheduler/Scheduler.h"
#include "V8/v8-conv.h"
#include "V8Server/V8Job.h"
#include "VocBase/server.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::rest;

V8TimerTask::V8TimerTask(std::string const& id, std::string const& name,
                         TRI_vocbase_t* vocbase, ApplicationV8* v8Dealer,
                         Scheduler* scheduler, Dispatcher* dispatcher,
                         double offset, std::string const& command,
                         std::shared_ptr<VPackBuilder> parameters,
                         bool allowUseDatabase)
    : Task(id, name),
      TimerTask(id, (offset <= 0.0 ? 0.00001 : offset)),  // offset must be (at
                                                          // least slightly)
                                                          // greater than zero,
                                                          // otherwise
      // the timertask will not execute the task at all
      _vocbase(vocbase),
      _v8Dealer(v8Dealer),
      _dispatcher(dispatcher),
      _command(command),
      _parameters(parameters),
      _created(TRI_microtime()),
      _allowUseDatabase(allowUseDatabase) {
  TRI_ASSERT(vocbase != nullptr);

  // increase reference counter for the database used
  TRI_UseVocBase(_vocbase);
}

V8TimerTask::~V8TimerTask() {
  // decrease reference counter for the database used
  TRI_ReleaseVocBase(_vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a task specific description in JSON format
////////////////////////////////////////////////////////////////////////////////

void V8TimerTask::getDescription(VPackBuilder& builder) const {
  TimerTask::getDescription(builder);
  builder.add("created", VPackValue(_created));
  builder.add("command", VPackValue(_command));
  builder.add("database", VPackValue(_vocbase->_name));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles the timer event
////////////////////////////////////////////////////////////////////////////////

bool V8TimerTask::handleTimeout() {
  std::unique_ptr<Job> job(new V8Job(
      _vocbase, _v8Dealer, "(function (params) { " + _command + " } )(params);",
      _parameters, _allowUseDatabase));

  _dispatcher->addJob(job);

  // note: this will destroy the task (i.e. ourselves!!)
  _scheduler->destroyTask(this);

  return true;
}
