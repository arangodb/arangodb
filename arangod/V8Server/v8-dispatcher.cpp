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
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/Logger.h"
#include "Scheduler/JobGuard.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Transaction/Hints.h"
#include "Transaction/V8Context.h"
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
  static void removeTasksForDatabase(std::string const&);

 private:
  static Mutex _tasksLock;
  static std::unordered_map<std::string, std::shared_ptr<V8Task>> _tasks;

 public:
  V8Task(std::string const& id, std::string const& name, TRI_vocbase_t*,
         std::string const& command, bool allowUseDatabase);
  ~V8Task();

 public:
  void setOffset(double offset);
  void setPeriod(double offset, double period);
  void setParameter(
      std::shared_ptr<arangodb::velocypack::Builder> const& parameters);
  void setUser(std::string const& user);

  void start();
  void cancel();

  std::shared_ptr<VPackBuilder> toVelocyPack() const;

  bool databaseMatches(std::string const&) const;

 private:
  void toVelocyPack(VPackBuilder&) const;
  void work();
  void queue(std::chrono::microseconds offset);
  void unqueue() noexcept;
  std::function<void(boost::system::error_code const&)> callbackFunction();
  std::string const& name() const { return _name; }

 private:
  std::string const _id;
  std::string const _name;
  double const _created;
  std::string _user;

  std::unique_ptr<boost::asio::steady_timer> _timer;

  // guard to make sure the database is not dropped while used by us
  std::unique_ptr<VocbaseGuard> _vocbaseGuard;

  std::string const _command;
  std::shared_ptr<arangodb::velocypack::Builder> _parameters;
  bool const _allowUseDatabase;

  std::chrono::microseconds _offset;
  std::chrono::microseconds _interval;
  bool _periodic = false;

  Mutex _queueMutex;
  bool _queued; 
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

void V8Task::removeTasksForDatabase(std::string const& name) {
  MUTEX_LOCKER(guard, _tasksLock);
    
  for (auto it = _tasks.begin(); it != _tasks.end(); /* no hoisting */) {
    if (!(*it).second->databaseMatches(name)) {
      ++it;
    } else {
      auto task = (*it).second;
      task->cancel();
      it = _tasks.erase(it);
    }
  }
}
  
bool V8Task::databaseMatches(std::string const& name) const {
  return (_vocbaseGuard->vocbase()->name() == name);
}

V8Task::V8Task(std::string const& id, std::string const& name,
               TRI_vocbase_t* vocbase, std::string const& command,
               bool allowUseDatabase)
    : _id(id),
      _name(name),
      _created(TRI_microtime()),
      _vocbaseGuard(new VocbaseGuard(vocbase)),
      _command(command),
      _allowUseDatabase(allowUseDatabase),
      _offset(0),
      _interval(0),
      _queued(false) {}

V8Task::~V8Task() { unqueue(); }

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

void V8Task::setUser(std::string const& user) {
  _user = user;
}

std::function<void(const boost::system::error_code&)>
V8Task::callbackFunction() {
  auto self = shared_from_this();

  return [self, this](const boost::system::error_code& error) {
    unqueue();
    
    // First tell the scheduler that this thread is working:
    JobGuard guard(SchedulerFeature::SCHEDULER);
    guard.work();

    if (error) {
      MUTEX_LOCKER(guard, _tasksLock);

      auto itr = _tasks.find(_id);

      if (itr != _tasks.end()) {
        // remove task from list of tasks if it is still active
        if (this == (*itr).second.get()) {
          // still the same task. must remove from map
          _tasks.erase(itr);
        }
      }
      return;
    }

    // get the permissions to be used by this task
    AuthLevel dbLvl = AuthLevel::RW;
    std::unique_ptr<ExecContext> execContext;
    TRI_DEFER(ExecContext::CURRENT = nullptr);
    if (!_user.empty()) { // not superuser
      AuthenticationFeature* auth = AuthenticationFeature::INSTANCE;
      std::string const& dbname = _vocbaseGuard->vocbase()->name();
      dbLvl = auth->canUseDatabase(_user, dbname);
      execContext.reset(new ExecContext(_user, dbname,
                                        auth->canUseDatabase(_user, TRI_VOC_SYSTEM_DATABASE),
                                        dbLvl));
      ExecContext::CURRENT = execContext.get();
    }

    // permissions might have changed since starting this task
    if (SchedulerFeature::SCHEDULER->isStopping() || dbLvl != AuthLevel::RW) {
      V8Task::unregisterTask(_id, false);
      return;
    }

    // now do the work:
    work();

    if (_periodic && !SchedulerFeature::SCHEDULER->isStopping()) {
      // requeue the task
      queue(_interval);
    } else {
      // in case of one-off tasks or in case of a shutdown, simply
      // remove the task from the list
      V8Task::unregisterTask(_id, false);
    }
  };
}

void V8Task::start() {
  TRI_ASSERT(ExecContext::CURRENT == nullptr ||
             (!_user.empty() && ExecContext::CURRENT->user() == _user));
  
  auto ioService = SchedulerFeature::SCHEDULER->ioService();
  _timer.reset(new boost::asio::steady_timer(*ioService));
  if (_offset.count() <= 0) {
    _offset = std::chrono::microseconds(1);
  }

  // initially queue the task
  queue(_offset);
}

void V8Task::queue(std::chrono::microseconds offset) {
  {
    MUTEX_LOCKER(locker, _queueMutex);
    TRI_ASSERT(!_queued);
    _queued = true; 
  }

  SchedulerFeature::SCHEDULER->queueJob();

  _timer->expires_from_now(offset);
  _timer->async_wait(callbackFunction());
}

void V8Task::unqueue() noexcept {
  bool wasQueued;

  {
    MUTEX_LOCKER(locker, _queueMutex);
    wasQueued = _queued;
    if (wasQueued) {
      _queued = false;
    }
  }
    
  if (wasQueued && SchedulerFeature::SCHEDULER != nullptr) {
    SchedulerFeature::SCHEDULER->unqueueJob();
  }
}

void V8Task::cancel() {
  // this will prevent the task from dispatching itself again
  _periodic = false;

  boost::system::error_code ec;
  _timer->cancel(ec);

  unqueue();
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
            LOG_TOPIC(WARN, arangodb::Logger::FIXME)
                << "caught non-catchable exception (aka termination) in job";
          }
        }
      } catch (arangodb::basics::Exception const& ex) {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "caught exception in V8 user task: "
                 << TRI_errno_string(ex.code()) << " " << ex.what();
      } catch (std::bad_alloc const&) {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "caught exception in V8 user task: "
                 << TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY);
      } catch (...) {
        LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "caught unknown exception in V8 user task";
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
// --SECTION--                                              Javascript functions
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

  if (ExecContext::CURRENT != nullptr &&
      ExecContext::CURRENT->databaseAuthLevel() != AuthLevel::RW) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "registerTask() needs db RW permissions");
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

  std::string runAsUser;
  if (obj->HasOwnProperty(TRI_V8_ASCII_STRING("runAsUser"))) {
    runAsUser = TRI_ObjectToString(obj->Get(TRI_V8_ASCII_STRING("runAsUser")));
  }
  // only the superroot is allowed to run tasks as an arbitrary user
  if (ExecContext::CURRENT != nullptr) {
    if (runAsUser.empty()) { // execute task as the same use
      runAsUser = ExecContext::CURRENT->user();
    } else if (ExecContext::CURRENT->user() != runAsUser) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
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
      V8Task::createTask(id, name, v8g->_vocbase, command, isSystem, res);

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
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("unregister(<id>)");
  }

  if (ExecContext::CURRENT != nullptr &&
      ExecContext::CURRENT->databaseAuthLevel() != AuthLevel::RW) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "unregisterTask() needs db RW permissions");
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

/// creates a new object in _queues
static void JS_CreateQueue(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("createQueue(<id>)");
  }

  std::string runAsUser;
  if (ExecContext::CURRENT != nullptr) {
    if (ExecContext::CURRENT->databaseAuthLevel() != AuthLevel::RW) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                     "createQueue() needs db RW permissions");
    }
    runAsUser = ExecContext::CURRENT->user();
  }
  // stupid trick to force superuser rights
  ExecContext *exe = ExecContext::CURRENT;
  ExecContext::CURRENT = nullptr;
  TRI_DEFER(ExecContext::CURRENT = exe);

  VPackBuilder docs;
  {
    int res = TRI_V8ToVPack(isolate, docs, args[0], true);
    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }
  docs.add("runAsUser", VPackValue(runAsUser));
  docs.close();

  TRI_GET_GLOBALS();

  auto transactionContext = std::make_shared<transaction::V8Context>(v8g->_vocbase, true);
  SingleCollectionTransaction trx(transactionContext, "_queues",
                                  AccessMode::Type::EXCLUSIVE);
  Result res = trx.begin();
  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  OperationOptions opts;
  OperationResult result = trx.insert("_queues", docs.slice(), opts);
  if (!result.successful() &&
      result.code == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) {
    result = trx.replace("_queues", docs.slice(), opts);
  }
  res = trx.finish(result.code);
  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN(v8::Boolean::New(isolate, result.successful()));
  TRI_V8_TRY_CATCH_END
}

static void JS_DeleteQueue(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("deleteQueue(<id>)");
  }
  std::string key = TRI_ObjectToString(args[0]);
  VPackBuilder keyB;
  keyB(VPackValue(VPackValueType::Object))(StaticStrings::KeyString, VPackValue(key))();

  ExecContext *exe = ExecContext::CURRENT;
  if (exe != nullptr && exe->databaseAuthLevel() != AuthLevel::RW) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   "deleteQueue() needs db RW permissions");
  }
  // stupid trick to force superuser rights
  ExecContext::CURRENT = nullptr;
  TRI_DEFER(ExecContext::CURRENT = exe);

  TRI_GET_GLOBALS();

  auto transactionContext = std::make_shared<transaction::V8Context>(v8g->_vocbase, true);
  SingleCollectionTransaction trx(transactionContext, "_queues",
                                  AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);
  Result res = trx.begin();
  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  OperationOptions opts;
  OperationResult result = trx.remove("_queues", keyB.slice(), opts);
  res = trx.finish(result.code);
  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN(v8::Boolean::New(isolate, result.successful()));
  TRI_V8_TRY_CATCH_END
}

// -----------------------------------------------------------------------------
// --SECTION--                                             module initialization
// -----------------------------------------------------------------------------

void TRI_InitV8Dispatcher(v8::Isolate* isolate,
                          v8::Handle<v8::Context> context) {
  v8::HandleScope scope(isolate);

  // _queues is a RO collection and can only be written in C++, as superroot
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_CREATE_QUEUE"),
                               JS_CreateQueue);

  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_DELETE_QUEUE"),
                               JS_DeleteQueue);

  // we need a scheduler and a dispatcher to define periodic tasks
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_REGISTER_TASK"),
                               JS_RegisterTask);

  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_UNREGISTER_TASK"),
                               JS_UnregisterTask);

  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING("SYS_GET_TASK"), JS_GetTask);
}

void TRI_ShutdownV8Dispatcher() { V8Task::shutdownTasks(); }

void TRI_RemoveDatabaseTasksV8Dispatcher(std::string const& name) { V8Task::removeTasksForDatabase(name); }
