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

#include "Cluster/ResultT.h"
#include "Transaction/Hints.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
//#include "Basics/StringRef.h"

#include "RestBatchDocumentHandlerHelper.h"

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
// BatchOperation and helpers
////////////////////////////////////////////////////////////////////////////////

namespace arangodb {
namespace rest {
namespace batch_document_handler {

Result
CheckAttributesInVelocypackArray(VPackSlice const dataSlice
                       ,AttributeSet const& required
                       ,AttributeSet const& optional
                       ,AttributeSet const& deprecated
                       ){

  Result result = expectedType(VPackValueType::Array, dataSlice.type());
  if(result.fail()){
    return result;
  }

  std::vector<PatternWithKey> data;

  for (auto const& dataItemSlice : VPackArrayIterator(dataSlice)) {
    auto maybeAttributes = expectedAttributes(dataItemSlice, required, optional, deprecated);

    if (maybeAttributes.fail()){
      return {maybeAttributes};
    }
  }

  return {};
};


ResultT<OperationOptions>
optionsFromVelocypack(VPackSlice const optionsSlice
                     ,AttributeSet const& required
                     ,AttributeSet const& optional
                     ,AttributeSet const& deprecated
                     ){
  Result res = expectedAttributes(optionsSlice, required, optional, deprecated);
  if (res.fail()) { return prefixResultMessage(res, "Error occured while pasing options for batchDocumentOperation: "); }
  OperationOptions options = createOperationOptions(optionsSlice);
  return ResultT<OperationOptions>::success(std::move(options));
};


struct BatchRequest {
  static ResultT<BatchRequest> fromVelocypack(VPackSlice const slice, BatchOperation batchOp) {

    AttributeSet required = {"data"};
    AttributeSet optional = {"options"};
    AttributeSet deprecated;

    auto const maybeAttributes = expectedAttributes(slice, required, optional, deprecated);
    if(maybeAttributes.fail()){ return {maybeAttributes}; }

    ////////////////////////////////////////////////////////////////////////////////////////
    // data
    VPackSlice const dataSlice = slice.get("data"); // data is requuired

    required.clear();
    optional.clear();
    deprecated.clear();


    switch (batchOp) {
      case BatchOperation::READ:
      case BatchOperation::REMOVE:
        required.insert("pattern");
        break;
      case BatchOperation::INSERT:
        required.insert("insertDocument");
        break;
      case BatchOperation::REPLACE:
        required.insert("replaceDocument");
        break;
      case BatchOperation::UPDATE:
        required.insert("updateDocument");
        break;
      case BatchOperation::UPSERT:
        required.insert("pattern");
        required.insert("insertDocument");
        required.insert("updateDocument");
        break;
      case BatchOperation::REPSERT:
        required.insert("pattern");
        required.insert("replaceDocument");
        required.insert("updateDocument");
        break;
    }

    auto maybeData = CheckAttributesInVelocypackArray(dataSlice, required, optional, deprecated);
    if (maybeData.fail()) {
      return prefixResultMessage(maybeData, "When parsing attribute 'data'");
    }

    ////////////////////////////////////////////////////////////////////////////////////////
    // options
    OperationOptions options;

    if(maybeAttributes.get().find("options") != maybeAttributes.get().end()) {
      required = {};
      optional = {"oneTransactionPerDOcument", "checkGraphs", "graphName"};
      // mergeObjects "ignoreRevs"  "isRestore" keepNull?!
      deprecated = {};
      switch (batchOp) {
        case BatchOperation::READ:
          optional.insert("graphName");
          break;
        case BatchOperation::INSERT:
        case BatchOperation::UPSERT:
        case BatchOperation::UPDATE:
        case BatchOperation::REPSERT:
        case BatchOperation::REPLACE:
          optional.insert("returnNew"); //please fall through
        case BatchOperation::REMOVE:
          optional.insert("waitForSync");
          optional.insert("returnOld");
          optional.insert("silent");
          break;
      }
      VPackSlice const optionsSlice = slice.get("options");

      //LOG_DEVEL << optionsSlice.toJson();

      auto const maybeOptions = optionsFromVelocypack(optionsSlice, required, optional, deprecated);
      if (maybeOptions.fail()) {
        return prefixResultMessage(maybeOptions, "When parsing attribute 'options'");
      }
      options = maybeOptions.get();
    }

    // LOG_DEVEL << "options.silent: " << std::boolalpha << options.silent;

    // create
    return BatchRequest(slice, /*std::move(maybeData.get()),*/ std::move(options), batchOp);
  }

  explicit BatchRequest(VPackSlice const slice
                       ,OperationOptions options_
                       ,BatchOperation op)
           :options(std::move(options_))
           ,operation(op)
           ,payload(slice)
  {};

  OperationOptions const options;
  BatchOperation operation;
  VPackSlice const payload;
};

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

  auto maybeOp = stringToBatch(opString);

  if (!maybeOp) {
    std::stringstream err;
    err << "Invalid operation " << opString << ": Expecting one of "
        << "read, insert, update, replace, remove, upsert or repsert.";
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  err.str());
    return RestStatus::DONE;
  }

  auto maybeRequest = BatchRequest::fromVelocypack(_request->payload(), maybeOp.get());
  if (maybeRequest.fail()) {
    generateError(maybeRequest);
    return RestStatus::FAIL;
  }

  executeBatchRequest(collection, maybeRequest.get());

  return RestStatus::DONE;
}

void arangodb::RestBatchDocumentHandler::executeBatchRequest(
    std::string const& collection, const BatchRequest& request) {

  TRI_ASSERT(request.payload.isObject()); //should be the same as the line above

  std::vector<OperationResult> opResults; // will hold the result

  velocypack::Options vOptions;
  bool vOptionsSet = false;
  VPackBuilder builder;

  VPackSlice data = request.payload.get("data");
  TRI_ASSERT(data.isArray());

  if(data.isEmptyArray()){
    generateError(rest::ResponseCode::BAD, TRI_ERROR_ARANGO_VALIDATION_FAILED, "you did not provide any data for the restBatchHandeler");
  }

  for(auto slice : VPackArrayIterator(data)){
    bool doBreak = false;
    VPackSlice payload;

    if(!request.options.oneTransactionPerDocument){
      payload = request.payload;
      doBreak = true;
    } else {
      builder.clear();
      builder.openObject();
      builder.add("data", slice);
      builder.close();
      payload = builder.slice();
    }

    auto trx = createTransaction(collection, AccessMode::Type::WRITE);
    if(!vOptionsSet){
      vOptions = *(trx->transactionContextPtr()->getVPackOptionsForDump());
    }

    Result transactionResult = trx->begin();

    if (transactionResult.fail()) {
      opResults.push_back(OperationResult(transactionResult));
      if(doBreak) { break; };
      continue;
    }


    OperationResult operationResult;
    switch (request.operation) {
      case BatchOperation::READ:
        generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                      TRI_ERROR_NOT_IMPLEMENTED);
        break;
      case BatchOperation::INSERT:
        generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                      TRI_ERROR_NOT_IMPLEMENTED);
        break;
      case BatchOperation::REMOVE:
        operationResult = trx->removeBatch(collection, payload, request.options);
        break;
      case BatchOperation::REPLACE: // implement first
        generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                      TRI_ERROR_NOT_IMPLEMENTED);
        //operationResult = trx->replaceBatch(collection, payload, request.options);
        generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                      TRI_ERROR_NOT_IMPLEMENTED);
        break;
      case BatchOperation::UPDATE:  // implement first
        //operationResult = trx->updateBatch(collection, payload, request.options);
        generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                      TRI_ERROR_NOT_IMPLEMENTED);
        break;
      case BatchOperation::UPSERT:
        generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                      TRI_ERROR_NOT_IMPLEMENTED);
        break;
      case BatchOperation::REPSERT:
        generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                      TRI_ERROR_NOT_IMPLEMENTED);
        break;
    }
    transactionResult = trx->finish(operationResult.result);

    if (operationResult.ok() && transactionResult.fail()) {
      opResults.push_back(OperationResult(transactionResult));
      if(doBreak) { break; };
      continue;
    }
    opResults.push_back(std::move(operationResult));

    if(doBreak) { break; };

  } // for item in "data"

  generateBatchResponse(
      opResults,
      &vOptions
  );
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
