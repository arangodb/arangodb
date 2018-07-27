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

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
#include <boost/optional.hpp>
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
  if (!batchToStringMap.empty()) {
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

static Result expectedButGotValidationError(const char* expected,
                                            const char* got) {
  std::stringstream err;
  err << "Expected type " << expected << ", got " << got << " instead.";
  return Result{TRI_ERROR_ARANGO_VALIDATION_FAILED, err.str()};
}

static Result withMessagePrefix(std::string const& prefix, Result const& res) {
  std::stringstream err;
  err << prefix << ": " << res.errorMessage();
  return Result{res.errorNumber(), err.str()};
}

ResultT<PatternWithKey> PatternWithKey::fromVelocypack(VPackSlice const slice) {
  if (!slice.isObject()) {
    return {expectedButGotValidationError("object", slice.typeName())};
  }

  VPackSlice key = slice.get("_key");

  if (!key.isNone()) {
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

 public:
  std::vector<PatternWithKey> const data;
  OperationOptions const options;

 protected:
  explicit RemoveRequest(std::vector<PatternWithKey>&& data_,
                         OperationOptions options_)
      : data(data_), options(std::move(options_)){};
};

ResultT<RemoveRequest> RemoveRequest::fromVelocypack(VPackSlice const slice) {
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
    for (auto const& it : VPackArrayIterator(dataSlice)) {
      auto maybePattern = PatternWithKey::fromVelocypack(it);
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
    if (!optionsSlice.isObject()) {
      return expectedButGotValidationError("object", optionsSlice.typeName());
    }

    // TODO parse options
    return ResultT<OperationOptions>::error(TRI_ERROR_NOT_IMPLEMENTED);
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
    return withMessagePrefix("When parsing attribute 'options'", maybeData);
  }
  auto const& options = maybeOptions.get();

  return RemoveRequest(std::move(data), options);
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

  doRemoveDocuments(removeRequest);

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

void arangodb::RestBatchDocumentHandler::doRemoveDocuments(
    const RemoveRequest& request) {
  // TODO
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}
