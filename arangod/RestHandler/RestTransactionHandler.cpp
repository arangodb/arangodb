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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Cluster/ClusterInfo.h"
#include "Rest/HttpRequest.h"
#include "RestTransactionHandler.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionManager.h"
#include "StorageEngine/TransactionManagerFeature.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "Transaction/SmartContext.h"
#include "V8Server/V8Context.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/Methods/Transactions.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

struct ManagingTransaction final : transaction::Methods {
  ManagingTransaction(std::shared_ptr<transaction::SmartContext> const& ctx,
                      transaction::Options const& opts)
      : Methods(ctx, opts) {
    TRI_ASSERT(_state->isEmbeddedTransaction());
  }
};

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

  TransactionManager* mgr = TransactionManagerFeature::manager();
  TransactionState* state = mgr->lookup(tid, TransactionManager::Ownership::Lease);
  TRI_DEFER(state->decreaseNesting());

  VPackBuilder b;
  b.openObject();
  b.add("state", VPackValue(transaction::statusString(state->status())));
  b.add("options", VPackValue(VPackValueType::Object));
  state->options().toVelocyPack(b);
  b.close();
  b.close();
  generateOk(rest::ResponseCode::OK, b.slice());
}

void RestTransactionHandler::executeBegin() {
  TRI_ASSERT(_request->suffixes().size() == 1 &&
             _request->suffixes()[0] == "begin");

  // figure out the transaction ID
  TRI_voc_tid_t tid = 0;
  bool found = false;
  std::string value = _request->header(StaticStrings::TransactionId, found);
  if (found) {
    if (ServerState::instance()->isSingleServerOrCoordinator()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_NOT_IMPLEMENTED,
                    "Not supported on this server type");
    }
    tid = basics::StringUtils::uint64(value);
    if (tid == 0 || !TransactionManager::isChildTransactionId(tid)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "invalid transaction ID");
    }
  } else {
    if (!ServerState::instance()->isSingleServerOrCoordinator()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_NOT_IMPLEMENTED,
                    "Not supported on this server type");
    }
    tid = ServerState::instance()->isCoordinator()
              ? TRI_NewServerSpecificTickMod4()       // coordinator
              : TRI_NewServerSpecificTickMod4() + 3;  // legacy
  }
  TRI_ASSERT(tid != 0);

  bool parseSuccess = false;
  VPackSlice body = parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    // error message generated in parseVPackBody
    return;
  }

  // extract the properties from the object
  transaction::Options trxOptions;
  trxOptions.fromVelocyPack(body);
  if (trxOptions.lockTimeout < 0.0) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "<lockTiemout> needs to be positive");
    return;
  }

  // parse the collections to register
  if (!body.isObject() || !body.get("collections").isObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
    return;
  }
  auto fillColls = [](VPackSlice const& slice, std::vector<std::string>& cols) {
    if (slice.isNone()) {  // ignore nonexistant keys
      return true;
    } else if (!slice.isArray()) {
      return false;
    }
    for (VPackSlice val : VPackArrayIterator(slice)) {
      if (!val.isString() || val.getStringLength() == 0) {
        return false;
      }
      cols.emplace_back(val.copyString());
    }
    return true;
  };
  std::vector<std::string> readCols, writeCols, exlusiveCols;
  VPackSlice collections = body.get("collections");
  bool isValid = fillColls(collections.get("read"), readCols) &&
                 fillColls(collections.get("write"), writeCols) &&
                 fillColls(collections.get("exclusive"), exlusiveCols);
  if (!isValid) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
    return;
  }

  // now start our own transaction
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  auto state = engine->createTransactionState(_vocbase, tid, trxOptions);
  TRI_ASSERT(state != nullptr);

  // lock collections
  CollectionNameResolver resolver(_vocbase);
  // if (!ServerState::instance()->isCoordinator()) {
  auto lockCols = [&](std::vector<std::string> cols, AccessMode::Type mode) {
    for (auto const& cname : cols) {
      TRI_voc_cid_t cid = 0;
      if (state->isCoordinator()) {
        cid = resolver.getCollectionIdCluster(cname);
      } else {  // only support local collections / shards
        cid = resolver.getCollectionIdLocal(cname);
      }
      if (cid == 0) {
        LOG_DEVEL << "collection " << cname << " not found";
        return false;
      }
      state->addCollection(cid, cname, mode, 0, false);
    }
    return true;
  };
  if (!lockCols(exlusiveCols, AccessMode::Type::EXCLUSIVE) ||
      !lockCols(writeCols, AccessMode::Type::WRITE) ||
      !lockCols(readCols, AccessMode::Type::READ)) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }
  //}

  // start the transaction
  transaction::Hints hints;
  hints.set(transaction::Hints::Hint::LOCK_ENTIRELY);
  hints.set(transaction::Hints::Hint::MANAGED);
  Result res = state->beginTransaction(hints);  // registers with transaction manager
  if (res.fail()) {
    generateError(res);
    return;
  }
  TRI_ASSERT(state->isRunning());

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TransactionManager* mgr = TransactionManagerFeature::manager();
  TransactionState* tmp = mgr->lookup(tid, TransactionManager::Ownership::Lease);
  TRI_ASSERT(tmp != nullptr);
  tmp->decreaseNesting();  // release
#endif

  generateTransactionResult(rest::ResponseCode::CREATED, state.get());
  state.release();  // this is owned by the TransactionManager now
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

  TransactionManager* mgr = TransactionManagerFeature::manager();
  std::unique_ptr<TransactionState> state(
      mgr->lookup(tid, TransactionManager::Ownership::Move));
  if (!state) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_TRANSACTION_NOT_FOUND);
    return;
  }

  auto ctx = std::make_shared<transaction::SmartContext>(_vocbase, state.get());
  transaction::Options trxOpts;
  ManagingTransaction trx(ctx, trxOpts);
  TRI_ASSERT(trx.state()->isRunning());
  TRI_ASSERT(trx.state()->nestingLevel() == 1);
  state->decreaseNesting();
  TRI_ASSERT(trx.state()->isTopLevelTransaction());
  state.release();  // top-level transactions are now owned by transaction::Methods
  Result res = trx.commit();
  TRI_ASSERT(!trx.state()->isRunning());

  if (res.fail()) {
    generateError(res);
  } else {
    generateTransactionResult(rest::ResponseCode::OK, trx.state());
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

  TransactionManager* mgr = TransactionManagerFeature::manager();
  std::unique_ptr<TransactionState> state(
      mgr->lookup(tid, TransactionManager::Ownership::Move));
  if (!state) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_TRANSACTION_NOT_FOUND);
    return;
  }

  auto ctx = std::make_shared<transaction::SmartContext>(_vocbase, state.get());
  transaction::Options trxOpts;
  ManagingTransaction trx(ctx, trxOpts);
  TRI_ASSERT(trx.state()->isRunning());
  TRI_ASSERT(trx.state()->nestingLevel() == 1);
  state->decreaseNesting();
  TRI_ASSERT(trx.state()->isTopLevelTransaction());
  state.release();  // top-level transactions are now owned by transaction::Methods
  Result res = trx.abort();
  TRI_ASSERT(!trx.state()->isRunning());

  if (res.fail()) {
    generateError(res);
  } else {
    generateTransactionResult(rest::ResponseCode::OK, trx.state());
  }
}

void RestTransactionHandler::generateTransactionResult(rest::ResponseCode code,
                                                       TransactionState* state) {
  VPackBuilder b;
  b.openObject();
  b.add("id", VPackValue(std::to_string(state->id())));
  b.add("state", VPackValue(transaction::statusString(state->status())));
  if (state->status() == transaction::Status::RUNNING) {
    b.add("options", VPackValue(VPackValueType::Object));
    state->options().toVelocyPack(b);
    b.close();
  }
  b.close();
  generateOk(code, b.slice());
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
        Result(TRI_ERROR_BAD_PARAMETER, "could not acquire v8 context"));
    return;
  }

  std::string portType = _request->connectionInfo().portType();

  _v8Context = V8DealerFeature::DEALER->enterContext(&_vocbase, true /*allow use database*/);

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
  if (!v8::V8::IsExecutionTerminating(isolate)) {
    v8::V8::TerminateExecution(isolate);
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
