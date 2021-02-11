////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "RestImportHandler.h"
#include "Basics/NumberUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "Transaction/Helpers.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/vocbase.h"

#include <velocypack/Collection.h>
#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestImportHandler::RestImportHandler(application_features::ApplicationServer& server,
                                     GeneralRequest* request, GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response),
      _onDuplicateAction(DUPLICATE_ERROR),
      _ignoreMissing(false) {}

RestStatus RestImportHandler::execute() {
  // set default value for onDuplicate
  _onDuplicateAction = DUPLICATE_ERROR;

  bool found;
  std::string const& duplicateType = _request->value("onDuplicate", found);

  if (found) {
    if (duplicateType == "update") {
      _onDuplicateAction = DUPLICATE_UPDATE;
    } else if (duplicateType == "replace") {
      _onDuplicateAction = DUPLICATE_REPLACE;
    } else if (duplicateType == "ignore") {
      _onDuplicateAction = DUPLICATE_IGNORE;
    }
  }

  // extract the sub-request type
  auto const type = _request->requestType();

  switch (type) {
    case rest::RequestType::POST: {
      std::string const& from = _request->value("fromPrefix", found);
      if (found) {
        _fromPrefix = from;
        if (!_fromPrefix.empty() && _fromPrefix[_fromPrefix.size() - 1] != '/') {
          _fromPrefix.push_back('/');
        }
      }

      std::string const& to = _request->value("toPrefix", found);
      if (found) {
        _toPrefix = to;
        if (!_toPrefix.empty() && _toPrefix[_toPrefix.size() - 1] != '/') {
          _toPrefix.push_back('/');
        }
      }

      // extract the import type
      std::string const& documentType = _request->value("type", found);

      switch (_response->transportType()) {
        case Endpoint::TransportType::HTTP: {
          if (_request->contentType() == arangodb::ContentType::VPACK) {
            createFromVPack(documentType);
          } else if (found && (documentType == "documents" || documentType == "array" ||
                               documentType == "list" || documentType == "auto")) {
            createFromJson(documentType);
          } else {
            // CSV
            createFromKeyValueList();
          }
          break;
        }
        case Endpoint::TransportType::VST: {
          if (found && (documentType == "documents" || documentType == "array" ||
                        documentType == "list" || documentType == "auto")) {
            createFromVPack(documentType);
          } else {
            generateNotImplemented("ILLEGAL " + IMPORT_PATH);
          }
          break;
        }
      }
      /////////////////////////////////////////////////////////////////////////////////
    } break;

    default:
      generateNotImplemented("ILLEGAL " + IMPORT_PATH);
      break;
  }

  // this handler is done
  return RestStatus::DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a position string
////////////////////////////////////////////////////////////////////////////////

std::string RestImportHandler::positionize(size_t i) const {
  return std::string("at position " +
                     StringUtils::itoa(static_cast<uint64_t>(i)) + ": ");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register an error
////////////////////////////////////////////////////////////////////////////////
void RestImportHandler::registerError(RestImportResult& result, std::string const& errorMsg) {
  ++result._numErrors;

  result._errors.push_back(errorMsg);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief construct an error message
////////////////////////////////////////////////////////////////////////////////
std::string RestImportHandler::buildParseError(size_t i, char const* lineStart) {
  if (lineStart != nullptr) {
    std::string part(lineStart);
    if (part.size() > 255) {
      // UTF-8 chars in string will be escaped so we can truncate it at any
      // point
      part = part.substr(0, 255) + "...";
    }

    return positionize(i) +
           "invalid JSON type (expecting object, probably parse error), "
           "offending context: " +
           part;
  }

  return positionize(i) +
         "invalid JSON type (expecting object, probably parse error)";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process a single VelocyPack document of Object Type
////////////////////////////////////////////////////////////////////////////////

ErrorCode RestImportHandler::handleSingleDocument(SingleCollectionTransaction& trx,
                                                  VPackBuilder& tempBuilder,
                                                  RestImportResult& result,
                                                  VPackBuilder& babies, VPackSlice slice,
                                                  bool isEdgeCollection, size_t i) {
  if (!slice.isObject()) {
    std::string part = VPackDumper::toString(slice);
    if (part.size() > 255) {
      // UTF-8 chars in string will be escaped so we can truncate it at any
      // point
      part = part.substr(0, 255) + "...";
    }

    std::string errorMsg =
        positionize(i) +
        "invalid JSON type (expecting object), offending document: " + part;

    registerError(result, errorMsg);
    return TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
  }

  if (!isEdgeCollection) {
    babies.add(slice);
    return TRI_ERROR_NO_ERROR;
  }

  // document ok, now import it
  transaction::BuilderLeaser newBuilder(&trx);
  tempBuilder.clear();

  // add prefixes to _from and _to
  if (!_fromPrefix.empty() || !_toPrefix.empty()) {
    tempBuilder.openObject();
    if (!_fromPrefix.empty()) {
      VPackSlice from = slice.get(StaticStrings::FromString);
      if (from.isString()) {
        std::string f = from.copyString();
        if (f.find('/') == std::string::npos) {
          tempBuilder.add(StaticStrings::FromString, VPackValue(_fromPrefix + f));
        }
      } else if (from.isInteger()) {
        uint64_t f = from.getNumber<uint64_t>();
        tempBuilder.add(StaticStrings::FromString,
                        VPackValue(_fromPrefix + std::to_string(f)));
      }
    }
    if (!_toPrefix.empty()) {
      VPackSlice to = slice.get(StaticStrings::ToString);
      if (to.isString()) {
        std::string t = to.copyString();
        if (t.find('/') == std::string::npos) {
          tempBuilder.add(StaticStrings::ToString, VPackValue(_toPrefix + t));
        }
      } else if (to.isInteger()) {
        uint64_t t = to.getNumber<uint64_t>();
        tempBuilder.add(StaticStrings::ToString,
                        VPackValue(_toPrefix + std::to_string(t)));
      }
    }
    tempBuilder.close();

    if (tempBuilder.slice().length() > 0) {
      VPackCollection::merge(*(newBuilder.builder()), slice,
                             tempBuilder.slice(), false, false);
      slice = newBuilder->slice();
    }
  }

  try {
    arangodb::basics::VelocyPackHelper::ensureStringValue(slice, StaticStrings::FromString);
    arangodb::basics::VelocyPackHelper::ensureStringValue(slice, StaticStrings::ToString);
  } catch (arangodb::basics::Exception const&) {
    std::string part = VPackDumper::toString(slice);
    if (part.size() > 255) {
      // UTF-8 chars in string will be escaped so we can truncate it at any
      // point
      part = part.substr(0, 255) + "...";
    }

    std::string errorMsg =
        positionize(i) +
        "missing '_from' or '_to' attribute, offending document: " + part;

    registerError(result, errorMsg);
    return TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE;
  }

  babies.add(slice);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_import_json
////////////////////////////////////////////////////////////////////////////////

bool RestImportHandler::createFromJson(std::string const& type) {
  RestImportResult result;

  std::vector<std::string> const& suffixes = _request->suffixes();

  if (!suffixes.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "superfluous suffix, expecting " + IMPORT_PATH +
                      "?collection=<identifier>");
    return false;
  }

  bool const complete = _request->parsedValue("complete", false);
  bool const overwrite = _request->parsedValue("overwrite", false);
  OperationOptions opOptions = buildOperationOptions();

  // extract the collection name
  bool found;
  std::string const& collectionName = _request->value("collection", found);

  if (!found || collectionName.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting " + IMPORT_PATH +
                      "?collection=<identifier>");
    return false;
  }

  bool linewise;

  if (type == "documents") {
    // linewise import
    linewise = true;
  } else if (type == "array" || type == "list") {
    // non-linewise import
    linewise = false;
  } else if (type == "auto") {
    linewise = true;

    if (_response == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid response");
    }

    // auto detect import type by peeking at first non-whitespace character

    VPackStringRef body = _request->rawPayload();

    char const* ptr = body.data();
    char const* end = ptr + body.size();

    while (ptr < end) {
      char const c = *ptr;
      if (c == '\r' || c == '\n' || c == '\t' || c == '\b' || c == '\f' || c == ' ') {
        ptr++;
        continue;
      } else if (c == '[') {
        linewise = false;
      }

      break;
    }
  } else {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "invalid value for 'type'");
    return false;
  }

  // find and load collection given by name or identifier
  auto ctx = transaction::StandaloneContext::Create(_vocbase);
  SingleCollectionTransaction trx(ctx, collectionName, AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);

  // .............................................................................
  // inside write transaction
  // .............................................................................

  Result res = trx.begin();

  if (res.fail()) {
    generateTransactionError(collectionName, OperationResult(res, opOptions),
                             "");
    return false;
  }

  bool const isEdgeCollection = trx.isEdgeCollection(collectionName);

  if (overwrite) {
    OperationOptions truncateOpts(_context);
    truncateOpts.waitForSync = false;
    // truncate collection first
    trx.truncate(collectionName, truncateOpts);
    // Ignore the result ...
  }

  VPackBuilder babies;
  babies.openArray();

  VPackBuilder tmpBuilder;

  if (linewise) {
    // each line is a separate JSON document
    VPackStringRef body = _request->rawPayload();
    char const* ptr = body.data();
    char const* end = ptr + body.size();
    size_t i = 0;

    VPackBuilder lineBuilder;
    while (ptr < end) {
      // read line until done
      i++;

      TRI_ASSERT(ptr != nullptr);

      // trim whitespace at start of line
      while (ptr < end && (*ptr == ' ' || *ptr == '\t' || *ptr == '\r' ||
                           *ptr == '\b' || *ptr == '\f')) {
        ++ptr;
      }

      if (ptr == end || *ptr == '\0') {
        break;
      }

      // now find end of line
      char const* pos = static_cast<char const*>(memchr(ptr, '\n', end - ptr));
      char const* oldPtr = nullptr;
      bool success = false;

      if (pos == ptr) {
        // line starting with \n, i.e. empty line
        ptr = pos + 1;
        ++result._numEmpty;
        continue;
      }

      TRI_ASSERT(ptr != nullptr);
      oldPtr = ptr;

      tmpBuilder.clear();
      if (pos != nullptr) {
        // non-empty line
        *(const_cast<char*>(pos)) = '\0';
        parseVelocyPackLine(tmpBuilder, ptr, pos, success);
        ptr = pos + 1;
      } else {
        // last-line, non-empty
        parseVelocyPackLine(tmpBuilder, ptr, end, success);
        ptr = end;
      }

      if (!success) {
        std::string errorMsg = buildParseError(i, oldPtr);
        registerError(result, errorMsg);
        if (complete) {
          // only perform a full import: abort
          break;
        }
        // Do not try to store illegal document
        continue;
      }

      res = handleSingleDocument(trx, lineBuilder, result, babies,
                                 tmpBuilder.slice(), isEdgeCollection, i);

      if (res.fail()) {
        if (complete) {
          // only perform a full import: abort
          break;
        }

        res.reset();
      }
    }
  }

  else {
    // the entire request body is one JSON document

    VPackSlice documents;
    try {
      documents = _request->payload();
    } catch (VPackException const& ex) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    std::string(
                        "expecting a valid JSON array in the request. got: ") +
                        ex.what());
      return false;
    }

    if (!documents.isArray()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "expecting a JSON array in the request");
      return false;
    }

    VPackBuilder lineBuilder;
    VPackArrayIterator it(documents);

    while (it.valid()) {
      res = handleSingleDocument(trx, lineBuilder, result, babies, it.value(),
                                 isEdgeCollection, static_cast<size_t>(it.index() + 1));

      if (res.fail()) {
        if (complete) {
          // only perform a full import: abort
          break;
        }

        res = TRI_ERROR_NO_ERROR;
      }

      it.next();
    }
  }

  babies.close();

  if (res.ok()) {
    // no error so far. go on and perform the actual insert
    res = performImport(trx, result, collectionName, babies, complete, opOptions);
  }

  res = trx.finish(res);

  if (res.fail()) {
    generateTransactionError(collectionName, OperationResult(res, opOptions),
                             "");
  } else {
    generateDocumentsCreated(result);
  }
  return true;
}

bool RestImportHandler::createFromVPack(std::string const& type) {
  if (_request == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid request");
  }

  RestImportResult result;

  std::vector<std::string> const& suffixes = _request->suffixes();

  if (!suffixes.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "superfluous suffix, expecting " + IMPORT_PATH +
                      "?collection=<identifier>");
    return false;
  }

  bool const complete = _request->parsedValue("complete", false);
  bool const overwrite = _request->parsedValue("overwrite", false);
  OperationOptions opOptions = buildOperationOptions();

  // extract the collection name
  bool found;
  std::string const& collectionName = _request->value("collection", found);

  if (!found || collectionName.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting " + IMPORT_PATH +
                      "?collection=<identifier>");
    return false;
  }

  // find and load collection given by name or identifier
  auto ctx = transaction::StandaloneContext::Create(_vocbase);
  SingleCollectionTransaction trx(ctx, collectionName, AccessMode::Type::WRITE);

  // .............................................................................
  // inside write transaction
  // .............................................................................

  Result res = trx.begin();

  if (res.fail()) {
    generateTransactionError(collectionName, OperationResult(res, opOptions),
                             "");

    return false;
  }

  bool const isEdgeCollection = trx.isEdgeCollection(collectionName);

  if (overwrite) {
    OperationOptions truncateOpts;
    truncateOpts.waitForSync = false;
    // truncate collection first
    trx.truncate(collectionName, truncateOpts);
    // Ignore the result ...
  }

  VPackBuilder babies;
  babies.openArray();

  VPackSlice const documents = _request->payload();

  if (!documents.isArray()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting a JSON array in the request");
    return false;
  }

  VPackBuilder lineBuilder;

  VPackArrayIterator it(documents);
  while (it.valid()) {
    res = handleSingleDocument(trx, lineBuilder, result, babies, it.value(),
                               isEdgeCollection, static_cast<size_t>(it.index() + 1));

    if (res.fail()) {
      if (complete) {
        // only perform a full import: abort
        break;
      }

      res = TRI_ERROR_NO_ERROR;
    }

    it.next();
  }

  babies.close();

  if (res.ok()) {
    // no error so far. go on and perform the actual insert
    res = performImport(trx, result, collectionName, babies, complete, opOptions);
  }

  res = trx.finish(res);

  if (res.fail()) {
    generateTransactionError(collectionName, OperationResult(res, opOptions),
                             "");
  } else {
    generateDocumentsCreated(result);
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_import_document
////////////////////////////////////////////////////////////////////////////////

bool RestImportHandler::createFromKeyValueList() {
  if (_request == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid request");
  }

  RestImportResult result;

  std::vector<std::string> const& suffixes = _request->suffixes();

  if (!suffixes.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "superfluous suffix, expecting " + IMPORT_PATH +
                      "?collection=<identifier>");
    return false;
  }

  bool const complete = _request->parsedValue("complete", false);
  bool const overwrite = _request->parsedValue("overwrite", false);
  _ignoreMissing = _request->parsedValue("ignoreMissing", false);
  OperationOptions opOptions(_context);
  opOptions.waitForSync = _request->parsedValue("waitForSync", false);

  // extract the collection name
  bool found;
  std::string const& collectionName = _request->value("collection", found);

  if (!found || collectionName.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting " + IMPORT_PATH +
                      "?collection=<identifier>");
    return false;
  }

  // read line number (optional)
  int64_t lineNumber = 0;
  std::string const& lineNumValue = _request->value("line", found);

  if (found) {
    lineNumber =
        NumberUtils::atoi_zero<int64_t>(lineNumValue.data(),
                                        lineNumValue.data() + lineNumValue.size());
  }

  // json required here
  // each line is a separate JSON document
  VPackStringRef body = _request->rawPayload();
  char const* current = body.data();
  char const* bodyEnd = current + body.size();

  // process header
  char const* next = static_cast<char const*>(memchr(current, '\n', bodyEnd - current));

  if (next == nullptr) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "no JSON array found in second line");
    return false;
  }

  char const* lineStart = current;
  char const* lineEnd = next;

  // trim line
  while (lineStart < bodyEnd &&
         (*lineStart == ' ' || *lineStart == '\t' || *lineStart == '\r' ||
          *lineStart == '\n' || *lineStart == '\b' || *lineStart == '\f')) {
    ++lineStart;
  }

  while (lineEnd > lineStart && (*(lineEnd - 1) == ' ' || *(lineEnd - 1) == '\t' ||
                                 *(lineEnd - 1) == '\r' || *(lineEnd - 1) == '\n' ||
                                 *(lineEnd - 1) == '\b' || *(lineEnd - 1) == '\f')) {
    --lineEnd;
  }

  *(const_cast<char*>(lineEnd)) = '\0';
  bool success = false;
  VPackBuilder parsedKeys;
  try {
    parseVelocyPackLine(parsedKeys, lineStart, lineEnd, success);
  } catch (...) {
    // This throws if the body is not parseable
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "no JSON string array found in first line");
    return false;
  }
  if (!success) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "no JSON string array found in first line");
    return false;
  }

  VPackSlice const keys = parsedKeys.slice();

  if (!checkKeys(keys)) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "no JSON string array found in first line");
    return false;
  }

  current = next + 1;

  // find and load collection given by name or identifier
  auto ctx = transaction::StandaloneContext::Create(_vocbase);
  SingleCollectionTransaction trx(ctx, collectionName, AccessMode::Type::WRITE);

  // .............................................................................
  // inside write transaction
  // .............................................................................

  Result res = trx.begin();

  if (res.fail()) {
    generateTransactionError(collectionName, OperationResult(res, opOptions),
                             "");
    return false;
  }

  bool const isEdgeCollection = trx.isEdgeCollection(collectionName);

  if (overwrite) {
    OperationOptions truncateOpts(_context);
    truncateOpts.waitForSync = false;
    // truncate collection first
    trx.truncate(collectionName, truncateOpts);
    // Ignore the result ...
  }

  VPackBuilder parsedValues;
  VPackBuilder babies;
  babies.openArray();

  size_t i = static_cast<size_t>(lineNumber);
  VPackBuilder lineBuilder;
  VPackBuilder objectBuilder;

  while (current != nullptr && current < bodyEnd) {
    i++;

    next = static_cast<char const*>(memchr(current, '\n', bodyEnd - current));

    char const* lineStart = current;
    char const* lineEnd = next;

    if (next == nullptr) {
      // reached the end
      lineEnd = bodyEnd;
      current = nullptr;
    } else {
      // got more to read
      current = next + 1;
      *(const_cast<char*>(lineEnd)) = '\0';
    }

    // trim line
    while (lineStart < bodyEnd &&
           (*lineStart == ' ' || *lineStart == '\t' || *lineStart == '\r' ||
            *lineStart == '\n' || *lineStart == '\b' || *lineStart == '\f')) {
      ++lineStart;
    }

    while (lineEnd > lineStart && (*(lineEnd - 1) == ' ' || *(lineEnd - 1) == '\t' ||
                                   *(lineEnd - 1) == '\r' || *(lineEnd - 1) == '\n' ||
                                   *(lineEnd - 1) == '\b' || *(lineEnd - 1) == '\f')) {
      --lineEnd;
    }

    if (lineStart == lineEnd) {
      ++result._numEmpty;
      continue;
    }

    bool success;
    parsedValues.clear();
    parseVelocyPackLine(parsedValues, lineStart, lineEnd, success);

    // build the json object from the array
    std::string errorMsg;
    if (!success) {
      errorMsg = buildParseError(i, lineStart);
      registerError(result, errorMsg);
      res = TRI_ERROR_INTERNAL;
    } else {
      VPackSlice const values = parsedValues.slice();
      try {
        objectBuilder.clear();
        createVelocyPackObject(objectBuilder, keys, values, errorMsg, i);
        res = handleSingleDocument(trx, lineBuilder, result, babies,
                                   objectBuilder.slice(), isEdgeCollection, i);
      } catch (...) {
        // raise any error
        res = TRI_ERROR_INTERNAL;
        registerError(result, errorMsg);
      }
    }

    if (res.fail()) {
      if (complete) {
        // only perform a full import: abort
        break;
      }

      res = TRI_ERROR_NO_ERROR;
    }
  }

  babies.close();

  if (res.ok()) {
    // no error so far. go on and perform the actual insert
    res = performImport(trx, result, collectionName, babies, complete, opOptions);
  }

  res = trx.finish(res);

  if (res.fail()) {
    generateTransactionError(collectionName, OperationResult(res, opOptions),
                             "");
  } else {
    generateDocumentsCreated(result);
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform the actual import (insert/update/replace) operations
////////////////////////////////////////////////////////////////////////////////

Result RestImportHandler::performImport(SingleCollectionTransaction& trx,
                                        RestImportResult& result,
                                        std::string const& collectionName,
                                        VPackBuilder const& babies, bool complete,
                                        OperationOptions const& opOptions) {
  auto makeError = [&](size_t i, ErrorCode res, VPackSlice const& slice,
                       RestImportResult& result) {
    VPackOptions options(VPackOptions::Defaults);
    options.escapeUnicode = false;
    std::string part = VPackDumper::toString(slice, &options);
    if (part.size() > 255) {
      // UTF-8 chars in string will be escaped so we can truncate it at any
      // point
      part = part.substr(0, 255) + "...";
    }

    auto errorMsg =
        StringUtils::concatT(positionize(i),
                             "creating document failed with error '",
                             TRI_errno_string(res), "', offending document: ", part);
    registerError(result, errorMsg);
  };

  Result res;
  OperationResult opResult = trx.insert(collectionName, babies.slice(), opOptions);

  if (!opResult.fail()) {
    VPackSlice resultSlice = opResult.slice();

    if (resultSlice.isArray()) {
      size_t pos = 0;

      for (VPackSlice it : VPackArrayIterator(resultSlice)) {
        VPackSlice s = it.get(StaticStrings::Error);
        if (!s.isBool() || !s.getBool()) {
          // no error
          if ((_onDuplicateAction == DUPLICATE_UPDATE || _onDuplicateAction == DUPLICATE_REPLACE) &&
              it.hasKey("_oldRev")) {
            // updated/replaced a previous version
            ++result._numUpdated;
          } else {
            // inserted a new document
            ++result._numCreated;
          }
        } else {
          // got an error, now handle it
          auto errorCode = ErrorCode{it.get(StaticStrings::ErrorNum).getNumber<int>()};
          VPackSlice const which = babies.slice().at(pos);
          // special behavior in case of unique constraint violation . . .
          if (errorCode == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED &&
              _onDuplicateAction == DUPLICATE_IGNORE) {
            // simply ignore unique key violations silently
            TRI_ASSERT(_onDuplicateAction == DUPLICATE_IGNORE);
            res = TRI_ERROR_NO_ERROR;
            ++result._numIgnored;
          } else { 
            makeError(pos, errorCode, which, result);
            if (complete) {
              res = errorCode;
              break;
            }
          }
        }

        ++pos;
      }
    }
  }

  if (opResult.fail() && res.ok()) {
    res = opResult.result;
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create response for number of documents created / failed
////////////////////////////////////////////////////////////////////////////////

void RestImportHandler::generateDocumentsCreated(RestImportResult const& result) {
  VPackBuilder builder;
  builder.add(VPackValue(VPackValueType::Object));
  builder.add(StaticStrings::Error, VPackValue(false));
  builder.add("created", VPackValue(result._numCreated));
  builder.add("errors", VPackValue(result._numErrors));
  builder.add("empty", VPackValue(result._numEmpty));
  builder.add("updated", VPackValue(result._numUpdated));
  builder.add("ignored", VPackValue(result._numIgnored));

  // include failure details?
  if (_request->parsedValue("details", false)) {
    builder.add("details", VPackValue(VPackValueType::Array));

    for (auto const& elem : result._errors) {
      builder.add(VPackValue(elem));
    }

    builder.close();
  }

  builder.close();

  generateResult(rest::ResponseCode::CREATED, builder.slice());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse a single document line
////////////////////////////////////////////////////////////////////////////////

void RestImportHandler::parseVelocyPackLine(VPackBuilder& builder, char const* start,
                                            char const* end, bool& success) {
  try {
    success = true;
    VPackParser parser(builder);
    parser.parse(start, std::distance(start, end));
  } catch (std::exception const&) {
    // The line is invalid and could not be transformed into a string
    success = false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a VelocyPack object from a key and value list
////////////////////////////////////////////////////////////////////////////////

void RestImportHandler::createVelocyPackObject(VPackBuilder& result, VPackSlice const& keys,
                                               VPackSlice const& values,
                                               std::string& errorMsg, size_t lineNumber) {
  if (!values.isArray()) {
    errorMsg = positionize(lineNumber) + "no valid JSON array data";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, errorMsg);
  }

  TRI_ASSERT(keys.isArray());

  VPackArrayIterator itKeys(keys);
  VPackArrayIterator itValues(values);

  if (!_ignoreMissing && itKeys.size() != itValues.size()) {
    errorMsg = positionize(lineNumber) + "wrong number of JSON values (got " +
               std::to_string(itValues.size()) + ", expected " +
               std::to_string(itKeys.size()) + ")";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, errorMsg);
  }

  result.openObject();

  while (itKeys.valid()) {
    if (!itValues.valid()) {
      break;
    }
    VPackSlice const key = itKeys.value();
    VPackSlice const value = itValues.value();

    if (key.isString() && !value.isNone() && !value.isNull()) {
      VPackValueLength l;
      char const* p = key.getString(l);
      result.add(p, l, value);
    }

    itKeys.next();
    itValues.next();
  }

  result.close();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate keys
////////////////////////////////////////////////////////////////////////////////

bool RestImportHandler::checkKeys(VPackSlice const& keys) const {
  if (!keys.isArray()) {
    return false;
  }

  VPackValueLength const n = keys.length();

  if (n == 0) {
    return false;
  }

  for (VPackSlice const& key : VPackArrayIterator(keys)) {
    if (!key.isString()) {
      return false;
    }
  }

  return true;
}

OperationOptions RestImportHandler::buildOperationOptions() const {
  OperationOptions opOptions(_context);

  opOptions.waitForSync = _request->parsedValue(StaticStrings::WaitForSyncString, false);
  opOptions.validate = !_request->parsedValue(StaticStrings::SkipDocumentValidation, false);
  if (_onDuplicateAction == DUPLICATE_UPDATE) {
    opOptions.overwriteMode = OperationOptions::OverwriteMode::Update;
    opOptions.returnOld = false; 
  } else if (_onDuplicateAction == DUPLICATE_REPLACE) {
    opOptions.overwriteMode = OperationOptions::OverwriteMode::Replace;
    opOptions.returnOld = false; 
  }

  return opOptions;
}
