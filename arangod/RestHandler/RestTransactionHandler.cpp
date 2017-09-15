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

#include "Basics/WriteLocker.h"
#include "Basics/ReadLocker.h"
#include "RestTransactionHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "VocBase/Methods/Transactions.h"
#include "Rest/HttpRequest.h"
#include "Basics/voc-errors.h"
#include "V8Server/V8Context.h"
#include "V8Server/V8DealerFeature.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestTransactionHandler::RestTransactionHandler(GeneralRequest* request, GeneralResponse* response)
  : RestVocbaseBaseHandler(request, response)
  , _v8Context(nullptr)
  , _lock()
{}

void RestTransactionHandler::returnContext(){
    WRITE_LOCKER(writeLock, _lock);
    V8DealerFeature::DEALER->exitContext(_v8Context);
    _v8Context = nullptr;
}

RestStatus RestTransactionHandler::execute() {
  if (_request->requestType() != rest::RequestType::POST) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, 405);
    return RestStatus::DONE;
  }

  auto slice = _request->payload();
  if(!slice.isObject()){
    generateError(Result(TRI_ERROR_BAD_PARAMETER, "could not acquire v8 context"));
    return RestStatus::DONE;
  }

  std::string portType = _request->connectionInfo().portType();

  _v8Context = V8DealerFeature::DEALER->enterContext(_vocbase, true /*allow use database*/);
  if (!_v8Context) {
    generateError(Result(TRI_ERROR_INTERNAL, "could not acquire v8 context"));
    return RestStatus::DONE;
  }

  TRI_DEFER(returnContext());

  VPackBuilder result;
  try {
    {
      WRITE_LOCKER(lock, _lock);
      if(_canceled){
        generateCanceled();
        return RestStatus::DONE;
      }
    }

    Result res = executeTransaction(_v8Context->_isolate, _lock, _canceled, slice , portType, result);
    if (res.ok()){
      VPackSlice slice = result.slice();
      if (slice.isNone()) {
        generateSuccess(rest::ResponseCode::OK, VPackSlice::nullSlice());
      } else {
        generateSuccess(rest::ResponseCode::OK, slice);
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

  return RestStatus::DONE;
}

bool RestTransactionHandler::cancel() {
  //cancel v8 transaction
  WRITE_LOCKER(writeLock, _lock);
  _canceled.store(true);
  auto isolate = _v8Context->_isolate;
  if (!v8::V8::IsExecutionTerminating(isolate)) {
      v8::V8::TerminateExecution(isolate);
  }
  return true;
}
