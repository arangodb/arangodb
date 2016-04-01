////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "WorkMonitorHandler.h"

#include "Basics/StringUtils.h"
#include "HttpServer/HttpHandler.h"
#include "Rest/HttpRequest.h"
#include "velocypack/Builder.h"
#include "velocypack/velocypack-aliases.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

using arangodb::HttpRequest;
using arangodb::rest::HttpHandler;

WorkMonitorHandler::WorkMonitorHandler(HttpRequest* request)
    : RestBaseHandler(request) {}

bool WorkMonitorHandler::isDirect() const { return true; }

HttpHandler::status_t WorkMonitorHandler::execute() {
  auto suffix = _request->suffix();
  size_t const len = suffix.size();
  auto const type = _request->requestType();

  if (type == GeneralRequest::RequestType::GET) {
    if (len != 0) {
      generateError(GeneralResponse::ResponseCode::BAD,
                    TRI_ERROR_HTTP_BAD_PARAMETER,
                    "expecting GET /_admin/work-monitor");
      return status_t(HANDLER_DONE);
    }

    WorkMonitor::requestWorkOverview(_taskId);
    return status_t(HANDLER_ASYNC);
  }

  if (type == GeneralRequest::RequestType::DELETE_REQ) {
    if (len != 1) {
      generateError(GeneralResponse::ResponseCode::BAD,
                    TRI_ERROR_HTTP_BAD_PARAMETER,
                    "expecting DELETE /_admin/work-monitor/<id>");

      return status_t(HANDLER_DONE);
    }

    uint64_t id = StringUtils::uint64(suffix[0]);
    WorkMonitor::cancelWork(id);

    VPackBuilder b;
    b.add(VPackValue(VPackValueType::Object));
    b.add("canceled", VPackValue(true));
    b.close();

    VPackSlice s(b.start());

    generateResult(GeneralResponse::ResponseCode::OK, s);
    return status_t(HANDLER_DONE);
  }

  generateError(GeneralResponse::ResponseCode::BAD,
                TRI_ERROR_HTTP_METHOD_NOT_ALLOWED, "expecting GET or DELETE");
  return status_t(HANDLER_DONE);
}
