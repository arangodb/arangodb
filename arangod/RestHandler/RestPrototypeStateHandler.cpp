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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#include "RestPrototypeStateHandler.h"

#include <velocypack/Iterator.h>

#include <Basics/StringUtils.h>
#include <Network/Methods.h>
#include <Network/NetworkFeature.h>
#include <Cluster/ClusterFeature.h>
#include "Inspection/VPack.h"

#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/StateMachines/Prototype/PrototypeStateMethods.h"

using namespace arangodb;
using namespace arangodb::replication2;

RestPrototypeStateHandler::RestPrototypeStateHandler(ArangodServer& server,
                                                     GeneralRequest* request,
                                                     GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response){};

RestStatus RestPrototypeStateHandler::execute() {
  if (!ExecContext::current().isAdminUser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    return RestStatus::DONE;
  }

  auto methods = PrototypeStateMethods::createInstance(_vocbase);
  return executeByMethod(*methods);
}

RestStatus RestPrototypeStateHandler::executeByMethod(
    PrototypeStateMethods const& methods) {
  switch (_request->requestType()) {
    case rest::RequestType::POST:
      return handlePostRequest(methods);
    case rest::RequestType::GET:
      return handleGetRequest(methods);
    case rest::RequestType::DELETE_REQ:
      return handleDeleteRequest(methods);
    default:
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                    TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  }
  return RestStatus::DONE;
}

RestStatus RestPrototypeStateHandler::handleCreateState(
    replication2::PrototypeStateMethods const& methods,
    velocypack::Slice payload) {
  auto options = velocypack::deserialize<
      replication2::PrototypeStateMethods::CreateOptions>(payload);

  bool isWaitForReady = options.waitForReady;
  return waitForFuture(
      methods.createState(std::move(options))
          .thenValue(
              [&, isWaitForReady](
                  ResultT<replication2::PrototypeStateMethods::CreateResult>&&
                      createResult) {
                if (createResult.ok()) {
                  VPackBuilder result;
                  velocypack::serialize(result, createResult.get());
                  generateOk(isWaitForReady ? rest::ResponseCode::CREATED
                                            : rest::ResponseCode::ACCEPTED,
                             result.slice());
                } else {
                  generateError(createResult.result());
                }
                return RestStatus::DONE;
              }));
}

RestStatus RestPrototypeStateHandler::handlePostRequest(
    PrototypeStateMethods const& methods) {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {  // error message generated in parseVPackBody
    return RestStatus::DONE;
  }

  if (suffixes.size() == 0) {
    return handleCreateState(methods, body);
  }

  if (suffixes.size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect POST /_api/prototype-state/<state-id>/[verb]");
    return RestStatus::DONE;
  }

  LogId logId{basics::StringUtils::uint64(suffixes[0])};
  if (auto& verb = suffixes[1]; verb == "insert") {
    return handlePostInsert(methods, logId, body);
  } else if (verb == "multi-get") {
    return handlePostRetrieveMulti(methods, logId, body);
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "expected one of the resources 'insert', 'multi-get'");
  }
  return RestStatus::DONE;
}

RestStatus RestPrototypeStateHandler::handlePostInsert(
    PrototypeStateMethods const& methods, replication2::LogId logId,
    velocypack::Slice payload) {
  if (!payload.isObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  basics::StringUtils::concatT("expected object containing "
                                               "key-value pairs, but got ",
                                               payload.toJson()));
    return RestStatus::DONE;
  }

  std::unordered_map<std::string, std::string> entries;
  for (auto const& [key, value] : VPackObjectIterator{payload}) {
    if (key.isString() && value.isString()) {
      entries.emplace(key.copyString(), value.copyString());
    } else {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    basics::StringUtils::concatT(
                        "expected key-value pair of strings but got {",
                        key.toJson(), ": ", value.toJson(), "}"));
      return RestStatus::DONE;
    }
  }

  return waitForFuture(
      methods.insert(logId, entries).thenValue([this](auto&& waitForResult) {
        if (waitForResult.ok()) {
          VPackBuilder result;
          {
            VPackObjectBuilder ob(&result);
            result.add("index", VPackValue(waitForResult.get()));
          }
          generateOk(rest::ResponseCode::OK, result.slice());
        } else {
          generateError(waitForResult.result());
        }
        return RestStatus::DONE;
      }));
}

RestStatus RestPrototypeStateHandler::handlePostRetrieveMulti(
    PrototypeStateMethods const& methods, replication2::LogId logId,
    velocypack::Slice payload) {
  auto const& suffixes = _request->suffixes();
  if (suffixes.size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expected GET /_api/prototype-state/<state-id>/multi-get");
    return RestStatus::DONE;
  }

  if (!payload.isArray()) {
    generateError(ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "array expected at top-level");
    return RestStatus::DONE;
  }

  std::vector<std::string> keys;
  for (VPackArrayIterator it{payload}; it.valid(); ++it) {
    auto entry = *it;
    if (!entry.isString()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    basics::StringUtils::concatT("expected string but got ",
                                                 entry.toJson(), " at index ",
                                                 it.index()));
      return RestStatus::DONE;
    }
    keys.push_back(entry.copyString());
  }

  return waitForFuture(
      methods.get(logId, std::move(keys))
          .thenValue(
              [this](std::unordered_map<std::string, std::string>&& map) {
                VPackBuilder result;
                {
                  VPackObjectBuilder ob(&result);
                  for (auto const& [key, value] : map)
                    result.add(key, VPackValue(value));
                }
                generateOk(rest::ResponseCode::OK, result.slice());
              }));
}

RestStatus RestPrototypeStateHandler::handleGetRequest(
    PrototypeStateMethods const& methods) {
  std::vector<std::string> const& suffixes = _request->suffixes();
  if (suffixes.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect GET /_api/prototype-state/<state-id>");
    return RestStatus::DONE;
  }
  LogId logId{basics::StringUtils::uint64(suffixes[0])};
  if (suffixes.size() == 1) {
    return waitForFuture(methods.status(logId).thenValue([&](auto&& result) {
      if (result.fail()) {
        generateError(result.result());
      } else {
        VPackBuilder response;
        velocypack::serialize(response, result.get());
        generateOk(rest::ResponseCode::OK, response.slice());
      }
      return RestStatus::DONE;
    }));
  }

  if (suffixes.size() < 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect GET /_api/prototype-state/<state-id>/[verb]");
    return RestStatus::DONE;
  }
  auto const& verb = suffixes[1];
  if (verb == "entry") {
    return handleGetEntry(methods, logId);
  } else if (verb == "snapshot") {
    return handleGetSnapshot(methods, logId);
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "expected one of the resources 'entry', 'snapshot'");
  }
  return RestStatus::DONE;
}

RestStatus RestPrototypeStateHandler::handleGetEntry(
    PrototypeStateMethods const& methods, replication2::LogId logId) {
  auto const& suffixes = _request->suffixes();
  if (suffixes.size() != 3) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect GET /_api/prototype-state/<state-id>/entry/<key>");
    return RestStatus::DONE;
  }

  return waitForFuture(
      methods.get(logId, suffixes[2])
          .thenValue(
              [this, key = suffixes[2]](std::optional<std::string>&& entry) {
                if (entry) {
                  VPackBuilder result;
                  {
                    VPackObjectBuilder ob(&result);
                    result.add(key, VPackValue(*entry));
                  }
                  generateOk(rest::ResponseCode::OK, result.slice());
                } else {
                  generateError(
                      rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                      basics::StringUtils::concatT("key ", key, " not found"));
                }
              }));
}

RestStatus RestPrototypeStateHandler::handleGetSnapshot(
    PrototypeStateMethods const& methods, replication2::LogId logId) {
  auto const& suffixes = _request->suffixes();
  if (suffixes.size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect GET /_api/prototype-state/<state-id>/snapshot");
    return RestStatus::DONE;
  }

  auto waitForIndex =
      LogIndex{_request->parsedValue<decltype(LogIndex::value)>("waitForIndex")
                   .value_or(0)};
  return waitForFuture(methods.getSnapshot(logId, waitForIndex)
                           .thenValue([this](auto&& waitForResult) {
                             if (waitForResult.fail()) {
                               generateError(waitForResult.result());
                             } else {
                               auto map = waitForResult.get();
                               VPackBuilder result;
                               {
                                 VPackObjectBuilder ob(&result);
                                 for (auto const& [key, value] : map) {
                                   result.add(key, VPackValue(value));
                                 }
                               }
                               generateOk(rest::ResponseCode::OK,
                                          result.slice());
                             }
                             return RestStatus::DONE;
                           }));
}

RestStatus RestPrototypeStateHandler::handleDeleteRequest(
    replication2::PrototypeStateMethods const& methods) {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  if (suffixes.size() < 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expected DELETE /_api/prototype-state/<state-id>/[verb]");
    return RestStatus::DONE;
  }

  LogId logId{basics::StringUtils::uint64(suffixes[0])};
  if (auto& verb = suffixes[1]; verb == "entry") {
    return handleDeleteRemove(methods, logId);
  } else if (verb == "multi-remove") {
    bool parseSuccess = false;
    VPackSlice body = this->parseVPackBody(parseSuccess);
    if (!parseSuccess) {  // error message generated in parseVPackBody
      return RestStatus::DONE;
    }
    return handleDeleteRemoveMulti(methods, logId, body);
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "expected one of the resources 'entry', 'multi-remove'");
  }
  return RestStatus::DONE;
}

RestStatus RestPrototypeStateHandler::handleDeleteRemove(
    replication2::PrototypeStateMethods const& methods,
    replication2::LogId logId) {
  auto const& suffixes = _request->suffixes();
  if (suffixes.size() != 3) {
    generateError(
        rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "expected DELETE /_api/prototype-state/<state-id>/entry/<key>");
    return RestStatus::DONE;
  }

  return waitForFuture(
      methods.remove(logId, suffixes[2])
          .thenValue([this](auto&& waitForResult) {
            if (waitForResult.ok()) {
              VPackBuilder result;
              {
                VPackObjectBuilder ob(&result);
                result.add("index", VPackValue(waitForResult.get()));
              }
              generateOk(rest::ResponseCode::OK, result.slice());
            } else {
              generateError(waitForResult.result());
            }
            return RestStatus::DONE;
          }));
}

RestStatus RestPrototypeStateHandler::handleDeleteRemoveMulti(
    replication2::PrototypeStateMethods const& methods,
    replication2::LogId logId, velocypack::Slice payload) {
  auto const& suffixes = _request->suffixes();
  if (suffixes.size() != 2) {
    generateError(
        rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "expected DELETE /_api/prototype-state/<state-id>/multi-remove");
    return RestStatus::DONE;
  }

  if (!payload.isArray()) {
    generateError(ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "array expected at top-level");
    return RestStatus::DONE;
  }

  std::vector<std::string> keys;
  for (VPackArrayIterator it{payload}; it.valid(); ++it) {
    auto entry = *it;
    if (!entry.isString()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    basics::StringUtils::concatT("expected string but got ",
                                                 entry.toJson(), " at index ",
                                                 it.index()));
      return RestStatus::DONE;
    }
    keys.push_back(entry.copyString());
  }

  return waitForFuture(
      methods.remove(logId, keys).thenValue([this](auto&& waitForResult) {
        if (waitForResult.ok()) {
          VPackBuilder result;
          {
            VPackObjectBuilder ob(&result);
            result.add("index", VPackValue(waitForResult.get()));
          }
          generateOk(rest::ResponseCode::OK, result.slice());
        } else {
          generateError(waitForResult.result());
        }
        return RestStatus::DONE;
      }));
}
