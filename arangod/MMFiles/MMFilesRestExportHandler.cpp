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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "MMFilesRestExportHandler.h"
#include "Basics/Exceptions.h"
#include "Basics/MutexLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "MMFiles/MMFilesCollectionExport.h"
#include "MMFiles/MMFilesExportCursor.h"
#include "MMFiles/MMFilesLogfileManager.h"
#include "Utils/Cursor.h"
#include "Utils/CursorRepository.h"
#include "VocBase/ticks.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::rest;

MMFilesRestExportHandler::MMFilesRestExportHandler(GeneralRequest* request,
                                                   GeneralResponse* response)
    : RestVocbaseBaseHandler(request, response), _restrictions() {}

RestStatus MMFilesRestExportHandler::execute() {
  if (ServerState::instance()->isCoordinator()) {
    generateError(rest::ResponseCode::NOT_IMPLEMENTED, TRI_ERROR_CLUSTER_UNSUPPORTED,
                  "'/_api/export' is not yet supported in a cluster");
    return RestStatus::DONE;
  }

  // extract the sub-request type
  auto const type = _request->requestType();

  if (type == rest::RequestType::POST) {
    createCursor();
    return RestStatus::DONE;
  }

  if (type == rest::RequestType::PUT) {
    modifyCursor();
    return RestStatus::DONE;
  }

  if (type == rest::RequestType::DELETE_REQ) {
    deleteCursor();
    return RestStatus::DONE;
  }

  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  return RestStatus::DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief build options for the query as JSON
////////////////////////////////////////////////////////////////////////////////

VPackBuilder MMFilesRestExportHandler::buildOptions(VPackSlice const& slice) {
  VPackBuilder options;
  options.openObject();

  VPackSlice count = slice.get("count");
  if (count.isBool()) {
    options.add("count", count);
  } else {
    options.add("count", VPackValue(false));
  }

  VPackSlice batchSize = slice.get("batchSize");
  if (batchSize.isNumber()) {
    if ((batchSize.isInteger() && batchSize.getUInt() == 0) ||
        (batchSize.isDouble() && batchSize.getDouble() == 0.0)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_TYPE_ERROR, "expecting non-zero value for 'batchSize'");
    }
    options.add("batchSize", batchSize);
  } else {
    options.add("batchSize", VPackValue(1000));
  }

  VPackSlice limit = slice.get("limit");
  if (limit.isNumber()) {
    options.add("limit", limit);
  }

  VPackSlice flush = slice.get("flush");
  if (flush.isBool()) {
    options.add("flush", flush);
  } else {
    options.add("flush", VPackValue(false));
  }

  VPackSlice ttl = slice.get("ttl");
  if (ttl.isNumber()) {
    options.add("ttl", ttl);
  } else {
    options.add("ttl", VPackValue(30));
  }

  VPackSlice flushWait = slice.get("flushWait");
  if (flushWait.isNumber()) {
    options.add("flushWait", flushWait);
  } else {
    options.add("flushWait", VPackValue(10));
  }
  options.close();

  // handle "restrict" parameter
  VPackSlice restrct = slice.get("restrict");
  if (!restrct.isObject()) {
    if (!restrct.isNone()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_TYPE_ERROR,
                                     "expecting object for 'restrict'");
    }
  } else {
    // "restrict"."type"
    VPackSlice type = restrct.get("type");
    if (!type.isString()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "expecting string for 'restrict.type'");
    }
    std::string typeString = type.copyString();

    if (typeString == "include") {
      _restrictions.type = CollectionExport::Restrictions::RESTRICTION_INCLUDE;
    } else if (typeString == "exclude") {
      _restrictions.type = CollectionExport::Restrictions::RESTRICTION_EXCLUDE;
    } else {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "expecting either 'include' or 'exclude' for 'restrict.type'");
    }

    // "restrict"."fields"
    VPackSlice fields = restrct.get("fields");
    if (!fields.isArray()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "expecting array for 'restrict.fields'");
    }
    for (auto const& name : VPackArrayIterator(fields)) {
      if (name.isString()) {
        _restrictions.fields.emplace(name.copyString());
      }
    }
  }

  return options;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_post_api_export
////////////////////////////////////////////////////////////////////////////////

void MMFilesRestExportHandler::createCursor() {
  std::vector<std::string> const& suffixes = _request->suffixes();

  if (!suffixes.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting POST /_api/export");
    return;
  }

  // extract the cid
  bool found;
  std::string const& name = _request->value("collection", found);

  if (!found || name.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,
                  "'collection' is missing, expecting "
                  "/_api/export?collection=<identifier>");
    return;
  }

  bool parseSuccess = true;
  VPackSlice body = this->parseVPackBody(parseSuccess);

  if (!parseSuccess) {
    // error message generated in parseVPackBody
    return;
  }

  VPackBuilder optionsBuilder;

  if (!body.isNone()) {
    if (!body.isObject()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_QUERY_EMPTY);
      return;
    }
    optionsBuilder = buildOptions(body);
  } else {
    // create an empty options object
    optionsBuilder.openObject();
    optionsBuilder.close();
  }

  VPackSlice options = optionsBuilder.slice();

  uint64_t waitTime = 0;
  bool flush =
      arangodb::basics::VelocyPackHelper::getBooleanValue(options, "flush", false);

  if (flush) {
    // flush the logfiles so the export can fetch all documents
    int res = MMFilesLogfileManager::instance()->flush(true, true, false);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    double flushWait =
        arangodb::basics::VelocyPackHelper::getNumericValue<double>(options,
                                                                    "flushWait", 10.0);

    waitTime = static_cast<uint64_t>(flushWait * 1000 * 1000);  // flushWait is specified in s, but we need ns
  }

  size_t limit =
      arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(options,
                                                                  "limit", 0);

  // this may throw!
  auto collectionExport =
      std::make_unique<MMFilesCollectionExport>(_vocbase, name, _restrictions);

  collectionExport->run(waitTime, limit);

  size_t batchSize =
      arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(options,
                                                                  "batchSize", 1000);
  double ttl =
      arangodb::basics::VelocyPackHelper::getNumericValue<double>(options, "ttl", 30);
  bool count =
      arangodb::basics::VelocyPackHelper::getBooleanValue(options, "count", false);
  auto cursors = _vocbase.cursorRepository();
  TRI_ASSERT(cursors != nullptr);

  Cursor* c = nullptr;

  {
    auto cursor = std::make_unique<MMFilesExportCursor>(_vocbase, TRI_NewTickServer(),
                                                        collectionExport.get(),
                                                        batchSize, ttl, count);

    collectionExport.release();
    cursor->use();
    c = cursors->addCursor(std::move(cursor));
  }

  TRI_ASSERT(c != nullptr);
  TRI_DEFER(cursors->release(c));

  resetResponse(rest::ResponseCode::CREATED);

  VPackBuffer<uint8_t> buffer;
  VPackBuilder builder(buffer);
  builder.openObject();
  builder.add(StaticStrings::Error, VPackValue(false));
  builder.add(StaticStrings::Code,
              VPackValue(static_cast<int>(_response->responseCode())));
  Result r = c->dumpSync(builder);
  if (r.fail()) {
    generateError(r);
    return;
  }
  builder.close();

  _response->setContentType(rest::ContentType::JSON);
  generateResult(rest::ResponseCode::CREATED, builder.slice());
}

void MMFilesRestExportHandler::modifyCursor() {
  std::vector<std::string> const& suffixes = _request->suffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting PUT /_api/export/<cursor-id>");
    return;
  }

  std::string const& id = suffixes[0];
  auto cursors = _vocbase.cursorRepository();
  TRI_ASSERT(cursors != nullptr);

  auto cursorId =
      static_cast<arangodb::CursorId>(arangodb::basics::StringUtils::uint64(id));
  bool busy;
  auto cursor = cursors->find(cursorId, Cursor::CURSOR_EXPORT, busy);

  if (cursor == nullptr) {
    if (busy) {
      generateError(GeneralResponse::responseCode(TRI_ERROR_CURSOR_BUSY), TRI_ERROR_CURSOR_BUSY);
    } else {
      generateError(GeneralResponse::responseCode(TRI_ERROR_CURSOR_NOT_FOUND),
                    TRI_ERROR_CURSOR_NOT_FOUND);
    }
    return;
  }
  TRI_DEFER(cursors->release(cursor));

  resetResponse(rest::ResponseCode::OK);

  VPackBuffer<uint8_t> buffer;
  VPackBuilder builder(buffer);
  builder.openObject();
  builder.add(StaticStrings::Error, VPackValue(false));
  builder.add(StaticStrings::Code, VPackValue((int)_response->responseCode()));
  Result r = cursor->dumpSync(builder);
  if (r.fail()) {
    generateError(r);
    return;
  }
  builder.close();

  _response->setContentType(rest::ContentType::JSON);
  generateResult(rest::ResponseCode::OK, builder.slice());
}

void MMFilesRestExportHandler::deleteCursor() {
  std::vector<std::string> const& suffixes = _request->suffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting DELETE /_api/export/<cursor-id>");
    return;
  }

  std::string const& id = suffixes[0];
  CursorRepository* cursors = _vocbase.cursorRepository();
  TRI_ASSERT(cursors != nullptr);

  auto cursorId =
      static_cast<arangodb::CursorId>(arangodb::basics::StringUtils::uint64(id));
  bool found = cursors->remove(cursorId, Cursor::CURSOR_EXPORT);

  if (!found) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_CURSOR_NOT_FOUND);
    return;
  }

  VPackBuilder result;
  result.openObject();
  result.add("id", VPackValue(id));
  result.add(StaticStrings::Error, VPackValue(false));
  result.add(StaticStrings::Code,
             VPackValue(static_cast<int>(rest::ResponseCode::ACCEPTED)));
  result.close();

  generateResult(rest::ResponseCode::ACCEPTED, result.slice());
}
