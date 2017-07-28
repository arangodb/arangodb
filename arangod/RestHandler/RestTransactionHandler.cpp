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

#include "ApplicationFeatures/ApplicationServer.h"
#include "VocBase/Methods/Transactions.h"
#include "Rest/HttpRequest.h"
#include "Basics/voc-errors.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestTransactionHandler::RestTransactionHandler(GeneralRequest* request, GeneralResponse* response)
  : RestVocbaseBaseHandler(request, response) {}

RestStatus RestTransactionHandler::execute() {
  if (_request->requestType() != rest::RequestType::POST) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, 405);
    return RestStatus::DONE;
  }

  VPackBuilder result;
  try {
    Result res = executeTransaction(_vocbase, _request->payload(), result);
    if (res.ok()){
      VPackSlice slice = result.slice();
      if (slice.isNone()) {
        //generateError(GeneralResponse::responseCode(TRI_ERROR_TYPE_ERROR), TRI_ERROR_TYPE_ERROR, "result is undefined");
        generateSuccess(rest::ResponseCode::OK, VPackSlice::nullSlice());
      } else {
        generateSuccess(rest::ResponseCode::OK, slice);
      }
    } else {
      generateError(res);
    }
  } catch (arangodb::basics::Exception const& ex) {
    generateError(GeneralResponse::responseCode(ex.code()),ex.code(), ex.what());
  } catch (std::exception const& ex) {
    generateError(GeneralResponse::responseCode(TRI_ERROR_INTERNAL), TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    generateError(GeneralResponse::responseCode(TRI_ERROR_INTERNAL), TRI_ERROR_INTERNAL);
  }

  return RestStatus::DONE;
}
