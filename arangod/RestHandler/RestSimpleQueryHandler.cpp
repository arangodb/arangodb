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

#include "RestSimpleQueryHandler.h"

#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "Basics/Exceptions.h"
#include "Basics/MutexLocker.h"
#include "Basics/ScopeGuard.h"
#include "Basics/VelocyPackHelper.h"
#include "Utils/Cursor.h"
#include "Utils/CursorRepository.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;
using namespace arangodb::rest;

RestSimpleQueryHandler::RestSimpleQueryHandler(
    GeneralRequest* request, GeneralResponse* response,
    arangodb::aql::QueryRegistry* queryRegistry)
    : RestCursorHandler(request, response, queryRegistry) {}

RestStatus RestSimpleQueryHandler::execute() {
  // extract the sub-request type
  auto const type = _request->requestType();

  std::string const& prefix = _request->requestPath();
  if (type == rest::RequestType::PUT) {
    if (prefix == RestVocbaseBaseHandler::SIMPLE_QUERY_ALL_PATH) {
      // all query
      allDocuments();
      return RestStatus::DONE;
    }
    if (prefix == RestVocbaseBaseHandler::SIMPLE_QUERY_ALL_KEYS_PATH) {
      // all-keys query
      allDocumentKeys();
      return RestStatus::DONE;
    }
  }

  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  return RestStatus::DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSA_put_api_simple_all
////////////////////////////////////////////////////////////////////////////////

void RestSimpleQueryHandler::allDocuments() {
  bool parseSuccess = true;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(&VPackOptions::Defaults, parseSuccess);

  if (!parseSuccess) {
    return;
  }
  
  std::string collectionName;
  VPackSlice body = parsedBody.get()->slice();
  
  if (body.isObject() && body.hasKey("collection")) {
    VPackSlice const value = body.get("collection");
    if (value.isString()) {
      collectionName = value.copyString();
    }
    collectionName = value.copyString();
  } else {
    collectionName = _request->value("collection");
  }
  
  if (collectionName.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_TYPE_ERROR,
                  "expecting string for <collection>");
    return;
  }
      
  auto const* col = _vocbase->lookupCollection(collectionName);

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
  {
    VPackSlice ttl = body.get("ttl");
    if (!ttl.isNone()) {
      data.add("ttl", ttl);
    }

    VPackSlice batchSize = body.get("batchSize");
    if (!batchSize.isNone()) {
      data.add("batchSize", batchSize);
    }
  }
  data.close();

  VPackSlice s = data.slice();
  // now run the actual query and handle the result
  processQuery(s);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief return a cursor with all document keys from the collection
//////////////////////////////////////////////////////////////////////////////

void RestSimpleQueryHandler::allDocumentKeys() {
  bool parseSuccess = true;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(&VPackOptions::Defaults, parseSuccess);

  if (!parseSuccess) {
    return;
  }

  std::string collectionName;
  VPackSlice body = parsedBody.get()->slice();

  if (body.isObject() && body.hasKey("collection")) {
    VPackSlice const value = body.get("collection");
    if (value.isString()) {
      collectionName = value.copyString();
    }
    collectionName = value.copyString();
  } else {
    collectionName = _request->value("collection");
  }
      
  if (collectionName.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_TYPE_ERROR,
                  "expecting string for <collection>");
    return;
  }
  
  auto const* col = _vocbase->lookupCollection(collectionName);

  if (col != nullptr && collectionName != col->name()) {
    // user has probably passed in a numeric collection id.
    // translate it into a "real" collection name
    collectionName = col->name();
  }

  std::string returnType;
  if (body.isObject() && body.hasKey("type")) {
    returnType = arangodb::basics::VelocyPackHelper::getStringValue(body, "type", "");
  } else {
    returnType = _request->value("type");
  }

  std::string aql("FOR doc IN @@collection RETURN ");
  if (returnType == "key") {
    aql.append("doc._key");
  } else if (returnType == "id") {
    aql.append("doc._id");
  } else {
    aql.append(std::string("CONCAT('/_db/") + _vocbase->name() +
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

  VPackSlice s = data.slice();
  // now run the actual query and handle the result
  processQuery(s);
}
