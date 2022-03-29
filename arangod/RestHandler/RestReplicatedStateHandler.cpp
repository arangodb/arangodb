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
#include "RestReplicatedStateHandler.h"

#include "Futures/Future.h"
#include "Replication2/Methods.h"
#include "Replication2/ReplicatedState/StateStatus.h"
#include "Rest/PathMatch.h"

arangodb::RestReplicatedStateHandler::RestReplicatedStateHandler(
    arangodb::ArangodServer& server, arangodb::GeneralRequest* request,
    arangodb::GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

auto arangodb::RestReplicatedStateHandler::execute() -> arangodb::RestStatus {
  // for now required admin access to the database
  if (!ExecContext::current().isAdminUser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    return RestStatus::DONE;
  }

  auto methods = replication2::ReplicatedStateMethods::createInstance(_vocbase);
  return executeByMethod(*methods);
}

auto arangodb::RestReplicatedStateHandler::executeByMethod(
    arangodb::replication2::ReplicatedStateMethods const& methods)
    -> arangodb::RestStatus {
  switch (_request->requestType()) {
    case rest::RequestType::GET:
      return handleGetRequest(methods);
    case rest::RequestType::POST:
      return handlePostRequest(methods);
    case rest::RequestType::DELETE_REQ:
      return handleDeleteRequest(methods);
    default:
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                    TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  }
  return RestStatus::DONE;
}

auto arangodb::RestReplicatedStateHandler::handleGetRequest(
    replication2::ReplicatedStateMethods const& methods)
    -> arangodb::RestStatus {
  std::vector<std::string> const& suffixes = _request->suffixes();
  if (suffixes.size() < 2 || suffixes[1] != "local-status") {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting _api/replicated-state/<state-id>/local-status");
    return RestStatus::DONE;
  }

  replication2::LogId logId{basics::StringUtils::uint64(suffixes[0])};
  return waitForFuture(
      methods.getLocalStatus(logId).thenValue([this](auto&& status) {
        VPackBuilder buffer;
        status.toVelocyPack(buffer);
        generateOk(rest::ResponseCode::OK, buffer.slice());
      }));
}

auto arangodb::RestReplicatedStateHandler::handlePostRequest(
    replication2::ReplicatedStateMethods const& methods)
    -> arangodb::RestStatus {
  auto const& suffixes = _request->suffixes();

  if (suffixes.empty()) {
    bool parseSuccess = false;
    VPackSlice body = this->parseVPackBody(parseSuccess);
    if (!parseSuccess) {  // error message generated in parseVPackBody
      return RestStatus::DONE;
    }

    // create a new state
    auto spec =
        replication2::replicated_state::agency::Target::fromVelocyPack(body);
    return waitForFuture(methods.createReplicatedState(std::move(spec))
                             .thenValue([this](auto&& result) {
                               if (result.ok()) {
                                 generateOk(rest::ResponseCode::OK,
                                            VPackSlice::emptyObjectSlice());
                               } else {
                                 generateError(result);
                               }
                             }));
  } else if (std::string_view logIdStr, toRemoveStr, toAddStr;
             rest::Match(suffixes).against(&logIdStr, "participant",
                                           &toRemoveStr, "replace-with",
                                           &toAddStr)) {
    auto const logId = replication2::LogId::fromString(logIdStr);
    auto const toRemove = replication2::ParticipantId(toRemoveStr);
    auto const toAdd = replication2::ParticipantId(toAddStr);
    if (!logId) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    basics::StringUtils::concatT("Not a log id: ", logIdStr));
      return RestStatus::DONE;
    }
    return waitForFuture(methods.replaceParticipant(*logId, toRemove, toAdd)
                             .thenValue([this](auto&& result) {
                               if (result.ok()) {
                                 generateOk(rest::ResponseCode::OK,
                                            VPackSlice::emptyObjectSlice());
                               } else {
                                 generateError(result);
                               }
                             }));
    return RestStatus::DONE;
  } else if (std::string_view logIdStr, newLeaderStr;
             rest::Match(suffixes).against(&logIdStr, "leader",
                                           &newLeaderStr)) {
    auto const logId = replication2::LogId::fromString(logIdStr);
    auto const newLeader = replication2::ParticipantId(newLeaderStr);
    if (!logId) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    basics::StringUtils::concatT("Not a log id: ", logIdStr));
      return RestStatus::DONE;
    }
    return waitForFuture(
        methods.setLeader(*logId, newLeader).thenValue([this](auto&& result) {
          if (result.ok()) {
            generateOk(rest::ResponseCode::OK, VPackSlice::emptyObjectSlice());
          } else {
            generateError(result);
          }
        }));
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
    return RestStatus::DONE;
  }
}

auto arangodb::RestReplicatedStateHandler::handleDeleteRequest(
    arangodb::replication2::ReplicatedStateMethods const& methods)
    -> arangodb::RestStatus {
  auto const& suffixes = _request->suffixes();
  if (std::string_view logIdStr;
      rest::Match(suffixes).against(&logIdStr, "leader")) {
    auto const logId = replication2::LogId::fromString(logIdStr);
    if (!logId) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    basics::StringUtils::concatT("Not a log id: ", logIdStr));
      return RestStatus::DONE;
    }
    return waitForFuture(methods.setLeader(*logId, std::nullopt)
                             .thenValue([this](auto&& result) {
                               if (result.ok()) {
                                 generateOk(rest::ResponseCode::OK,
                                            VPackSlice::emptyObjectSlice());
                               } else {
                                 generateError(result);
                               }
                             }));
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
    return RestStatus::DONE;
  }
}
