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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "ClusterMethods.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterComm.h"
#include "Basics/conversions.h"
#include "Basics/json.h"
#include "Basics/tri-strings.h"
#include "Basics/vector.h"
#include "Basics/json-utilities.h"
#include "Basics/StringUtils.h"
#include "Indexes/Index.h"
#include "VocBase/Traverser.h"
#include "VocBase/server.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::basics;
using namespace arangodb::rest;

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a numeric value from an hierarchical VelocyPack
////////////////////////////////////////////////////////////////////////////////

template <typename T>
static T ExtractFigure(VPackSlice const& slice, char const* group,
                       char const* name) {
  TRI_ASSERT(slice.isObject());
  VPackSlice g = slice.get(group);

  if (!g.isObject()) {
    return static_cast<T>(0);
  }
  return arangodb::basics::VelocyPackHelper::getNumericValue<T>(g, name, 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts answer from response into a VPackBuilder.
///        If there was an error extracting the answer the builder will be
///        empty.
///        No Error can be thrown.
////////////////////////////////////////////////////////////////////////////////

static std::shared_ptr<VPackBuilder> ExtractAnswer(
    ClusterCommResult const& res) {
  try {
    return VPackParser::fromJson(res.answer->body());
  } catch (...) {
    // Return an empty Builder
    return std::make_shared<VPackBuilder>();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge headers of a DB server response into the current response
////////////////////////////////////////////////////////////////////////////////

void mergeResponseHeaders(HttpResponse* response,
                          std::map<std::string, std::string> const& headers) {
  std::map<std::string, std::string>::const_iterator it = headers.begin();

  while (it != headers.end()) {
    // skip first header line (which is the HTTP response code)
    std::string const& key = (*it).first;

    // the following headers are ignored
    if (key != "http/1.1" && key != "connection" && key != "content-length" &&
        key != "server") {
      response->setHeader(key, (*it).second);
    }
    ++it;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a copy of all HTTP headers to forward
////////////////////////////////////////////////////////////////////////////////

std::map<std::string, std::string> getForwardableRequestHeaders(
    arangodb::rest::HttpRequest* request) {
  std::map<std::string, std::string> const& headers = request->headers();
  std::map<std::string, std::string>::const_iterator it = headers.begin();

  std::map<std::string, std::string> result;

  while (it != headers.end()) {
    std::string const& key = (*it).first;

    // ignore the following headers
    if (key != "x-arango-async" && key != "authorization" &&
        key != "content-length" && key != "connection" && key != "expect" &&
        key != "host" && key != "origin" &&
        key.substr(0, 14) != "access-control") {
      result.emplace(key, (*it).second);
    }
    ++it;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a list of attributes have the same values in two JSON
/// documents
////////////////////////////////////////////////////////////////////////////////

bool shardKeysChanged(std::string const& dbname, std::string const& collname,
                      TRI_json_t const* oldJson, TRI_json_t const* newJson,
                      bool isPatch) {
  if (!TRI_IsObjectJson(oldJson) || !TRI_IsObjectJson(newJson)) {
    // expecting two objects. everything else is an error
    return true;
  }

  TRI_json_t nullJson;
  TRI_InitNullJson(&nullJson);

  ClusterInfo* ci = ClusterInfo::instance();
  std::shared_ptr<CollectionInfo> c = ci->getCollection(dbname, collname);
  std::vector<std::string> const& shardKeys = c->shardKeys();

  for (size_t i = 0; i < shardKeys.size(); ++i) {
    if (shardKeys[i] == TRI_VOC_ATTRIBUTE_KEY) {
      continue;
    }

    TRI_json_t const* n = TRI_LookupObjectJson(newJson, shardKeys[i].c_str());

    if (n == nullptr && isPatch) {
      // attribute not set in patch document. this means no update
      continue;
    }

    TRI_json_t const* o = TRI_LookupObjectJson(oldJson, shardKeys[i].c_str());

    if (o == nullptr) {
      // if attribute is undefined, use "null" instead
      o = &nullJson;
    }

    if (n == nullptr) {
      // if attribute is undefined, use "null" instead
      n = &nullJson;
    }

    if (!TRI_CheckSameValueJson(o, n)) {
      return true;
    }
  }

  return false;
}

bool shardKeysChanged(std::string const& dbname, std::string const& collname,
                      VPackSlice const& oldSlice, VPackSlice const& newSlice,
                      bool isPatch) {
  std::unique_ptr<TRI_json_t> tmpOld(
      arangodb::basics::VelocyPackHelper::velocyPackToJson(oldSlice));
  if (tmpOld == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  std::unique_ptr<TRI_json_t> tmpNew(
      arangodb::basics::VelocyPackHelper::velocyPackToJson(newSlice));
  if (tmpNew == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  return shardKeysChanged(dbname, collname, tmpOld.get(), tmpNew.get(), isPatch);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns users
////////////////////////////////////////////////////////////////////////////////

int usersOnCoordinator(std::string const& dbname, VPackBuilder& result,
                       double timeout) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();

  // First determine the collection ID from the name:
  std::shared_ptr<CollectionInfo> collinfo =
      ci->getCollection(dbname, TRI_COL_NAME_USERS);

  if (collinfo->empty()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  // If we get here, the sharding attributes are not only _key, therefore
  // we have to contact everybody:
  auto shards = collinfo->shardIds();
  CoordTransactionID coordTransactionID = TRI_NewTickServer();

  for (auto const& p : *shards) {
    std::unique_ptr<std::map<std::string, std::string>> headers(
        new std::map<std::string, std::string>());

    // set collection name (shard id)
    auto body = std::make_shared<std::string>();
    body->append("{\"collection\":\"");
    body->append(p.first);
    body->append("\"}");

    cc->asyncRequest(
        "", coordTransactionID, "shard:" + p.first,
        arangodb::rest::HttpRequest::HTTP_REQUEST_PUT,
        "/_db/" + StringUtils::urlEncode(dbname) + "/_api/simple/all", body,
        headers, nullptr, 10.0);
  }

  try {
    // Now listen to the results:
    int count;
    int nrok = 0;
    for (count = (int)shards->size(); count > 0; count--) {
      auto res = cc->wait("", coordTransactionID, 0, "", timeout);
      if (res.status == CL_COMM_RECEIVED) {
        if (res.answer_code == arangodb::rest::HttpResponse::OK ||
            res.answer_code == arangodb::rest::HttpResponse::CREATED) {
          std::shared_ptr<VPackBuilder> answerBuilder = ExtractAnswer(res);
          VPackSlice answer = answerBuilder->slice();

          if (answer.isObject()) {
            VPackSlice r = answer.get("result");
            if (r.isArray()) {
              for (auto const& p : VPackArrayIterator(r)) {
                if (p.isObject()) {
                  result.add(p);
                }
              }
            }
            nrok++;
          }
        }
      }
    }
    if (nrok != (int)shards->size()) {
      return TRI_ERROR_INTERNAL;
    }
  } catch (...) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  return TRI_ERROR_NO_ERROR;  // the cluster operation was OK, however,
                              // the DBserver could have reported an error.
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns revision for a sharded collection
////////////////////////////////////////////////////////////////////////////////

int revisionOnCoordinator(std::string const& dbname,
                          std::string const& collname, TRI_voc_rid_t& rid) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();

  // First determine the collection ID from the name:
  std::shared_ptr<CollectionInfo> collinfo =
      ci->getCollection(dbname, collname);

  if (collinfo->empty()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  rid = 0;

  // If we get here, the sharding attributes are not only _key, therefore
  // we have to contact everybody:
  auto shards = collinfo->shardIds();
  CoordTransactionID coordTransactionID = TRI_NewTickServer();

  for (auto const& p : *shards) {
    std::unique_ptr<std::map<std::string, std::string>> headers(
        new std::map<std::string, std::string>());
    cc->asyncRequest(
        "", coordTransactionID, "shard:" + p.first,
        arangodb::rest::HttpRequest::HTTP_REQUEST_GET,
        "/_db/" + StringUtils::urlEncode(dbname) + "/_api/collection/" +
            StringUtils::urlEncode(p.first) + "/revision",
        std::shared_ptr<std::string const>(), headers, nullptr, 300.0);
  }

  // Now listen to the results:
  int count;
  int nrok = 0;
  for (count = (int)shards->size(); count > 0; count--) {
    auto res = cc->wait("", coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code == arangodb::rest::HttpResponse::OK) {
        std::shared_ptr<VPackBuilder> answerBuilder = ExtractAnswer(res);
        VPackSlice answer = answerBuilder->slice();

        if (answer.isObject()) {
          VPackSlice r = answer.get("revision");

          if (r.isString()) {
            TRI_voc_rid_t cmp = StringUtils::uint64(r.copyString());

            if (cmp > rid) {
              // get the maximum value
              rid = cmp;
            }
          }
          nrok++;
        }
      }
    }
  }

  if (nrok != (int)shards->size()) {
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;  // the cluster operation was OK, however,
                              // the DBserver could have reported an error.
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns figures for a sharded collection
////////////////////////////////////////////////////////////////////////////////

int figuresOnCoordinator(std::string const& dbname, std::string const& collname,
                         TRI_doc_collection_info_t*& result) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();

  // First determine the collection ID from the name:
  std::shared_ptr<CollectionInfo> collinfo =
      ci->getCollection(dbname, collname);

  if (collinfo->empty()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  // prefill with 0s
  result = (TRI_doc_collection_info_t*)TRI_Allocate(
      TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_doc_collection_info_t), true);

  if (result == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // If we get here, the sharding attributes are not only _key, therefore
  // we have to contact everybody:
  auto shards = collinfo->shardIds();
  CoordTransactionID coordTransactionID = TRI_NewTickServer();

  for (auto const& p : *shards) {
    std::unique_ptr<std::map<std::string, std::string>> headers(
        new std::map<std::string, std::string>());
    cc->asyncRequest(
        "", coordTransactionID, "shard:" + p.first,
        arangodb::rest::HttpRequest::HTTP_REQUEST_GET,
        "/_db/" + StringUtils::urlEncode(dbname) + "/_api/collection/" +
            StringUtils::urlEncode(p.first) + "/figures",
        std::shared_ptr<std::string const>(), headers, nullptr, 300.0);
  }

  // Now listen to the results:
  int count;
  int nrok = 0;
  for (count = (int)shards->size(); count > 0; count--) {
    auto res = cc->wait("", coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code == arangodb::rest::HttpResponse::OK) {
        std::shared_ptr<VPackBuilder> answerBuilder = ExtractAnswer(res);
        VPackSlice answer = answerBuilder->slice();

        if (answer.isObject()) {
          VPackSlice figures = answer.get("figures");
          if (figures.isObject()) {
            // add to the total
            result->_numberAlive +=
                ExtractFigure<TRI_voc_ssize_t>(figures, "alive", "count");
            result->_numberDead +=
                ExtractFigure<TRI_voc_ssize_t>(figures, "dead", "count");
            result->_numberDeletions +=
                ExtractFigure<TRI_voc_ssize_t>(figures, "dead", "deletion");
            result->_numberShapes +=
                ExtractFigure<TRI_voc_ssize_t>(figures, "shapes", "count");
            result->_numberAttributes +=
                ExtractFigure<TRI_voc_ssize_t>(figures, "attributes", "count");
            result->_numberIndexes +=
                ExtractFigure<TRI_voc_ssize_t>(figures, "indexes", "count");

            result->_sizeAlive +=
                ExtractFigure<int64_t>(figures, "alive", "size");
            result->_sizeDead +=
                ExtractFigure<int64_t>(figures, "dead", "size");
            result->_sizeShapes +=
                ExtractFigure<int64_t>(figures, "shapes", "size");
            result->_sizeAttributes +=
                ExtractFigure<int64_t>(figures, "attributes", "size");
            result->_sizeIndexes +=
                ExtractFigure<int64_t>(figures, "indexes", "size");

            result->_numberDatafiles +=
                ExtractFigure<TRI_voc_ssize_t>(figures, "datafiles", "count");
            result->_numberJournalfiles +=
                ExtractFigure<TRI_voc_ssize_t>(figures, "journals", "count");
            result->_numberCompactorfiles +=
                ExtractFigure<TRI_voc_ssize_t>(figures, "compactors", "count");
            result->_numberShapefiles +=
                ExtractFigure<TRI_voc_ssize_t>(figures, "shapefiles", "count");

            result->_datafileSize +=
                ExtractFigure<int64_t>(figures, "datafiles", "fileSize");
            result->_journalfileSize +=
                ExtractFigure<int64_t>(figures, "journals", "fileSize");
            result->_compactorfileSize +=
                ExtractFigure<int64_t>(figures, "compactors", "fileSize");
            result->_shapefileSize +=
                ExtractFigure<int64_t>(figures, "shapefiles", "fileSize");

            result->_numberDocumentDitches +=
                arangodb::basics::VelocyPackHelper::getNumericValue<uint64_t>(
                    figures, "documentReferences", 0);
          }
          nrok++;
        }
      }
    }
  }

  if (nrok != (int)shards->size()) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, result);
    result = 0;
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;  // the cluster operation was OK, however,
                              // the DBserver could have reported an error.
}

////////////////////////////////////////////////////////////////////////////////
/// @brief counts number of documents in a coordinator
////////////////////////////////////////////////////////////////////////////////

int countOnCoordinator(std::string const& dbname, std::string const& collname,
                       uint64_t& result) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();

  result = 0;

  // First determine the collection ID from the name:
  std::shared_ptr<CollectionInfo> collinfo =
      ci->getCollection(dbname, collname);

  if (collinfo->empty()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  auto shards = collinfo->shardIds();
  CoordTransactionID coordTransactionID = TRI_NewTickServer();
  for (auto const& p : *shards) {
    std::unique_ptr<std::map<std::string, std::string>> headers(
        new std::map<std::string, std::string>());
    cc->asyncRequest(
        "", coordTransactionID, "shard:" + p.first,
        arangodb::rest::HttpRequest::HTTP_REQUEST_GET,
        "/_db/" + StringUtils::urlEncode(dbname) + "/_api/collection/" +
            StringUtils::urlEncode(p.first) + "/count",
        std::shared_ptr<std::string>(nullptr), headers, nullptr, 300.0);
  }
  // Now listen to the results:
  int count;
  int nrok = 0;
  for (count = (int)shards->size(); count > 0; count--) {
    auto res = cc->wait("", coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code == arangodb::rest::HttpResponse::OK) {
        std::shared_ptr<VPackBuilder> answerBuilder = ExtractAnswer(res);
        VPackSlice answer = answerBuilder->slice();

        if (answer.isObject()) {
          // add to the total
          result +=
              arangodb::basics::VelocyPackHelper::getNumericValue<uint64_t>(
                  answer, "count", 0);
          nrok++;
        }
      }
    }
  }

  if (nrok != (int)shards->size()) {
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;  // the cluster operation was OK, however,
                              // the DBserver could have reported an error.
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int createDocumentOnCoordinator(
    std::string const& dbname, std::string const& collname, bool waitForSync,
    VPackSlice const& slice, std::map<std::string, std::string> const& headers,
    arangodb::rest::HttpResponse::HttpResponseCode& responseCode,
    std::map<std::string, std::string>& resultHeaders,
    std::string& resultBody) {
  std::unique_ptr<TRI_json_t> json(
      arangodb::basics::VelocyPackHelper::velocyPackToJson(slice));
  return createDocumentOnCoordinator(dbname, collname, waitForSync, json,
                                     headers, responseCode, resultHeaders,
                                     resultBody);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int createDocumentOnCoordinator(
    std::string const& dbname, std::string const& collname, bool waitForSync,
    std::unique_ptr<TRI_json_t>& json,
    std::map<std::string, std::string> const& headers,
    arangodb::rest::HttpResponse::HttpResponseCode& responseCode,
    std::map<std::string, std::string>& resultHeaders,
    std::string& resultBody) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();

  // First determine the collection ID from the name:
  std::shared_ptr<CollectionInfo> collinfo =
      ci->getCollection(dbname, collname);

  if (collinfo->empty()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  std::string const collid = StringUtils::itoa(collinfo->id());

  // Sort out the _key attribute:
  // The user is allowed to specify _key, provided that _key is the one
  // and only sharding attribute, because in this case we can delegate
  // the responsibility to make _key attributes unique to the responsible
  // shard. Otherwise, we ensure uniqueness here and now by taking a
  // cluster-wide unique number. Note that we only know the sharding
  // attributes a bit further down the line when we have determined
  // the responsible shard.
  TRI_json_t* subjson = TRI_LookupObjectJson(json.get(), TRI_VOC_ATTRIBUTE_KEY);
  bool userSpecifiedKey = false;
  std::string _key;
  if (subjson == nullptr) {
    // The user did not specify a key, let's create one:
    uint64_t uid = ci->uniqid();
    _key = arangodb::basics::StringUtils::itoa(uid);
    TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json.get(),
                          TRI_VOC_ATTRIBUTE_KEY,
                          TRI_CreateStringReferenceJson(
                              TRI_UNKNOWN_MEM_ZONE, _key.c_str(), _key.size()));
  } else {
    userSpecifiedKey = true;
  }

  // Now find the responsible shard:
  bool usesDefaultShardingAttributes;
  ShardID shardID;
  int error = ci->getResponsibleShard(collid, json.get(), true, shardID,
                                      usesDefaultShardingAttributes);
  if (error == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
    return TRI_ERROR_CLUSTER_SHARD_GONE;
  }

  // Now perform the above mentioned check:
  if (userSpecifiedKey && !usesDefaultShardingAttributes) {
    return TRI_ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY;
  }

  if (userSpecifiedKey && !collinfo->allowUserKeys()) {
    return TRI_ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY;
  }

  std::string const body = JsonHelper::toString(json.get());

  // Send a synchronous request to that shard using ClusterComm:
  auto res = cc->syncRequest(
      "", TRI_NewTickServer(), "shard:" + shardID,
      arangodb::rest::HttpRequest::HTTP_REQUEST_POST,
      "/_db/" + StringUtils::urlEncode(dbname) + "/_api/document?collection=" +
          StringUtils::urlEncode(shardID) + "&waitForSync=" +
          (waitForSync ? "true" : "false"),
      body, headers, 60.0);

  if (res->status == CL_COMM_TIMEOUT) {
    // No reply, we give up:
    return TRI_ERROR_CLUSTER_TIMEOUT;
  }
  if (res->status == CL_COMM_ERROR) {
    // This could be a broken connection or an Http error:
    if (res->result == nullptr || !res->result->isComplete()) {
      // there is not result
      return TRI_ERROR_CLUSTER_CONNECTION_LOST;
    }
    // In this case a proper HTTP error was reported by the DBserver,
    // this can be 400 or 404, we simply forward the result.
    // We intentionally fall through here.
  }
  responseCode = static_cast<arangodb::rest::HttpResponse::HttpResponseCode>(
      res->result->getHttpReturnCode());
  resultHeaders = res->result->getHeaderFields();
  resultBody.assign(res->result->getBody().c_str(),
                    res->result->getBody().length());
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int deleteDocumentOnCoordinator(
    std::string const& dbname, std::string const& collname,
    std::string const& key, TRI_voc_rid_t const rev,
    TRI_doc_update_policy_e policy, bool waitForSync,
    std::unique_ptr<std::map<std::string, std::string>>& headers,
    arangodb::rest::HttpResponse::HttpResponseCode& responseCode,
    std::map<std::string, std::string>& resultHeaders,
    std::string& resultBody) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();

  // First determine the collection ID from the name:
  std::shared_ptr<CollectionInfo> collinfo =
      ci->getCollection(dbname, collname);
  if (collinfo->empty()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }
  std::string collid = StringUtils::itoa(collinfo->id());

  // If _key is the one and only sharding attribute, we can do this quickly,
  // because we can easily determine which shard is responsible for the
  // document. Otherwise we have to contact all shards and ask them to
  // delete the document. All but one will not know it.
  // Now find the responsible shard:
  TRI_json_t* json = TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE);
  if (json == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, TRI_VOC_ATTRIBUTE_KEY,
                        TRI_CreateStringReferenceJson(TRI_UNKNOWN_MEM_ZONE,
                                                      key.c_str(), key.size()));
  bool usesDefaultShardingAttributes;
  ShardID shardID;
  int error = ci->getResponsibleShard(collid, json, true, shardID,
                                      usesDefaultShardingAttributes);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  // Some stuff to prepare cluster-intern requests:
  std::string revstr;
  if (rev != 0) {
    revstr = "&rev=" + StringUtils::itoa(rev);
  }

  std::string policystr;
  if (policy == TRI_DOC_UPDATE_LAST_WRITE) {
    policystr = "&policy=last";
  }

  if (usesDefaultShardingAttributes) {
    // OK, this is the fast method, we only have to ask one shard:
    if (error == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
      return TRI_ERROR_CLUSTER_SHARD_GONE;
    }

    // Send a synchronous request to that shard using ClusterComm:
    auto res = cc->syncRequest(
        "", TRI_NewTickServer(), "shard:" + shardID,
        arangodb::rest::HttpRequest::HTTP_REQUEST_DELETE,
        "/_db/" + dbname + "/_api/document/" + StringUtils::urlEncode(shardID) +
            "/" + StringUtils::urlEncode(key) + "?waitForSync=" +
            (waitForSync ? "true" : "false") + revstr + policystr,
        "", *headers, 60.0);

    if (res->status == CL_COMM_TIMEOUT) {
      // No reply, we give up:
      return TRI_ERROR_CLUSTER_TIMEOUT;
    }
    if (res->status == CL_COMM_ERROR) {
      // This could be a broken connection or an Http error:
      if (res->result == nullptr || !res->result->isComplete()) {
        return TRI_ERROR_CLUSTER_CONNECTION_LOST;
      }
      // In this case a proper HTTP error was reported by the DBserver,
      // this can be 400 or 404, we simply forward the result.
      // We intentionally fall through here.
    }
    responseCode = static_cast<arangodb::rest::HttpResponse::HttpResponseCode>(
        res->result->getHttpReturnCode());
    resultHeaders = res->result->getHeaderFields();
    resultBody.assign(res->result->getBody().c_str(),
                      res->result->getBody().length());
    return TRI_ERROR_NO_ERROR;
  }

  // If we get here, the sharding attributes are not only _key, therefore
  // we have to contact everybody:
  auto shards = collinfo->shardIds();
  CoordTransactionID coordTransactionID = TRI_NewTickServer();
  for (auto const& p : *shards) {
    std::unique_ptr<std::map<std::string, std::string>> headersCopy(
        new std::map<std::string, std::string>(*headers));
    cc->asyncRequest("", coordTransactionID, "shard:" + p.first,
                     arangodb::rest::HttpRequest::HTTP_REQUEST_DELETE,
                     "/_db/" + StringUtils::urlEncode(dbname) +
                         "/_api/document/" + StringUtils::urlEncode(p.first) +
                         "/" + StringUtils::urlEncode(key) + "?waitForSync=" +
                         (waitForSync ? "true" : "false") + revstr + policystr,
                     std::shared_ptr<std::string const>(), headersCopy, nullptr,
                     60.0);
  }
  // Now listen to the results:
  int count;
  int nrok = 0;
  for (count = (int)shards->size(); count > 0; count--) {
    auto res = cc->wait("", coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code != arangodb::rest::HttpResponse::NOT_FOUND ||
          (nrok == 0 && count == 1)) {
        nrok++;
        responseCode = res.answer_code;
        resultHeaders = res.answer->headers();
        resultBody = std::string(res.answer->body(), res.answer->bodySize());
      }
    }
  }

  // Note that nrok is always at least 1!
  if (nrok > 1) {
    return TRI_ERROR_CLUSTER_GOT_CONTRADICTING_ANSWERS;
  }
  return TRI_ERROR_NO_ERROR;  // the cluster operation was OK, however,
                              // the DBserver could have reported an error.
}

////////////////////////////////////////////////////////////////////////////////
/// @brief truncate a cluster collection on a coordinator
////////////////////////////////////////////////////////////////////////////////

int truncateCollectionOnCoordinator(std::string const& dbname,
                                    std::string const& collname) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();

  // First determine the collection ID from the name:
  std::shared_ptr<CollectionInfo> collinfo =
      ci->getCollection(dbname, collname);

  if (collinfo->empty()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  // Some stuff to prepare cluster-intern requests:
  // We have to contact everybody:
  auto shards = collinfo->shardIds();
  CoordTransactionID coordTransactionID = TRI_NewTickServer();
  for (auto const& p : *shards) {
    std::unique_ptr<std::map<std::string, std::string>> headers(
        new std::map<std::string, std::string>());
    cc->asyncRequest("", coordTransactionID, "shard:" + p.first,
                     arangodb::rest::HttpRequest::HTTP_REQUEST_PUT,
                     "/_db/" + StringUtils::urlEncode(dbname) +
                         "/_api/collection/" + p.first + "/truncate",
                     std::shared_ptr<std::string>(nullptr), headers, nullptr,
                     60.0);
  }
  // Now listen to the results:
  unsigned int count;
  unsigned int nrok = 0;
  for (count = (unsigned int)shards->size(); count > 0; count--) {
    auto res = cc->wait("", coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code == arangodb::rest::HttpResponse::OK) {
        nrok++;
      }
    }
  }

  // Note that nrok is always at least 1!
  if (nrok < shards->size()) {
    return TRI_ERROR_CLUSTER_COULD_NOT_TRUNCATE_COLLECTION;
  }
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int getDocumentOnCoordinator(
    std::string const& dbname, std::string const& collname,
    std::string const& key, TRI_voc_rid_t const rev,
    std::unique_ptr<std::map<std::string, std::string>>& headers,
    bool generateDocument,
    arangodb::rest::HttpResponse::HttpResponseCode& responseCode,
    std::map<std::string, std::string>& resultHeaders,
    std::string& resultBody) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();

  // First determine the collection ID from the name:
  std::shared_ptr<CollectionInfo> collinfo =
      ci->getCollection(dbname, collname);
  if (collinfo->empty()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }
  std::string collid = StringUtils::itoa(collinfo->id());

  // If _key is the one and only sharding attribute, we can do this quickly,
  // because we can easily determine which shard is responsible for the
  // document. Otherwise we have to contact all shards and ask them to
  // delete the document. All but one will not know it.
  // Now find the responsible shard:
  TRI_json_t* json = TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE);
  if (json == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json, "_key",
                        TRI_CreateStringReferenceJson(TRI_UNKNOWN_MEM_ZONE,
                                                      key.c_str(), key.size()));
  bool usesDefaultShardingAttributes;
  ShardID shardID;
  int error = ci->getResponsibleShard(collid, json, true, shardID,
                                      usesDefaultShardingAttributes);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  // Some stuff to prepare cluster-intern requests:
  std::string revstr;
  if (rev != 0) {
    revstr = "?rev=" + StringUtils::itoa(rev);
  }
  arangodb::rest::HttpRequest::HttpRequestType reqType;
  if (generateDocument) {
    reqType = arangodb::rest::HttpRequest::HTTP_REQUEST_GET;
  } else {
    reqType = arangodb::rest::HttpRequest::HTTP_REQUEST_HEAD;
  }

  if (usesDefaultShardingAttributes) {
    // OK, this is the fast method, we only have to ask one shard:
    if (error == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
      return TRI_ERROR_CLUSTER_SHARD_GONE;
    }

    // Send a synchronous request to that shard using ClusterComm:
    auto res = cc->syncRequest(
        "", TRI_NewTickServer(), "shard:" + shardID, reqType,
        "/_db/" + dbname + "/_api/document/" + StringUtils::urlEncode(shardID) +
            "/" + StringUtils::urlEncode(key) + revstr,
        "", *headers, 60.0);

    if (res->status == CL_COMM_TIMEOUT) {
      // No reply, we give up:
      return TRI_ERROR_CLUSTER_TIMEOUT;
    }
    if (res->status == CL_COMM_ERROR) {
      // This could be a broken connection or an Http error:
      if (!res->result || !res->result->isComplete()) {
        return TRI_ERROR_CLUSTER_CONNECTION_LOST;
      }
      // In this case a proper HTTP error was reported by the DBserver,
      // this can be 400 or 404, we simply forward the result.
      // We intentionally fall through here.
    }
    responseCode = static_cast<arangodb::rest::HttpResponse::HttpResponseCode>(
        res->result->getHttpReturnCode());
    resultHeaders = res->result->getHeaderFields();
    resultBody.assign(res->result->getBody().c_str(),
                      res->result->getBody().length());
    return TRI_ERROR_NO_ERROR;
  }

  // If we get here, the sharding attributes are not only _key, therefore
  // we have to contact everybody:
  auto shards = collinfo->shardIds();
  CoordTransactionID coordTransactionID = TRI_NewTickServer();
  for (auto const& p : *shards) {
    std::unique_ptr<std::map<std::string, std::string>> headersCopy(
        new std::map<std::string, std::string>(*headers));
    cc->asyncRequest("", coordTransactionID, "shard:" + p.first, reqType,
                     "/_db/" + StringUtils::urlEncode(dbname) +
                         "/_api/document/" + StringUtils::urlEncode(p.first) +
                         "/" + StringUtils::urlEncode(key) + revstr,
                     std::shared_ptr<std::string const>(), headersCopy, nullptr,
                     60.0);
  }
  // Now listen to the results:
  int count;
  int nrok = 0;
  for (count = (int)shards->size(); count > 0; count--) {
    auto res = cc->wait("", coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code != arangodb::rest::HttpResponse::NOT_FOUND ||
          (nrok == 0 && count == 1)) {
        nrok++;
        responseCode = res.answer_code;
        resultHeaders = res.answer->headers();
        resultBody = std::string(res.answer->body(), res.answer->bodySize());
      }
    }
  }

  // Note that nrok is always at least 1!
  if (nrok > 1) {
    return TRI_ERROR_CLUSTER_GOT_CONTRADICTING_ANSWERS;
  }
  return TRI_ERROR_NO_ERROR;  // the cluster operation was OK, however,
                              // the DBserver could have reported an error.
}

static void insertIntoShardMap(
    ClusterInfo* ci, std::string const& dbname, std::string const& documentId,
    std::unordered_map<ShardID, std::vector<std::string>>& shardMap) {
  std::vector<std::string> splitId =
      arangodb::basics::StringUtils::split(documentId, '/');
  TRI_ASSERT(splitId.size() == 2);

  // First determine the collection ID from the name:
  std::shared_ptr<CollectionInfo> collinfo =
      ci->getCollection(dbname, splitId[0]);
  if (collinfo->empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                   "Collection not found: " + splitId[0]);
  }
  std::string collid = StringUtils::itoa(collinfo->id());
  if (collinfo->usesDefaultShardKeys()) {
    // We only need add one resp. shard
    arangodb::basics::Json partial(arangodb::basics::Json::Object, 1);
    partial.set("_key", arangodb::basics::Json(splitId[1]));
    bool usesDefaultShardingAttributes;
    ShardID shardID;

    int error = ci->getResponsibleShard(collid, partial.json(), true, shardID,
                                        usesDefaultShardingAttributes);
    if (error != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(error);
    }
    TRI_ASSERT(usesDefaultShardingAttributes);  // If this is false the if
                                                // condition should be false in
                                                // the first place
    auto it = shardMap.find(shardID);
    if (it == shardMap.end()) {
      shardMap.emplace(shardID, std::vector<std::string>({splitId[1]}));
    } else {
      it->second.push_back(splitId[1]);
    }
  } else {
    // Sorry we do not know the responsible shard yet
    // Ask all of them
    auto shardList = ci->getShardList(collid);
    for (auto const& shard : *shardList) {
      auto it = shardMap.find(shard);
      if (it == shardMap.end()) {
        shardMap.emplace(shard, std::vector<std::string>({splitId[1]}));
      } else {
        it->second.push_back(splitId[1]);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a list of filtered documents in a coordinator
///        All found documents will be inserted into result.
///        After execution documentIds will contain all id's of documents
///        that could not be found.
////////////////////////////////////////////////////////////////////////////////

int getFilteredDocumentsOnCoordinator(
    std::string const& dbname,
    std::vector<traverser::TraverserExpression*> const& expressions,
    std::unique_ptr<std::map<std::string, std::string>>& headers,
    std::unordered_set<std::string>& documentIds,
    std::unordered_map<std::string, TRI_json_t*>& result) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();

  std::unordered_map<ShardID, std::vector<std::string>> shardRequestMap;
  for (auto const& doc : documentIds) {
    insertIntoShardMap(ci, dbname, doc, shardRequestMap);
  }

  // Now start the request.
  // We do not have to care for shard attributes esp. shard by key.
  // If it is by key the key was only added to one key list, if not
  // it is contained multiple times.
  CoordTransactionID coordTransactionID = TRI_NewTickServer();
  for (auto const& shard : shardRequestMap) {
    std::unique_ptr<std::map<std::string, std::string>> headersCopy(
        new std::map<std::string, std::string>(*headers));
    arangodb::basics::Json reqBody(arangodb::basics::Json::Object, 2);
    reqBody("collection", arangodb::basics::Json(static_cast<std::string>(
                              shard.first)));  // ShardID is a string
    arangodb::basics::Json keyList(arangodb::basics::Json::Array,
                                   shard.second.size());
    for (auto const& key : shard.second) {
      keyList.add(arangodb::basics::Json(key));
    }
    reqBody("keys", keyList.steal());
    if (!expressions.empty()) {
      arangodb::basics::Json filter(Json::Array, expressions.size());
      for (auto const& e : expressions) {
        arangodb::basics::Json tmp(Json::Object);
        e->toJson(tmp, TRI_UNKNOWN_MEM_ZONE);
        filter.add(tmp.steal());
      }
      reqBody("filter", filter);
    }
    auto bodyString = std::make_shared<std::string>(reqBody.toString());

    cc->asyncRequest("", coordTransactionID, "shard:" + shard.first,
                     arangodb::rest::HttpRequest::HTTP_REQUEST_PUT,
                     "/_db/" + StringUtils::urlEncode(dbname) +
                         "/_api/simple/lookup-by-keys",
                     bodyString, headersCopy, nullptr, 60.0);
  }
  // All requests send, now collect results.
  for (size_t i = 0; i < shardRequestMap.size(); ++i) {
    auto res = cc->wait("", coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_RECEIVED) {
      std::unique_ptr<TRI_json_t> resultBody(
          arangodb::basics::JsonHelper::fromString(res.answer->body(),
                                                   res.answer->bodySize()));
      if (!TRI_IsObjectJson(resultBody.get())) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL, "Received an invalid result in cluster.");
      }
      bool isError = arangodb::basics::JsonHelper::checkAndGetBooleanValue(
          resultBody.get(), "error");
      if (isError) {
        int errorNum = arangodb::basics::JsonHelper::getNumericValue<int>(
            resultBody.get(), "errorNum", TRI_ERROR_INTERNAL);
        std::string message = arangodb::basics::JsonHelper::getStringValue(
            resultBody.get(), "errorMessage", "");
        THROW_ARANGO_EXCEPTION_MESSAGE(errorNum, message);
      }
      TRI_json_t* documents =
          TRI_LookupObjectJson(resultBody.get(), "documents");
      if (!TRI_IsArrayJson(documents)) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL, "Received an invalid result in cluster.");
      }
      size_t resCount = TRI_LengthArrayJson(documents);
      for (size_t k = 0; k < resCount; ++k) {
        try {
          TRI_json_t* element = TRI_LookupArrayJson(documents, k);
          std::string id = arangodb::basics::JsonHelper::checkAndGetStringValue(
              element, "_id");
          result.emplace(id, TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, element));
          documentIds.erase(id);
        } catch (...) {
          // Ignore this error.
        }
      }
      TRI_json_t* filtered = TRI_LookupObjectJson(resultBody.get(), "filtered");
      if (filtered != nullptr && TRI_IsArrayJson(filtered)) {
        size_t resCount = TRI_LengthArrayJson(filtered);
        for (size_t k = 0; k < resCount; ++k) {
          TRI_json_t* element = TRI_LookupArrayJson(filtered, k);
          std::string def;
          std::string id =
              arangodb::basics::JsonHelper::getStringValue(element, def);
          documentIds.erase(id);
        }
      }
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get all documents in a coordinator
////////////////////////////////////////////////////////////////////////////////

int getAllDocumentsOnCoordinator(
    std::string const& dbname, std::string const& collname,
    std::string const& returnType,
    arangodb::rest::HttpResponse::HttpResponseCode& responseCode,
    std::string& contentType, std::string& resultBody) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();

  // First determine the collection ID from the name:
  std::shared_ptr<CollectionInfo> collinfo =
      ci->getCollection(dbname, collname);
  if (collinfo->empty()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  auto shards = collinfo->shardIds();
  CoordTransactionID coordTransactionID = TRI_NewTickServer();
  for (auto const& p : *shards) {
    std::unique_ptr<std::map<std::string, std::string>> headers(
        new std::map<std::string, std::string>());
    cc->asyncRequest("", coordTransactionID, "shard:" + p.first,
                     arangodb::rest::HttpRequest::HTTP_REQUEST_GET,
                     "/_db/" + StringUtils::urlEncode(dbname) +
                         "/_api/document?collection=" + p.first + "&type=" +
                         StringUtils::urlEncode(returnType),
                     std::shared_ptr<std::string const>(), headers, nullptr,
                     3600.0);
  }
  // Now listen to the results:
  int count;
  responseCode = arangodb::rest::HttpResponse::OK;
  contentType = "application/json; charset=utf-8";

  arangodb::basics::Json result(arangodb::basics::Json::Object);
  arangodb::basics::Json documents(arangodb::basics::Json::Array);

  for (count = (int)shards->size(); count > 0; count--) {
    auto res = cc->wait("", coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_TIMEOUT) {
      cc->drop("", coordTransactionID, 0, "");
      return TRI_ERROR_CLUSTER_TIMEOUT;
    }
    if (res.status == CL_COMM_ERROR || res.status == CL_COMM_DROPPED ||
        res.answer_code == arangodb::rest::HttpResponse::NOT_FOUND) {
      cc->drop("", coordTransactionID, 0, "");
      return TRI_ERROR_INTERNAL;
    }

    std::unique_ptr<TRI_json_t> shardResult(
        TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, res.answer->body()));

    if (shardResult == nullptr || !TRI_IsObjectJson(shardResult.get())) {
      return TRI_ERROR_INTERNAL;
    }

    auto docs = TRI_LookupObjectJson(shardResult.get(), "documents");

    if (!TRI_IsArrayJson(docs)) {
      return TRI_ERROR_INTERNAL;
    }

    size_t const n = TRI_LengthArrayJson(docs);
    documents.reserve(n);

    for (size_t j = 0; j < n; ++j) {
      auto doc =
          static_cast<TRI_json_t*>(TRI_AtVector(&docs->_value._objects, j));

      // this will transfer the ownership for the JSON into "documents"
      documents.transfer(doc);
    }
  }

  result("documents", documents);

  resultBody = arangodb::basics::JsonHelper::toString(result.json());

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get all edges on coordinator
////////////////////////////////////////////////////////////////////////////////

int getAllEdgesOnCoordinator(
    std::string const& dbname, std::string const& collname,
    std::string const& vertex, TRI_edge_direction_e const& direction,
    arangodb::rest::HttpResponse::HttpResponseCode& responseCode,
    std::string& contentType, std::string& resultBody) {
  arangodb::basics::Json result(arangodb::basics::Json::Object);
  std::vector<traverser::TraverserExpression*> expTmp;
  int res =
      getFilteredEdgesOnCoordinator(dbname, collname, vertex, direction, expTmp,
                                    responseCode, contentType, result);
  resultBody = arangodb::basics::JsonHelper::toString(result.json());
  return res;
}

int getFilteredEdgesOnCoordinator(
    std::string const& dbname, std::string const& collname,
    std::string const& vertex, TRI_edge_direction_e const& direction,
    std::vector<traverser::TraverserExpression*> const& expressions,
    arangodb::rest::HttpResponse::HttpResponseCode& responseCode,
    std::string& contentType, arangodb::basics::Json& result) {
  TRI_ASSERT(result.isObject());
  TRI_ASSERT(result.members() == 0);

  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();

  // First determine the collection ID from the name:
  std::shared_ptr<CollectionInfo> collinfo =
      ci->getCollection(dbname, collname);
  if (collinfo->empty()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  auto shards = collinfo->shardIds();
  CoordTransactionID coordTransactionID = TRI_NewTickServer();
  std::string queryParameters = "?vertex=" + StringUtils::urlEncode(vertex);
  if (direction == TRI_EDGE_IN) {
    queryParameters += "&direction=in";
  } else if (direction == TRI_EDGE_OUT) {
    queryParameters += "&direction=out";
  }
  auto reqBodyString = std::make_shared<std::string>();
  if (!expressions.empty()) {
    arangodb::basics::Json body(Json::Array, expressions.size());
    for (auto& e : expressions) {
      arangodb::basics::Json tmp(Json::Object);
      e->toJson(tmp, TRI_UNKNOWN_MEM_ZONE);
      body.add(tmp.steal());
    }
    reqBodyString->append(body.toString());
  }
  for (auto const& p : *shards) {
    std::unique_ptr<std::map<std::string, std::string>> headers(
        new std::map<std::string, std::string>());
    cc->asyncRequest("", coordTransactionID, "shard:" + p.first,
                     arangodb::rest::HttpRequest::HTTP_REQUEST_PUT,
                     "/_db/" + StringUtils::urlEncode(dbname) + "/_api/edges/" +
                         p.first + queryParameters,
                     reqBodyString, headers, nullptr, 3600.0);
  }
  // Now listen to the results:
  int count;
  responseCode = arangodb::rest::HttpResponse::OK;
  contentType = "application/json; charset=utf-8";
  size_t filtered = 0;
  size_t scannedIndex = 0;

  arangodb::basics::Json documents(arangodb::basics::Json::Array);

  for (count = (int)shards->size(); count > 0; count--) {
    auto res = cc->wait("", coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_TIMEOUT) {
      cc->drop("", coordTransactionID, 0, "");
      return TRI_ERROR_CLUSTER_TIMEOUT;
    }
    if (res.status == CL_COMM_ERROR || res.status == CL_COMM_DROPPED) {
      cc->drop("", coordTransactionID, 0, "");
      return TRI_ERROR_INTERNAL;
    }
    if (res.status == CL_COMM_RECEIVED) {
    }

    std::unique_ptr<TRI_json_t> shardResult(
        TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, res.answer->body()));

    if (shardResult == nullptr || !TRI_IsObjectJson(shardResult.get())) {
      return TRI_ERROR_INTERNAL;
    }

    bool const isError = arangodb::basics::JsonHelper::checkAndGetBooleanValue(
        shardResult.get(), "error");
    if (isError) {
      // shared returned an error
      return arangodb::basics::JsonHelper::getNumericValue<int>(
          shardResult.get(), "errorNum", TRI_ERROR_INTERNAL);
    }

    auto docs = TRI_LookupObjectJson(shardResult.get(), "edges");

    if (!TRI_IsArrayJson(docs)) {
      return TRI_ERROR_INTERNAL;
    }

    size_t const n = TRI_LengthArrayJson(docs);
    documents.reserve(n);

    for (size_t j = 0; j < n; ++j) {
      auto doc =
          static_cast<TRI_json_t*>(TRI_AtVector(&docs->_value._objects, j));

      // this will transfer the ownership for the JSON into "documents"
      documents.transfer(doc);
    }

    TRI_json_t* stats = arangodb::basics::JsonHelper::getObjectElement(
        shardResult.get(), "stats");
    // We do not own stats, do not delete it.

    if (stats != nullptr) {
      filtered += arangodb::basics::JsonHelper::getNumericValue<size_t>(
          stats, "filtered", 0);
      scannedIndex += arangodb::basics::JsonHelper::getNumericValue<size_t>(
          stats, "scannedIndex", 0);
    }
  }

  result("edges", documents);

  arangodb::basics::Json stats(arangodb::basics::Json::Object, 2);
  stats("scannedIndex",
        arangodb::basics::Json(static_cast<int32_t>(scannedIndex)));
  stats("filtered", arangodb::basics::Json(static_cast<int32_t>(filtered)));
  result("stats", stats);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief modify a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int modifyDocumentOnCoordinator(
    std::string const& dbname, std::string const& collname,
    std::string const& key, TRI_voc_rid_t const rev,
    TRI_doc_update_policy_e policy, bool waitForSync, bool isPatch,
    bool keepNull,      // only counts for isPatch == true
    bool mergeObjects,  // only counts for isPatch == true
    VPackSlice const& slice,
    std::unique_ptr<std::map<std::string, std::string>>& headers,
    arangodb::rest::HttpResponse::HttpResponseCode& responseCode,
    std::map<std::string, std::string>& resultHeaders,
    std::string& resultBody) {
  std::unique_ptr<TRI_json_t> json(
      arangodb::basics::VelocyPackHelper::velocyPackToJson(slice));
  return modifyDocumentOnCoordinator(
      dbname, collname, key, rev, policy, waitForSync, isPatch, keepNull,
      mergeObjects, json, headers, responseCode, resultHeaders, resultBody);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief modify a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int modifyDocumentOnCoordinator(
    std::string const& dbname, std::string const& collname,
    std::string const& key, TRI_voc_rid_t const rev,
    TRI_doc_update_policy_e policy, bool waitForSync, bool isPatch,
    bool keepNull,      // only counts for isPatch == true
    bool mergeObjects,  // only counts for isPatch == true
    std::unique_ptr<TRI_json_t>& json,
    std::unique_ptr<std::map<std::string, std::string>>& headers,
    arangodb::rest::HttpResponse::HttpResponseCode& responseCode,
    std::map<std::string, std::string>& resultHeaders,
    std::string& resultBody) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();

  // First determine the collection ID from the name:
  std::shared_ptr<CollectionInfo> collinfo =
      ci->getCollection(dbname, collname);
  if (collinfo->empty()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }
  std::string collid = StringUtils::itoa(collinfo->id());

  // We have a fast path and a slow path. The fast path only asks one shard
  // to do the job and the slow path asks them all and expects to get
  // "not found" from all but one shard. We have to cover the following
  // cases:
  //   isPatch == false    (this is a "replace" operation)
  //     Here, the complete new document is given, we assume that we
  //     can read off the responsible shard, therefore can use the fast
  //     path, this is always true if _key is the one and only sharding
  //     attribute, however, if there is any other sharding attribute,
  //     it is possible that the user has changed the values in any of
  //     them, in that case we will get a "not found" or a "sharding
  //     attributes changed answer" in the fast path. In the latter case
  //     we have to delegate to the slow path.
  //   isPatch == true     (this is an "update" operation)
  //     In this case we might or might not have all sharding attributes
  //     specified in the partial document given. If _key is the one and
  //     only sharding attribute, it is always given, if not all sharding
  //     attributes are explicitly given (at least as value `null`), we must
  //     assume that the fast path cannot be used. If all sharding attributes
  //     are given, we first try the fast path, but might, as above,
  //     have to use the slow path after all.

  bool usesDefaultShardingAttributes;
  ShardID shardID;

  int error = ci->getResponsibleShard(collid, json.get(), !isPatch, shardID,
                                      usesDefaultShardingAttributes);

  if (error == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
    return error;
  }

  // Some stuff to prepare cluster-internal requests:
  std::string revstr;
  if (rev != 0) {
    revstr = "&rev=" + StringUtils::itoa(rev);
  }
  arangodb::rest::HttpRequest::HttpRequestType reqType;
  if (isPatch) {
    reqType = arangodb::rest::HttpRequest::HTTP_REQUEST_PATCH;
    if (!keepNull) {
      revstr += "&keepNull=false";
    }
    if (mergeObjects) {
      revstr += "&mergeObjects=true";
    } else {
      revstr += "&mergeObjects=false";
    }
  } else {
    reqType = arangodb::rest::HttpRequest::HTTP_REQUEST_PUT;
  }

  std::string policystr;
  if (policy == TRI_DOC_UPDATE_LAST_WRITE) {
    policystr = "&policy=last";
  }

  auto body = std::make_shared<std::string const>(
      std::string(JsonHelper::toString(json.get())));

  if (!isPatch ||
      error != TRI_ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN) {
    // This is the fast method, we only have to ask one shard, unless
    // the we are in isPatch==false and the user has actually changed the
    // sharding attributes

    // Send a synchronous request to that shard using ClusterComm:
    auto res = cc->syncRequest(
        "", TRI_NewTickServer(), "shard:" + shardID, reqType,
        "/_db/" + StringUtils::urlEncode(dbname) + "/_api/document/" +
            StringUtils::urlEncode(shardID) + "/" +
            StringUtils::urlEncode(key) + "?waitForSync=" +
            (waitForSync ? "true" : "false") + revstr + policystr,
        *(body.get()), *headers, 60.0);

    if (res->status == CL_COMM_TIMEOUT) {
      // No reply, we give up:
      return TRI_ERROR_CLUSTER_TIMEOUT;
    }
    if (res->status == CL_COMM_ERROR) {
      // This could be a broken connection or an Http error:
      if (res->result == nullptr || !res->result->isComplete()) {
        return TRI_ERROR_CLUSTER_CONNECTION_LOST;
      }
      // In this case a proper HTTP error was reported by the DBserver,
      // this can be 400 or 404, we simply forward the result.
      // We intentionally fall through here.
    }
    // Now we have to distinguish whether we still have to go the slow way:
    responseCode = static_cast<arangodb::rest::HttpResponse::HttpResponseCode>(
        res->result->getHttpReturnCode());
    if (responseCode < arangodb::rest::HttpResponse::BAD) {
      // OK, we are done, let's report:
      resultHeaders = res->result->getHeaderFields();
      resultBody.assign(res->result->getBody().c_str(),
                        res->result->getBody().length());
      return TRI_ERROR_NO_ERROR;
    }
  }

  // If we get here, we have to do it the slow way and contact everybody:
  auto shards = collinfo->shardIds();
  CoordTransactionID coordTransactionID = TRI_NewTickServer();
  for (auto const& p : *shards) {
    std::unique_ptr<std::map<std::string, std::string>> headersCopy(
        new std::map<std::string, std::string>(*headers));
    cc->asyncRequest("", coordTransactionID, "shard:" + p.first, reqType,
                     "/_db/" + StringUtils::urlEncode(dbname) +
                         "/_api/document/" + StringUtils::urlEncode(p.first) +
                         "/" + StringUtils::urlEncode(key) + "?waitForSync=" +
                         (waitForSync ? "true" : "false") + revstr + policystr,
                     body, headersCopy, nullptr, 60.0);
  }
  // Now listen to the results:
  int count;
  int nrok = 0;
  for (count = (int)shards->size(); count > 0; count--) {
    auto res = cc->wait("", coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code != arangodb::rest::HttpResponse::NOT_FOUND ||
          (nrok == 0 && count == 1)) {
        nrok++;
        responseCode = res.answer_code;
        resultHeaders = res.answer->headers();
        resultBody = std::string(res.answer->body(), res.answer->bodySize());
      }
    }
  }

  // Note that nrok is always at least 1!
  if (nrok > 1) {
    return TRI_ERROR_CLUSTER_GOT_CONTRADICTING_ANSWERS;
  }
  return TRI_ERROR_NO_ERROR;  // the cluster operation was OK, however,
                              // the DBserver could have reported an error.
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an edge in a coordinator
////////////////////////////////////////////////////////////////////////////////

int createEdgeOnCoordinator(
    std::string const& dbname, std::string const& collname, bool waitForSync,
    std::unique_ptr<TRI_json_t>& json, char const* from, char const* to,
    arangodb::rest::HttpResponse::HttpResponseCode& responseCode,
    std::map<std::string, std::string>& resultHeaders,
    std::string& resultBody) {
  // Set a few variables needed for our work:
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();

  // First determine the collection ID from the name:
  std::shared_ptr<CollectionInfo> collinfo =
      ci->getCollection(dbname, collname);
  if (collinfo->empty()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }
  std::string collid = StringUtils::itoa(collinfo->id());

  // Sort out the _key attribute:
  // The user is allowed to specify _key, provided that _key is the one
  // and only sharding attribute, because in this case we can delegate
  // the responsibility to make _key attributes unique to the responsible
  // shard. Otherwise, we ensure uniqueness here and now by taking a
  // cluster-wide unique number. Note that we only know the sharding
  // attributes a bit further down the line when we have determined
  // the responsible shard.
  TRI_json_t* subjson = TRI_LookupObjectJson(json.get(), "_key");
  bool userSpecifiedKey = false;
  std::string _key;
  if (subjson == nullptr) {
    // The user did not specify a key, let's create one:
    uint64_t uid = ci->uniqid();
    _key = arangodb::basics::StringUtils::itoa(uid);
    TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, json.get(), "_key",
                          TRI_CreateStringReferenceJson(
                              TRI_UNKNOWN_MEM_ZONE, _key.c_str(), _key.size()));
  } else {
    userSpecifiedKey = true;
  }

  // Now find the responsible shard:
  bool usesDefaultShardingAttributes;
  ShardID shardID;
  int error = ci->getResponsibleShard(collid, json.get(), true, shardID,
                                      usesDefaultShardingAttributes);
  if (error == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
    return TRI_ERROR_CLUSTER_SHARD_GONE;
  }

  // Now perform the above mentioned check:
  if (userSpecifiedKey && !usesDefaultShardingAttributes) {
    return TRI_ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY;
  }

  std::string body = JsonHelper::toString(json.get());

  // Send a synchronous request to that shard using ClusterComm:
  std::map<std::string, std::string> headers;
  auto res = cc->syncRequest(
      "", TRI_NewTickServer(), "shard:" + shardID,
      arangodb::rest::HttpRequest::HTTP_REQUEST_POST,
      "/_db/" + dbname + "/_api/edge?collection=" +
          StringUtils::urlEncode(shardID) + "&waitForSync=" +
          (waitForSync ? "true" : "false") + "&from=" +
          StringUtils::urlEncode(from) + "&to=" + StringUtils::urlEncode(to),
      body, headers, 60.0);

  if (res->status == CL_COMM_TIMEOUT) {
    // No reply, we give up:
    return TRI_ERROR_CLUSTER_TIMEOUT;
  }
  if (res->status == CL_COMM_ERROR) {
    // This could be a broken connection or an Http error:
    if (res->result == nullptr || !res->result->isComplete()) {
      // there is not result
      return TRI_ERROR_CLUSTER_CONNECTION_LOST;
    }
    // In this case a proper HTTP error was reported by the DBserver,
    // this can be 400 or 404, we simply forward the result.
    // We intentionally fall through here.
  }
  responseCode = static_cast<arangodb::rest::HttpResponse::HttpResponseCode>(
      res->result->getHttpReturnCode());
  resultHeaders = res->result->getHeaderFields();
  resultBody.assign(res->result->getBody().c_str(),
                    res->result->getBody().length());
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flush Wal on all DBservers
////////////////////////////////////////////////////////////////////////////////

int flushWalOnAllDBServers(bool waitForSync, bool waitForCollector) {
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();
  std::vector<ServerID> DBservers = ci->getCurrentDBServers();
  CoordTransactionID coordTransactionID = TRI_NewTickServer();
  std::string url = std::string("/_admin/wal/flush?waitForSync=") +
                    (waitForSync ? "true" : "false") + "&waitForCollector=" +
                    (waitForCollector ? "true" : "false");
  auto body = std::make_shared<std::string const>();
  for (auto it = DBservers.begin(); it != DBservers.end(); ++it) {
    std::unique_ptr<std::map<std::string, std::string>> headers(
        new std::map<std::string, std::string>());
    // set collection name (shard id)
    cc->asyncRequest("", coordTransactionID, "server:" + *it,
                     arangodb::rest::HttpRequest::HTTP_REQUEST_PUT, url, body,
                     headers, nullptr, 120.0);
  }

  // Now listen to the results:
  int count;
  int nrok = 0;
  for (count = (int)DBservers.size(); count > 0; count--) {
    auto res = cc->wait("", coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code == arangodb::rest::HttpResponse::OK) {
        nrok++;
      }
    }
  }

  if (nrok != (int)DBservers.size()) {
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

}  // namespace arangodb
