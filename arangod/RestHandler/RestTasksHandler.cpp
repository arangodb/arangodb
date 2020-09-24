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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "RestTasksHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "V8/JavaScriptSecurityContext.h"
#include "V8/v8-globals.h"
#include "V8/v8-vpack.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/Methods/Tasks.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::basics;
using namespace arangodb::rest;

namespace arangodb {

RestTasksHandler::RestTasksHandler(application_features::ApplicationServer& server,
                                   GeneralRequest* request, GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

RestStatus RestTasksHandler::execute() {
  if (!V8DealerFeature::DEALER || !V8DealerFeature::DEALER->isEnabled()) {
    generateError(rest::ResponseCode::NOT_IMPLEMENTED, TRI_ERROR_NOT_IMPLEMENTED, "JavaScript operations are disabled");
    return RestStatus::DONE;
  }

  auto const type = _request->requestType();

  switch (type) {
    case rest::RequestType::POST: {
      registerTask(false);
      break;
    }
    case rest::RequestType::PUT: {
      registerTask(true);
      break;
    }
    case rest::RequestType::DELETE_REQ: {
      deleteTask();
      break;
    }
    case rest::RequestType::GET: {
      getTasks();
      break;
    }
    default: {
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    }
  }
  return RestStatus::DONE;
}

/// @brief returns the short id of the server which should handle this request
ResultT<std::pair<std::string, bool>> RestTasksHandler::forwardingTarget() {
  auto base = RestVocbaseBaseHandler::forwardingTarget();
  if (base.ok() && !std::get<0>(base.get()).empty()) {
    return base;
  }

  rest::RequestType const type = _request->requestType();
  if (type != rest::RequestType::POST && type != rest::RequestType::PUT &&
      type != rest::RequestType::GET && type != rest::RequestType::DELETE_REQ) {
    return {std::make_pair(StaticStrings::Empty, false)};
  }

  std::vector<std::string> const& suffixes = _request->suffixes();
  if (suffixes.size() < 1) {
    return {std::make_pair(StaticStrings::Empty, false)};
  }

  uint64_t tick = arangodb::basics::StringUtils::uint64(suffixes[0]);
  uint32_t sourceServer = TRI_ExtractServerIdFromTick(tick);

  if (sourceServer == ServerState::instance()->getShortId()) {
    return {std::make_pair(StaticStrings::Empty, false)};
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  return {std::make_pair(ci.getCoordinatorByShortID(sourceServer), false)};
}

void RestTasksHandler::getTasks() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  if (suffixes.size() > 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "superfluous parameter, expecting /_api/tasks[/<id>]");
    return;
  }

  std::shared_ptr<VPackBuilder> builder;

  if (suffixes.size() == 1) {
    // get a single task
    builder = Task::registeredTask(suffixes[0]);
  } else {
    // get all tasks
    builder = Task::registeredTasks();
  }

  if (builder == nullptr) {
    generateError(TRI_ERROR_TASK_NOT_FOUND);
    return;
  }

  generateResult(rest::ResponseCode::OK, builder->slice());
}

void RestTasksHandler::registerTask(bool byId) {
  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    // error message generated in parseVPackBody
    return;
  }

  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  // job id
  std::string id;

  if (byId) {
    if ((suffixes.size() != 1) || suffixes[0].empty()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                    "expected PUT /_api/tasks/<task-id>");
      return;
    } else {
      id = suffixes[0];
    }
  }

  ExecContext const& exec = ExecContext::current();
  if (exec.databaseAuthLevel() != auth::Level::RW) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN,
                  "registering a task needs db RW permissions");
    return;
  }

  // job id
  if (id.empty()) {
    id = VelocyPackHelper::getStringValue(body, "id",
                                          std::to_string(TRI_NewServerSpecificTick()));
  }

  // job name
  std::string name =
      VelocyPackHelper::getStringValue(body, "name", "user-defined task");

  bool isSystem = VelocyPackHelper::getBooleanValue(body, StaticStrings::DataSourceSystem, false);

  // offset in seconds into period or from now on if no period
  double offset = VelocyPackHelper::getNumericValue<double>(body, "offset", 0.0);

  // period in seconds & count
  double period = 0.0;
  auto periodSlice = body.get("period");
  if (periodSlice.isNumber()) {
    period = VelocyPackHelper::getNumericValue<double>(body, "period", 0.0);
    if (period <= 0.0) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                    "task period must be specified and positive");
      return;
    }
  }

  std::string runAsUser =
      VelocyPackHelper::getStringValue(body, "runAsUser", "");

  // only the superroot is allowed to run tasks as an arbitrary user
  if (runAsUser.empty()) {  // execute task as the same user
    runAsUser = exec.user();
  } else if (exec.user() != runAsUser) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN,
                  "cannot run task as a different user");
    return;
  }

  // extract the command
  std::string command;
  auto cmdSlice = body.get("command");
  if (!cmdSlice.isString()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "command must be specified");
    return;
  }

  try {
    JavaScriptSecurityContext securityContext = JavaScriptSecurityContext::createRestrictedContext();
    V8ContextGuard guard(&_vocbase, securityContext);
   
    v8::Isolate* isolate = guard.isolate();
    v8::HandleScope scope(isolate);
    auto context = TRI_IGETC;
    v8::Handle<v8::Object> bv8 = TRI_VPackToV8(isolate, body).As<v8::Object>();

    if (bv8->Get(context, TRI_V8_ASCII_STRING(isolate, "command")).FromMaybe(v8::Handle<v8::Value>())->IsFunction()) {
      // need to add ( and ) around function because call will otherwise break
      command = "(" + cmdSlice.copyString() + ")(params)";
    } else {
      command = cmdSlice.copyString();
    }

    if (!Task::tryCompile(isolate, command)) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                    "cannot compile command");
      return;
    }
  } catch (arangodb::basics::Exception const& ex) {
    generateError(Result{ex.code(), ex.what()});
    return;
  } catch (std::exception const& ex) {
    generateError(Result{TRI_ERROR_INTERNAL, ex.what()});
    return;
  } catch (...) {
    generateError(TRI_ERROR_INTERNAL);
    return;
  }

  // extract the parameters
  auto parameters = std::make_shared<VPackBuilder>(body.get("params"));

  command = "(function (params) { " + command + " } )(params);";

  int res;
  std::shared_ptr<Task> task =
      Task::createTask(id, name, &_vocbase, command, isSystem, res);

  if (res != TRI_ERROR_NO_ERROR) {
    generateError(res);
    return;
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
    generateError(TRI_ERROR_INTERNAL);
    return;
  }

  task->start();

  generateResult(rest::ResponseCode::OK, builder->slice());
}

void RestTasksHandler::deleteTask() {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  if ((suffixes.size() != 1) || suffixes[0].empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "bad parameter, expecting /_api/tasks/<id>");
    return;
  }

  ExecContext const& exec = ExecContext::current();
  if (exec.databaseAuthLevel() != auth::Level::RW) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN,
                  "unregister task needs db RW permissions");
    return;
  }

  int res = Task::unregisterTask(suffixes[0], true);
  if (res != TRI_ERROR_NO_ERROR) {
    generateError(res);
    return;
  }
  generateOk(rest::ResponseCode::OK, velocypack::Slice());
}

}  // namespace arangodb
