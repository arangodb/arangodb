////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RestAdminStatisticsHandler.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "Statistics/Descriptions.h"
#include "Statistics/StatisticsFeature.h"
#include "Utils/ExecContext.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestAdminStatisticsHandler::RestAdminStatisticsHandler(GeneralRequest* request,
                                                       GeneralResponse* response)
    : RestBaseHandler(request, response) {}

RestStatus RestAdminStatisticsHandler::execute() {
  if (_request->requestType() != rest::RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }
  
  ServerSecurityFeature* security =
      application_features::ApplicationServer::getFeature<ServerSecurityFeature>(
          "ServerSecurity");
  TRI_ASSERT(security != nullptr);

  bool hardened = security->isRestApiHardened();
  bool allowInfo = !hardened;  // allow access if harden flag was not given

  ExecContext const* exec = ExecContext::CURRENT;
  if (exec == nullptr || exec->isAdminUser()) {
    // also allow access if there is not authentication
    // enabled or when the user is an administrator
    allowInfo = true;
  }
    
  if (!allowInfo) {
    // dont leak information about server internals here
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN); 
    return RestStatus::DONE;
  }

  if (_request->requestPath() == "/_admin/statistics") {
    getStatistics();
  } else if (_request->requestPath() == "/_admin/statistics-description") {
    getStatisticsDescription();
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
  }

  // this handler is done
  return RestStatus::DONE;
}

void RestAdminStatisticsHandler::getStatistics() {
  stats::Descriptions const* desc = StatisticsFeature::descriptions();
  if (!desc) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_DISABLED,
                  "statistics not enabled");
    return;
  }

  VPackBuffer<uint8_t> buffer;
  VPackBuilder tmp(buffer);
  tmp.add(VPackValue(VPackValueType::Object, true));

  tmp.add("time", VPackValue(TRI_microtime()));
  tmp.add("enabled", VPackValue(StatisticsFeature::enabled()));

  tmp.add("system", VPackValue(VPackValueType::Object, true));
  desc->processStatistics(tmp);
  tmp.close();  // system

  tmp.add("client", VPackValue(VPackValueType::Object, true));
  desc->clientStatistics(tmp);
  tmp.close();  // client

  tmp.add("http", VPackValue(VPackValueType::Object, true));
  desc->httpStatistics(tmp);
  tmp.close();  // http

  tmp.add("server", VPackValue(VPackValueType::Object, true));
  desc->serverStatistics(tmp);
  tmp.close();  // server

  tmp.add(StaticStrings::Error, VPackValue(false));
  tmp.add(StaticStrings::Code, VPackValue(static_cast<int>(ResponseCode::OK)));
  tmp.close();  // outer
  generateResult(ResponseCode::OK, std::move(buffer));
}

void RestAdminStatisticsHandler::getStatisticsDescription() {
  stats::Descriptions const* desc = StatisticsFeature::descriptions();
  if (!desc) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_DISABLED,
                  "statistics not enabled");
    return;
  }

  VPackBuffer<uint8_t> buffer;
  VPackBuilder tmp(buffer);
  tmp.add(VPackValue(VPackValueType::Object));

  tmp.add("groups", VPackValue(VPackValueType::Array, true));
  for (stats::Group const& group : desc->groups()) {
    tmp.openObject();
    group.toVPack(tmp);
    tmp.close();
  }
  tmp.close();  // groups

  tmp.add("figures", VPackValue(VPackValueType::Array, true));
  for (stats::Figure const& figure : desc->figures()) {
    tmp.openObject();
    figure.toVPack(tmp);
    tmp.close();
  }
  tmp.close();  // figures

  tmp.add(StaticStrings::Error, VPackValue(false));
  tmp.add(StaticStrings::Code, VPackValue(static_cast<int>(ResponseCode::OK)));
  tmp.close();  // outer
  generateResult(ResponseCode::OK, std::move(buffer));
}
