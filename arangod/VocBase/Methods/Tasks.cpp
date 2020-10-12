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
/// @author Dr. Frank Celler
/// @author Jan Steemann
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "VocBase/Methods/Tasks.h"

#include <v8.h>
#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/FunctionUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/system-functions.h"
#include "Basics/tri-strings.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Transaction/Hints.h"
#include "Transaction/V8Context.h"
#include "Utils/DatabaseGuard.h"
#include "Utils/ExecContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "V8/JavaScriptSecurityContext.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/V8Context.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/AccessMode.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

using namespace arangodb::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                                     task handling
// -----------------------------------------------------------------------------

namespace {
bool authorized(std::pair<std::string, std::shared_ptr<arangodb::Task>> const& task) {
  arangodb::ExecContext const& exec = arangodb::ExecContext::current();
  if (exec.isSuperuser()) {
    return true;
  }

  return (task.first == exec.user());
}
}  // namespace

namespace arangodb {

Mutex Task::_tasksLock;
std::unordered_map<std::string, std::pair<std::string, std::shared_ptr<Task>>> Task::_tasks;

std::shared_ptr<Task> Task::createTask(std::string const& id, std::string const& name,
                                       TRI_vocbase_t* vocbase, std::string const& command,
                                       bool allowUseDatabase, int& ec) {
  if (id.empty()) {
    ec = TRI_ERROR_TASK_INVALID_ID;

    return nullptr;
  }

  TRI_ASSERT(nullptr != vocbase);  // this check was previously in the
  // DatabaseGuard constructor which on failure
  // would fail Task constructor

  if (vocbase->server().isStopping()) {
    ec = TRI_ERROR_SHUTTING_DOWN;
    return nullptr;
  }
  
  TRI_ASSERT(nullptr != vocbase);  // this check was previously in the
                                   // DatabaseGuard constructor which on failure
                                   // would fail Task constructor

  std::string const& user = ExecContext::current().user();
  auto task = std::make_shared<Task>(id, name, *vocbase, command, allowUseDatabase);
  
  MUTEX_LOCKER(guard, _tasksLock);

  if (!_tasks.try_emplace(id, user, task).second) {
    ec = TRI_ERROR_TASK_DUPLICATE_ID;

    return {nullptr};
  }

  ec = TRI_ERROR_NO_ERROR;

  return task;
}

int Task::unregisterTask(std::string const& id, bool cancel) {
  if (id.empty()) {
    return TRI_ERROR_TASK_INVALID_ID;
  }

  MUTEX_LOCKER(guard, _tasksLock);

  auto itr = _tasks.find(id);

  if (itr == _tasks.end() || !::authorized(itr->second)) {
    return TRI_ERROR_TASK_NOT_FOUND;
  }

  if (cancel) {
    itr->second.second->cancel();
  }

  _tasks.erase(itr);

  return TRI_ERROR_NO_ERROR;
}

std::shared_ptr<VPackBuilder> Task::registeredTask(std::string const& id) {
  MUTEX_LOCKER(guard, _tasksLock);

  auto itr = _tasks.find(id);

  if (itr == _tasks.end() || !::authorized(itr->second)) {
    return nullptr;
  }

  return itr->second.second->toVelocyPack();
}

std::shared_ptr<VPackBuilder> Task::registeredTasks() {
  auto builder = std::make_shared<VPackBuilder>();

  try {
    VPackArrayBuilder b1(builder.get());

    MUTEX_LOCKER(guard, _tasksLock);

    for (auto& it : _tasks) {
      if (::authorized(it.second)) {
        VPackObjectBuilder b2(builder.get());
        it.second.second->toVelocyPack(*builder);
      }
    }
  } catch (...) {
    return std::make_shared<VPackBuilder>();
  }

  return builder;
}

void Task::shutdownTasks() {
  {
    MUTEX_LOCKER(guard, _tasksLock);
    for (auto& it : _tasks) {
      it.second.second->cancel();
    }
  }

  // wait for the tasks to be cleaned up
  int iterations = 0;
  while (true) {
    size_t size;
    {
      MUTEX_LOCKER(guard, _tasksLock);
      size = _tasks.size();
    }

    if (size == 0) {
      break;
    }

    if (++iterations % 10 == 0) {
      LOG_TOPIC("3966b", INFO, Logger::FIXME) << "waiting for " << size << " task(s) to complete";
    } else if (iterations >= 25) {
      LOG_TOPIC("54653", INFO, Logger::FIXME) << "giving up waiting for unfinished tasks";
      MUTEX_LOCKER(guard, _tasksLock);
      _tasks.clear();
      break;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
}

void Task::removeTasksForDatabase(std::string const& name) {
  MUTEX_LOCKER(guard, _tasksLock);

  for (auto it = _tasks.begin(); it != _tasks.end(); /* no hoisting */) {
    if (!(*it).second.second->databaseMatches(name)) {
      ++it;
    } else {
      auto task = (*it).second.second;
      task->cancel();
      it = _tasks.erase(it);
    }
  }
}

bool Task::tryCompile(v8::Isolate* isolate, std::string const& command) {
  if (!V8DealerFeature::DEALER || !V8DealerFeature::DEALER->isEnabled()) {
    return false;
  }

  TRI_ASSERT(isolate != nullptr);

  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;
  // get built-in Function constructor (see ECMA-262 5th edition 15.3.2)
  auto current = isolate->GetCurrentContext()->Global();
  auto ctor = v8::Local<v8::Function>::Cast(
                                            current->Get(context,
                                                         TRI_V8_ASCII_STRING(isolate, "Function"))
                                            .FromMaybe(v8::Local<v8::Value>())
                                            );

  // Invoke Function constructor to create function with the given body and no
  // arguments
  v8::Handle<v8::Value> args[2] = {TRI_V8_ASCII_STRING(isolate, "params"),
                                   TRI_V8_STD_STRING(isolate, command)};
  v8::Local<v8::Object> function = ctor->NewInstance(TRI_IGETC, 2, args).FromMaybe(v8::Local<v8::Object>());

  v8::Handle<v8::Function> action = v8::Local<v8::Function>::Cast(function);

  return (!action.IsEmpty());
}

bool Task::databaseMatches(std::string const& name) const {
  return (_dbGuard->database().name() == name);
}

Task::Task(std::string const& id, std::string const& name, TRI_vocbase_t& vocbase,
           std::string const& command, bool allowUseDatabase)
    : _id(id),
      _name(name),
      _created(TRI_microtime()),
      _dbGuard(new DatabaseGuard(vocbase)),
      _command(command),
      _allowUseDatabase(allowUseDatabase),
      _offset(0),
      _interval(0) {}

Task::~Task() = default;

void Task::setOffset(double offset) {
  _offset = std::chrono::milliseconds(static_cast<long long>(offset * 1000));
  _periodic.store(false);
}

void Task::setPeriod(double offset, double period) {
  _offset = std::chrono::milliseconds(static_cast<long long>(offset * 1000));
  _interval = std::chrono::milliseconds(static_cast<long long>(period * 1000));
  _periodic.store(true);
}

void Task::setParameter(std::shared_ptr<arangodb::velocypack::Builder> const& parameters) {
  _parameters = parameters;
}

void Task::setUser(std::string const& user) { _user = user; }

std::function<void(bool cancelled)> Task::callbackFunction() {
  return [self = shared_from_this(), this](bool cancelled) {
    if (cancelled) {
      MUTEX_LOCKER(guard, _tasksLock);

      auto itr = _tasks.find(_id);

      if (itr != _tasks.end()) {
        // remove task from list of tasks if it is still active
        if (this == (*itr).second.second.get()) {
          // still the same task. must remove from map
          _tasks.erase(itr);
        }
      }
      return;
    }

    // get the permissions to be used by this task
    bool allowContinue = true;
    std::shared_ptr<ExecContext> execContext;

    if (!_user.empty()) {  // not superuser
      auto& dbname = _dbGuard->database().name();

      execContext.reset(ExecContext::create(_user, dbname).release());
      allowContinue = execContext->canUseDatabase(dbname, auth::Level::RW);
    }

    // permissions might have changed since starting this task
    if (_dbGuard->database().server().isStopping() || !allowContinue) {
      Task::unregisterTask(_id, true);
      return;
    }

    // now do the work:
    bool queued = basics::function_utils::retryUntilTimeout(
        [this, self, execContext]() -> bool {
          return SchedulerFeature::SCHEDULER->queue(RequestLane::INTERNAL_LOW, [self, this, execContext] {
            ExecContextScope scope(_user.empty() ? &ExecContext::superuser()
                                                 : execContext.get());
            work(execContext.get());

            if (_periodic.load() && !_dbGuard->database().server().isStopping()) {
              // requeue the task
              bool queued = basics::function_utils::retryUntilTimeout(
                  [this]() -> bool { return queue(_interval); }, Logger::FIXME,
                  "queue task");
              if (!queued) {
                THROW_ARANGO_EXCEPTION_MESSAGE(
                    TRI_ERROR_QUEUE_FULL,
                    "Failed to queue task for 5 minutes, gave up.");
              }
            } else {
              // in case of one-off tasks or in case of a shutdown, simply
              // remove the task from the list
              Task::unregisterTask(_id, true);
            }
          });
        },
        Logger::FIXME, "queue task");
    if (!queued) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUEUE_FULL, "Failed to queue task for 5 minutes, gave up.");
    }
  };
}

void Task::start() {
  ExecContext const& exec = ExecContext::current();
  TRI_ASSERT(exec.isAdminUser() || (!_user.empty() && exec.user() == _user));

  {
    MUTEX_LOCKER(lock, _taskHandleMutex);
    _taskHandle.reset();
  }

  if (_offset.count() <= 0) {
    _offset = std::chrono::microseconds(1);
  }

  // initially queue the task
  bool queued = basics::function_utils::retryUntilTimeout(
      [this]() -> bool { return queue(_offset); }, Logger::FIXME, "queue task");
  if (!queued) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUEUE_FULL, "Failed to queue task for 5 minutes, gave up.");
  }
}

bool Task::queue(std::chrono::microseconds offset) {
  if (!V8DealerFeature::DEALER || !V8DealerFeature::DEALER->isEnabled()) {
    return false;
  }

  MUTEX_LOCKER(lock, _taskHandleMutex);
  bool queued = false;
  std::tie(queued, _taskHandle) =
      SchedulerFeature::SCHEDULER->queueDelay(RequestLane::INTERNAL_LOW, offset,
                                              callbackFunction());
  return queued;
}

void Task::cancel() {
  // this will prevent the task from dispatching itself again
  _periodic.store(false);
  MUTEX_LOCKER(lock, _taskHandleMutex);
  _taskHandle.reset();
}

std::shared_ptr<VPackBuilder> Task::toVelocyPack() const {
  try {
    auto builder = std::make_shared<VPackBuilder>();
    {
      VPackObjectBuilder b(builder.get());
      toVelocyPack(*builder);
    }
    return builder;
  } catch (...) {
    return std::make_shared<VPackBuilder>();
  }
}

void Task::toVelocyPack(VPackBuilder& builder) const {
  builder.add("id", VPackValue(_id));
  builder.add("name", VPackValue(_name));
  builder.add("created", VPackValue(_created));

  if (_periodic.load()) {
    builder.add("type", VPackValue("periodic"));
    builder.add("period", VPackValue(_interval.count() / 1000000.0));
  } else {
    builder.add("type", VPackValue("timed"));
  }

  builder.add("offset", VPackValue(_offset.count() / 1000000.0));

  builder.add("command", VPackValue(_command));
  builder.add("database", VPackValue(_dbGuard->database().name()));
}

void Task::work(ExecContext const* exec) {
  JavaScriptSecurityContext securityContext =
      _allowUseDatabase
          ? JavaScriptSecurityContext::createInternalContext() // internal context that may access internal data
          : JavaScriptSecurityContext::createTaskContext(false /*_allowUseDatabase*/); // task context that has no access to dbs
  V8ContextGuard guard(&(_dbGuard->database()), securityContext);

  // now execute the function within this context
  {
    auto isolate = guard.isolate();
    v8::HandleScope scope(isolate);
    auto context = TRI_IGETC;

    // get built-in Function constructor (see ECMA-262 5th edition 15.3.2)
    auto current = isolate->GetCurrentContext()->Global();
    auto ctor = v8::Local<v8::Function>::Cast(
                                              current->Get(context,
                                                           TRI_V8_ASCII_STRING(isolate, "Function"))
                                              .FromMaybe(v8::Local<v8::Value>())
                                              );

    // Invoke Function constructor to create function with the given body and
    // no
    // arguments
    v8::Handle<v8::Value> args[2] = {TRI_V8_ASCII_STRING(isolate, "params"),
                                     TRI_V8_STD_STRING(isolate, _command)};
    v8::Local<v8::Object> function = ctor->NewInstance(TRI_IGETC, 2, args).FromMaybe(v8::Local<v8::Object>());

    v8::Handle<v8::Function> action = v8::Local<v8::Function>::Cast(function);

    if (!action.IsEmpty()) {
      // only go in here if action is a function
      v8::Handle<v8::Value> fArgs;

      if (_parameters != nullptr) {
        fArgs = TRI_VPackToV8(isolate, _parameters->slice());
      } else {
        fArgs = v8::Undefined(isolate);
      }

      // call the function within a try/catch
      try {
        v8::TryCatch tryCatch(isolate);
        action->Call(TRI_IGETC, current, 1, &fArgs).FromMaybe(v8::Local<v8::Value>());
        if (tryCatch.HasCaught()) {
          if (tryCatch.CanContinue()) {
            TRI_LogV8Exception(isolate, &tryCatch);
          } else {
            TRI_GET_GLOBALS();

            v8g->_canceled = true;
            LOG_TOPIC("131e8", WARN, arangodb::Logger::FIXME)
                << "caught non-catchable exception (aka termination) in job";
          }
        }
      } catch (arangodb::basics::Exception const& ex) {
        LOG_TOPIC("d6729", ERR, arangodb::Logger::FIXME)
            << "caught exception in V8 user task: " << TRI_errno_string(ex.code())
            << " " << ex.what();
      } catch (std::bad_alloc const&) {
        LOG_TOPIC("bfe8a", ERR, arangodb::Logger::FIXME)
            << "caught exception in V8 user task: "
            << TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY);
      } catch (...) {
        LOG_TOPIC("342ec", ERR, arangodb::Logger::FIXME)
            << "caught unknown exception in V8 user task";
      }
    }
  }
}

}  // namespace arangodb
