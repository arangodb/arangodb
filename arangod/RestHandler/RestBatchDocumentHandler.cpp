////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "RestBatchDocumentHandler.h"


#include "Basics/VelocyPackHelper.h"
#include "Cluster/ResultT.h"
#include "Transaction/Hints.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"

#include "Transaction/BatchRequests.h"

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
// BatchOperation and helpers
////////////////////////////////////////////////////////////////////////////////

namespace arangodb {
namespace rest {
namespace batch_document_handler {


}
}
}

////////////////////////////////////////////////////////////////////////////////
// RestBatchDocumentHandler
////////////////////////////////////////////////////////////////////////////////

using namespace arangodb::rest::batch_document_handler;

RestBatchDocumentHandler::RestBatchDocumentHandler(
    arangodb::GeneralRequest* request, arangodb::GeneralResponse* response)
    : RestVocbaseBaseHandler(request, response) {}

RestStatus arangodb::RestBatchDocumentHandler::execute() {
  auto const type = _request->requestType();

  std::string const usage{
      "expecting POST /_api/batch/document/<collection>/<operation> with a "
      "BODY"};

  if (type != rest::RequestType::POST) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED, usage);
  }

  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  if (suffixes.size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER, usage);
    return RestStatus::DONE;
  }

  std::string const& collection = suffixes[0];
  std::string const& opString = suffixes[1];

  auto maybeOp = batch::stringToBatch(opString);

  if (!maybeOp) {
    std::stringstream err;
    err << "Invalid operation " << opString << ": Expecting one of "
        << "read, insert, update, replace, remove, upsert or repsert.";
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  err.str());
    return RestStatus::DONE;
  }

  //auto maybeRequest = barch::fromVelocypack(_request->payload(), maybeOp.get());
  //if (maybeRequest.fail()) {
  //  generateError(maybeRequest);
  //  return RestStatus::FAIL;
  //}
  auto maybeRequest = batch::createRequestFromSlice<batch::RemoveDoc>(_request->payload());
  executeBatchRequest(collection, std::move(maybeRequest.get()));

  return RestStatus::DONE;
}


void RestBatchDocumentHandler::generateBatchResponse(
    std::unique_ptr<VPackBuilder> result,
    ExtraInformation&& extra,
    VPackOptions const* options
  ){

  if(result){
    // expects object that is open
    // and has an open array results
    //
    // {
    //   result: [ <item>...
    //
    //

    TRI_ASSERT(result->isOpenArray());
    result->close();
    TRI_ASSERT(result->isOpenObject());
     extra.addToOpenOject(*result);
    result->close();
    TRI_ASSERT(result->isClosed());
  } else {
    result = std::make_unique<VPackBuilder>();
    result->openObject();
    extra.addToOpenOject(*result);
    result->close();
  }


  resetResponse(extra.code);

  writeResult(result->slice(), *options);
}

void RestBatchDocumentHandler::generateBatchResponse(
    rest::ResponseCode restResponseCode,
    VPackSlice result,
    VPackOptions const* vpackOptions
  ){

  TRI_ASSERT(result.isObject());

  // set code
  resetResponse(restResponseCode);
  writeResult(result, *vpackOptions);
}



void RestBatchDocumentHandler::generateBatchResponse(
    std::vector<OperationResult> const& opVec,
    VPackOptions const* vOptions
  ){

  TRI_ASSERT(opVec.size() > 0); //at least one result
  auto& opOptions = opVec[0]._options;

  // set response code
  // on error the rest code will be updated in the addErrorInfo lambda
  ExtraInformation extraInfo;
  if (opOptions.waitForSync) {
    extraInfo.code = rest::ResponseCode::OK;
  }

  if(opVec.size() == 1) {
    const OperationResult& opRes = opVec[0];
    if(opRes.fail()) {
      extraInfo.code = arangodb::GeneralResponse::responseCode(opRes.errorNumber());
    }

    if(opRes.buffer){
      generateBatchResponse(extraInfo.code, opRes.slice(),vOptions);
    } else {
      extraInfo.errorMessage = opRes.errorMessage();
      extraInfo.errorMessage = opRes.errorNumber();
      generateBatchResponse(nullptr, std::move(extraInfo),vOptions);
    }
    return;
  }

  std::size_t errorIndex = 0;
  auto result = std::make_unique<VPackBuilder>();
  result->openObject();
  result->add(VPackValue("result"));
  result->openArray();
  // result looks like ->    { result : [

  // flatten result vector
  // search for first failed result
  for(auto& item : opVec) {
    if (item.buffer) { //check for vaild buffer
      VPackSlice slice = item.slice();
      TRI_ASSERT(slice.isObject());
      TRI_ASSERT(slice.hasKey("result"));
      TRI_ASSERT(slice.hasKey(StaticStrings::Error));

      // copy results
      TRI_ASSERT(result->isOpenArray());
      VPackSlice resultArray = item.slice().get("result");
      TRI_ASSERT(resultArray.isArray());
      for(auto r : VPackArrayIterator(resultArray)){
        result->add(r);
      }

      if(item.fail()){
        if(!extraInfo.errorOccured()){
          extraInfo.errorMessage = item.errorMessage();
          extraInfo.errorNumber = item.errorNumber();
          extraInfo.errorDataIndex = errorIndex + slice.get("errorDataIndex").getUInt();
        }
      } else {
        errorIndex += resultArray.length();
      }
    } else {
      if(item.fail()){
        if(!extraInfo.errorOccured()){
          extraInfo.errorMessage = item.errorMessage();
          extraInfo.errorNumber = item.errorNumber();
          extraInfo.errorDataIndex = errorIndex;
        }
      }
    }
  } // item : opVec

  result->close();
  generateBatchResponse(std::move(result), std::move(extraInfo), vOptions);
}
