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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "v8-dispatcher.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/StringUtils.h"
#include "Basics/tri-strings.h"
#include "Logger/Logger.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/V8Context.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                     task handling
// -----------------------------------------------------------------------------

namespace {
class V8Task : public std::enable_shared_from_this<V8Task> {
 public:
  static std::shared_ptr<V8Task> createTask(std::string const& id,
                                            std::string const& name,
                                            TRI_vocbase_t*,
                                            std::string const& command,
                                            bool allowUseDatabase, int& ec);

  static int unregisterTask(std::string const& id, bool cancel);

  static std::shared_ptr<VPackBuilder> registeredTask(std::string const& id);
  static std::shared_ptr<VPackBuilder> registeredTasks();
  static void shutdownTasks();

 private:
  static Mutex _tasksLock;
  static std::unordered_map<std::string, std::shared_ptr<V8Task>> _tasks;

 public:
  V8Task(std::string const& id, std::string const& name, TRI_vocbase_t*,
         std::string const& command, bool allowUseDatabase);

 public:
  void setOffset(double offset);
  void setPeriod(double offset, double period);
  void setParameter(
      std::shared_ptr<arangodb::velocypack::Builder> const& parameters);

  void start(boost::asio::io_service*);
  void cancel();

  std::shared_ptr<VPackBuilder> toVelocyPack() const;

 private:
  void toVelocyPack(VPackBuilder&) const;
  void work();
  std::function<void(boost::system::error_code const&)> callbackFunction();

 private:
  std::string const _id;
  std::string const _name;
  double const _created;

  std::unique_ptr<boost::asio::steady_timer> _timer;

  // guard to make sure the database is not dropped while used by us
  std::unique_ptr<VocbaseGuard> _vocbaseGuard;

  std::string const _command;
  std::shared_ptr<arangodb::velocypack::Builder> _parameters;
  bool const _allowUseDatabase;

  std::chrono::microseconds _offset;
  std::chrono::microseconds _interval;
  bool _periodic = false;
};

Mutex V8Task::_tasksLock;
std::unordered_map<std::string, std::shared_ptr<V8Task>> V8Task::_tasks;

std::shared_ptr<V8Task> V8Task::createTask(std::string const& id,
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

  auto itr = _tasks.emplace(
      id,
      std::make_shared<V8Task>(id, name, vocbase, command, allowUseDatabase));

  ec = TRI_ERROR_NO_ERROR;
  return itr.first->second;
}

int V8Task::unregisterTask(std::string const& id, bool cancel) {
  if (id.empty()) {
    return TRI_ERROR_TASK_INVALID_ID;
  }

  MUTEX_LOCKER(guard, _tasksLock);

  auto itr = _tasks.find(id);

  if (itr == _tasks.end()) {
    return TRI_ERROR_TASK_NOT_FOUND;
  }

  if (cancel) {
    itr->second->cancel();
  }

  _tasks.erase(itr);

  return TRI_ERROR_NO_ERROR;
}

std::shared_ptr<VPackBuilder> V8Task::registeredTask(std::string const& id) {
  MUTEX_LOCKER(guard, _tasksLock);

  auto itr = _tasks.find(id);

  if (itr == _tasks.end()) {
    return nullptr;
  }

  return itr->second->toVelocyPack();
}

std::shared_ptr<VPackBuilder> V8Task::registeredTasks() {
  auto builder = std::make_shared<VPackBuilder>();

  try {
    VPackArrayBuilder b1(builder.get());

    MUTEX_LOCKER(guard, _tasksLock);

    for (auto& it : _tasks) {
      VPackObjectBuilder b2(builder.get());
      it.second->toVelocyPack(*builder);
    }
  } catch (...) {
    return std::make_shared<VPackBuilder>();
  }

  return builder;
}

void V8Task::shutdownTasks() {
  MUTEX_LOCKER(guard, _tasksLock);

  for (auto& it : _tasks) {
    it.second->cancel();
  }

  _tasks.clear();
}

V8Task::V8Task(std::string const& id, std::string const& name,
               TRI_vocbase_t* vocbase, std::string const& command,
               bool allowUseDatabase)
    : _id(id),
      _name(name),
      _created(TRI_microtime()),
      _vocbaseGuard(new VocbaseGuard(vocbase)),
      _command(command),
      _allowUseDatabase(allowUseDatabase) {}

void V8Task::setOffset(double offset) {
  _offset = std::chrono::microseconds(static_cast<long long>(offset * 1000000));
  _periodic = false;
}

void V8Task::setPeriod(double offset, double period) {
  _offset = std::chrono::microseconds(static_cast<long long>(offset * 1000000));
  _interval =
      std::chrono::microseconds(static_cast<long long>(period * 1000000));
  _periodic = true;
}

void V8Task::setParameter(
    std::shared_ptr<arangodb::velocypack::Builder> const& parameters) {
  _parameters = parameters;
}

std::function<void(const boost::system::error_code&)>
V8Task::callbackFunction() {
  auto self = shared_from_this();

  return [self, this](const boost::system::error_code& error) {
    if (error) {
      V8Task::unregisterTask(_id, false);
      return;
    }

    if (SchedulerFeature::SCHEDULER->isStopping()) {
      V8Task::unregisterTask(_id, false);
      return;
    }

    work();

    if (_periodic) {
      _timer->expires_from_now(_interval);
      _timer->async_wait(callbackFunction());
    } else {
      V8Task::unregisterTask(_id, false);
    }
  };
}

void V8Task::start(boost::asio::io_service* ioService) {
  _timer.reset(new boost::asio::steady_timer(*ioService));

  if (_offset.count() <= 0) {
    _offset = std::chrono::microseconds(1);
  }

  _timer->expires_from_now(_offset);
  _timer->async_wait(callbackFunction());
}

void V8Task::cancel() {
  boost::system::error_code ec;
  _timer->cancel(ec);
}

std::shared_ptr<VPackBuilder> V8Task::toVelocyPack() const {
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

void V8Task::toVelocyPack(VPackBuilder& builder) const {
  builder.add("id", VPackValue(_id));
  builder.add("name", VPackValue(_name));
  builder.add("created", VPackValue(_created));

  if (_periodic) {
    builder.add("type", VPackValue("periodic"));
    builder.add("period", VPackValue(_interval.count() / 1000000.0));
  } else {
    builder.add("type", VPackValue("timed"));
  }

  builder.add("offset", VPackValue(_offset.count() / 1000000.0));

  builder.add("command", VPackValue(_command));
  builder.add("database", VPackValue(_vocbaseGuard->vocbase()->name()));
}

void V8Task::work() {
  auto context = V8DealerFeature::DEALER->enterContext(_vocbaseGuard->vocbase(),
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
        current->Get(TRI_V8_ASCII_STRING("Function")));

    // Invoke Function constructor to create function with the given body and
    // no
    // arguments
    v8::Handle<v8::Value> args[2] = {TRI_V8_ASCII_STRING("params"),
                                     TRI_V8_STD_STRING(_command)};
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
            LOG(WARN)
                << "caught non-catchable exception (aka termination) in job";
          }
        }
      } catch (arangodb::basics::Exception const& ex) {
        LOG(ERR) << "caught exception in V8 user task: "
                 << TRI_errno_string(ex.code()) << " " << ex.what();
      } catch (std::bad_alloc const&) {
        LOG(ERR) << "caught exception in V8 user task: "
                 << TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY);
      } catch (...) {
        LOG(ERR) << "caught unknown exception in V8 user task";
      }
    }
  }
}
}

// -----------------------------------------------------------------------------
// --SECTION--                                          private helper
// functions
// -----------------------------------------------------------------------------

static bool TryCompile(v8::Isolate* isolate, std::string const& command) {
  v8::HandleScope scope(isolate);

  // get built-in Function constructor (see ECMA-262 5th edition 15.3.2)
  auto current = isolate->GetCurrentContext()->Global();
  auto ctor = v8::Local<v8::Function>::Cast(
      current->Get(TRI_V8_ASCII_STRING("Function")));

  // Invoke Function constructor to create function with the given body and no
  // arguments
  v8::Handle<v8::Value> args[2] = {TRI_V8_ASCII_STRING("params"),
                                   TRI_V8_STD_STRING(command)};
  v8::Local<v8::Object> function = ctor->NewInstance(2, args);

  v8::Handle<v8::Function> action = v8::Local<v8::Function>::Cast(function);

  return (!action.IsEmpty());
}

static std::string GetTaskId(v8::Isolate* isolate, v8::Handle<v8::Value> arg) {
  if (arg->IsObject()) {
    // extract "id" from object
    v8::Handle<v8::Object> obj = arg.As<v8::Object>();
    if (obj->Has(TRI_V8_ASCII_STRING("id"))) {
      return TRI_ObjectToString(obj->Get(TRI_V8_ASCII_STRING("id")));
    }
  }

  return TRI_ObjectToString(arg);
}

// -----------------------------------------------------------------------------
// --SECTION--                                              Javascript
// functions
// -----------------------------------------------------------------------------

static void JS_RegisterTask(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (SchedulerFeature::SCHEDULER == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "no scheduler found");
  }

  if (args.Length() != 1 || !args[0]->IsObject()) {
    TRI_V8_THROW_EXCEPTION_USAGE("register(<task>)");
  }

  v8::Handle<v8::Object> obj = args[0].As<v8::Object>();

  // job id
  std::string id;

  if (obj->HasOwnProperty(TRI_V8_ASCII_STRING("id"))) {
    // user-specified id
    id = TRI_ObjectToString(obj->Get(TRI_V8_ASCII_STRING("id")));
  } else {
    // auto-generated id
    id = std::to_string(TRI_NewTickServer());
  }

  // job name
  std::string name;

  if (obj->HasOwnProperty(TRI_V8_ASCII_STRING("name"))) {
    name = TRI_ObjectToString(obj->Get(TRI_V8_ASCII_STRING("name")));
  } else {
    name = "user-defined task";
  }

  bool isSystem = false;

  if (obj->HasOwnProperty(TRI_V8_ASCII_STRING("isSystem"))) {
    isSystem = TRI_ObjectToBoolean(obj->Get(TRI_V8_ASCII_STRING("isSystem")));
  }

  // offset in seconds into period or from now on if no period
  double offset = 0.0;

  if (obj->HasOwnProperty(TRI_V8_ASCII_STRING("offset"))) {
    offset = TRI_ObjectToDouble(obj->Get(TRI_V8_ASCII_STRING("offset")));
  }

  // period in seconds & count
  double period = 0.0;

  if (obj->HasOwnProperty(TRI_V8_ASCII_STRING("period"))) {
    period = TRI_ObjectToDouble(obj->Get(TRI_V8_ASCII_STRING("period")));

    if (period <= 0.0) {
      TRI_V8_THROW_EXCEPTION_PARAMETER(
          "task period must be specified and positive");
    }
  }

  // extract the command
  if (!obj->HasOwnProperty(TRI_V8_ASCII_STRING("command"))) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("command must be specified");
  }

  std::string command;
  if (obj->Get(TRI_V8_ASCII_STRING("command"))->IsFunction()) {
    // need to add ( and ) around function because call would otherwise break
    command = "(" +
              TRI_ObjectToString(obj->Get(TRI_V8_ASCII_STRING("command"))) +
              ")(params)";
  } else {
    command = TRI_ObjectToString(obj->Get(TRI_V8_ASCII_STRING("command")));
  }

  if (!TryCompile(isolate, command)) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("cannot compile command");
  }

  // extract the parameters
  auto parameters = std::make_shared<VPackBuilder>();

  if (obj->HasOwnProperty(TRI_V8_ASCII_STRING("params"))) {
    int res = TRI_V8ToVPack(isolate, *parameters,
                            obj->Get(TRI_V8_ASCII_STRING("params")), false);
    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }

  TRI_GET_GLOBALS();

  command = "(function (params) { " + command + " } )(params);";

  int res;
  std::shared_ptr<V8Task> task =
      V8Task::createTask(id, name, static_cast<TRI_vocbase_t*>(v8g->_vocbase),
                         command, isSystem, res);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  // set execution parameters
  task->setParameter(parameters);

  if (period > 0.0) {
    // create a new periodic task
    task->setPeriod(offset, period);
  } else {
    // create a run-once timer task
    task->setOffset(offset);
  }

  // get the VelocyPack representation of the task
  std::shared_ptr<VPackBuilder> builder = task->toVelocyPack();

  if (builder == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  // and start
  auto ioService = SchedulerFeature::SCHEDULER->ioService();
  task->start(ioService);

  v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, builder->slice());

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

static void JS_UnregisterTask(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("unregister(<id>)");
  }

  int res = V8Task::unregisterTask(GetTaskId(isolate, args[0]), true);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

static void JS_GetTask(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() > 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("get(<id>)");
  }

  std::shared_ptr<VPackBuilder> builder;

  if (args.Length() == 1) {
    // get a single task
    builder = V8Task::registeredTask(GetTaskId(isolate, args[0]));
  } else {
    // get all tasks
    builder = V8Task::registeredTasks();
  }

  if (builder == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_TASK_NOT_FOUND);
  }

  v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, builder->slice());

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

// -----------------------------------------------------------------------------
// --SECTION--                                             module initialization
// -----------------------------------------------------------------------------

void TRI_InitV8Dispatcher(v8::Isolate* isolate,
                          v8::Handle<v8::Context> context) {
  v8::HandleScope scope(isolate);

  // we need a scheduler and a dispatcher to define periodic tasks
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("SYS_REGISTER_TASK"),
                               JS_RegisterTask);

  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("SYS_UNREGISTER_TASK"),
                               JS_UnregisterTask);

  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("SYS_GET_TASK"), JS_GetTask);
}

void TRI_ShutdownV8Dispatcher() { V8Task::shutdownTasks(); }
