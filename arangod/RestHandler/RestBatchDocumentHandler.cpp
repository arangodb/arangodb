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

class RemoveRequest : public BatchRequest {
 public:
  static ResultT<RemoveRequest> fromVelocypack(VPackSlice);

  void toSearch(VPackBuilder& builder) const;
  void toPattern(VPackBuilder& builder) const;

  bool empty() const;
  size_t size() const;

  OperationOptions const& getOptions() const;

  std::vector<PatternWithKey> const& getData() const;

 protected:
  explicit RemoveRequest(std::vector<PatternWithKey>&& data_,
                         OperationOptions options_)
      : data(data_), options(std::move(options_)){};

  std::vector<PatternWithKey> const data;
  OperationOptions const options;
};

ResultT<RemoveRequest> RemoveRequest::fromVelocypack(VPackSlice const slice) {
  isObjectAndDoesNotHaveExtraAttributes(slice,
      {"data", "options"} /*required*/, {} /*optional*/, {} /*deprecated*/);
  if (!slice.isObject()) {
    return expectedButGotValidationError("object", slice.typeName());
  }
  auto dataFromVelocypack =
      [](VPackSlice const dataSlice) -> ResultT<std::vector<PatternWithKey>> {
    if (!dataSlice.isArray()) {
      return expectedButGotValidationError("array", dataSlice.typeName());
    }

    std::vector<PatternWithKey> data;
    data.reserve(dataSlice.length());

    size_t i = 0;
    for (auto const& dataItemSlice : VPackArrayIterator(dataSlice)) {
      isObjectAndDoesNotHaveExtraAttributes( dataItemSlice,
          {"pattern"} /*required*/, {} /*optional*/, {} /*deprecated*/);

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

  auto optionsFromVelocypack =
      [](VPackSlice const optionsSlice) -> ResultT<OperationOptions> {
    // TODO add possible options
    Result res = isObjectAndDoesNotHaveExtraAttributes( optionsSlice,
          {} /*required*/, {} /*optional*/, {} /*deprecated*/);
    if (res.fail()) {
      return {res};
    }

    OperationOptions options{};

    // TODO parse and set options

    return ResultT<OperationOptions>::success(options);
  };

  VPackSlice const dataSlice = slice.get("data");
  auto maybeData = dataFromVelocypack(dataSlice);
  if (maybeData.fail()) {
    return withMessagePrefix("When parsing attribute 'data'", maybeData);
  }
  auto& data = maybeData.get();

  VPackSlice const optionsSlice = slice.get("options");
  auto const maybeOptions = optionsFromVelocypack(optionsSlice);
  if (maybeOptions.fail()) {
    return withMessagePrefix("When parsing attribute 'options'", maybeOptions);
  }
  auto const& options = maybeOptions.get();

  return RemoveRequest(std::move(data), options);
}

OperationOptions const& RemoveRequest::getOptions() const {
  return options;
}

// builds an array of "_key"s, or a single string in case data.size() == 1
void RemoveRequest::toSearch(VPackBuilder &builder) const {
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
void RemoveRequest::toPattern(VPackBuilder &builder) const {
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

bool RemoveRequest::empty() const {
  return data.empty();
}

size_t RemoveRequest::size() const {
  return data.size();
}

std::vector<PatternWithKey> const &RemoveRequest::getData() const {
  return data;
}

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
      removeDocumentsAction(collection);
      break;
    case BatchOperation::REPLACE:
      replaceDocumentsAction(collection);
      break;
    case BatchOperation::UPDATE:
      updateDocumentsAction(collection);
      break;
    case BatchOperation::READ:
    case BatchOperation::INSERT:
    case BatchOperation::UPSERT:
    case BatchOperation::REPSERT:
      generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                    TRI_ERROR_NOT_IMPLEMENTED);
  }

  return RestStatus::DONE;
}

void RestBatchDocumentHandler::removeDocumentsAction(
    std::string const& collection) {
  auto parseResult = RemoveRequest::fromVelocypack(_request->payload());

  if (parseResult.fail()) {
    generateError(parseResult);
    return;
  }
  RemoveRequest const& removeRequest = parseResult.get();

  doRemoveDocuments(collection, removeRequest, _request->payload());

  // TODO take a result from doRemoveDocuments and generate a response with it.
}

void RestBatchDocumentHandler::replaceDocumentsAction(
    std::string const& collection) {
  // TODO
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

void RestBatchDocumentHandler::updateDocumentsAction(
    std::string const& collection) {
  // TODO
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

// TODO This method should not generate the response but only return some
// result(s) - the response-handling should be done in removeDocumentsAction()!
// TODO This is more or less a copy&paste from the RestDocumentHandler.
// However, our response looks quite different, so this has to be reimplemented.
void arangodb::RestBatchDocumentHandler::doRemoveDocuments(
    std::string const& collection, const RemoveRequest& request,
    VPackSlice vpack_request) {
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

  VPackBuilder searchBuilder;
  VPackBuilder patternBuilder;
  request.toSearch(searchBuilder);
  request.toPattern(patternBuilder);
  VPackSlice const search = searchBuilder.slice();
  VPackSlice const pattern = patternBuilder.slice();
  std::vector<OperationResult> opResults; // will hold the result

  auto trx = createTransaction(collection, AccessMode::Type::WRITE);

  // TODO I don't know why yet, but this causes a deadlock currently.
  // It may be a bug introduced in Methods.cpp
  // if (request.size() == 1) {
  //   trx->addHint(transaction::Hints::Hint::SINGLE_OPERATION);
  // }

  Result res = trx->begin();

  if (!res.ok()) {
    generateTransactionError(collection, res, "");
    return;
  }

  OperationOptions options = request.getOptions();
  OperationResult result =
      //trx->remove(collection, search, request.getOptions(), pattern);
    trx->removeBatch(collection, vpack_request, options);

  res = trx->finish(result.result);

  if (result.ok() && !res.ok()) {
    std::string key;
    if (request.size() == 1) {
      key = request.getData().at(0).key;
    }
    generateTransactionError(collection, res, key);
    return;
  }

  opResults.push_back(std::move(result));

  // TEST TEST TEST
  //generateBatchResponse(
  //    opResults,
  //    trx->transactionContextPtr()->getVPackOptionsForDump()
  //);
}

void RestBatchDocumentHandler::generateBatchResponse(
    rest::ResponseCode restResponseCode,
    std::unique_ptr<VPackBuilder> result,
    std::unique_ptr<VPackBuilder> extra,
    VPackOptions const* options
  ){

  TRI_ASSERT(result->slice().isArray());
  TRI_ASSERT(extra->slice().isObject());

  // set code
  resetResponse(restResponseCode);

  VPackBuilder builder;
  {
    // open response object
    builder.openObject();
    // add result
    builder.add("result", result->slice());
    result.release();

    // add other values
    for(auto item : VPackObjectIterator(extra->slice())){
      StringRef ref(item.key);
      builder.add(ref.data(),ref.size(), item.value);
    }
    // close response object
    builder.close();
  }

  writeResult(builder.slice(), *options);
}

void RestBatchDocumentHandler::generateBatchResponse(
    rest::ResponseCode restResponseCode,
    VPackSlice result,
    VPackOptions const* options
  ){

  TRI_ASSERT(result.isObject());

  // set code
  resetResponse(restResponseCode);
  writeResult(result, *options);
}

void RestBatchDocumentHandler::generateBatchResponse(
    std::vector<OperationResult> const& opVec,
    VPackOptions const* vOptions
  ){

  TRI_ASSERT(opVec.size() > 0); //at least one result
  auto& opOptions = opVec[0]._options;

  // set response code
  // on error the rest code will be updated in the addErrorInfo lambda
  auto restResponseCode = rest::ResponseCode::ACCEPTED;
  if (opOptions.waitForSync) {
    restResponseCode = rest::ResponseCode::OK;
  }

  // create and open extra
  auto extra = std::make_unique<VPackBuilder>();
  extra->openObject();


  std::size_t indexOfFailed = 0;
  bool foundFirstFailed = false;
  auto result = std::make_unique<VPackBuilder>();

  auto addErrorInfo = [&extra, &indexOfFailed, &foundFirstFailed, &restResponseCode]
    (VPackSlice errorSlice, std::string message = "defult error message", int errorNum = TRI_ERROR_FAILED) {
    if(foundFirstFailed) {
      return;
    }
    foundFirstFailed = true;

    if(errorSlice.isNull()){
      extra->add(StaticStrings::ErrorMessage
                ,VPackValue(message));
      extra->add(StaticStrings::ErrorNum
                ,VPackValue(errorNum));
    } else {
      extra->add(StaticStrings::ErrorMessage
                ,errorSlice.get(StaticStrings::ErrorMessage)); //if silent?
      extra->add(StaticStrings::ErrorNum
                ,errorSlice.get(StaticStrings::ErrorNum));
    }
    extra->add("errorDataIndex", VPackValue(indexOfFailed));
    restResponseCode = arangodb::GeneralResponse::responseCode(errorNum);
  };

  // add single element to results
  auto addSingle = [&](VPackSlice slice, OperationResult const& item){
    if(slice.hasKey("error")){
      if(slice.get("error").getBool()){
        result->openObject();
        result->add(StaticStrings::ErrorMessage
                   ,slice.get(StaticStrings::ErrorMessage));
        result->add(StaticStrings::ErrorNum
                   ,slice.get(StaticStrings::ErrorNum));
        result->close();
        addErrorInfo(slice);
      } else {
        result->add(slice);
      }
    } else {
      result->add(slice);
    }

    if(!foundFirstFailed) {
      indexOfFailed++;
    }
  };

  // flatten result vector
  // search for first failed result
  result->openArray();
  for(auto& item : opVec) {
    if (item.buffer) { //check for vaild buffer
      VPackSlice slice = item.slice();
      if (slice.isObject()) {
        if(slice.hasKey(StaticStrings::KeyString)){
          addSingle(slice, item);
        } else {
          generateError(rest::ResponseCode::I_AM_A_TEAPOT
                       ,TRI_ERROR_FAILED
                       ,"Invalid object in OperationResult"
                       );
        }
      } else if (slice.isArray()){
        for (auto r : VPackArrayIterator(slice)){
          addSingle(r, item);
        }
      } else {
        //error - internal error?!
      }
    } else { // no valid buffer
      addErrorInfo(VPackSlice::nullSlice()
                  ,item.errorMessage()
                  ,item.errorNumber()
                  );
    }// if(item.buffer)
    if(item.fail()){
      if(!foundFirstFailed) {
      addErrorInfo(VPackSlice::nullSlice()
                  ,item.errorMessage()
                  ,item.errorNumber()
                  );
      }
      result->openObject();
      result->add(StaticStrings::ErrorMessage
                 ,VPackValue(item.errorMessage())
                 );
      result->add(StaticStrings::ErrorNum
                 ,VPackValue(item.errorNumber())
                 );
      result->close();
      TRI_ASSERT(result->isOpenArray());
    }
  } // item : opVec

  extra->add(StaticStrings::Error, VPackValue(foundFirstFailed));

  TRI_ASSERT(result->isOpenArray());
  result->close();
  TRI_ASSERT(extra->isOpenObject());
  extra->close();

  generateBatchResponse(restResponseCode, std::move(result), std::move(extra), vOptions);
}
