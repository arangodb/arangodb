////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RestIndexHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Rest/HttpRequest.h"
#include "VocBase/Methods/Indexes.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestIndexHandler::RestIndexHandler(GeneralRequest* request,
                                   GeneralResponse* response)
    : RestVocbaseBaseHandler(request, response) {}

RestStatus RestIndexHandler::execute() {
  // extract the request type
  rest::RequestType const type = _request->requestType();
  if (type == rest::RequestType::GET) {
    return getIndexes();
  } else if (type == rest::RequestType::POST) {
    return createIndex();
  } else if (type == rest::RequestType::DELETE_REQ) {
    return dropIndex();
  } else {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }
}

LogicalCollection* RestIndexHandler::collection(
    std::string const& cName, std::shared_ptr<LogicalCollection>& coll) {
  if (!cName.empty()) {
    if (ServerState::instance()->isCoordinator()) {
      try {
        coll = ClusterInfo::instance()->getCollection(_vocbase->name(), cName);
        return coll.get();
      } catch (...) {
      }
    } else {
      return _vocbase->lookupCollection(cName);
    }
  }
  return nullptr;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief get database infos
// //////////////////////////////////////////////////////////////////////////////
RestStatus RestIndexHandler::getIndexes() {
  std::shared_ptr<LogicalCollection> tmpColl;
  std::vector<std::string> const& suffixes = _request->suffixes();
  if (suffixes.empty()) {
    // .............................................................................
    // /_api/index?collection=<collection-name>
    // .............................................................................

    bool found = false;
    std::string cName = _request->value("collection", found);
    LogicalCollection* coll = collection(cName, tmpColl);
    if (coll == nullptr) {
      generateError(rest::ResponseCode::NOT_FOUND,
                    TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
      return RestStatus::DONE;
    }

    bool withFigures = false;
    std::string t = _request->value("withStats", found);
    if (found) {
      withFigures = StringUtils::tolower(t) == "true";
    }

    VPackBuilder indexes;
    Result res = methods::Indexes::getAll(coll, withFigures, indexes);
    if (!res.ok()) {
      generateError(rest::ResponseCode::BAD, res.errorNumber(),
                    res.errorMessage());
      return RestStatus::DONE;
    }

    TRI_ASSERT(indexes.slice().isArray());
    VPackBuilder tmp;
    tmp.openObject();
    tmp.add("error", VPackValue(false));
    tmp.add("code", VPackValue(static_cast<int>(rest::ResponseCode::OK)));
    tmp.add("indexes", indexes.slice());
    tmp.add("identifiers", VPackValue(VPackValueType::Object));
    for (VPackSlice const& index : VPackArrayIterator(indexes.slice())) {
      VPackSlice idd = index.get("id");
      VPackValueLength l = 0;
      char const* str = idd.getString(l);
      tmp.add(str, l, index);
    }
    tmp.close();
    tmp.close();
    generateResult(rest::ResponseCode::OK, tmp.slice());
  } else if (suffixes.size() == 2) {
    // .............................................................................
    // /_api/index/<collection-name>/<index-identifier>
    // .............................................................................

    std::string const& cName = suffixes[0];
    LogicalCollection* coll = collection(cName, tmpColl);
    if (coll == nullptr) {
      generateError(rest::ResponseCode::NOT_FOUND,
                    TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
      return RestStatus::DONE;
    }

    std::string const& iid = suffixes[1];
    VPackBuilder b;
    b.add(VPackValue(cName + TRI_INDEX_HANDLE_SEPARATOR_CHR + iid));

    VPackBuilder output;
    Result res = methods::Indexes::getIndex(coll, b.slice(), output);
    if (res.ok()) {
      VPackBuilder b;
      b.openObject();
      b.add("error", VPackValue(false));
      b.add("code", VPackValue(static_cast<int>(rest::ResponseCode::OK)));
      b.close();
      output = VPackCollection::merge(output.slice(), b.slice(), false);
      generateResult(rest::ResponseCode::OK, output.slice());
    } else {
      generateError(res);
    }
  } else {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
  }
  return RestStatus::DONE;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_get_api_database_create
// //////////////////////////////////////////////////////////////////////////////
RestStatus RestIndexHandler::createIndex() {
  std::vector<std::string> const& suffixes = _request->suffixes();
  bool parseSuccess = true;
  std::shared_ptr<VPackBuilder> parsedBody = parseVelocyPackBody(parseSuccess);
  if (!suffixes.empty() || !parseSuccess) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting POST /" + _request->requestPath() +
                      "?collection=<collection-name>");
    return RestStatus::DONE;
  }

  bool found = false;
  std::string cName = _request->value("collection", found);
  if (!found) {
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return RestStatus::DONE;
  }

  std::shared_ptr<LogicalCollection> tmpColl;
  LogicalCollection* coll = collection(cName, tmpColl);
  if (coll == nullptr) {
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return RestStatus::DONE;
  }

  VPackBuilder copy;
  VPackSlice body = parsedBody->slice();
  if (body.get("collection").isNone()) {
    copy.openObject();
    copy.add("collection", VPackValue(cName));
    copy.close();
    copy = VPackCollection::merge(body, copy.slice(), false);
    body = copy.slice();
  }

  VPackBuilder output;
  Result res = methods::Indexes::ensureIndex(coll, body, true, output);
  if (res.ok()) {
    VPackSlice created = output.slice().get("isNewlyCreated");
    auto r = created.isBool() && created.getBool() ? rest::ResponseCode::CREATED
                                                   : rest::ResponseCode::OK;

    VPackBuilder b;
    b.openObject();
    b.add("error", VPackValue(false));
    b.add("code", VPackValue(static_cast<int>(r)));
    b.close();
    output = VPackCollection::merge(output.slice(), b.slice(), false);

    generateResult(r, output.slice());
  } else {
    if (res.errorNumber() == TRI_ERROR_FORBIDDEN ||
        res.errorNumber() == TRI_ERROR_ARANGO_INDEX_NOT_FOUND) {
      generateError(res);
    } else {  // http_server compatibility
      generateError(rest::ResponseCode::BAD, res.errorNumber(),
                    res.errorMessage());
    }
  }
  return RestStatus::DONE;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_get_api_database_delete
// //////////////////////////////////////////////////////////////////////////////
RestStatus RestIndexHandler::dropIndex() {
  std::vector<std::string> const& suffixes = _request->suffixes();
  if (suffixes.size() != 2) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting DELETE /<collection-name>/<index-identifier>");
    return RestStatus::DONE;
  }

  std::string const& cName = suffixes[0];
  std::shared_ptr<LogicalCollection> tmpColl;
  LogicalCollection* coll = collection(cName, tmpColl);
  if (coll == nullptr) {
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    return RestStatus::DONE;
  }

  std::string const& iid = suffixes[1];
  VPackBuilder idBuilder;
  idBuilder.add(VPackValue(cName + TRI_INDEX_HANDLE_SEPARATOR_CHR + iid));

  Result res = methods::Indexes::drop(coll, idBuilder.slice());
  if (res.ok()) {
    VPackBuilder b;
    b.openObject();
    b.add("id", idBuilder.slice());
    b.add("error", VPackValue(false));
    b.add("code", VPackValue(static_cast<int>(rest::ResponseCode::OK)));
    b.close();
    generateResult(rest::ResponseCode::OK, b.slice());
  } else {
    generateError(res);
  }
  return RestStatus::DONE;
}
