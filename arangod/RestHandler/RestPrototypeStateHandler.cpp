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
#include "Replication2/StateMachines/Prototype/PrototypeFollowerState.h"
#include "Replication2/StateMachines/Prototype/PrototypeLeaderState.h"
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
    case rest::RequestType::PUT:
      return handlePutRequest(methods);
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
              }));
}

RestStatus RestPrototypeStateHandler::handlePutRequest(
    replication2::PrototypeStateMethods const& methods) {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {  // error message generated in parseVPackBody
    return RestStatus::DONE;
  }

  if (suffixes.size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expect PUT /_api/prototype-state/<state-id>/[verb]");
    return RestStatus::DONE;
  }

  LogId logId{basics::StringUtils::uint64(suffixes[0])};
  if (auto& verb = suffixes[1]; verb == "cmp-ex") {
    return handlePutCompareExchange(methods, logId, body);
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "expected 'cmp-ex'");
  }
  return RestStatus::DONE;
}

RestStatus RestPrototypeStateHandler::handlePostRequest(
    PrototypeStateMethods const& methods) {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {  // error message generated in parseVPackBody
    return RestStatus::DONE;
  }

  if (suffixes.empty()) {
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

RestStatus RestPrototypeStateHandler::handlePutCompareExchange(
    replication2::PrototypeStateMethods const& methods,
    replication2::LogId logId, velocypack::Slice payload) {
  if (!payload.isObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  basics::StringUtils::concatT("expected object containing "
                                               "key-value pairs, but got ",
                                               payload.toJson()));
    return RestStatus::DONE;
  }

  std::unordered_map<std::string, std::pair<std::string, std::string>> entries;
  for (auto const& [key, value] : VPackObjectIterator{payload}) {
    if (key.isString() && value.isObject()) {
      if (auto oldValue{value.get("oldValue")}, newValue{value.get("newValue")};
          oldValue.isString() && newValue.isString()) {
        entries.emplace(
            key.copyString(),
            std::make_pair(oldValue.copyString(), newValue.copyString()));
      } else {
        generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                      basics::StringUtils::concatT(
                          "expected key-value pair of strings but got {",
                          oldValue.toJson(), ": ", newValue.toJson(), "}"));
        return RestStatus::DONE;
      }
    }
  }

  if (entries.size() != 1) {
    generateError(
        rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        basics::StringUtils::concatT("the compare-exchange operation currently "
                                     "supports one key at the time, but got ",
                                     entries.size(), " keys"));
    return RestStatus::DONE;
  }

  auto options = PrototypeStateMethods::PrototypeWriteOptions{};
  options.waitForApplied =
      _request->parsedValue<bool>("waitForApplied").value_or(true);

  auto [key, values] = *entries.begin();
  return waitForFuture(
      methods.compareExchange(logId, key, values.first, values.second, options)
          .thenValue([this, options](ResultT<LogIndex>&& waitForResult) {
            if (waitForResult.fail()) {
              generateError(waitForResult.result());
            } else {
              VPackBuilder result;
              {
                VPackObjectBuilder ob(&result);
                result.add("index", VPackValue(waitForResult.get()));
              }
              generateOk(options.waitForApplied ? rest::ResponseCode::OK
                                                : rest::ResponseCode::ACCEPTED,
                         result.slice());
            }
          }));
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

  auto options = PrototypeStateMethods::PrototypeWriteOptions{};
  options.waitForApplied =
      _request->parsedValue<bool>("waitForApplied").value_or(true);

  return waitForFuture(methods.insert(logId, entries, options)
                           .thenValue([this, options](auto&& logIndex) {
                             VPackBuilder result;
                             {
                               VPackObjectBuilder ob(&result);
                               result.add("index", VPackValue(logIndex));
                             }
                             generateOk(options.waitForApplied
                                            ? rest::ResponseCode::OK
                                            : rest::ResponseCode::ACCEPTED,
                                        result.slice());
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

  PrototypeStateMethods::ReadOptions readOptions;
  readOptions.waitForApplied = LogIndex{
      _request->parsedValue<decltype(LogIndex::value)>("waitForApplied")
          .value_or(0)};
  readOptions.readFrom = _request->parsedValue<ParticipantId>("readFrom");

  return waitForFuture(
      methods.get(logId, std::move(keys), readOptions)
          .thenValue([this](
                         ResultT<std::unordered_map<std::string, std::string>>&&
                             waitForResult) {
            if (waitForResult.fail()) {
              generateError(waitForResult.result());
            } else {
              VPackBuilder result;
              {
                VPackObjectBuilder ob(&result);
                for (auto const& [key, value] : std::move(waitForResult.get()))
                  result.add(key, VPackValue(value));
              }
              generateOk(rest::ResponseCode::OK, result.slice());
            }
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
  } else if (verb == "wait-for-applied") {
    return handleGetWaitForApplied(methods, logId);
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "expected one of the resources 'entry', 'snapshot', "
                  "'wait-for-applied'");
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

  PrototypeStateMethods::ReadOptions readOptions;
  readOptions.waitForApplied = LogIndex{
      _request->parsedValue<decltype(LogIndex::value)>("waitForApplied")
          .value_or(0)};
  readOptions.readFrom = _request->parsedValue<ParticipantId>("readFrom");

  return waitForFuture(
      methods.get(logId, suffixes[2], readOptions)
          .thenValue([this, key = suffixes[2]](auto&& waitForResult) {
            if (waitForResult.fail()) {
              generateError(waitForResult.result());
            } else {
              if (auto entry = std::move(waitForResult.get())) {
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
            }
          }));
}

RestStatus RestPrototypeStateHandler::handleGetWaitForApplied(
    const replication2::PrototypeStateMethods& methods,
    replication2::LogId logId) {
  auto const& suffixes = _request->suffixes();
  if (suffixes.size() != 3) {
    generateError(
        rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
        "expect GET /_api/prototype-state/<state-id>/wait-for-applied/<idx>");
    return RestStatus::DONE;
  }

  LogIndex idx{basics::StringUtils::uint64(suffixes[2])};

  return waitForFuture(methods.waitForApplied(logId, idx)
                           .thenValue([this](auto&& waitForResult) {
                             if (waitForResult.fail()) {
                               generateError(waitForResult);
                             } else {
                               generateOk(rest::ResponseCode::OK,
                                          VPackSlice::noneSlice());
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
                           }));
}

RestStatus RestPrototypeStateHandler::handleDeleteRequest(
    replication2::PrototypeStateMethods const& methods) {
  std::vector<std::string> const& suffixes = _request->decodedSuffixes();
  if (suffixes.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expected DELETE /_api/prototype-state/<state-id>(/[verb])");
    return RestStatus::DONE;
  }

  LogId logId{basics::StringUtils::uint64(suffixes[0])};
  if (suffixes.size() == 1) {
    return waitForFuture(methods.drop(logId).thenValue([this](auto&& result) {
      if (result.ok()) {
        generateOk(rest::ResponseCode::OK, VPackSlice::noneSlice());
      } else {
        generateError(result);
      }
    }));

  } else if (auto& verb = suffixes[1]; verb == "entry") {
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

  auto options = PrototypeStateMethods::PrototypeWriteOptions{};
  options.waitForApplied =
      _request->parsedValue<bool>("waitForApplied").value_or(true);

  return waitForFuture(methods.remove(logId, suffixes[2], options)
                           .thenValue([this, options](auto&& waitForResult) {
                             VPackBuilder result;
                             {
                               VPackObjectBuilder ob(&result);
                               result.add("index", VPackValue(waitForResult));
                             }
                             generateOk(options.waitForApplied
                                            ? rest::ResponseCode::OK
                                            : rest::ResponseCode::ACCEPTED,
                                        result.slice());
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

  auto options = PrototypeStateMethods::PrototypeWriteOptions{};
  options.waitForApplied =
      _request->parsedValue<bool>("waitForApplied").value_or(true);

  return waitForFuture(methods.remove(logId, keys, options)
                           .thenValue([this, options](auto&& waitForResult) {
                             VPackBuilder result;
                             {
                               VPackObjectBuilder ob(&result);
                               result.add("index", VPackValue(waitForResult));
                             }
                             generateOk(options.waitForApplied
                                            ? rest::ResponseCode::OK
                                            : rest::ResponseCode::ACCEPTED,
                                        result.slice());
                           }));
}
