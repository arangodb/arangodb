////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "Basics/ScopeGuard.h"
#include "Basics/WriteLocker.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Helpers.h"
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
#include "Transaction/History.h"
#endif
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Transaction/OperationOrigin.h"
#include "Transaction/Status.h"
#include "Utils/ExecContext.h"
#ifdef USE_V8
#include "V8/JavaScriptSecurityContext.h"
#include "V8Server/V8Executor.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/Methods/Transactions.h"
#endif
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestTransactionHandler::RestTransactionHandler(ArangodServer& server,
                                               GeneralRequest* request,
                                               GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response), _v8Context(nullptr) {}

RequestLane RestTransactionHandler::lane() const {
  if (_request->requestType() == RequestType::GET) {
    // a GET request only returns the list of ongoing transactions.
    // this is used only for debugging and should not be blocked
    // by if most scheduler threads are busy.
    return RequestLane::CLUSTER_ADMIN;
  }

  bool isCommit = _request->requestType() == rest::RequestType::PUT;
  bool isAbort = _request->requestType() == rest::RequestType::DELETE_REQ;

  if ((isCommit || isAbort) &&
      ServerState::instance()->isSingleServerOrCoordinator()) {
    // give commits and aborts a higer priority than normal document
    // operations on coordinators and single servers, because these
    // operations can unblock other operations.
    // strictly speaking, the request lane should not be "continuation"
    // here, as it is no continuation. but we don't have a better
    // other request lane with medium priority. the only important
    // thing here is that the request lane priority is set to medium.
    return RequestLane::CONTINUATION;
  }

  if (ServerState::instance()->isDBServer()) {
    bool isSyncReplication = false;
    // We do not care for the real value, enough if it is there.
    std::ignore = _request->value(StaticStrings::IsSynchronousReplicationString,
                                  isSyncReplication);
    if (isSyncReplication) {
      return RequestLane::SERVER_SYNCHRONOUS_REPLICATION;
      // This leads to the high queue, we want replication requests (for
      // commit or abort in the El Cheapo case) to be executed with a
      // higher prio than leader requests, even if they are done from
      // AQL.
    }

    if (isCommit || isAbort) {
      // commit or abort on leader gets a medium priority, because it
      // can unblock other operations
      return RequestLane::CONTINUATION;
    }
  }

  return RequestLane::CLIENT_V8;
}

RestStatus RestTransactionHandler::execute() {
  switch (_request->requestType()) {
    case rest::RequestType::POST:
      if (_request->suffixes().size() == 1 &&
          _request->suffixes()[0] == "begin") {
        return waitForFuture(executeBegin());
      } else if (_request->suffixes().empty()) {
        executeJSTransaction();
      } else {
        generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
      }
      break;

    case rest::RequestType::PUT:
      return waitForFuture(executeCommit());

    case rest::RequestType::DELETE_REQ:
      return waitForFuture(executeAbort());

    case rest::RequestType::GET:
      executeGetState();
      break;

    default:
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                    TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
      break;
  }
  return RestStatus::DONE;
}

void RestTransactionHandler::executeGetState() {
  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  TRI_ASSERT(mgr != nullptr);

  if (_request->suffixes().empty()) {
    // no transaction id given - so list all the transactions
    ExecContext const& exec = ExecContext::current();

    VPackBuilder builder;
    builder.openObject();
    builder.add("transactions", VPackValue(VPackValueType::Array));

    bool fanout = ServerState::instance()->isCoordinator() &&
                  !_request->parsedValue("local", false);
    // note: "details" parameter is not documented and not part of the public
    // API, so the output format of toVelocyPack(details=true) may change
    // without notice
    bool details = _request->parsedValue("details", false);
    mgr->toVelocyPack(builder, _vocbase.name(), exec.user(), fanout, details);

    builder.close();  // array
    builder.close();  // object

    generateResult(rest::ResponseCode::OK, builder.slice());
    return;
  }

  if (_request->suffixes().size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "expecting GET /_api/transaction/<transaction-ID>");
    return;
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // unofficial API to retrieve the transactions history. NOT A PUBLIC API!
  if (_request->suffixes()[0] == "history") {
    auto auth = AuthenticationFeature::instance();
    if ((auth == nullptr || !auth->isActive()) ||
        (auth->isActive() && ExecContext::current().isSuperuser())) {
      velocypack::Builder builder;
      mgr->history().toVelocyPack(builder);
      generateResult(rest::ResponseCode::OK, builder.slice());
    } else {
      generateError(TRI_ERROR_FORBIDDEN);
    }
    return;
  }
#endif

  TransactionId tid{StringUtils::uint64(_request->suffixes()[0])};
  if (tid.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "Illegal transaction ID");
    return;
  }

  transaction::Status status = mgr->getManagedTrxStatus(tid);

  if (status == transaction::Status::UNDEFINED) {
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_TRANSACTION_NOT_FOUND);
  } else {
    generateTransactionResult(rest::ResponseCode::OK, tid, status);
  }
}

futures::Future<futures::Unit> RestTransactionHandler::executeBegin() {
  TRI_ASSERT(_request->suffixes().size() == 1 &&
             _request->suffixes()[0] == "begin");

  bool parseSuccess = false;
  VPackSlice slice = parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    // error message generated in parseVPackBody
    co_return;
  }

  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  TRI_ASSERT(mgr != nullptr);

  bool found = false;
  std::string const& value =
      _request->header(StaticStrings::TransactionId, found);
  ServerState::RoleEnum role = ServerState::instance()->getRole();

  auto origin = transaction::OperationOriginREST{"streaming transaction"};

  if (found) {
    if (!ServerState::isDBServer(role)) {
      // it is not expected that the user sends a transaction ID to begin
      // a transaction
      generateError(
          rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
          "unexpected transaction ID received in begin transaction request");
      co_return;
    }
    // figure out the transaction ID
    TransactionId tid = TransactionId{basics::StringUtils::uint64(value)};
    if (tid.empty() || !tid.isChildTransactionId()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                    "invalid transaction ID on DBServer");
      co_return;
    }
    TRI_ASSERT(tid.isSet());
    TRI_ASSERT(!tid.isLegacyTransactionId());

    Result res =
        co_await mgr->ensureManagedTrx(_vocbase, tid, slice, origin, false);
    if (res.fail()) {
      generateError(res);
    } else {
      generateTransactionResult(rest::ResponseCode::CREATED, tid,
                                transaction::Status::RUNNING);
    }
  } else {
    if (!ServerState::isCoordinator(role) &&
        !ServerState::isSingleServer(role)) {
      generateError(
          rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
          "missing transaction ID in internal transaction begin request");
      co_return;
    }

    // Check if dirty reads are allowed:
    bool found = false;
    bool allowDirtyReads = false;
    std::string const& val =
        _request->header(StaticStrings::AllowDirtyReads, found);
    if (found && StringUtils::boolean(val)) {
      // This will be used in `createTransaction` below, if that creates
      // a new transaction. Otherwise, we use the default given by the
      // existing transaction.
      allowDirtyReads = true;
      setOutgoingDirtyReadsHeader(true);
    }

    // start
    ResultT<TransactionId> res = co_await mgr->createManagedTrx(
        _vocbase, slice, origin, allowDirtyReads);
    if (res.fail()) {
      generateError(res.result());
    } else {
      generateTransactionResult(rest::ResponseCode::CREATED, res.get(),
                                transaction::Status::RUNNING);
    }
  }
}

futures::Future<futures::Unit> RestTransactionHandler::executeCommit() {
  if (_request->suffixes().size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
    co_return;
  }

  TransactionId tid{basics::StringUtils::uint64(_request->suffixes()[0])};
  if (tid.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "bad transaction ID");
    co_return;
  }

  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  TRI_ASSERT(mgr != nullptr);

  Result res = co_await mgr->commitManagedTrx(tid, _vocbase.name());
  if (res.fail()) {
    generateError(res);
  } else {
    generateTransactionResult(rest::ResponseCode::OK, tid,
                              transaction::Status::COMMITTED);
  }
}

futures::Future<futures::Unit> RestTransactionHandler::executeAbort() {
  if (_request->suffixes().size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
    co_return;
  }

  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  TRI_ASSERT(mgr != nullptr);

  if (_request->suffixes()[0] == "write") {
    // abort all write transactions
    bool fanout = ServerState::instance()->isCoordinator() &&
                  !_request->parsedValue("local", false);
    ExecContext const& exec = ExecContext::current();
    Result res = mgr->abortAllManagedWriteTrx(exec.user(), fanout);

    if (res.ok()) {
      generateOk(rest::ResponseCode::OK, VPackSlice::emptyObjectSlice());
    } else {
      generateError(res);
    }
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    // unofficial API to clear the transactions history. NOT A PUBLIC API!
  } else if (_request->suffixes()[0] == "history") {
    auto auth = AuthenticationFeature::instance();
    if ((auth == nullptr || !auth->isActive()) ||
        (auth->isActive() && ExecContext::current().isSuperuser())) {
      mgr->history().clear();
      generateOk(rest::ResponseCode::OK, VPackSlice::emptyObjectSlice());
    } else {
      generateError(TRI_ERROR_FORBIDDEN);
    }
#endif
  } else {
    TransactionId tid{basics::StringUtils::uint64(_request->suffixes()[0])};
    if (tid.empty()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                    "bad transaction ID");
      co_return;
    }

    Result res = co_await mgr->abortManagedTrx(tid, _vocbase.name());

    if (res.fail()) {
      generateError(res);
    } else {
      generateTransactionResult(rest::ResponseCode::OK, tid,
                                transaction::Status::ABORTED);
    }
  }
}

void RestTransactionHandler::generateTransactionResult(
    rest::ResponseCode code, TransactionId tid, transaction::Status status) {
  VPackBuffer<uint8_t> buffer;
  VPackBuilder tmp(buffer);
  tmp.add(VPackValue(VPackValueType::Object, true));
  tmp.add(StaticStrings::Code, VPackValue(static_cast<int>(code)));
  tmp.add(StaticStrings::Error, VPackValue(false));
  tmp.add("result", VPackValue(VPackValueType::Object, true));
  tmp.add("id", VPackValue(std::to_string(tid.id())));
  tmp.add("status", VPackValue(transaction::statusString(status)));
  tmp.close();
  tmp.close();

  generateResult(code, std::move(buffer));
}

// ====================== V8 stuff ===================

/// start a legacy JS transaction
void RestTransactionHandler::executeJSTransaction() {
#ifdef USE_V8
  if (!server().isEnabled<V8DealerFeature>()) {
    generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                  TRI_ERROR_NOT_IMPLEMENTED,
                  "JavaScript operations are disabled");
    return;
  }

  auto slice = _request->payload();
  if (!slice.isObject()) {
    generateError(
        Result(TRI_ERROR_BAD_PARAMETER, "expecting object input data"));
    return;
  }

  std::string portType = _request->connectionInfo().portType();

  bool allowUseDatabase =
      server().getFeature<ActionFeature>().allowUseDatabase();
  JavaScriptSecurityContext securityContext =
      JavaScriptSecurityContext::createRestActionContext(allowUseDatabase);
  V8Executor* v8Context = server().getFeature<V8DealerFeature>().enterExecutor(
      &_vocbase, securityContext);

  if (!v8Context) {
    generateError(Result(TRI_ERROR_INTERNAL, "could not acquire v8 context"));
    return;
  }

  // register a function to release the V8Executor whenever we exit from this
  // scope
  auto guard = scopeGuard([this]() noexcept {
    try {
      WRITE_LOCKER(lock, _lock);
      if (_v8Context != nullptr) {
        server().getFeature<V8DealerFeature>().exitExecutor(_v8Context);
        _v8Context = nullptr;
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC("1b20f", ERR, Logger::V8)
          << "Failed to exit V8 context while executing JS transaction: "
          << ex.what();
    }
  });

  {
    // make our V8Executor available to the cancel function
    WRITE_LOCKER(lock, _lock);
    _v8Context = v8Context;
    if (_canceled) {
      // if we cancel here, the shutdown function above will perform the
      // necessary cleanup
      lock.unlock();
      generateCanceled();
      return;
    }
  }

  VPackBuilder result;
  try {
    Result res = executeTransaction(v8Context, _lock, _canceled, slice,
                                    portType, result);
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
#else
  generateError(
      rest::ResponseCode::NOT_IMPLEMENTED, TRI_ERROR_NOT_IMPLEMENTED,
      "JavaScript operations are not available in this build of ArangoDB");
#endif
}

void RestTransactionHandler::cancel() {
  // cancel v8 transaction
  WRITE_LOCKER(writeLock, _lock);
  _canceled.store(true);
#ifdef USE_V8
  if (_v8Context != nullptr) {
    v8::Isolate* isolate = _v8Context->isolate();
    if (!isolate->IsExecutionTerminating()) {
      isolate->TerminateExecution();
    }
  }
#endif
}

/// @brief returns the short id of the server which should handle this
/// request
ResultT<std::pair<std::string, bool>>
RestTransactionHandler::forwardingTarget() {
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
    // do not forward if we don't have a transaction suffix. the
    // number of suffixes validation will still be performed for PUT
    // and DELETE requests later, so not returning an error from here
    // is ok.
    return {std::make_pair(StaticStrings::Empty, false)};
  }

  if (type == rest::RequestType::DELETE_REQ && suffixes[0] == "write") {
    // no request forwarding for stopping write transactions
    return {std::make_pair(StaticStrings::Empty, false)};
  }

  uint64_t tick = arangodb::basics::StringUtils::uint64(suffixes[0]);
  uint32_t sourceServer = TRI_ExtractServerIdFromTick(tick);

  if (sourceServer == ServerState::instance()->getShortId()) {
    // we need to handle the request ourselves, because we own the
    // id used in the request.
    return {std::make_pair(StaticStrings::Empty, false)};
  }
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  auto coordinatorId = ci.getCoordinatorByShortID(sourceServer);

  if (coordinatorId.empty()) {
    return ResultT<std::pair<std::string, bool>>::error(
        TRI_ERROR_TRANSACTION_NOT_FOUND,
        "cannot find target server for transaction id");
  }

  return {std::make_pair(std::move(coordinatorId), false)};
}
