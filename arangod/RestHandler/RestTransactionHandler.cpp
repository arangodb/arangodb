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
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Helpers.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
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

RestTransactionHandler::RestTransactionHandler(application_features::ApplicationServer& server,
                                               GeneralRequest* request,
                                               GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response), _v8Context(nullptr), _lock() {}

RestStatus RestTransactionHandler::execute() {
    
  switch (_request->requestType()) {

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

    case rest::RequestType::GET:
      executeGetState();
      break;
      
    default:
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
      break;
  }
  return RestStatus::DONE;
}

void RestTransactionHandler::executeGetState() {
  if (_request->suffixes().empty()) {
    // no transaction id given - so list all the transactions
    ExecContext const& exec = ExecContext::current();

    VPackBuilder builder;
    builder.openObject();
    builder.add("transactions", VPackValue(VPackValueType::Array));
    
    bool const fanout = ServerState::instance()->isCoordinator() && !_request->parsedValue("local", false);
    transaction::Manager* mgr = transaction::ManagerFeature::manager();
    TRI_ASSERT(mgr != nullptr);
    mgr->toVelocyPack(builder, _vocbase.name(), exec.user(), fanout);
 
    builder.close(); // array
    builder.close(); // object
  
    generateResult(rest::ResponseCode::OK, builder.slice());
    return;
  }
  
  if (_request->suffixes().size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "expecting GET /_api/transaction/<transaction-ID>");
    return;
  }

  TRI_voc_tid_t tid = StringUtils::uint64(_request->suffixes()[0]);
  if (tid == 0) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "Illegal transaction ID");
    return;
  }

  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  transaction::Status status = mgr->getManagedTrxStatus(tid);
  
  if (status == transaction::Status::UNDEFINED) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_TRANSACTION_NOT_FOUND);
  } else {
    generateTransactionResult(rest::ResponseCode::OK, tid, status);
  }
}

void RestTransactionHandler::executeBegin() {
  TRI_ASSERT(_request->suffixes().size() == 1 &&
             _request->suffixes()[0] == "begin");

  bool parseSuccess = false;
  VPackSlice slice = parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    // error message generated in parseVPackBody
    return;
  }

  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  TRI_ASSERT(mgr != nullptr);

  bool found = false;
  std::string const& value = _request->header(StaticStrings::TransactionId, found);
  ServerState::RoleEnum role = ServerState::instance()->getRole();

  if (found) {
    if (!ServerState::isDBServer(role)) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_NOT_IMPLEMENTED,
                    "Not supported on this server type");
      return;
    }
    TRI_voc_tid_t tid = basics::StringUtils::uint64(value);
    if (tid == 0 || !transaction::isChildTransactionId(tid)) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                    "invalid transaction ID on DBServer");
      return;
    }

    TRI_ASSERT(tid != 0);
    TRI_ASSERT(!transaction::isLegacyTransactionId(tid));

    Result res = mgr->ensureManagedTrx(_vocbase, tid, slice, false);
    if (res.fail()) {
      generateError(res);
    } else {
      generateTransactionResult(rest::ResponseCode::CREATED, tid,
                                transaction::Status::RUNNING);
    }
  } else {
    if (!ServerState::isCoordinator(role) && !ServerState::isSingleServer(role)) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_NOT_IMPLEMENTED,
                    "Not supported on this server type");
      return;
    }

    // start
    ResultT<TRI_voc_tid_t> res = mgr->createManagedTrx(_vocbase, slice);
    if (res.fail()) {
      generateError(res.result());
    } else {
      generateTransactionResult(rest::ResponseCode::CREATED, res.get(),
                                transaction::Status::RUNNING);
    }
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
  
  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  TRI_ASSERT(mgr != nullptr);

  if (_request->suffixes()[0] == "write") {
    // abort all write transactions
    bool const fanout = ServerState::instance()->isCoordinator() && !_request->parsedValue("local", false);
    ExecContext const& exec = ExecContext::current();
    Result res = mgr->abortAllManagedWriteTrx(exec.user(), fanout);
        
    if (res.ok()) {
      generateOk(rest::ResponseCode::OK, VPackSlice::emptyObjectSlice());
    } else {
      generateError(res);
    }
  } else {
    TRI_voc_tid_t tid = basics::StringUtils::uint64(_request->suffixes()[0]);
    if (tid == 0) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                    "bad transaction ID");
      return;
    }
  
    Result res = mgr->abortManagedTrx(tid);

    if (res.fail()) {
      generateError(res);
    } else {
      generateTransactionResult(rest::ResponseCode::OK, tid, transaction::Status::ABORTED);
    }
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
  
  generateResult(code, std::move(buffer));
}

// ====================== V8 stuff ===================

/// start a legacy JS transaction
void RestTransactionHandler::executeJSTransaction() {
  if (!V8DealerFeature::DEALER || !V8DealerFeature::DEALER->isEnabled()) {
    generateError(rest::ResponseCode::NOT_IMPLEMENTED, TRI_ERROR_NOT_IMPLEMENTED, "JavaScript operations are disabled");
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
  V8Context* v8Context = V8DealerFeature::DEALER->enterContext(&_vocbase, securityContext);

  if (!v8Context) {
    generateError(Result(TRI_ERROR_INTERNAL, "could not acquire v8 context"));
    return;
  }

  // register a function to release the V8Context whenever we exit from this scope
  auto guard = scopeGuard([this]() {
    WRITE_LOCKER(lock, _lock);
    if (_v8Context != nullptr) {
      V8DealerFeature::DEALER->exitContext(_v8Context);
      _v8Context = nullptr;
    }
  });
     
  {
    // make our V8Context available to the cancel function
    WRITE_LOCKER(lock, _lock);
    _v8Context = v8Context;
    if (_canceled) {
      // if we cancel here, the shutdown function above will perform the necessary cleanup
      lock.unlock();
      generateCanceled();
      return;
    }
  }

  VPackBuilder result;
  try {
    Result res = executeTransaction(v8Context->_isolate, _lock, _canceled,
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

void RestTransactionHandler::cancel() {
  // cancel v8 transaction
  WRITE_LOCKER(writeLock, _lock);
  _canceled.store(true);
  if (_v8Context != nullptr) {
    auto isolate = _v8Context->_isolate;
    if (!isolate->IsExecutionTerminating()) {
      isolate->TerminateExecution();
    }
  }
}

/// @brief returns the short id of the server which should handle this request
ResultT<std::pair<std::string, bool>> RestTransactionHandler::forwardingTarget() {
  auto base = RestVocbaseBaseHandler::forwardingTarget();
  if (base.ok() && !std::get<0>(base.get()).empty()) {
    return base;
  }

  rest::RequestType const type = _request->requestType();
  if (type != rest::RequestType::GET && type != rest::RequestType::PUT &&
      type != rest::RequestType::DELETE_REQ) {
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
