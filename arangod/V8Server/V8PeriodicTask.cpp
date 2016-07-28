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

#include "V8PeriodicTask.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Dispatcher/Dispatcher.h"
#include "Dispatcher/DispatcherFeature.h"
#include "V8Server/V8Job.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::rest;

std::unordered_set<Task*> V8PeriodicTask::RUNNING;
Mutex V8PeriodicTask::RUNNING_LOCK;

void V8PeriodicTask::jobDone(Task* task) {
  try {
    MUTEX_LOCKER(guard, V8PeriodicTask::RUNNING_LOCK);
    RUNNING.erase(task);
  } catch (...) {
    // ignore any memory error
  }
}

V8PeriodicTask::V8PeriodicTask(std::string const& id, std::string const& name,
                               TRI_vocbase_t* vocbase,
                               double offset, double period,
                               std::string const& command,
                               std::shared_ptr<VPackBuilder> parameters,
                               bool allowUseDatabase)
    : Task(id, name),
      PeriodicTask(id, offset, period),
      _vocbaseGuard(vocbase),
      _command(command),
      _parameters(parameters),
      _created(TRI_microtime()),
      _allowUseDatabase(allowUseDatabase) {
}

// get a task specific description in JSON format
void V8PeriodicTask::getDescription(VPackBuilder& builder) const {
  PeriodicTask::getDescription(builder);
  TRI_ASSERT(builder.isOpenObject());

  builder.add("created", VPackValue(_created));
  builder.add("command", VPackValue(_command));
  builder.add("database", VPackValue(_vocbaseGuard.vocbase()->name()));
}

// handles the next tick
bool V8PeriodicTask::handlePeriod() {
  if (DispatcherFeature::DISPATCHER == nullptr) {
    LOG(WARN) << "could not add task " << _command << ", no dispatcher known";
    return false;
  }

  {
    MUTEX_LOCKER(guard, V8PeriodicTask::RUNNING_LOCK);

    if (RUNNING.find(this) != RUNNING.end()) {
      LOG(DEBUG) << "old task still running, skipping";
      return true;
    }

    RUNNING.insert(this);
  }

  std::unique_ptr<Job> job(new V8Job(
      _vocbaseGuard.vocbase(), "(function (params) { " + _command + " } )(params);",
      _parameters, _allowUseDatabase, this));

  DispatcherFeature::DISPATCHER->addJob(job, false);

  return true;
}
