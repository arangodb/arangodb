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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "RestTransactionHandler.h"

#include "Actions/ActionFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Transaction/Helpers.h"
#include "Transaction/Status.h"
#include "V8/JavaScriptSecurityContext.h"
#include "V8Server/V8Context.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/Methods/Transactions.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestTransactionHandler::RestTransactionHandler(GeneralRequest* request, GeneralResponse* response)
    : RestVocbaseBaseHandler(request, response), _v8Context(nullptr), _lock() {}

RestStatus RestTransactionHandler::execute() {
    
  switch (_request->requestType()) {
    case rest::RequestType::GET:
      executeGetState();
      break;

    case rest::RequestType::POST:
      if (_request->suffixes().size() == 1 &&
          _request->suffixes()[0] == "begin") {
        executeBegin();
      } else if (_request->suffixes().empty()) {
        executeJSTransaction();
      } else {
        generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
      }
      break;

    case rest::RequestType::PUT:
      executeCommit();
      break;

    case rest::RequestType::DELETE_REQ:
      executeAbort();
      break;

    default:
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
      break;
  }
  return RestStatus::DONE;
}

void RestTransactionHandler::executeGetState() {
  if (_request->suffixes().size() != 1) {
    generateError(rest::ResponseCode::NOT_IMPLEMENTED, TRI_ERROR_NOT_IMPLEMENTED);
  }

  TRI_voc_tid_t tid = StringUtils::uint64(_request->suffixes()[1]);
  if (tid == 0) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "Illegal transaction ID");
    return;
  }

  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  transaction::Status status = mgr->getManagedTrxStatus(tid);
  
  if (status == transaction::Status::UNDEFINED) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_TRANSACTION_NOT_FOUND);
  } else {
    generateTransactionResult(rest::ResponseCode::OK, tid, status);
  }
}

void RestTransactionHandler::executeBegin() {
  TRI_ASSERT(_request->suffixes().size() == 1 &&
             _request->suffixes()[0] == "begin");
  
  // figure out the transaction ID
  TRI_voc_tid_t tid = 0;
  bool found = false;
  std::string value = _request->header(StaticStrings::TransactionId, found);
  ServerState::RoleEnum role = ServerState::instance()->getRole();
  if (found) {
    if (!ServerState::isDBServer(role)) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_NOT_IMPLEMENTED,
                    "Not supported on this server type");
      return;
    }
    tid = basics::StringUtils::uint64(value);
    if (tid == 0 || !transaction::isChildTransactionId(tid)) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                    "invalid transaction ID on DBServer");
      return;
    }
    TRI_ASSERT(tid != 0);
    TRI_ASSERT(!transaction::isLegacyTransactionId(tid));
  } else {
    if (!(ServerState::isCoordinator(role) || ServerState::isSingleServer(role))) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_NOT_IMPLEMENTED,
                    "Not supported on this server type");
      return;
    }
    tid = ServerState::isSingleServer(role) ? TRI_NewTickServer() :
                                              TRI_NewServerSpecificTickMod4();
  }
  TRI_ASSERT(tid != 0);
  
  
  bool parseSuccess = false;
  VPackSlice body = parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    // error message generated in parseVPackBody
    return;
  }
  
  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  TRI_ASSERT(mgr != nullptr);
  
  Result res = mgr->createManagedTrx(_vocbase, tid, body);
  if (res.fail()) {
    generateError(res);
  } else {
    generateTransactionResult(rest::ResponseCode::CREATED, tid, transaction::Status::RUNNING);
  }
}

void RestTransactionHandler::executeCommit() {
  if (_request->suffixes().size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
    return;
  }

  TRI_voc_tid_t tid = basics::StringUtils::uint64(_request->suffixes()[0]);
  if (tid == 0) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "bad transaction ID");
    return;
  }

  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  TRI_ASSERT(mgr != nullptr);
  
  Result res = mgr->commitManagedTrx(tid);
  if (res.fail()) {
    generateError(res);
  } else {
    generateTransactionResult(rest::ResponseCode::OK, tid, transaction::Status::COMMITTED);
  }
}

void RestTransactionHandler::executeAbort() {
  if (_request->suffixes().size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
    return;
  }

  TRI_voc_tid_t tid = basics::StringUtils::uint64(_request->suffixes()[0]);
  if (tid == 0) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "bad transaction ID");
    return;
  }
  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  TRI_ASSERT(mgr != nullptr);
  
  Result res = mgr->abortManagedTrx(tid);
  if (res.fail()) {
    generateError(res);
  } else {
    generateTransactionResult(rest::ResponseCode::OK, tid, transaction::Status::ABORTED);
  }
}

void RestTransactionHandler::generateTransactionResult(rest::ResponseCode code,
                                                       TRI_voc_tid_t tid,
                                                       transaction::Status status) {
  VPackBuffer<uint8_t> buffer;
  VPackBuilder tmp(buffer);
  tmp.add(VPackValue(VPackValueType::Object, true));
  tmp.add(StaticStrings::Code, VPackValue(static_cast<int>(code)));
  tmp.add(StaticStrings::Error, VPackValue(false));
  tmp.add("result", VPackValue(VPackValueType::Object, true));
  tmp.add("id", VPackValue(std::to_string(tid)));
  tmp.add("status", VPackValue(transaction::statusString(status)));
  tmp.close();
  tmp.close();
  
  generateResult(rest::ResponseCode::OK, std::move(buffer));
}

// ====================== V8 stuff ===================

/// start a legacy JS transaction
void RestTransactionHandler::executeJSTransaction() {
  if (!V8DealerFeature::DEALER) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                  "JavaScript transactions are not available");
    return;
  }

  auto slice = _request->payload();
  if (!slice.isObject()) {
    generateError(
        Result(TRI_ERROR_BAD_PARAMETER, "expecting object input data"));
    return;
  }

  std::string portType = _request->connectionInfo().portType();

  bool allowUseDatabase = ActionFeature::ACTION->allowUseDatabase();
  JavaScriptSecurityContext securityContext = JavaScriptSecurityContext::createRestActionContext(allowUseDatabase);
  _v8Context = V8DealerFeature::DEALER->enterContext(&_vocbase, securityContext);

  if (!_v8Context) {
    generateError(Result(TRI_ERROR_INTERNAL, "could not acquire v8 context"));
    return;
  }

  TRI_DEFER(returnContext());

  VPackBuilder result;
  try {
    {
      WRITE_LOCKER(lock, _lock);
      if (_canceled) {
        generateCanceled();
        return;
      }
    }

    Result res = executeTransaction(_v8Context->_isolate, _lock, _canceled,
                                    slice, portType, result);
    if (res.ok()) {
      VPackSlice slice = result.slice();
      if (slice.isNone()) {
        generateOk(rest::ResponseCode::OK, VPackSlice::nullSlice());
      } else {
        generateOk(rest::ResponseCode::OK, slice);
      }
    } else {
      generateError(res);
    }
  } catch (arangodb::basics::Exception const& ex) {
    generateError(Result(ex.code(), ex.what()));
  } catch (std::exception const& ex) {
    generateError(Result(TRI_ERROR_INTERNAL, ex.what()));
  } catch (...) {
    generateError(Result(TRI_ERROR_INTERNAL));
  }
}

void RestTransactionHandler::returnContext() {
  WRITE_LOCKER(writeLock, _lock);
  V8DealerFeature::DEALER->exitContext(_v8Context);
  _v8Context = nullptr;
}

bool RestTransactionHandler::cancel() {
  // cancel v8 transaction
  WRITE_LOCKER(writeLock, _lock);
  _canceled.store(true);
  auto isolate = _v8Context->_isolate;
  if (!isolate->IsExecutionTerminating()) {
    isolate->TerminateExecution();
  }
  return true;
}

/// @brief returns the short id of the server which should handle this request
uint32_t RestTransactionHandler::forwardingTarget() {
  rest::RequestType const type = _request->requestType();
  if (type != rest::RequestType::GET && type != rest::RequestType::PUT &&
      type != rest::RequestType::DELETE_REQ) {
    return 0;
  }

  std::vector<std::string> const& suffixes = _request->suffixes();
  if (suffixes.size() < 1) {
    return 0;
  }

  uint64_t tick = arangodb::basics::StringUtils::uint64(suffixes[0]);
  uint32_t sourceServer = TRI_ExtractServerIdFromTick(tick);

  return (sourceServer == ServerState::instance()->getShortId()) ? 0 : sourceServer;
}
