////////////////////////////////////////////////////////////////////////////////
/// @brief V8 dispatcher functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "v8-dispatcher.h"

#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "Basics/StringUtils.h"
#include "Dispatcher/ApplicationDispatcher.h"
#include "Dispatcher/Dispatcher.h"
#include "Scheduler/ApplicationScheduler.h"
#include "Scheduler/Scheduler.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8Server/ApplicationV8.h"
#include "V8Server/V8DispatcherThread.h"
#include "V8Server/V8PeriodicTask.h"
#include "V8Server/V8QueueJob.h"
#include "V8Server/V8TimerTask.h"
#include "VocBase/server.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a task id from an argument
////////////////////////////////////////////////////////////////////////////////

static string GetTaskId (v8::Isolate* isolate, v8::Handle<v8::Value> arg) {
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
/// @brief returns the V8 dispatcher thread
////////////////////////////////////////////////////////////////////////////////

static DispatcherThread* CreateV8DispatcherThread (DispatcherQueue* queue, void* data) {
  return new V8DispatcherThread(queue, (const char*) data);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                      JS functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief registers a task
////////////////////////////////////////////////////////////////////////////////

static void JS_RegisterTask (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (GlobalScheduler == nullptr || GlobalDispatcher == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "no scheduler found");
  }

  if (args.Length() != 1 || ! args[0]->IsObject()) {
    TRI_V8_THROW_EXCEPTION_USAGE("register(<task>)");
  }

  v8::Handle<v8::Object> obj = args[0].As<v8::Object>();

  // job id
  string id;

  if (obj->HasOwnProperty(TRI_V8_ASCII_STRING("id"))) {
    // user-specified id
    id = TRI_ObjectToString(obj->Get(TRI_V8_ASCII_STRING("id")));
  }
  else {
    // auto-generated id
    uint64_t tick = TRI_NewTickServer();
    id = StringUtils::itoa(tick);
  }

  // job name
  string name;

  if (obj->HasOwnProperty(TRI_V8_ASCII_STRING("name"))) {
    name = TRI_ObjectToString(obj->Get(TRI_V8_ASCII_STRING("name")));
  }
  else {
    name = "user-defined task";
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
      TRI_V8_THROW_EXCEPTION_PARAMETER("task period must be specified and positive");
    }
  }

  // extract the command
  if (! obj->HasOwnProperty(TRI_V8_ASCII_STRING("command"))) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("command must be specified");
  }

  string command;
  if (obj->Get(TRI_V8_ASCII_STRING("command"))->IsFunction()) {
    // need to add ( and ) around function because call would otherwise break
    command = "(" + TRI_ObjectToString(obj->Get(TRI_V8_ASCII_STRING("command"))) + ")(params)";
  }
  else {
    command = TRI_ObjectToString(obj->Get(TRI_V8_ASCII_STRING("command")));
  }

  // extract the parameters
  TRI_json_t* parameters = nullptr;

  if (obj->HasOwnProperty(TRI_V8_ASCII_STRING("params"))) {
    parameters = TRI_ObjectToJson(isolate, obj->Get(TRI_V8_ASCII_STRING("params")));
  }

  TRI_GET_GLOBALS();

  Task* task;

  if (period > 0.0) {
    // create a new periodic task
    task = new V8PeriodicTask(
        id,
        name,
        static_cast<TRI_vocbase_t*>(v8g->_vocbase),
        GlobalV8Dealer,
        GlobalScheduler,
        GlobalDispatcher,
        offset,
        period,
        command,
        parameters);
  }
  else {
    // create a run-once timer task
    task = new V8TimerTask(
        id,
        name,
        static_cast<TRI_vocbase_t*>(v8g->_vocbase),
        GlobalV8Dealer,
        GlobalScheduler,
        GlobalDispatcher,
        offset,
        command,
        parameters);
  }
  
  // get the JSON representation of the task
  TRI_json_t* json = task->toJson();

  if (json == nullptr) {
    if (period > 0.0) {
      V8PeriodicTask* t = dynamic_cast<V8PeriodicTask*>(task);
      delete t;
    }
    else {
      V8TimerTask* t = dynamic_cast<V8TimerTask*>(task);
      delete t;
    }

    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  TRI_ASSERT(json != nullptr);

  int res = GlobalScheduler->registerTask(task);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    if (period > 0.0) {
      V8PeriodicTask* t = dynamic_cast<V8PeriodicTask*>(task);
      delete t;
    }
    else {
      V8TimerTask* t = dynamic_cast<V8TimerTask*>(task);
      delete t;
    }

    TRI_V8_THROW_EXCEPTION(res);
  }

  v8::Handle<v8::Value> result = TRI_ObjectJson(isolate, json);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregisters a task
///
/// @FUN{internal.unregisterTask(@FA{id})}
////////////////////////////////////////////////////////////////////////////////

static void JS_UnregisterTask (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("unregister(<id>)");
  }

  string const id = GetTaskId(isolate, args[0]);

  if (GlobalScheduler == nullptr || GlobalDispatcher == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "no scheduler found");
  }

  int res = GlobalScheduler->unregisterUserTask(id);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets one or all registered tasks
///
/// @FUN{internal.getTask(@FA{id})}
////////////////////////////////////////////////////////////////////////////////

static void JS_GetTask (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() > 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("get(<id>)");
  }

  if (GlobalScheduler == nullptr || GlobalDispatcher == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "no scheduler found");
  }

  TRI_json_t* json;

  if (args.Length() == 1) {
    // get a single task
    string const id = GetTaskId(isolate, args[0]);
    json = GlobalScheduler->getUserTask(id);
  }
  else {
    // get all tasks
    json = GlobalScheduler->getUserTasks();
  }

  if (json == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_TASK_NOT_FOUND);
  }

  v8::Handle<v8::Value> result = TRI_ObjectJson(isolate, json);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, json);

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new named dispatcher queue
///
/// @FUN{internal.createNamedQueue(@FA{options})}
///
/// name: the name of the queue
/// threads: number of threads
/// size maximal queue size
/// worker: the Javascript for the worker, must evaluate to a function
////////////////////////////////////////////////////////////////////////////////

static void JS_CreateNamedQueue (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (GlobalDispatcher == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "no dispatcher found");
  }

  if (args.Length() != 1 || ! args[0]->IsObject()) {
    TRI_V8_THROW_EXCEPTION_USAGE("createNamedQueue(<options>)");
  }

  v8::Handle<v8::Object> obj = args[0].As<v8::Object>();

  // name of the queue
  if (! obj->HasOwnProperty(TRI_V8_ASCII_STRING("name"))) {
    TRI_V8_THROW_EXCEPTION_USAGE("<options>.name is missing");
  }

  string name = TRI_ObjectToString(obj->Get(TRI_V8_ASCII_STRING("name")));

  // number of threads
  if (! obj->HasOwnProperty(TRI_V8_ASCII_STRING("threads"))) {
    TRI_V8_THROW_EXCEPTION_USAGE("<options>.threads is missing");
  }

  int nrThreads = static_cast<int>(TRI_ObjectToInt64(obj->Get(TRI_V8_ASCII_STRING("threads"))));

  if (nrThreads < 1) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("<options>.threads must be at least 1");
  }

  // queue size
  if (! obj->HasOwnProperty(TRI_V8_ASCII_STRING("size"))) {
    TRI_V8_THROW_EXCEPTION_USAGE("<options>.size is missing");
  }

  int size = static_cast<int>(TRI_ObjectToInt64(obj->Get(TRI_V8_ASCII_STRING("size"))));

  if (size < nrThreads) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("<options>.size must be at least <options>.threads");
  }

  // worker for the queue
  if (! obj->HasOwnProperty(TRI_V8_ASCII_STRING("worker"))) {
    TRI_V8_THROW_EXCEPTION_USAGE("<options>.worker is missing");
  }

  string worker = TRI_ObjectToString(obj->Get(TRI_V8_ASCII_STRING("worker")));

  // create V8 contexts
  bool ok = GlobalV8Dealer->prepareNamedContexts(name, nrThreads, worker);

  if (! ok) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "cannot create V8 queue engines");
  }

  // create a new queue
  int result = GlobalDispatcher->startNamedQueue(
    name,
    CreateV8DispatcherThread,
    (void*) TRI_DuplicateString(name.c_str()),
    nrThreads,
    size);

  if (result == TRI_ERROR_QUEUE_ALREADY_EXISTS) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(result, "named queue already exists");
  }

  if (result != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "cannot create queue");
  }

  TRI_V8_RETURN_UNDEFINED();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a job to a named queue
///
/// @FUN{internal.addJob(@FA{queue}, @FA{parameter})}
////////////////////////////////////////////////////////////////////////////////

static void JS_AddJob (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  
  if (args.Length() != 2 || ! args[1]->IsObject()) {
    TRI_V8_THROW_EXCEPTION_USAGE("addJob(<queue>, <options>)");
  }

  string name = TRI_ObjectToString(args[0]);
  TRI_json_t* parameters = TRI_ObjectToJson(isolate, args[1]);

  TRI_GET_GLOBALS();

  V8QueueJob* job = new V8QueueJob(name,
                                   static_cast<TRI_vocbase_t*>(v8g->_vocbase),
                                   GlobalV8Dealer,
                                   parameters);

  int result = GlobalDispatcher->addJob(job);

  if (result == TRI_ERROR_QUEUE_FULL) {
    TRI_V8_RETURN_FALSE();
  }

  if (result != TRI_ERROR_NO_ERROR) {
    delete job;

    TRI_V8_THROW_EXCEPTION_MESSAGE(result, "cannot add job");
  }

  TRI_V8_RETURN_TRUE();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief stores the V8 dispatcher function inside the global variable
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Dispatcher (v8::Isolate* isolate,
                           v8::Handle<v8::Context> context,
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
    TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("SYS_REGISTER_TASK"), JS_RegisterTask);
    TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("SYS_UNREGISTER_TASK"), JS_UnregisterTask);
    TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("SYS_GET_TASK"), JS_GetTask);
    TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("SYS_CREATE_NAMED_QUEUE"), JS_CreateNamedQueue);
    TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("SYS_ADD_JOB"), JS_AddJob);
  }
  else {
    LOG_ERROR("cannot initialise tasks, scheduler or dispatcher unknown");
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
