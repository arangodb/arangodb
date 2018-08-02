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

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
#include <boost/optional.hpp>
#include <boost/range/join.hpp>
#include <utility>

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
// BatchOperation and helpers
////////////////////////////////////////////////////////////////////////////////

namespace arangodb {
namespace rest {
namespace batch_document_handler {

enum class BatchOperation {
  READ,
  INSERT,
  REMOVE,
  REPLACE,
  UPDATE,
  UPSERT,
  REPSERT,
};

struct BatchOperationHash {
  size_t operator()(BatchOperation value) const noexcept {
    typedef std::underlying_type<decltype(value)>::type UnderlyingType;
    return std::hash<UnderlyingType>()(UnderlyingType(value));
  }
};

std::unordered_map<BatchOperation, std::string, BatchOperationHash>
    // NOLINTNEXTLINE(cert-err58-cpp)
    batchToStringMap{
        {BatchOperation::READ, "read"},
        {BatchOperation::INSERT, "insert"},
        {BatchOperation::REMOVE, "remove"},
        {BatchOperation::REPLACE, "replace"},
        {BatchOperation::UPDATE, "update"},
        {BatchOperation::UPSERT, "upsert"},
        {BatchOperation::REPSERT, "repsert"},
    };

std::unordered_map<std::string, BatchOperation> stringToBatchMap;

static std::string batchToString(BatchOperation op) {
  return batchToStringMap.at(op);
}

static void ensureStringToBatchMapIsInitialized() {
  if (!stringToBatchMap.empty()) {
    return;
  }

  for (auto const& it : batchToStringMap) {
    stringToBatchMap.insert({it.second, it.first});
  }
}

static boost::optional<BatchOperation> stringToBatch(std::string const& op) {
  ensureStringToBatchMapIsInitialized();

  auto it = stringToBatchMap.find(op);

  if (it == stringToBatchMap.end()) {
    return boost::none;
  }

  return it->second;
}
}
}
}

////////////////////////////////////////////////////////////////////////////////
// Request structs and parsers
////////////////////////////////////////////////////////////////////////////////

namespace arangodb {
namespace rest {
namespace batch_document_handler {

// The following stuff is for named parameters:
template <typename Tag, typename Type>
struct tagged_argument {
  Type const& value;
};

template <typename Tag, typename Type>
struct keyword {
  // NOLINTNEXTLINE(cppcoreguidelines-c-copy-assignment-signature,misc-unconventional-assign-operator)
  struct tagged_argument<Tag, Type> const operator=(Type const& arg) const {
    return tagged_argument<Tag, Type>{arg};
  }

  static keyword<Tag, Type> const instance;
};

template <typename Tag, typename Type>
struct keyword<Tag, Type> const keyword<Tag, Type>::instance = {};

// Parameters
namespace tag {
struct required;
struct optional;
struct deprecated;
}

using AttributeSet = std::unordered_set<std::string>;

namespace {
keyword<tag::required, AttributeSet> _required = decltype(_required)::instance;
keyword<tag::optional, AttributeSet> _optional = decltype(_optional)::instance;
keyword<tag::deprecated, AttributeSet> _deprecated =
    decltype(_deprecated)::instance;
}
// Named parameters end here

// Helper functions for parsers:

static Result expectedButGotValidationError(const char* expected,
                                            const char* got) {
  std::stringstream err;
  err << "Expected type " << expected << ", got " << got << " instead.";
  return Result{TRI_ERROR_ARANGO_VALIDATION_FAILED, err.str()};
}

static Result unexpectedAttributeError(AttributeSet const& required,
                                       AttributeSet const& optional,
                                       AttributeSet const& deprecated,
                                       std::string const& got) {
  std::stringstream err;
  err << "Encountered unexpected attribute `" << got
      << "`, allowed attributes are {";

  auto attributes = boost::join(boost::join(required, optional), deprecated);
  bool first = true;
  for (auto const& it : attributes) {
    if (!first) {
      err << ", ";
    }
    first = false;

    err << it;
  }
  err << "}.";

  return Result{TRI_ERROR_ARANGO_VALIDATION_FAILED, err.str()};
}

static Result withMessagePrefix(std::string const& prefix, Result const& res) {
  std::stringstream err;
  err << prefix << ": " << res.errorMessage();
  return Result{res.errorNumber(), err.str()};
}

static Result isObjectAndDoesNotHaveExtraAttributes(
    VPackSlice slice, tagged_argument<tag::required, AttributeSet> required_,
    tagged_argument<tag::optional, AttributeSet> optional_,
    tagged_argument<tag::deprecated, AttributeSet> deprecated_) {
  AttributeSet const& required = required_.value;
  AttributeSet const& optional = optional_.value;
  AttributeSet const& deprecated = deprecated_.value;

  if (!slice.isObject()) {
    return {expectedButGotValidationError("object", slice.typeName())};
  }

  auto contains = [](AttributeSet const& set,
                     std::string const& needle) -> bool {
    return set.find(needle) != set.end();
  };

  for (auto const& it : VPackObjectIterator(slice)) {
    std::string const key = it.key.copyString();

    if (contains(required, key) || contains(optional, key)) {
      // ok, continue
    } else if (contains(deprecated, key)) {
      // ok but warn
      // TODO some more context information would be nice, but this would have
      // have to be passed down somehow.
      LOG_TOPIC(WARN, Logger::FIXME)
          << "Deprecated attribute `" << key
          << "` encountered during request to "
          << RestVocbaseBaseHandler::BATCH_DOCUMENT_PATH;
    } else {
      return unexpectedAttributeError(required, optional, deprecated, key);
    }
  }

  return {TRI_ERROR_NO_ERROR};
}

class BatchRequest {
 public:
 protected:
  BatchRequest() = default;
};

struct PatternWithKey {
 public:
  static ResultT<PatternWithKey> fromVelocypack(VPackSlice slice);

 public:
  std::string const key;
  VPackSlice const pattern;

 protected:
  // NOLINTNEXTLINE(modernize-pass-by-value)
  PatternWithKey(std::string const& key_, VPackSlice const pattern_)
      : key(key_), pattern(pattern_) {}

  PatternWithKey(std::string&& key_, VPackSlice const pattern_)
      : key(std::move(key_)), pattern(pattern_) {}
};

ResultT<PatternWithKey> PatternWithKey::fromVelocypack(VPackSlice const slice) {
  if (!slice.isObject()) {
    return {expectedButGotValidationError("object", slice.typeName())};
  }

  VPackSlice key = slice.get("_key");

  if (key.isNone()) {
    std::stringstream err;
    err << "Attribute '_key' missing";
    return ResultT<PatternWithKey>::error(TRI_ERROR_ARANGO_VALIDATION_FAILED,
                                          err.str());
  }

  if (!key.isString()) {
    Result error = expectedButGotValidationError("string", key.typeName());

    return withMessagePrefix("When parsing attribute '_key'", error);
  }

  return ResultT<PatternWithKey>::success(
      PatternWithKey(key.copyString(), slice));
}

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
  isObjectAndDoesNotHaveExtraAttributes(
      slice, _required = AttributeSet{"data", "options"},
      _optional = AttributeSet{}, _deprecated = AttributeSet{});

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
      isObjectAndDoesNotHaveExtraAttributes(
          dataItemSlice, _required = AttributeSet{"pattern"},
          _optional = AttributeSet{}, _deprecated = AttributeSet{});

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
    Result res = isObjectAndDoesNotHaveExtraAttributes(
        optionsSlice, _required = AttributeSet{}, _optional = AttributeSet{},
        _deprecated = AttributeSet{});
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

  doRemoveDocuments(collection, removeRequest);

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
    std::string const& collection, const RemoveRequest& request) {
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

  OperationResult result =
      trx->remove(collection, search, request.getOptions(), pattern);

  res = trx->finish(result.result);

  if (result.fail()) {
    generateTransactionError(result);
    return;
  }

  if (!res.ok()) {
    std::string key;
    if (request.size() == 1) {
      key = request.getData().at(0).key;
    }
    generateTransactionError(collection, res, key);
    return;
  }

  opResults.push_back(std::move(result));

  generateBatchResponseSuccess(
      opResults,
      nullptr,
      trx->transactionContextPtr()->getVPackOptionsForDump()
  );
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

  LOG_DEVEL << builder.slice().toJson();
  writeResult(builder.slice(), *options);
}


// accepting a single operation result is not feasible because
// there might be multiple transactions involved
void RestBatchDocumentHandler::generateBatchResponseSuccess(
    std::vector<OperationResult> const& opVec,
    std::unique_ptr<VPackBuilder> extra,
    VPackOptions const* vOptions
  ){

  TRI_ASSERT(opVec.size() > 0); //at least one result
  auto& opOptions = opVec[0]._options;

  // set response code - it is assumend that all other options are the same
  auto restResponseCode = rest::ResponseCode::ACCEPTED;
  if (opOptions.waitForSync) {
    restResponseCode = rest::ResponseCode::OK;
  }

  // create and open extra if no open object has been passed
  if (!extra) {
    extra = std::make_unique<VPackBuilder>();
    extra->openObject();
  }

  extra->add(StaticStrings::Error, VPackValue(false));
  extra->close();

  // flatten result vector
  auto  result = std::make_unique<VPackBuilder>();
  result->openArray();
  for(auto& item : opVec) {
    auto slice = item.slice();
    if (slice.isObject()) {
      if(slice.hasKey(StaticStrings::KeyString)){
        result->add(slice);
      } else {
        generateError(rest::ResponseCode::I_AM_A_TEAPOT, TRI_ERROR_FAILED, "Invalid object in OperationResult");
      }
    } else if (slice.isArray()){
      for (auto r : VPackArrayIterator(slice)){
        result->add(r);
      }
    }
  }
  result->close();

  LOG_DEVEL << result->slice().toJson();
  generateBatchResponse(restResponseCode, std::move(result), std::move(extra), vOptions);
}


void RestBatchDocumentHandler::generateBatchResponseFailed(
    std::vector<OperationResult> const& opVec,
    std::unique_ptr<VPackBuilder> extra,
    VPackOptions const* vOptions
  ){

  TRI_ASSERT(opVec.size() > 0); //at least one result
  auto& opOptions = opVec[0]._options;

  // set response code - it is assumend that all other options are the same
  auto restResponseCode = rest::ResponseCode::ACCEPTED;
  if (opOptions.waitForSync) {
    restResponseCode = rest::ResponseCode::OK;
  }

  // create and open extra if no open object has been passed
  if (!extra) {
    extra = std::make_unique<VPackBuilder>();
    extra->openObject();
  }

  extra->add(StaticStrings::Error, VPackValue(false));

  std::size_t indexOfFailed = 0;
  bool foundFirstFailed = false;
  // flatten result vector
  // search for first failed result
  auto result = std::make_unique<VPackBuilder>();

  auto addErrorInfo = [&extra, &indexOfFailed, &foundFirstFailed]
    (VPackSlice errorSlice, std::string message = "defult error message", int code = TRI_ERROR_FAILED) {
    if(foundFirstFailed) {
      return;
    }
    foundFirstFailed = true;

    if(errorSlice.isNull()){
      extra->add(StaticStrings::ErrorMessage
                ,VPackValue(message));
      extra->add(StaticStrings::ErrorNum
                ,VPackValue(code));
    } else {
      extra->add(StaticStrings::ErrorMessage
                ,errorSlice.get(StaticStrings::ErrorMessage)); //if silent?
      extra->add(StaticStrings::ErrorNum
                ,errorSlice.get(StaticStrings::ErrorNum));
    }
    extra->add("errorDataIndex", VPackValue(indexOfFailed));
  };

  auto addSingle = [&](VPackSlice slice, OperationResult const& item){
    if(slice.hasKey("error")){
      if(slice.get("error").getBool()){
        result->add(slice.get("errorMessage")); //if silent?
        result->add(slice.get("errorNum")); //if silent?
        addErrorInfo(slice);
      } else {
        result->add(VPackSlice::emptyObjectSlice()); //if silent?
      }
    } else {
      result->add(slice);
    }

    if(!foundFirstFailed) {
      indexOfFailed++;
    }
  };

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
  } // item : opVec
  result->close();
  extra->close();

  LOG_DEVEL << result->slice().toJson();

  generateBatchResponse(restResponseCode, nullptr, std::move(extra), vOptions);
}


// void RestHandler::generateError(rest::ResponseCode code, int errorNumber,
//                                 std::string const& message) {
//   resetResponse(code);
//
//   VPackBuffer<uint8_t> buffer;
//   VPackBuilder builder(buffer);
//   try {
//     builder.add(VPackValue(VPackValueType::Object));
//     builder.add(StaticStrings::Error, VPackValue(true));
//     builder.add(StaticStrings::ErrorMessage, VPackValue(message));
//     builder.add(StaticStrings::Code,
//                 VPackValue(static_cast<int>(code)));
//     builder.add(StaticStrings::ErrorNum,
//                 VPackValue(errorNumber));
//     builder.close();
//
//     VPackOptions options(VPackOptions::Defaults);
//     options.escapeUnicode = true;
//
//     TRI_ASSERT(options.escapeUnicode);
//     if (_request != nullptr) {
//       _response->setContentType(_request->contentTypeResponse());
//     }
//     _response->setPayload(std::move(buffer), true, options);
//   } catch (...) {
//     // exception while generating error
//   }
// }
//
