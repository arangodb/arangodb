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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RestSimpleQueryHandler.h"

#include "Aql/QueryRegistry.h"
#include "Basics/Exceptions.h"
#include "Basics/MutexLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Utils/Cursor.h"
#include "Utils/CursorRepository.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::rest;

RestSimpleQueryHandler::RestSimpleQueryHandler(application_features::ApplicationServer& server,
                                               GeneralRequest* request,
                                               GeneralResponse* response,
                                               arangodb::aql::QueryRegistry* queryRegistry)
    : RestCursorHandler(server, request, response, queryRegistry) {}

RestStatus RestSimpleQueryHandler::execute() {
  // extract the sub-request type
  auto const type = _request->requestType();

  std::string const& prefix = _request->requestPath();
  if (type == rest::RequestType::PUT) {
    if (prefix == RestVocbaseBaseHandler::SIMPLE_QUERY_ALL_PATH) {
      // all query
      return allDocuments();
    } else if (prefix == RestVocbaseBaseHandler::SIMPLE_QUERY_ALL_KEYS_PATH) {
      // all-keys query
      return allDocumentKeys();
    } else if (prefix == RestVocbaseBaseHandler::SIMPLE_QUERY_BY_EXAMPLE) {
      // by-example query
      return byExample();
    }
  }

  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  return RestStatus::DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSA_put_api_simple_all
////////////////////////////////////////////////////////////////////////////////

RestStatus RestSimpleQueryHandler::allDocuments() {
  bool parseSuccess = false;
  VPackSlice const body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    // error message generated in parseVPackBody
    return RestStatus::DONE;
  }

  std::string collectionName;
  if (body.isObject() && body.hasKey("collection")) {
    VPackSlice const value = body.get("collection");
    if (value.isString()) {
      collectionName = value.copyString();
    }
  } else {
    collectionName = _request->value("collection");
  }

  if (collectionName.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_TYPE_ERROR,
                  "expecting string for <collection>");
    return RestStatus::DONE;
  }

  auto col = _vocbase.lookupCollection(collectionName);

  if (col != nullptr && collectionName != col->name()) {
    // user has probably passed in a numeric collection id.
    // translate it into a "real" collection name
    collectionName = col->name();
  }

  VPackBuilder bindVars;
  bindVars.openObject();
  bindVars.add("@collection", VPackValue(collectionName));

  std::string aql("FOR doc IN @@collection ");

  VPackSlice const skip = body.get("skip");
  VPackSlice const limit = body.get("limit");
  if (skip.isNumber() || limit.isNumber()) {
    aql.append("LIMIT @skip, @limit ");

    if (skip.isNumber()) {
      bindVars.add("skip", skip);
    } else {
      bindVars.add("skip", VPackValue(VPackValueType::Null));
    }

    if (limit.isNumber()) {
      bindVars.add("limit", limit);
    } else {
      bindVars.add("limit", VPackValue(VPackValueType::Null));
    }
  }
  bindVars.close();
  aql.append("RETURN doc");

  VPackBuilder data;
  data.openObject();
  data.add("query", VPackValue(aql));
  data.add("bindVars", bindVars.slice());
  data.add("count", VPackValue(true));

  // pass on standard options
  VPackSlice ttl = body.get("ttl");
  if (!ttl.isNone()) {
    data.add("ttl", ttl);
  }

  VPackSlice batchSize = body.get("batchSize");
  if (!batchSize.isNone()) {
    data.add("batchSize", batchSize);
  }

  VPackSlice stream = body.get("stream");
  if (stream.isBool()) {
    VPackObjectBuilder obj(&data, "options");
    obj->add("stream", stream);
  }
  data.close();

  // now run the actual query and handle the result
  return registerQueryOrCursor(data.slice());
}

//////////////////////////////////////////////////////////////////////////////
/// @brief return a cursor with all document keys from the collection
//////////////////////////////////////////////////////////////////////////////

RestStatus RestSimpleQueryHandler::allDocumentKeys() {
  bool parseSuccess = false;
  VPackSlice const body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    // error message generated in parseVPackBody
    return RestStatus::DONE;
  }

  std::string collectionName;
  if (body.isObject() && body.hasKey("collection")) {
    VPackSlice const value = body.get("collection");
    if (value.isString()) {
      collectionName = value.copyString();
    }
  } else {
    collectionName = _request->value("collection");
  }

  if (collectionName.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_TYPE_ERROR,
                  "expecting string for <collection>");
    return RestStatus::DONE;
  }

  auto col = _vocbase.lookupCollection(collectionName);

  if (col != nullptr && collectionName != col->name()) {
    // user has probably passed in a numeric collection id.
    // translate it into a "real" collection name
    collectionName = col->name();
  }

  std::string returnType;
  if (body.isObject() && body.hasKey("type")) {
    returnType =
        arangodb::basics::VelocyPackHelper::getStringValue(body, "type", "");
  } else {
    returnType = _request->value("type");
  }

  std::string aql("FOR doc IN @@collection RETURN ");
  if (returnType == "key") {
    aql.append("doc._key");
  } else if (returnType == "id") {
    aql.append("doc._id");
  } else {
    aql.append(std::string("CONCAT('/_db/") + _vocbase.name() +
               "/_api/document/', doc._id)");
  }

  VPackBuilder data;
  data.openObject();
  data.add("query", VPackValue(aql));

  data.add(VPackValue("bindVars"));
  data.openObject();  // bindVars
  data.add("@collection", VPackValue(collectionName));
  data.close();  // bindVars
  data.close();

  return registerQueryOrCursor(data.slice());
}

static void buildExampleQuery(VPackBuilder& result, std::string const& cname,
                              VPackSlice const& doc, size_t skip, size_t limit) {
  TRI_ASSERT(doc.isObject());
  std::string query = "FOR doc IN @@collection";

  result.add("bindVars", VPackValue(VPackValueType::Object));
  result.add("@collection", VPackValue(cname));
  size_t i = 0;
  for (auto pair : VPackObjectIterator(doc, true)) {
    std::string key =
        basics::StringUtils::replace(pair.key.copyString(), "`", "");
    key =
        basics::StringUtils::join(basics::StringUtils::split(key, '.'), "`.`");
    std::string istr = std::to_string(i++);
    query.append(" FILTER doc.`").append(key).append("` == @value").append(istr);
    result.add(std::string("value") + istr, pair.value);
  }
  result.close();

  if (limit > 0 || skip > 0) {
    query.append(" LIMIT ")
        .append(std::to_string(skip))
        .append(", ")
        .append(limit > 0 ? std::to_string(limit) : "null")
        .append(" ");
  }
  query.append(" RETURN doc");

  result.add("query", VPackValue(query));
}

//////////////////////////////////////////////////////////////////////////////
/// @brief return a cursor with all documents matching the example
//////////////////////////////////////////////////////////////////////////////

RestStatus RestSimpleQueryHandler::byExample() {
  bool parseSuccess = false;
  VPackSlice const body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    // error message generated in parseVPackBody
    return RestStatus::DONE;
  }

  if (!body.isObject() || !body.hasKey("example") || !body.get("example").isObject()) {
    generateError(ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
    return RestStatus::DONE;
  }

  // velocypack will throw an exception for negative numbers
  size_t skip = basics::VelocyPackHelper::getNumericValue(body, "skip", 0);
  size_t limit = basics::VelocyPackHelper::getNumericValue(body, "limit", 0);
  size_t batchSize = basics::VelocyPackHelper::getNumericValue(body, "batchSize", 0);
  VPackSlice example = body.get("example");

  std::string cname;
  if (body.hasKey("collection")) {
    VPackSlice const value = body.get("collection");
    if (value.isString()) {
      cname = value.copyString();
    }
  } else {
    cname = _request->value("collection");
  }

  if (cname.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_TYPE_ERROR,
                  "expecting string for <collection>");
    return RestStatus::DONE;
  }

  auto col = _vocbase.lookupCollection(cname);

  if (col != nullptr && cname != col->name()) {
    // user has probably passed in a numeric collection id.
    // translate it into a "real" collection name
    cname = col->name();
  }

  VPackBuilder data;
  data.openObject();
  buildExampleQuery(data, cname, example, skip, limit);

  if (batchSize > 0) {
    data.add("batchSize", VPackValue(batchSize));
  }

  data.add("count", VPackSlice::trueSlice());
  data.close();

  return registerQueryOrCursor(data.slice());
}
