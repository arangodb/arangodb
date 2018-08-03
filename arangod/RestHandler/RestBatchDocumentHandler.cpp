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
////////////////////////////////////////////////////////////////////////////////

#include "RestBatchDocumentHandler.h"

#include "Cluster/ResultT.h"
#include "Transaction/Hints.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "Basics/StringRef.h"
#include "Basics/TaggedParameters.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
#include <boost/optional.hpp>
#include <boost/range/join.hpp>
#include <utility>

#include "RestBatchDocumentHandlerHelper.h"

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
// BatchOperation and helpers
////////////////////////////////////////////////////////////////////////////////

namespace arangodb {
namespace rest {
namespace batch_document_handler {

ResultT<std::vector<PatternWithKey>>
dataFromVelocypackArray(VPackSlice const dataSlice
                       ,AttributeSet const& required
                       ,AttributeSet const& optional
                       ,AttributeSet const& deprecated
                       ){
  Result result = expectedType(VPackValueType::Array, dataSlice.type());
  if(result.fail()){
    return result;
  }

  std::vector<PatternWithKey> data;
  data.reserve(dataSlice.length());

  size_t i = 0;
  for (auto const& dataItemSlice : VPackArrayIterator(dataSlice)) {
    isObjectAndDoesNotHaveExtraAttributes(dataItemSlice, required, optional, deprecated);

    VPackSlice patternSlice = dataItemSlice.get("pattern");
    if (patternSlice.isNone()) {
      std::stringstream err;
      err << "Attribute 'pattern' missing";
      return ResultT<PatternWithKey>::error(
          TRI_ERROR_ARANGO_VALIDATION_FAILED, err.str());
    }

    auto maybePattern = PatternWithKey::fromVelocypack(patternSlice);
    if (maybePattern.fail()) {
      std::stringstream err;
      err << "In array index " << i;
      return withMessagePrefix(err.str(), maybePattern);
    }

    data.emplace_back(maybePattern.get());

    ++i;
  }

  return {std::move(data)};
};


ResultT<OperationOptions>
optionsFromVelocypack(VPackSlice const optionsSlice
                     ,AttributeSet const& required
                     ,AttributeSet const& optional
                     ,AttributeSet const& deprecated
                     ){
  // TODO add possible options -- should really return attribute set containing set options
  ResultT<AttributeSet> res = isObjectAndDoesNotHaveExtraAttributes(
                                optionsSlice, required, optional, deprecated
                              );

  if (res.fail()) { return res; }

  OperationOptions options;
  //TODO implement: OperationOptionsFromSlice(slice)

  return ResultT<OperationOptions>::success(options);
};


struct BatchRequest {
  static ResultT<BatchRequest> fromVelocypack(VPackSlice const slice, BatchOperation batchOp) {

    AttributeSet required, optional, deprecated;

    switch (batchOp) {
      case BatchOperation::REMOVE:
        required = {"data", "options"};
        break;
      case BatchOperation::REPLACE:
      case BatchOperation::UPDATE:
      case BatchOperation::READ:
      case BatchOperation::INSERT:
      case BatchOperation::UPSERT:
      case BatchOperation::REPSERT:
        break;
    }

    isObjectAndDoesNotHaveExtraAttributes(slice, required, optional, deprecated);

    // get data
    required = {};
    optional = {};
    deprecated = {};
    switch (batchOp) {
      case BatchOperation::REMOVE:
        required = {"pattern"};
        break;
      case BatchOperation::REPLACE:
      case BatchOperation::UPDATE:
      case BatchOperation::READ:
      case BatchOperation::INSERT:
      case BatchOperation::UPSERT:
      case BatchOperation::REPSERT:
        break;
    }

    VPackSlice const dataSlice = slice.get("data");
    auto maybeData = dataFromVelocypackArray(dataSlice, required, optional, deprecated);
    if (maybeData.fail()) {
      return withMessagePrefix("When parsing attribute 'data'", maybeData);
    }
    auto& data = maybeData.get();

    // get options
    required = {};
    optional = {};
    deprecated = {};
    switch (batchOp) {
      case BatchOperation::REMOVE:
        break;
      case BatchOperation::REPLACE:
      case BatchOperation::UPDATE:
      case BatchOperation::READ:
      case BatchOperation::INSERT:
      case BatchOperation::UPSERT:
      case BatchOperation::REPSERT:
        break;
    }

    VPackSlice const optionsSlice = slice.get("options");
    auto const maybeOptions = optionsFromVelocypack(optionsSlice, required, optional, deprecated);
    if (maybeOptions.fail()) {
      return withMessagePrefix("When parsing attribute 'options'", maybeOptions);
    }
    auto const& options = maybeOptions.get();

    return BatchRequest(slice, std::move(data), options, batchOp);
  }

  // builds an array of "_key"s, or a single string in case data.size() == 1
  void toSearch(VPackBuilder &builder) const {
    // TODO I'm in favour of removing the special case for 1, but the existing
    // methods currently somewhat rely on an array containing at least 2 elements.
    if (size() == 1) {
      builder.add(VPackValue(data.at(0).key));
      return;
    }

    builder.openArray();
    for (auto const& it : data) {
      builder.add(VPackValue(it.key));
    }
    builder.close();
  }

  // builds an array of patterns as externals, or a single pattern in case data.size() == 1
  void toPattern(VPackBuilder &builder) const {
    // TODO I'm in favour of removing the special case for 1, but the existing
    // methods currently somewhat rely on an array containing at least 2 elements.
    if (size() == 1) {
      builder.addExternal(data.at(0).pattern.start());
      return;
    }

    builder.openArray();
    for (auto const& it : data) {
      builder.addExternal(it.pattern.start());
    }
    builder.close();
  }

  bool empty() const { return data.empty(); }
  size_t size() const { return data.size(); }

  explicit BatchRequest(VPackSlice const slice
                       ,std::vector<PatternWithKey>&& data_
                       ,OperationOptions options_
                       ,BatchOperation op)
           :data(data_)
           ,options(std::move(options_))
           ,operation(op)
           ,payload(slice)
  {};

  std::vector<PatternWithKey> const data;
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

  BatchOperation const operation = maybeOp.get();

  switch (operation) {
    case BatchOperation::REMOVE:
      createBatchRequest(collection, operation);
      break;
    case BatchOperation::REPLACE: // implement first
    case BatchOperation::UPDATE:  // implement first

    case BatchOperation::READ:
    case BatchOperation::INSERT:
    case BatchOperation::UPSERT:
      generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                    TRI_ERROR_NOT_IMPLEMENTED);
      break;
    case BatchOperation::REPSERT:
      generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                    TRI_ERROR_NOT_IMPLEMENTED);
  }

  return RestStatus::DONE;
}

void RestBatchDocumentHandler::createBatchRequest(
    std::string const& collection,
    BatchOperation batchOp
    ) {

  //parse and validate result
  auto parseResult = BatchRequest::fromVelocypack(_request->payload(), batchOp);
  if (parseResult.fail()) {
    generateError(parseResult);
    return;
  }

  TRI_ASSERT(_request->payload() == parseResult.get().payload);

  BatchRequest const& request = parseResult.get();
  executeBatchRequest(collection, request);
}

void arangodb::RestBatchDocumentHandler::executeBatchRequest(
    std::string const& collection, const BatchRequest& request) {
  if (request.empty()) {
    // If request.data = [], the operation succeeds unless the collection lookup
    // fails.
    TRI_col_type_e colType;
    Result res = arangodb::methods::Collections::lookup(
        &_vocbase, collection,
        [&colType](LogicalCollection const& coll) { colType = coll.type(); });

    auto ctx = transaction::StandaloneContext::Create(_vocbase);
    generateDeleted(OperationResult{res}, collection, colType,
                    ctx->getVPackOptionsForDump());
    return;
  }

  std::vector<OperationResult> opResults; // will hold the result

  auto trx = createTransaction(collection, AccessMode::Type::WRITE);

  // TODO I don't know why yet, but this causes a deadlock currently.
  // It may be a bug introduced in Methods.cpp
  // if (request.size() == 1) {
  //   trx->addHint(transaction::Hints::Hint::SINGLE_OPERATION);
  // }

  Result transactionResult = trx->begin();

  if (!transactionResult.ok()) {
    generateTransactionError(collection, transactionResult, "");
    return;
  }

  TRI_ASSERT(_request->payload().isObject());
  TRI_ASSERT(request.payload.isObject()); //should be the same as the line above

  auto operationResult = trx->removeBatch(collection, request.payload, request.options);
  transactionResult = trx->finish(operationResult.result);

  if (operationResult.ok() && transactionResult.fail()) {
    std::string key;
    if (request.size() == 1) {
      key = request.data.at(0).key;
    }
    generateTransactionError(collection, transactionResult, key);
    return;
  }

  opResults.push_back(std::move(operationResult));

  generateBatchResponse(
      opResults,
      trx->transactionContextPtr()->getVPackOptionsForDump()
  );
}

void RestBatchDocumentHandler::generateBatchResponse(
    std::unique_ptr<VPackBuilder> result,
    ExtraInformation&& extra,
    VPackOptions const* options
  ){

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
    generateBatchResponse(extraInfo.code, opRes.slice(),vOptions);
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
    LOG_DEVEL << item.slice().toJson();
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
