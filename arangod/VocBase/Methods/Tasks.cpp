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
/// @author Dr. Frank Celler
/// @author Jan Steemann
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "VocBase/Methods/Tasks.h"

#include <v8.h>
#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/StringUtils.h"
#include "Basics/tri-strings.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "Scheduler/JobGuard.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Transaction/Hints.h"
#include "Transaction/V8Context.h"
#include "Utils/DatabaseGuard.h"
#include "Utils/ExecContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
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
  auto context = arangodb::ExecContext::CURRENT;
  if (context == nullptr || !arangodb::ExecContext::isAuthEnabled()) {
    return true;
  }

  if (context->isSuperuser()) {
    return true;
  }

  return (task.first == context->user());
}
}

namespace arangodb {

Mutex Task::_tasksLock;
std::unordered_map<std::string, std::pair<std::string, std::shared_ptr<Task>>> Task::_tasks;

std::shared_ptr<Task> Task::createTask(std::string const& id,
                                       std::string const& name,
                                       TRI_vocbase_t* vocbase,
                                       std::string const& command,
                                       bool allowUseDatabase, int& ec) {
  if (id.empty()) {
    ec = TRI_ERROR_TASK_INVALID_ID;

    return nullptr;
  }

  MUTEX_LOCKER(guard, _tasksLock);

  if (_tasks.find(id) != _tasks.end()) {
    ec = TRI_ERROR_TASK_DUPLICATE_ID;

    return {nullptr};
  }

  TRI_ASSERT(nullptr != vocbase);  // this check was previously in the
                                   // DatabaseGuard constructor which on failure
                                   // would fail Task constructor

  std::string user = ExecContext::CURRENT ? ExecContext::CURRENT->user() : "";
  auto task =
      std::make_shared<Task>(id, name, *vocbase, command, allowUseDatabase);
  auto itr = _tasks.emplace(id, std::make_pair(user, std::move(task)));

  ec = TRI_ERROR_NO_ERROR;

  return itr.first->second.second;
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
  MUTEX_LOCKER(guard, _tasksLock);

  for (auto& it : _tasks) {
    it.second.second->cancel();
  }

  _tasks.clear();
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
  v8::HandleScope scope(isolate);

  // get built-in Function constructor (see ECMA-262 5th edition 15.3.2)
  auto current = isolate->GetCurrentContext()->Global();
  auto ctor = v8::Local<v8::Function>::Cast(
      current->Get(TRI_V8_ASCII_STRING(isolate, "Function")));

  // Invoke Function constructor to create function with the given body and no
  // arguments
  v8::Handle<v8::Value> args[2] = {TRI_V8_ASCII_STRING(isolate, "params"),
                                   TRI_V8_STD_STRING(isolate, command)};
  v8::Local<v8::Object> function = ctor->NewInstance(2, args);

  v8::Handle<v8::Function> action = v8::Local<v8::Function>::Cast(function);

  return (!action.IsEmpty());
}

bool Task::databaseMatches(std::string const& name) const {
  return (_dbGuard->database().name() == name);
}

Task::Task(std::string const& id, std::string const& name,
           TRI_vocbase_t& vocbase, std::string const& command,
           bool allowUseDatabase)
    : _id(id),
      _name(name),
      _created(TRI_microtime()),
      _dbGuard(new DatabaseGuard(vocbase)),
      _command(command),
      _allowUseDatabase(allowUseDatabase),
      _offset(0),
      _interval(0) {}

Task::~Task() {}

void Task::setOffset(double offset) {
  _offset = std::chrono::microseconds(static_cast<long long>(offset * 1000000));
  _periodic.store(false);
}

void Task::setPeriod(double offset, double period) {
  _offset = std::chrono::microseconds(static_cast<long long>(offset * 1000000));
  _interval =
      std::chrono::microseconds(static_cast<long long>(period * 1000000));
  _periodic.store(true);
}

void Task::setParameter(
    std::shared_ptr<arangodb::velocypack::Builder> const& parameters) {
  _parameters = parameters;
}

void Task::setUser(std::string const& user) { _user = user; }

std::function<void(const asio::error_code&)> Task::callbackFunction() {
  auto self = shared_from_this();

  return [self, this](const asio::error_code& error) {
    if (error) {
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

      execContext.reset(ExecContext::create(_user, dbname));
      allowContinue = execContext->canUseDatabase(dbname, auth::Level::RW);
    }

    // permissions might have changed since starting this task
    if (SchedulerFeature::SCHEDULER->isStopping() || !allowContinue) {
      Task::unregisterTask(_id, false);
      return;
    }

    // now do the work:
    SchedulerFeature::SCHEDULER->queue(
      RequestPriority::LOW, [self, this, execContext] {
          ExecContextScope scope(_user.empty() ? ExecContext::superuser()
                                               : execContext.get());

          work(execContext.get());

          if (_periodic.load() && !SchedulerFeature::SCHEDULER->isStopping()) {
            // requeue the task
            queue(_interval);
          } else {
            // in case of one-off tasks or in case of a shutdown, simply
            // remove the task from the list
            Task::unregisterTask(_id, false);
          }
        });
  };
}

void Task::start() {
  TRI_ASSERT(ExecContext::CURRENT == nullptr ||
             ExecContext::CURRENT->isAdminUser() ||
             (!_user.empty() && ExecContext::CURRENT->user() == _user));
  {
    MUTEX_LOCKER(lock, _timerMutex);
    _timer.reset(SchedulerFeature::SCHEDULER->newSteadyTimer());
  }

  if (_offset.count() <= 0) {
    _offset = std::chrono::microseconds(1);
  }

  // initially queue the task
  queue(_offset);
}

void Task::queue(std::chrono::microseconds offset) {
  MUTEX_LOCKER(lock, _timerMutex);
  _timer->expires_from_now(offset);
  _timer->async_wait(callbackFunction());
}

void Task::cancel() {
  // this will prevent the task from dispatching itself again
  _periodic.store(false);

  asio::error_code ec;
  {
    MUTEX_LOCKER(lock, _timerMutex);
    _timer->cancel(ec);
  }
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
  auto context = V8DealerFeature::DEALER->enterContext(&(_dbGuard->database()),
                                                       _allowUseDatabase);

  // note: the context might be 0 in case of shut-down
  if (context == nullptr) {
    return;
  }

  TRI_DEFER(V8DealerFeature::DEALER->exitContext(context));

  // now execute the function within this context
  {
    auto isolate = context->_isolate;
    v8::HandleScope scope(isolate);

    // get built-in Function constructor (see ECMA-262 5th edition 15.3.2)
    auto current = isolate->GetCurrentContext()->Global();
    auto ctor = v8::Local<v8::Function>::Cast(
        current->Get(TRI_V8_ASCII_STRING(isolate, "Function")));

    // Invoke Function constructor to create function with the given body and
    // no
    // arguments
    v8::Handle<v8::Value> args[2] = {TRI_V8_ASCII_STRING(isolate, "params"),
                                     TRI_V8_STD_STRING(isolate, _command)};
    v8::Local<v8::Object> function = ctor->NewInstance(2, args);

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
        v8::TryCatch tryCatch;
        action->Call(current, 1, &fArgs);
        if (tryCatch.HasCaught()) {
          if (tryCatch.CanContinue()) {
            TRI_LogV8Exception(isolate, &tryCatch);
          } else {
            TRI_GET_GLOBALS();

            v8g->_canceled = true;
            LOG_TOPIC(WARN, arangodb::Logger::FIXME)
                << "caught non-catchable exception (aka termination) in job";
          }
        }
      } catch (arangodb::basics::Exception const& ex) {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME)
            << "caught exception in V8 user task: "
            << TRI_errno_string(ex.code()) << " " << ex.what();
      } catch (std::bad_alloc const&) {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME)
            << "caught exception in V8 user task: "
            << TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY);
      } catch (...) {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME)
            << "caught unknown exception in V8 user task";
      }
    }
  }
}

}  // namespace arangodb
