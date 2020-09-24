////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#include "MaintenanceRestHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StringUtils.h"
#include "Basics/conversions.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/DBServerAgencySync.h"
#include "Cluster/HeartbeatThread.h"
#include "Cluster/MaintenanceFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

#include "velocypack/Iterator.h"
#include "velocypack/velocypack-aliases.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::rest;

MaintenanceRestHandler::MaintenanceRestHandler(application_features::ApplicationServer& server,
                                               GeneralRequest* request,
                                               GeneralResponse* response)
    : RestBaseHandler(server, request, response) {}

RestStatus MaintenanceRestHandler::execute() {
  // extract the sub-request type
  auto const type = _request->requestType();

  switch (type) {
    // retrieve list of all actions
    case rest::RequestType::GET:
      getAction();
      break;

    // administrative commands for hot restore 
    case rest::RequestType::POST:
      return postAction();
      break;

    // add an action to the list (or execute it directly)
    case rest::RequestType::PUT:
      putAction();
      break;

    // remove an action, stopping it if executing
    case rest::RequestType::DELETE_REQ:
      deleteAction();
      break;

    default:
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                    (int)rest::ResponseCode::METHOD_NOT_ALLOWED);
      break;
  }  // switch

  return RestStatus::DONE;
}

RestStatus MaintenanceRestHandler::postAction() {
  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {  // error message generated in parseVPackBody
    return RestStatus::DONE;
  }
  
  LOG_TOPIC("a0212", DEBUG, Logger::MAINTENANCE) << "parsed post action " << body.toJson();
  
  std::stringstream es;

  if (body.isObject()) {
    std::chrono::seconds dur(0);
    if (body.get("execute").isString()) {
      // {"execute": "pause", "duration": 60} / {"execute": "proceed"}
      auto const ex = body.get("execute").copyString();
      if (ex == "pause") {
        if (body.get("duration").isNumber()) {
          dur = std::chrono::seconds(body.get("duration").getNumber<int64_t>());
          if (dur.count() <= 0 || dur.count() > 300) {
            es << "invalid mainenance pause duration: " << dur.count()
                  << " seconds";
          }
          // Pause maintenance
          LOG_TOPIC("1ee7a", DEBUG, Logger::MAINTENANCE)
            << "Maintenance is paused for " << dur.count() << " seconds";
          server().getFeature<MaintenanceFeature>().pause(dur);
        }
      } else if (ex == "proceed") {
        LOG_TOPIC("6c38a", DEBUG, Logger::MAINTENANCE)
          << "Maintenance is prceeded "  << dur.count() << " seconds";
        server().getFeature<MaintenanceFeature>().proceed();
      } else {
        es << "invalid POST command";
          }
    } else {
      es << "invalid POST object";
    }
  } else {
    es << "invalid POST body";
  }

  if (es.str().empty()) {
    VPackBuilder ok;
    {
      VPackObjectBuilder o(&ok);
      ok.add(StaticStrings::Error, VPackValue(false));
      ok.add(StaticStrings::Code, VPackValue(200));
      ok.add("result", VPackValue(true));
    }
    generateResult(rest::ResponseCode::OK, ok.slice());
  } else {
    LOG_TOPIC("9faa1", ERR, Logger::MAINTENANCE) << es.str(); 
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER, es.str());
  }

  return RestStatus::DONE;

}

void MaintenanceRestHandler::putAction() {
  bool good(true);
  VPackSlice parameters;

  try {
    parameters = _request->payload();
  } catch (VPackException const& ex) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  std::string(
                      "expecting a valid JSON object in the request. got: ") +
                      ex.what());
    good = false;
  }  // catch

  if (good && _request->payload().isEmptyObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_CORRUPTED_JSON);
    good = false;
  }

  // convert vpack into key/value map
  if (good) {
    good = parsePutBody(parameters);

    // bad json
    if (!good) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    std::string(
                        "unable to parse JSON object into key/value pairs."));
    }  // if
  }    // if

  if (good) {
    Result result;

    // build the action
    auto& maintenance = server().getFeature<MaintenanceFeature>();
    result = maintenance.addAction(_actionDesc);

    if (!result.ok()) {
      // possible errors? TRI_ERROR_BAD_PARAMETER    TRI_ERROR_TASK_DUPLICATE_ID
      // TRI_ERROR_SHUTTING_DOWN
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    result.errorMessage());
    }  // if
  }    // if

}  // MaintenanceRestHandler::putAction

bool MaintenanceRestHandler::parsePutBody(VPackSlice const& parameters) {
  bool good(true);

  std::map<std::string, std::string> desc;
  auto prop = std::make_shared<VPackBuilder>();
  int priority = 1;
  bool forced = false;

  VPackObjectIterator it(parameters, true);
  for (; it.valid() && good; ++it) {
    VPackSlice key, value;

    key = it.key();
    value = it.value();

    // attempt insert into map ... but needs to be unique
    if (key.isString() && value.isString()) {
      good = desc.insert({key.copyString(), value.copyString()}).second;
    } else if (key.isString() && (key.copyString() == "properties") && value.isObject()) {
      // code here
      prop.reset(new VPackBuilder(value));
    } else if (key.isString() && (key.copyString() == "priority") && value.isInteger()) {
      priority = static_cast<int>(value.getInt());
    } else if (key.isString() && (key.stringRef() == "forced") && value.isBool()) {
      forced = value.isTrue();
    } else {
      good = false;
    }  // else
  }    // for

  _actionDesc = std::make_shared<maintenance::ActionDescription>(std::move(desc), priority, forced, std::move(prop));

  return good;

}  // MaintenanceRestHandler::parsePutBody

void MaintenanceRestHandler::getAction() {
  // build the action
  auto& maintenance = server().getFeature<MaintenanceFeature>();
  VPackBuilder builder;
  {
    VPackObjectBuilder o(&builder);
    builder.add("status", VPackValue(maintenance.isPaused() ? "paused" : "running"));
    builder.add(VPackValue("registry"));
    maintenance.toVelocyPack(builder);
  }

  generateResult(rest::ResponseCode::OK, builder.slice());

}  // MaintenanceRestHandler::getAction

void MaintenanceRestHandler::deleteAction() {
  auto& maintenance = server().getFeature<MaintenanceFeature>();

  std::vector<std::string> const& suffixes = _request->suffixes();

  // must be one parameter: "all" or number
  if (1 == suffixes.size()) {
    std::string param = suffixes[0];
    Result result;

    if (param == "all") {
      // Jobs supports all.  Should Action too?  Going with "no" for now.
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    result.errorMessage());
    } else {
      uint64_t action_id = StringUtils::uint64(param);
      result = maintenance.deleteAction(action_id);

      // can fail on bad id or if action already succeeded.
      if (!result.ok()) {
        generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                      result.errorMessage());
      }  // if
    }    // else

  } else {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
  }

}  // MaintenanceRestHandler::deleteAction
