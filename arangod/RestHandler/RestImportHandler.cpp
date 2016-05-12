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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "RestImportHandler.h"
#include "Basics/json-utilities.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "Rest/HttpRequest.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "VocBase/MasterPointer.h"
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

RestImportHandler::RestImportHandler(HttpRequest* request)
    : RestVocbaseBaseHandler(request), _onDuplicateAction(DUPLICATE_ERROR) {}

HttpHandler::status_t RestImportHandler::execute() {
  if (ServerState::instance()->isCoordinator()) {
    generateError(GeneralResponse::ResponseCode::NOT_IMPLEMENTED,
                  TRI_ERROR_CLUSTER_UNSUPPORTED,
                  "'/_api/import' is not yet supported in a cluster");
    return status_t(HANDLER_DONE);
  }

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
    case GeneralRequest::RequestType::POST: {
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

      if (found && (documentType == "documents" || documentType == "array" ||
                    documentType == "list" || documentType == "auto")) {
        createFromJson(documentType);
      } else {
        // CSV
        createFromKeyValueList();
      }
      break;
    }

    default:
      generateNotImplemented("ILLEGAL " + IMPORT_PATH);
      break;
  }

  // this handler is done
  return status_t(HANDLER_DONE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a position string
////////////////////////////////////////////////////////////////////////////////

std::string RestImportHandler::positionize(size_t i) const {
  return std::string("at position " + StringUtils::itoa(i) + ": ");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register an error
////////////////////////////////////////////////////////////////////////////////

void RestImportHandler::registerError(RestImportResult& result,
                                      std::string const& errorMsg) {
  ++result._numErrors;

  result._errors.push_back(errorMsg);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief construct an error message
////////////////////////////////////////////////////////////////////////////////

std::string RestImportHandler::buildParseError(size_t i,
                                               char const* lineStart) {
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
/// @brief process a single VelocyPack document
////////////////////////////////////////////////////////////////////////////////

int RestImportHandler::handleSingleDocument(
    SingleCollectionTransaction& trx, RestImportResult& result, 
    VPackBuilder& tempBuilder, char const* lineStart,
    VPackSlice slice, std::string const& collectionName,
    bool isEdgeCollection, OperationOptions const& opOptions, size_t i) {

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

  // document ok, now import it
  TRI_doc_mptr_t document;
  int res = TRI_ERROR_NO_ERROR;

  VPackBuilder newBuilder;

  if (isEdgeCollection) {
    // Validate from and to
    // TODO: Check if this is unified in trx.insert
    
    if (!_fromPrefix.empty() || !_toPrefix.empty()) {
      tempBuilder.clear();
      tempBuilder.openObject();
      if (!_fromPrefix.empty()) {
        VPackSlice from = slice.get(StaticStrings::FromString);
        if (from.isString()) {
          std::string f = from.copyString();
          if (f.find('/') == std::string::npos) {
            tempBuilder.add(StaticStrings::FromString, VPackValue(_fromPrefix + f));
          }
        }
      }
      if (!_toPrefix.empty()) {
        VPackSlice to = slice.get(StaticStrings::ToString);
        if (to.isString()) {
          std::string t = to.copyString();
          if (t.find('/') == std::string::npos) {
            tempBuilder.add(StaticStrings::ToString, VPackValue(_toPrefix + t));
          }
        }
      }
      tempBuilder.close();

      if (tempBuilder.slice().length() > 0) {
        newBuilder = VPackCollection::merge(slice, tempBuilder.slice(), false, false);
        slice = newBuilder.slice();
      }
    }

    try {
      arangodb::basics::VelocyPackHelper::checkAndGetStringValue(
          slice, StaticStrings::FromString);
      arangodb::basics::VelocyPackHelper::checkAndGetStringValue(
          slice, StaticStrings::ToString);
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
  
  }

  OperationResult opResult = trx.insert(collectionName, slice, opOptions);

  if (opResult.successful()) {
    ++result._numCreated;
  }
  res = opResult.code;

  // special behavior in case of unique constraint violation . . .
  if (res == TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED &&
      _onDuplicateAction != DUPLICATE_ERROR) {
    VPackSlice const keySlice = slice.get(StaticStrings::KeyString);

    if (keySlice.isString()) {
      // insert failed. now try an update/replace

      if (_onDuplicateAction == DUPLICATE_UPDATE) {
        // update
        opResult = trx.update(collectionName, slice, opOptions);
        if (opResult.successful()) {
          ++result._numUpdated;
        }
        res = opResult.code;
        // We silently ignore all failed updates
      } else if (_onDuplicateAction == DUPLICATE_REPLACE) {
        // replace
        opResult = trx.replace(collectionName, slice, opOptions);
        if (opResult.successful()) {
          ++result._numUpdated;
        }
        res = opResult.code;
      } else {
        // simply ignore unique key violations silently
        TRI_ASSERT(_onDuplicateAction == DUPLICATE_IGNORE);
        res = TRI_ERROR_NO_ERROR;
        ++result._numIgnored;
      }
    }
  }

  if (res != TRI_ERROR_NO_ERROR) {
    std::string part = VPackDumper::toString(slice);
    if (part.size() > 255) {
      // UTF-8 chars in string will be escaped so we can truncate it at any
      // point
      part = part.substr(0, 255) + "...";
    }

    std::string errorMsg =
        positionize(i) + "creating document failed with error '" +
        TRI_errno_string(res) + "', offending document: " + part;

    registerError(result, errorMsg);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_import_json
////////////////////////////////////////////////////////////////////////////////

bool RestImportHandler::createFromJson(std::string const& type) {
  RestImportResult result;

  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 0) {
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "superfluous suffix, expecting " + IMPORT_PATH +
                      "?collection=<identifier>");
    return false;
  }

  bool const complete = extractBooleanParameter("complete", false);
  bool const overwrite = extractBooleanParameter("overwrite", false);
  OperationOptions opOptions;
  opOptions.waitForSync = extractBooleanParameter("waitForSync", false);

  // extract the collection name
  bool found;
  std::string const& collectionName = _request->value("collection", found);

  if (!found || collectionName.empty()) {
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
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

    // auto detect import type by peeking at first character
    std::string const& body = _request->body();
    char const* ptr = body.c_str();
    char const* end = ptr + body.size();

    while (ptr < end) {
      char const c = *ptr;
      if (c == '\r' || c == '\n' || c == '\t' || c == '\b' || c == '\f' ||
          c == ' ') {
        ptr++;
        continue;
      } else if (c == '[') {
        linewise = false;
      }

      break;
    }
  } else {
    generateError(GeneralResponse::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "invalid value for 'type'");
    return false;
  }

  // find and load collection given by name or identifier
  SingleCollectionTransaction trx(StandaloneTransactionContext::Create(_vocbase),
                            collectionName, TRI_TRANSACTION_WRITE);

  // .............................................................................
  // inside write transaction
  // .............................................................................

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collectionName, res, "");
    return false;
  }

 
  VPackBuilder tempBuilder; 
  bool const isEdgeCollection = trx.isEdgeCollection(collectionName);

  if (overwrite) {
    OperationOptions truncateOpts;
    truncateOpts.waitForSync = false;
    // truncate collection first
    trx.truncate(collectionName, truncateOpts);
    // Ignore the result ...
  }

  if (linewise) {
    // each line is a separate JSON document
    std::string const& body = _request->body();
    char const* ptr = body.c_str();
    char const* end = ptr + body.size();
    size_t i = 0;

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
      char const* pos = strchr(ptr, '\n');
      char const* oldPtr = nullptr;

      std::shared_ptr<VPackBuilder> builder;

      bool success = false;

      if (pos == ptr) {
        // line starting with \n, i.e. empty line
        ptr = pos + 1;
        ++result._numEmpty;
        continue;
      } else if (pos != nullptr) {
        // non-empty line
        *(const_cast<char*>(pos)) = '\0';
        TRI_ASSERT(ptr != nullptr);
        oldPtr = ptr;
        builder = parseVelocyPackLine(ptr, pos, success);
        ptr = pos + 1;
      } else {
        // last-line, non-empty
        TRI_ASSERT(pos == nullptr);
        TRI_ASSERT(ptr != nullptr);
        oldPtr = ptr;
        builder = parseVelocyPackLine(ptr, success);
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

      res = handleSingleDocument(trx, result, tempBuilder, oldPtr, builder->slice(),
                                 collectionName, isEdgeCollection, opOptions, i);

      if (res != TRI_ERROR_NO_ERROR) {
        if (complete) {
          // only perform a full import: abort
          break;
        }

        res = TRI_ERROR_NO_ERROR;
      }
    }
  }

  else {
    // the entire request body is one JSON document
    std::shared_ptr<VPackBuilder> parsedDocuments;
    try {
      parsedDocuments = VPackParser::fromJson(_request->body());
    } catch (VPackException const&) {
      generateError(GeneralResponse::ResponseCode::BAD,
                    TRI_ERROR_HTTP_BAD_PARAMETER,
                    "expecting a JSON array in the request");
      return false;
    }

    VPackSlice const documents = parsedDocuments->slice();

    if (!documents.isArray()) {
      generateError(GeneralResponse::ResponseCode::BAD,
                    TRI_ERROR_HTTP_BAD_PARAMETER,
                    "expecting a JSON array in the request");
      return false;
    }

    VPackValueLength const n = documents.length();

    for (VPackValueLength i = 0; i < n; ++i) {
      VPackSlice const slice = documents.at(i);

      res = handleSingleDocument(trx, result, tempBuilder, nullptr, slice, collectionName,
                                 isEdgeCollection, opOptions, static_cast<size_t>(i + 1));

      if (res != TRI_ERROR_NO_ERROR) {
        if (complete) {
          // only perform a full import: abort
          break;
        }

        res = TRI_ERROR_NO_ERROR;
      }
    }
  }

  // this may commit, even if previous errors occurred
  res = trx.finish(res);

  // .............................................................................
  // outside write transaction
  // .............................................................................

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collectionName, res, "");
  } else {
    // generate result
    generateDocumentsCreated(result);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_import_document
////////////////////////////////////////////////////////////////////////////////

bool RestImportHandler::createFromKeyValueList() {
  RestImportResult result;

  std::vector<std::string> const& suffix = _request->suffix();

  if (suffix.size() != 0) {
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,
                  "superfluous suffix, expecting " + IMPORT_PATH +
                      "?collection=<identifier>");
    return false;
  }

  bool const complete = extractBooleanParameter("complete", false);
  bool const overwrite = extractBooleanParameter("overwrite", false);
  OperationOptions opOptions;
  opOptions.waitForSync = extractBooleanParameter("waitForSync", false);

  // extract the collection name
  bool found;
  std::string const& collectionName = _request->value("collection", found);

  if (!found || collectionName.empty()) {
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting " + IMPORT_PATH +
                      "?collection=<identifier>");
    return false;
  }

  // read line number (optional)
  int64_t lineNumber = 0;
  std::string const& lineNumValue = _request->value("line", found);

  if (found) {
    lineNumber = StringUtils::int64(lineNumValue);
  }

  std::string const& bodyStr = _request->body();
  char const* current = bodyStr.c_str();
  char const* bodyEnd = current + bodyStr.size();

  // process header
  char const* next = strchr(current, '\n');

  if (next == nullptr) {
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
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

  while (lineEnd > lineStart &&
         (*(lineEnd - 1) == ' ' || *(lineEnd - 1) == '\t' ||
          *(lineEnd - 1) == '\r' || *(lineEnd - 1) == '\n' ||
          *(lineEnd - 1) == '\b' || *(lineEnd - 1) == '\f')) {
    --lineEnd;
  }

  *(const_cast<char*>(lineEnd)) = '\0';
  bool success = false;
  std::shared_ptr<VPackBuilder> parsedKeys;
  try {
    parsedKeys = parseVelocyPackLine(lineStart, lineEnd, success);
  } catch (...) {
    // This throws if the body is not parseable
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "no JSON string array found in first line");
    return false;
  }
  VPackSlice const keys = parsedKeys->slice();

  if (!success || !checkKeys(keys)) {
    generateError(GeneralResponse::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "no JSON string array found in first line");
    return false;
  }

  current = next + 1;

  // find and load collection given by name or identifier
  SingleCollectionTransaction trx(StandaloneTransactionContext::Create(_vocbase),
                            collectionName, TRI_TRANSACTION_WRITE);

  // .............................................................................
  // inside write transaction
  // .............................................................................

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collectionName, res, "");
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

  VPackBuilder tempBuilder; 
  size_t i = static_cast<size_t>(lineNumber);

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

    while (lineEnd > lineStart &&
           (*(lineEnd - 1) == ' ' || *(lineEnd - 1) == '\t' ||
            *(lineEnd - 1) == '\r' || *(lineEnd - 1) == '\n' ||
            *(lineEnd - 1) == '\b' || *(lineEnd - 1) == '\f')) {
      --lineEnd;
    }

    if (lineStart == lineEnd) {
      ++result._numEmpty;
      continue;
    }

    bool success;
    std::shared_ptr<VPackBuilder> parsedValues =
        parseVelocyPackLine(lineStart, lineEnd, success);

    // build the json object from the array
    std::string errorMsg;
    if (!success) {
      errorMsg = buildParseError(i, lineStart);
      registerError(result, errorMsg);
      res = TRI_ERROR_INTERNAL;
    } else {
      VPackSlice const values = parsedValues->slice();
      try {
        std::shared_ptr<VPackBuilder> objectBuilder =
            createVelocyPackObject(keys, values, errorMsg, i);
        res =
            handleSingleDocument(trx, result, tempBuilder, lineStart, objectBuilder->slice(),
                                 collectionName, isEdgeCollection, opOptions, i);
      } catch (...) {
        // raise any error
        res = TRI_ERROR_INTERNAL;
        registerError(result, errorMsg);
      }
    }

    if (res != TRI_ERROR_NO_ERROR) {
      if (complete) {
        // only perform a full import: abort
        break;
      }

      res = TRI_ERROR_NO_ERROR;
    }
  }

  // we'll always commit, even if previous errors occurred
  res = trx.finish(res);

  // .............................................................................
  // outside write transaction
  // .............................................................................

  if (res != TRI_ERROR_NO_ERROR) {
    generateTransactionError(collectionName, res, "");
  } else {
    // generate result
    generateDocumentsCreated(result);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create response for number of documents created / failed
////////////////////////////////////////////////////////////////////////////////

void RestImportHandler::generateDocumentsCreated(
    RestImportResult const& result) {
  // TODO: is it necessary to create a response object here already
  createResponse(GeneralResponse::ResponseCode::CREATED);
  _response->setContentType(HttpResponse::CONTENT_TYPE_JSON);

  try {
    VPackBuilder json;
    json.add(VPackValue(VPackValueType::Object));
    json.add("error", VPackValue(false));
    json.add("created", VPackValue(result._numCreated));
    json.add("errors", VPackValue(result._numErrors));
    json.add("empty", VPackValue(result._numEmpty));
    json.add("updated", VPackValue(result._numUpdated));
    json.add("ignored", VPackValue(result._numIgnored));

    bool found;
    std::string const& detailsStr = _request->value("details", found);

    // include failure details?
    if (found && StringUtils::boolean(detailsStr)) {
      json.add("details", VPackValue(VPackValueType::Array));

      for (auto const& elem : result._errors) {
        json.add(VPackValue(elem));
      }
      json.close();
    }
    json.close();
    generateResult(GeneralResponse::ResponseCode::CREATED, json.slice());
  } catch (...) {
    // Ignore the error
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse a single document line
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackBuilder> RestImportHandler::parseVelocyPackLine(
    std::string const& line, bool& success) {
  try {
    success = true;
    return VPackParser::fromJson(line);
  } catch (VPackException const&) {
    success = false;
    VPackParser p;
    return p.steal();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse a single document line
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackBuilder> RestImportHandler::parseVelocyPackLine(
    char const* start, char const* end, bool& success) {
  try {
    std::string tmp(start, end);
    return parseVelocyPackLine(tmp, success);
  } catch (std::exception const&) {
    // The line is invalid and could not be transformed into a string
    success = false;
    VPackParser p;
    return p.steal();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a VelocyPack object from a key and value list
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<VPackBuilder> RestImportHandler::createVelocyPackObject(
    VPackSlice const& keys, VPackSlice const& values, std::string& errorMsg,
    size_t lineNumber) {
  if (!values.isArray()) {
    errorMsg = positionize(lineNumber) + "no valid JSON array data";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, errorMsg);
  }

  TRI_ASSERT(keys.isArray());
  VPackValueLength const n = keys.length();
  VPackValueLength const m = values.length();

  if (n != m) {
    errorMsg = positionize(lineNumber) + "wrong number of JSON values (got " +
               std::to_string(m) + ", expected " + std::to_string(n) + ")";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, errorMsg);
  }

  try {
    auto result = std::make_shared<VPackBuilder>();
    result->openObject();

    for (size_t i = 0; i < n; ++i) {
      VPackSlice const key = keys.at(i);
      VPackSlice const value = values.at(i);

      if (key.isString() && !value.isNone() && !value.isNull()) {
        std::string tmp = key.copyString();
        result->add(tmp, value);
      }
    }
    result->close();

    return result;
  } catch (std::bad_alloc const&) {
    LOG(ERR) << "out of memory";
    throw;
  }
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
