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
////////////////////////////////////////////////////////////////////////////////

#include "v8-dispatcher.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
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
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/V8Context.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/AccessMode.h"
#include "VocBase/Methods/Tasks.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                          private helper
// functions
// -----------------------------------------------------------------------------

static std::string GetTaskId(v8::Isolate* isolate, v8::Handle<v8::Value> arg) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  if (arg->IsObject()) {
    // extract "id" from object
    v8::Handle<v8::Object> obj = arg.As<v8::Object>();
    if (TRI_HasProperty(context, isolate, obj, "id")) {
      return TRI_ObjectToString(isolate,
                                obj->Get(TRI_IGETC,
                                         TRI_V8_ASCII_STRING(isolate, "id"))
                                    .FromMaybe(v8::Local<v8::Value>()));
    }
  }

  return TRI_ObjectToString(isolate, arg);
}

// -----------------------------------------------------------------------------
// --SECTION--                                              JavaScript functions
// -----------------------------------------------------------------------------

static void JS_RegisterTask(v8::FunctionCallbackInfo<v8::Value> const& args) {
  using arangodb::Task;

  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::HandleScope scope(isolate);

  TRI_GET_GLOBALS();
  if (SchedulerFeature::SCHEDULER == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "no scheduler found");
  } else if (v8g->_server.isStopping()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }

  if (args.Length() != 1 || !args[0]->IsObject()) {
    TRI_V8_THROW_EXCEPTION_USAGE("register(<task>)");
  }

  if (ExecContext::current().databaseAuthLevel() != auth::Level::RW) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "registerTask() needs db RW permissions");
  }

  v8::Handle<v8::Object> obj = args[0].As<v8::Object>();

  // job id
  std::string id;

  if (TRI_HasProperty(context, isolate, obj, "id")) {
    // user-specified id
    id =
        TRI_ObjectToString(isolate, obj->Get(TRI_IGETC,
                                             TRI_V8_ASCII_STRING(isolate, "id"))
                                        .FromMaybe(v8::Local<v8::Value>()));
  } else {
    // auto-generated id
    id = std::to_string(TRI_NewServerSpecificTick());
  }

  // job name
  std::string name;

  if (TRI_HasProperty(context, isolate, obj, "name")) {
    name = TRI_ObjectToString(isolate, obj->Get(TRI_IGETC, TRI_V8_ASCII_STRING(isolate,
                                                                               "name"))
                                           .FromMaybe(v8::Local<v8::Value>()));
  } else {
    name = "user-defined task";
  }

  bool isSystem = false;

  if (TRI_HasProperty(context, isolate, obj, "isSystem")) {
    isSystem = TRI_ObjectToBoolean(
        isolate, obj->Get(TRI_IGETC, TRI_V8_ASCII_STRING(isolate, "isSystem"))
                     .FromMaybe(v8::Local<v8::Value>()));
  }

  if (isSystem && !v8g->_securityContext.isInternal()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN, "Only internal context may create system tasks");
  }

  // offset in seconds into period or from now on if no period
  double offset = 0.0;

  if (TRI_HasProperty(context, isolate, obj, "offset")) {
    offset = TRI_ObjectToDouble(isolate,
                                obj->Get(TRI_IGETC,
                                         TRI_V8_ASCII_STRING(isolate, "offset"))
                                    .FromMaybe(v8::Local<v8::Value>()));
  }

  // period in seconds & count
  double period = 0.0;

  if (TRI_HasProperty(context, isolate, obj, "period")) {
    period = TRI_ObjectToDouble(isolate,
                                obj->Get(TRI_IGETC,
                                         TRI_V8_ASCII_STRING(isolate, "period"))
                                    .FromMaybe(v8::Local<v8::Value>()));

    if (period <= 0.0) {
      TRI_V8_THROW_EXCEPTION_PARAMETER(
          "task period must be specified and positive");
    }
  }

  std::string runAsUser;
  if (TRI_HasProperty(context, isolate, obj, "runAsUser")) {
    runAsUser = TRI_ObjectToString(
        isolate, obj->Get(TRI_IGETC, TRI_V8_ASCII_STRING(isolate, "runAsUser"))
                     .FromMaybe(v8::Local<v8::Value>()));
  }

  // only the superroot is allowed to run tasks as an arbitrary user
  if (runAsUser.empty()) {  // execute task as the same user
    runAsUser = ExecContext::current().user();
  } else if (ExecContext::current().user() != runAsUser) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
  }

  // extract the command
  if (!TRI_HasProperty(context, isolate, obj, "command")) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("command must be specified");
  }

  std::string command;
  if (obj->Get(TRI_IGETC, TRI_V8_ASCII_STRING(isolate, "command"))
          .FromMaybe(v8::Local<v8::Value>())
          ->IsFunction()) {
    // need to add ( and ) around function because call would otherwise break
    command = "(" +
              TRI_ObjectToString(isolate, obj->Get(TRI_IGETC, TRI_V8_ASCII_STRING(isolate,
                                                                                  "command"))
                                              .FromMaybe(v8::Local<v8::Value>())) +
              ")(params)";
  } else {
    command = TRI_ObjectToString(
        isolate, obj->Get(TRI_IGETC, TRI_V8_ASCII_STRING(isolate, "command"))
                     .FromMaybe(v8::Local<v8::Value>()));
  }

  if (!Task::tryCompile(isolate, command)) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("cannot compile command");
  }

  // extract the parameters
  auto parameters = std::make_shared<VPackBuilder>();

  if (TRI_HasProperty(context, isolate, obj, "params")) {
    TRI_V8ToVPack(isolate, *parameters,
                  obj->Get(TRI_IGETC,
                           TRI_V8_ASCII_STRING(isolate, "params"))
                      .FromMaybe(v8::Local<v8::Value>()),
                  false);
  }

  command = "(function (params) { " + command + " } )(params);";

  int res;
  std::shared_ptr<Task> task =
      Task::createTask(id, name, v8g->_vocbase, command, isSystem, res);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  // set the user this will run as
  if (!runAsUser.empty()) {
    task->setUser(runAsUser);
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

  task->start();

  v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, builder->slice());

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

static void JS_UnregisterTask(v8::FunctionCallbackInfo<v8::Value> const& args) {
  using arangodb::Task;
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("unregister(<id>)");
  }

  if (ExecContext::current().databaseAuthLevel() != auth::Level::RW) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "registerTask() needs db RW permissions");
  }

  int res = Task::unregisterTask(GetTaskId(isolate, args[0]), true);
  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

static void JS_GetTask(v8::FunctionCallbackInfo<v8::Value> const& args) {
  using arangodb::Task;
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() > 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("get(<id>)");
  }

  std::shared_ptr<VPackBuilder> builder;

  if (args.Length() == 1) {
    // get a single task
    builder = Task::registeredTask(GetTaskId(isolate, args[0]));
  } else {
    // get all tasks
    builder = Task::registeredTasks();
  }

  if (builder == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_TASK_NOT_FOUND);
  }

  v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, builder->slice());

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

/// creates a new object in _queues, circumvents permission blocks
static void JS_CreateQueue(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_GET_GLOBALS();
  TRI_vocbase_t* vocbase = v8g->_vocbase;

  if (vocbase == nullptr || vocbase->isDropped()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsNumber()) {
    TRI_V8_THROW_EXCEPTION_USAGE("createQueue(<id>, <maxWorkers>)");
  }

  auto const& exec = ExecContext::current();
  if (exec.databaseAuthLevel() != auth::Level::RW) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "createQueue() needs db RW permissions");
  }

  std::string const runAsUser = exec.user();
  TRI_ASSERT(exec.isAdminUser() || !runAsUser.empty());
  
  std::string key = TRI_ObjectToString(isolate, args[0]);
  uint64_t maxWorkers =
      std::min(TRI_ObjectToUInt64(isolate, args[1], false), (uint64_t)64);

  VPackBuilder doc;

  doc.openObject();
  doc.add(StaticStrings::KeyString, VPackValue(key));
  doc.add("maxWorkers", VPackValue(maxWorkers));
  doc.add("runAsUser", VPackValue(runAsUser));
  doc.close();

  LOG_TOPIC("aeb56", TRACE, Logger::FIXME) << "Adding queue " << key;
  ExecContextSuperuserScope exscope;
  auto ctx = transaction::V8Context::Create(*vocbase, true);
  SingleCollectionTransaction trx(ctx, StaticStrings::QueuesCollection, AccessMode::Type::EXCLUSIVE);
  Result res = trx.begin();

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  OperationOptions opts;
  opts.overwriteMode = OperationOptions::OverwriteMode::Replace;
  OperationResult result = trx.insert(StaticStrings::QueuesCollection, doc.slice(), opts);

  res = trx.finish(result.result);

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN(v8::Boolean::New(isolate, result.ok()));
  TRI_V8_TRY_CATCH_END
}

static void JS_DeleteQueue(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_GET_GLOBALS();
  TRI_vocbase_t* vocbase = v8g->_vocbase;

  if (vocbase == nullptr || vocbase->isDropped()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("deleteQueue(<id>)");
  }

  std::string key = TRI_ObjectToString(isolate, args[0]);
  VPackBuilder doc;
  doc(VPackValue(VPackValueType::Object))(StaticStrings::KeyString, VPackValue(key))();

  if (ExecContext::current().databaseAuthLevel() != auth::Level::RW) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "deleteQueue() needs db RW permissions");
  }

  LOG_TOPIC("2cef9", TRACE, Logger::FIXME) << "Removing queue " << key;
  ExecContextSuperuserScope exscope;
  auto ctx = transaction::V8Context::Create(*vocbase, true);
  SingleCollectionTransaction trx(ctx, StaticStrings::QueuesCollection, AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);
  Result res = trx.begin();

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  OperationOptions opts;
  OperationResult result = trx.remove(StaticStrings::QueuesCollection, doc.slice(), opts);

  res = trx.finish(result.result);

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN(v8::Boolean::New(isolate, result.ok()));
  TRI_V8_TRY_CATCH_END
}

// -----------------------------------------------------------------------------
// --SECTION--                                             module initialization
// -----------------------------------------------------------------------------

void TRI_InitV8Dispatcher(v8::Isolate* isolate, v8::Handle<v8::Context> context) {
  v8::HandleScope scope(isolate);

  // _queues is a RO collection and can only be written in C++, as superroot
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "SYS_CREATE_QUEUE"),
                               JS_CreateQueue);

  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "SYS_DELETE_QUEUE"),
                               JS_DeleteQueue);

  // we need a scheduler and a dispatcher to define periodic tasks
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "SYS_REGISTER_TASK"), JS_RegisterTask);

  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "SYS_UNREGISTER_TASK"), JS_UnregisterTask);

  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "SYS_GET_TASK"), JS_GetTask);
}
