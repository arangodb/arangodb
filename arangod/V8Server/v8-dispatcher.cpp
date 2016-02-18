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
#include "Basics/Logger.h"
#include "Basics/tri-strings.h"
#include "Basics/StringUtils.h"
#include "Dispatcher/ApplicationDispatcher.h"
#include "Dispatcher/Dispatcher.h"
#include "Scheduler/ApplicationScheduler.h"
#include "Scheduler/Scheduler.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/ApplicationV8.h"
#include "V8Server/V8PeriodicTask.h"
#include "V8Server/V8QueueJob.h"
#include "V8Server/V8TimerTask.h"
#include "VocBase/server.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief global V8 dealer
////////////////////////////////////////////////////////////////////////////////

static ApplicationV8* GlobalV8Dealer = nullptr;

////////////////////////////////////////////////////////////////////////////////
/// @brief global scheduler
////////////////////////////////////////////////////////////////////////////////

static Scheduler* GlobalScheduler = nullptr;

////////////////////////////////////////////////////////////////////////////////
/// @brief global dispatcher
////////////////////////////////////////////////////////////////////////////////

static Dispatcher* GlobalDispatcher = nullptr;

////////////////////////////////////////////////////////////////////////////////
/// @brief try to compile the command
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a task id from an argument
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a task
////////////////////////////////////////////////////////////////////////////////

static void JS_RegisterTask(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (GlobalScheduler == nullptr || GlobalDispatcher == nullptr) {
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
    uint64_t tick = TRI_NewTickServer();
    id = StringUtils::itoa(tick);
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

  Task* task;

  if (period > 0.0) {
    // create a new periodic task
    task =
        new V8PeriodicTask(id, name, static_cast<TRI_vocbase_t*>(v8g->_vocbase),
                           GlobalV8Dealer, GlobalScheduler, GlobalDispatcher,
                           offset, period, command, parameters, isSystem);
  } else {
    // create a run-once timer task
    task = new V8TimerTask(id, name, static_cast<TRI_vocbase_t*>(v8g->_vocbase),
                           GlobalV8Dealer, GlobalScheduler, GlobalDispatcher,
                           offset, command, parameters, isSystem);
  }

  // get the VelocyPack representation of the task
  std::shared_ptr<VPackBuilder> builder = task->toVelocyPack();

  if (builder == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  int res = GlobalScheduler->registerTask(task);

  if (res != TRI_ERROR_NO_ERROR) {
    if (period > 0.0) {
      V8PeriodicTask* t = dynamic_cast<V8PeriodicTask*>(task);
      delete t;
    } else {
      V8TimerTask* t = dynamic_cast<V8TimerTask*>(task);
      delete t;
    }

    TRI_V8_THROW_EXCEPTION(res);
  }

  v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, builder->slice());

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregisters a task
///
/// @FUN{internal.unregisterTask(@FA{id})}
////////////////////////////////////////////////////////////////////////////////

static void JS_UnregisterTask(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("unregister(<id>)");
  }

  std::string const id = GetTaskId(isolate, args[0]);

  if (GlobalScheduler == nullptr || GlobalDispatcher == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "no scheduler found");
  }

  int res = GlobalScheduler->unregisterUserTask(id);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets one or all registered tasks
///
/// @FUN{internal.getTask(@FA{id})}
////////////////////////////////////////////////////////////////////////////////

static void JS_GetTask(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() > 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("get(<id>)");
  }

  if (GlobalScheduler == nullptr || GlobalDispatcher == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "no scheduler found");
  }

  std::shared_ptr<VPackBuilder> builder;

  if (args.Length() == 1) {
    // get a single task
    std::string const id = GetTaskId(isolate, args[0]);
    builder = GlobalScheduler->getUserTask(id);
  } else {
    // get all tasks
    builder = GlobalScheduler->getUserTasks();
  }

  if (builder == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_TASK_NOT_FOUND);
  }

  v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, builder->slice());

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores the V8 dispatcher function inside the global variable
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Dispatcher(v8::Isolate* isolate, v8::Handle<v8::Context> context,
                          TRI_vocbase_t* vocbase,
                          ApplicationScheduler* scheduler,
                          ApplicationDispatcher* dispatcher,
                          ApplicationV8* applicationV8) {
  v8::HandleScope scope(isolate);

  GlobalV8Dealer = applicationV8;

  // .............................................................................
  // create the global functions
  // .............................................................................

  // we need a scheduler and a dispatcher to define periodic tasks
  GlobalScheduler = scheduler->scheduler();
  GlobalDispatcher = dispatcher->dispatcher();

  if (GlobalScheduler != nullptr && GlobalDispatcher != nullptr) {
    TRI_AddGlobalFunctionVocbase(isolate, context,
                                 TRI_V8_ASCII_STRING("SYS_REGISTER_TASK"),
                                 JS_RegisterTask);
    TRI_AddGlobalFunctionVocbase(isolate, context,
                                 TRI_V8_ASCII_STRING("SYS_UNREGISTER_TASK"),
                                 JS_UnregisterTask);
    TRI_AddGlobalFunctionVocbase(
        isolate, context, TRI_V8_ASCII_STRING("SYS_GET_TASK"), JS_GetTask);
  } else {
    LOG(ERR) << "cannot initialize tasks, scheduler or dispatcher unknown";
  }
}
