////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "RestLogInternalHandler.h"

#include "Replication2/ReplicatedLog/NetworkMessages.h"
#include "Replication2/ReplicatedLog/LogFollower.h"

using namespace arangodb;
using namespace arangodb::replication2;

RestLogInternalHandler::RestLogInternalHandler(
    application_features::ApplicationServer& server, GeneralRequest* req,
    GeneralResponse* resp)
    : RestVocbaseBaseHandler(server, req, resp) {}
RestLogInternalHandler::~RestLogInternalHandler() = default;

RestStatus RestLogInternalHandler::execute() {
  // for now required admin access to the database
  if (!ExecContext::current().isSuperuser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    return RestStatus::DONE;
  }

  if (_request->requestType() != rest::RequestType::POST) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  if (suffixes.size() != 2 || suffixes[1] != "append-entries") {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect POST /_api/log-internal/<log-id>/append-entries");
    return RestStatus::DONE;
  }

  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {  // error message generated in parseVPackBody
    return RestStatus::DONE;
  }

  LogId logId{basics::StringUtils::uint64(suffixes[0])};
  auto request = replicated_log::AppendEntriesRequest::fromVelocyPack(body);
  auto f = _vocbase.getReplicatedLogFollowerById(logId)
               ->appendEntries(request)
               .thenValue([this](replicated_log::AppendEntriesResult&& res) {
                 VPackBuilder builder;
                 res.toVelocyPack(builder);
                 // TODO fix the result type here. Currently we always return
                 // the error under the
                 //      `result` field. Maybe we want to change the HTTP status
                 //      code as well? Don't forget to update the deserializer
                 //      that reads the response!
                 generateOk(rest::ResponseCode::ACCEPTED, builder.slice());
               });

  return waitForFuture(std::move(f));
}
