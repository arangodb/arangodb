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
#include "Basics/conversions.h"
#include "Basics/json.h"
#include "Basics/json-utilities.h"
#include "Basics/StringUtils.h"
#include "Basics/tri-strings.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Indexes/Index.h"
#include "VocBase/Traverser.h"
#include "VocBase/server.h"

#include <velocypack/Buffer.h>
#include <velocypack/Helpers.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::basics;
using namespace arangodb::rest;

namespace arangodb {

static int handleGeneralCommErrors(ClusterCommResult const* res) {
  if (res->status == CL_COMM_TIMEOUT) {
    // No reply, we give up:
    return TRI_ERROR_CLUSTER_TIMEOUT;
  } else if (res->status == CL_COMM_ERROR) {
    // This could be a broken connection or an Http error:
    if (res->result == nullptr || !res->result->isComplete()) {
      // there is not result
      return TRI_ERROR_CLUSTER_CONNECTION_LOST;
    }
  } else if (res->status == CL_COMM_BACKEND_UNAVAILABLE) {
    return TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE;
  }

  return TRI_ERROR_NO_ERROR;
}

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
/// @brief merge the baby-object results.
///        The shard map contains the ordering of elements, the vector in this
///        Map is expected to be sorted from front to back.
///        The second map contains the answers for each shard.
///        The builder in the third parameter will be cleared and will contain
///        the resulting array. It is guaranteed that the resulting array indexes
///        are equal to the original request ordering before it was destructured
///        for babies.
////////////////////////////////////////////////////////////////////////////////

static void mergeResults(
    std::vector<std::pair<ShardID, VPackValueLength>> const& reverseMapping,
    std::unordered_map<ShardID, std::shared_ptr<VPackBuilder>> const& resultMap,
    std::shared_ptr<VPackBuilder>& resultBody) {
  resultBody->clear();
  resultBody->openArray();
  for (auto const& pair : reverseMapping) {
    VPackSlice arr = resultMap.find(pair.first)->second->slice();
    resultBody->add(arr.at(pair.second));
  }
  resultBody->close();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge the baby-object results. (all shards version)
///        results contians the result from all shards in any order.
///        resultBody will be cleared and contains the merged result after this function
///        errorCounter will correctly compute the NOT_FOUND counter, all other
///        codes remain unmodified.
///        
///        The merge is executed the following way:
///        FOR every expected document we scan iterate over the corresponding response
///        of each shard. If any of them returned sth. different than NOT_FOUND
///        we take this result as correct.
///        If none returned sth different than NOT_FOUND we return NOT_FOUND as well
////////////////////////////////////////////////////////////////////////////////

static void mergeResultsAllShards(
    std::vector<std::shared_ptr<VPackBuilder>> const& results,
    std::shared_ptr<VPackBuilder>& resultBody,
    std::unordered_map<int, size_t>& errorCounter,
    VPackValueLength const expectedResults) {
  // errorCounter is not allowed to contain any NOT_FOUND entry.
  TRI_ASSERT(errorCounter.find(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) == errorCounter.end());
  size_t realNotFound = 0;
  VPackBuilder cmp;
  cmp.openObject();
  cmp.add("error", VPackValue(true));
  cmp.add("errorNum", VPackValue(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND));
  cmp.close();
  VPackSlice notFound = cmp.slice();
  bool foundRes = false;
  resultBody->clear();
  resultBody->openArray();
  for (VPackValueLength currentIndex = 0; currentIndex < expectedResults; ++currentIndex) {
    foundRes = false;
    for (auto const& it: results) {
      VPackSlice oneRes = it->slice();
      TRI_ASSERT(oneRes.isArray());
      oneRes = oneRes.at(currentIndex);
      if (!oneRes.equals(notFound)) {
        // This is the correct result
        // Use it
        resultBody->add(oneRes);
        foundRes = true;
        break;
      }
    }
    if (!foundRes) {
      // Found none, use NOT_FOUND
      resultBody->add(notFound);
      realNotFound++;
    }
  }
  resultBody->close();
  if (realNotFound > 0) {
    errorCounter.emplace(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, realNotFound);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Extract all error baby-style error codes and store them in a map
////////////////////////////////////////////////////////////////////////////////

static void extractErrorCodes(ClusterCommResult const& res,
                              std::unordered_map<int, size_t>& errorCounter,
                              bool includeNotFound) {
  auto resultHeaders = res.answer->headers();
  auto codes = resultHeaders.find("X-Arango-Error-Codes");
  if (codes != resultHeaders.end()) {
    auto parsedCodes = VPackParser::fromJson(codes->second);
    VPackSlice codesSlice = parsedCodes->slice();
    TRI_ASSERT(codesSlice.isObject());
    for (auto const& code : VPackObjectIterator(codesSlice)) {
      VPackValueLength codeLength;
      char const* codeString = code.key.getString(codeLength);
      int codeNr = static_cast<int>(
          arangodb::basics::StringUtils::int64(codeString, codeLength));
      if (includeNotFound || codeNr != TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        errorCounter[codeNr] += code.value.getNumericValue<size_t>();
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Distribute one document onto a shard map. If this returns
///        TRI_ERROR_NO_ERROR the correct shard could be determined, if
///        it returns sth. else this document is NOT contained in the shardMap
////////////////////////////////////////////////////////////////////////////////

static int distributeBabyOnShards(
    std::unordered_map<ShardID, std::vector<VPackValueLength>>& shardMap,
    ClusterInfo* ci, std::string const& collid,
    std::shared_ptr<CollectionInfo> collinfo,
    std::vector<std::pair<ShardID, VPackValueLength>>& reverseMapping,
    VPackSlice const node, VPackValueLength const index) {
  // Now find the responsible shard:
  bool usesDefaultShardingAttributes;
  ShardID shardID;
  int error = ci->getResponsibleShard(collid, node, false, shardID,
                                      usesDefaultShardingAttributes);
  if (error == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
    return TRI_ERROR_CLUSTER_SHARD_GONE;
  }
  if (error != TRI_ERROR_NO_ERROR) {
    // We can not find a responsible shard
    return error;
  }

  // We found the responsible shard. Add it to the list.
  auto it = shardMap.find(shardID);
  if (it == shardMap.end()) {
    std::vector<VPackValueLength> counter({index});
    shardMap.emplace(shardID, counter);
    reverseMapping.emplace_back(shardID, 0);
  } else {
    it->second.emplace_back(index);
    reverseMapping.emplace_back(shardID, it->second.size() - 1);
  }
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Distribute one document onto a shard map. If this returns
///        TRI_ERROR_NO_ERROR the correct shard could be determined, if
///        it returns sth. else this document is NOT contained in the shardMap.
///        Also generates a key if necessary.
////////////////////////////////////////////////////////////////////////////////

static int distributeBabyOnShards(
    std::unordered_map<ShardID, std::vector<std::pair<VPackValueLength, std::string>>>& shardMap,
    ClusterInfo* ci, std::string const& collid,
    std::shared_ptr<CollectionInfo> collinfo,
    std::vector<std::pair<ShardID, VPackValueLength>>& reverseMapping,
    VPackSlice const node, VPackValueLength const index) {
  // Sort out the _key attribute:
  // The user is allowed to specify _key, provided that _key is the one
  // and only sharding attribute, because in this case we can delegate
  // the responsibility to make _key attributes unique to the responsible
  // shard. Otherwise, we ensure uniqueness here and now by taking a
  // cluster-wide unique number. Note that we only know the sharding
  // attributes a bit further down the line when we have determined
  // the responsible shard.

  bool userSpecifiedKey = false;
  std::string _key;
  VPackSlice keySlice = node.get(TRI_VOC_ATTRIBUTE_KEY);
  if (keySlice.isNone()) {
    // The user did not specify a key, let's create one:
    uint64_t uid = ci->uniqid();
    _key = arangodb::basics::StringUtils::itoa(uid);
  } else {
    userSpecifiedKey = true;
  }

  // Now find the responsible shard:
  bool usesDefaultShardingAttributes;
  ShardID shardID;
  int error = TRI_ERROR_NO_ERROR;
  if (userSpecifiedKey) {
    error = ci->getResponsibleShard(collid, node, true, shardID,
                                    usesDefaultShardingAttributes);
  } else {
    error = ci->getResponsibleShard(collid, node, true, shardID,
                                    usesDefaultShardingAttributes, _key);
  }
  if (error == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
    return TRI_ERROR_CLUSTER_SHARD_GONE;
  }

  // Now perform the above mentioned check:
  if (userSpecifiedKey &&
      (!usesDefaultShardingAttributes || !collinfo->allowUserKeys())) {
    return TRI_ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY;
  }
  // We found the responsible shard. Add it to the list.
  auto it = shardMap.find(shardID);
  if (it == shardMap.end()) {
    if (userSpecifiedKey) {
      std::vector<std::pair<VPackValueLength, std::string>> counter(
          {{index, ""}});
      shardMap.emplace(shardID, counter);
    } else {
      std::vector<std::pair<VPackValueLength, std::string>> counter(
          {{index, _key}});
      shardMap.emplace(shardID, counter);
    }
    reverseMapping.emplace_back(shardID, 0);
  } else {
    if (userSpecifiedKey) {
      it->second.emplace_back(index, "");
    } else {
      it->second.emplace_back(index, _key);
    }
    reverseMapping.emplace_back(shardID, it->second.size() - 1);
  }
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Collect the results from all shards (fastpath variant)
///        All result bodies are stored in resultMap
////////////////////////////////////////////////////////////////////////////////

template <typename T>
static void collectResultsFromAllShards(
    std::unordered_map<ShardID, std::vector<T>> const& shardMap,
    ClusterComm* cc, CoordTransactionID const& coordTransactionID,
    std::unordered_map<int, size_t>& errorCounter,
    std::unordered_map<ShardID, std::shared_ptr<VPackBuilder>>& resultMap) {
  size_t count;
  for (count = shardMap.size(); count > 0; count--) {
    auto res = cc->wait("", coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_RECEIVED) {
      int commError = handleGeneralCommErrors(&res);
      if (commError != TRI_ERROR_NO_ERROR) {
        auto tmpBuilder = std::make_shared<VPackBuilder>();
        auto weSend = shardMap.find(res.shardID);
        TRI_ASSERT(weSend != shardMap.end());  // We send sth there earlier.
        size_t count = weSend->second.size();
        for (size_t i = 0; i < count; ++i) {
          tmpBuilder->openObject();
          tmpBuilder->add("error", VPackValue(true));
          tmpBuilder->add("errorNum", VPackValue(commError));
          tmpBuilder->close();
        }
        resultMap.emplace(res.shardID, tmpBuilder);
      } else {
        TRI_ASSERT(res.answer != nullptr);
        resultMap.emplace(res.shardID,
                          res.answer->toVelocyPack(&VPackOptions::Defaults));
        extractErrorCodes(res, errorCounter, true);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge headers of a DB server response into the current response
////////////////////////////////////////////////////////////////////////////////

void mergeResponseHeaders(GeneralResponse* response,
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
    arangodb::HttpRequest* request) {
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

  result["content-length"] = StringUtils::itoa(request->contentLength());

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a list of attributes have the same values in two vpack
/// documents
////////////////////////////////////////////////////////////////////////////////

bool shardKeysChanged(std::string const& dbname, std::string const& collname,
                      VPackSlice const& oldValue, VPackSlice const& newValue,
                      bool isPatch) {
  if (!oldValue.isObject() || !newValue.isObject()) {
    // expecting two objects. everything else is an error
    return true;
  }

  ClusterInfo* ci = ClusterInfo::instance();
  std::shared_ptr<CollectionInfo> c = ci->getCollection(dbname, collname);
  std::vector<std::string> const& shardKeys = c->shardKeys();

  for (size_t i = 0; i < shardKeys.size(); ++i) {
    if (shardKeys[i] == TRI_VOC_ATTRIBUTE_KEY) {
      continue;
    }

    VPackSlice n = newValue.get(shardKeys[i]);

    if (n.isNone() && isPatch) {
      // attribute not set in patch document. this means no update
      continue;
    }
   
    // a temporary buffer to hold a null value 
    char buffer[1];
    VPackSlice nullValue = arangodb::velocypack::buildNullValue(&buffer[0], sizeof(buffer));

    VPackSlice o = oldValue.get(shardKeys[i]);

    if (o.isNone()) {
      // if attribute is undefined, use "null" instead
      o = nullValue;
    }

    if (n.isNone()) {
      // if attribute is undefined, use "null" instead
      n = nullValue;
    }

    if (arangodb::basics::VelocyPackHelper::compare(n, o, false) != 0) {
      return true;
    }
  }

  return false;
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
        arangodb::GeneralRequest::RequestType::PUT,
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
        if (res.answer_code == arangodb::GeneralResponse::ResponseCode::OK ||
            res.answer_code == arangodb::GeneralResponse::ResponseCode::CREATED) {
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
        arangodb::GeneralRequest::RequestType::GET,
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
      if (res.answer_code == arangodb::GeneralResponse::ResponseCode::OK) {
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
        arangodb::GeneralRequest::RequestType::GET,
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
      if (res.answer_code == arangodb::GeneralResponse::ResponseCode::OK) {
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
            result->_numberIndexes +=
                ExtractFigure<TRI_voc_ssize_t>(figures, "indexes", "count");

            result->_sizeAlive +=
                ExtractFigure<int64_t>(figures, "alive", "size");
            result->_sizeDead +=
                ExtractFigure<int64_t>(figures, "dead", "size");
            result->_sizeIndexes +=
                ExtractFigure<int64_t>(figures, "indexes", "size");

            result->_numberDatafiles +=
                ExtractFigure<TRI_voc_ssize_t>(figures, "datafiles", "count");
            result->_numberJournalfiles +=
                ExtractFigure<TRI_voc_ssize_t>(figures, "journals", "count");
            result->_numberCompactorfiles +=
                ExtractFigure<TRI_voc_ssize_t>(figures, "compactors", "count");

            result->_datafileSize +=
                ExtractFigure<int64_t>(figures, "datafiles", "fileSize");
            result->_journalfileSize +=
                ExtractFigure<int64_t>(figures, "journals", "fileSize");
            result->_compactorfileSize +=
                ExtractFigure<int64_t>(figures, "compactors", "fileSize");

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
        arangodb::GeneralRequest::RequestType::GET,
        "/_db/" + StringUtils::urlEncode(dbname) + "/_api/collection/" +
            StringUtils::urlEncode(p.first) + "/count",
        std::shared_ptr<std::string>(), headers, nullptr, 300.0);
  }
  // Now listen to the results:
  int count;
  int nrok = 0;
  for (count = (int)shards->size(); count > 0; count--) {
    auto res = cc->wait("", coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code == arangodb::GeneralResponse::ResponseCode::OK) {
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
/// @brief creates one or many documents in a coordinator
///
/// In case of many documents (slice is a VPackArray) it will send to each
/// shard all the relevant documents for this shard only.
/// If one of them fails, this error is reported.
/// There is NO guarantee for the stored documents of all other shards, they may
/// be stored or not. All answers of these shards are dropped.
/// If we return with NO_ERROR it is guaranteed that all shards reported success
/// for their documents.
////////////////////////////////////////////////////////////////////////////////

int createDocumentOnCoordinator(
    std::string const& dbname, std::string const& collname,
    arangodb::OperationOptions const& options, VPackSlice const& slice,
    std::map<std::string, std::string> const& headers,
    arangodb::GeneralResponse::ResponseCode& responseCode,
    std::unordered_map<int, size_t>& errorCounter,
    std::shared_ptr<VPackBuilder>& resultBody) {
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
  std::unordered_map<
      ShardID, std::vector<std::pair<VPackValueLength, std::string>>> shardMap;
  std::vector<std::pair<ShardID, VPackValueLength>> reverseMapping;
  bool useMultiple = slice.isArray();

  int res = TRI_ERROR_NO_ERROR;
  if (useMultiple) {
    VPackValueLength length = slice.length();
    for (VPackValueLength idx = 0; idx < length; ++idx) {
      res = distributeBabyOnShards(shardMap, ci, collid, collinfo,
                                   reverseMapping, slice.at(idx), idx);
      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }
    }
  } else {
    res = distributeBabyOnShards(shardMap, ci, collid, collinfo, reverseMapping,
                                 slice, 0);
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  std::string const baseUrl =
      "/_db/" + StringUtils::urlEncode(dbname) + "/_api/document?collection=";

  std::string const optsUrlPart =
      std::string("&waitForSync=") + (options.waitForSync ? "true" : "false") +
      "&returnNew=" + (options.returnNew ? "true" : "false") + "&returnOld=" +
      (options.returnOld ? "true" : "false");

  VPackBuilder reqBuilder;
  CoordTransactionID coordTransactionID = TRI_NewTickServer();

  auto body = std::make_shared<std::string>();
  for (auto const& it : shardMap) {
    if (!useMultiple) {
      TRI_ASSERT(it.second.size() == 1);
      auto idx = it.second.front();
      if (idx.second.empty()) {
        body = std::make_shared<std::string>(std::move(slice.toJson()));
      } else {
        reqBuilder.clear();
        reqBuilder.openObject();
        TRI_SanitizeObject(slice, reqBuilder);
        reqBuilder.add(TRI_VOC_ATTRIBUTE_KEY, VPackValue(idx.second));
        reqBuilder.close();
        body = std::make_shared<std::string>(std::move(reqBuilder.slice().toJson()));
      }
    } else {
      reqBuilder.clear();
      reqBuilder.openArray();
      for (auto const& idx : it.second) {
        if (idx.second.empty()) {
          reqBuilder.add(slice.at(idx.first));
        } else {
          reqBuilder.openObject();
          TRI_SanitizeObject(slice.at(idx.first), reqBuilder);
          reqBuilder.add(TRI_VOC_ATTRIBUTE_KEY, VPackValue(idx.second));
          reqBuilder.close();
        }
      }
      reqBuilder.close();
      body = std::make_shared<std::string>(std::move(reqBuilder.slice().toJson()));
    }
    auto headersCopy =
        std::make_unique<std::map<std::string, std::string>>(headers);
    cc->asyncRequest("", coordTransactionID, "shard:" + it.first,
                     arangodb::GeneralRequest::RequestType::POST,
                     baseUrl + StringUtils::urlEncode(it.first) + optsUrlPart,
                     body, headersCopy, nullptr, 60.0);
  }

  // Now listen to the results:
  if (!useMultiple) {
    auto res = cc->wait("", coordTransactionID, 0, "", 0.0);

    int commError = handleGeneralCommErrors(&res);
    if (commError != TRI_ERROR_NO_ERROR) {
      return commError;
    }

    responseCode = res.answer_code;
    TRI_ASSERT(res.answer != nullptr);
    auto parsedResult = res.answer->toVelocyPack(&VPackOptions::Defaults);
    resultBody.swap(parsedResult);
    return TRI_ERROR_NO_ERROR;
  }

  std::unordered_map<ShardID, std::shared_ptr<VPackBuilder>> resultMap;

  collectResultsFromAllShards<std::pair<VPackValueLength, std::string>>(
      shardMap, cc, coordTransactionID, errorCounter, resultMap);

  responseCode =
      (options.waitForSync ? GeneralResponse::ResponseCode::CREATED
                           : GeneralResponse::ResponseCode::ACCEPTED);
  mergeResults(reverseMapping, resultMap, resultBody);

  // the cluster operation was OK, however,
  // the DBserver could have reported an error.
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a document in a coordinator
////////////////////////////////////////////////////////////////////////////////

int createDocumentOnCoordinator(
    std::string const& dbname, std::string const& collname, arangodb::OperationOptions const& options,
    std::unique_ptr<TRI_json_t>& json,
    std::map<std::string, std::string> const& headers,
    arangodb::GeneralResponse::ResponseCode& responseCode,
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
      arangodb::GeneralRequest::RequestType::POST,
      "/_db/" + StringUtils::urlEncode(dbname) + "/_api/document?collection=" +
          StringUtils::urlEncode(shardID) + "&waitForSync=" +
          (options.waitForSync ? "true" : "false") + "&returnNew=" +
          (options.returnNew ? "true" : "false") + "&returnOld=" +
            (options.returnOld ? "true" : "false"),
      body, headers, 60.0);

  int commError = handleGeneralCommErrors(res.get());
  if (commError != TRI_ERROR_NO_ERROR) {
    return commError;
  }
  responseCode = static_cast<arangodb::GeneralResponse::ResponseCode>(
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
    VPackSlice const slice,
    arangodb::OperationOptions const& options,
    std::unique_ptr<std::map<std::string, std::string>>& headers,
    arangodb::GeneralResponse::ResponseCode& responseCode,
    std::unordered_map<int, size_t>& errorCounter,
    std::shared_ptr<arangodb::velocypack::Builder>& resultBody) {
  // Set a few variables needed for our work:

  std::map<std::string, std::string> resultHeaders;
  ClusterInfo* ci = ClusterInfo::instance();
  ClusterComm* cc = ClusterComm::instance();

  // First determine the collection ID from the name:
  std::shared_ptr<CollectionInfo> collinfo =
      ci->getCollection(dbname, collname);
  if (collinfo->empty()) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }
  bool useDefaultSharding = collinfo->usesDefaultShardKeys();
  std::string collid = StringUtils::itoa(collinfo->id());
  bool useMultiple = slice.isArray();

  std::string const baseUrl =
      "/_db/" + StringUtils::urlEncode(dbname) + "/_api/document/";

  std::string const optsUrlPart =
      std::string("?waitForSync=") + (options.waitForSync ? "true" : "false") +
      "&returnOld=" + (options.returnOld ? "true" : "false") +
      "&ignoreRevs=" + (options.ignoreRevs ? "true" : "false");


  VPackBuilder reqBuilder;
  CoordTransactionID coordTransactionID = TRI_NewTickServer();

  if (useDefaultSharding) {
    // fastpath we know which server is responsible.

    // decompose the input into correct shards.
    // Send the correct documents to the correct shards
    // Merge the results with static merge helper

    std::unordered_map<ShardID, std::vector<VPackValueLength>> shardMap;
    std::vector<std::pair<ShardID, VPackValueLength>> reverseMapping;
    auto workOnOneNode = [&shardMap, &ci, &collid, &collinfo, &reverseMapping](
        VPackSlice const node, VPackValueLength const index) -> int {
      // Sort out the _key attribute and identify the shard responsible for it.

      std::string _key(Transaction::extractKey(node));
      if (_key.empty()) {
        return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
      }
      // Now find the responsible shard:
      bool usesDefaultShardingAttributes;
      ShardID shardID;
      int error = ci->getResponsibleShard(
          collid, arangodb::basics::VelocyPackHelper::EmptyObjectValue(), true,
          shardID, usesDefaultShardingAttributes, _key);

      if (error == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
        return TRI_ERROR_CLUSTER_SHARD_GONE;
      }

      // We found the responsible shard. Add it to the list.
      auto it = shardMap.find(shardID);
      if (it == shardMap.end()) {
        std::vector<VPackValueLength> counter({index});
        shardMap.emplace(shardID, counter);
        reverseMapping.emplace_back(shardID, 0);
      } else {
        it->second.emplace_back(index);
        reverseMapping.emplace_back(shardID, it->second.size() - 1);
      }
      return TRI_ERROR_NO_ERROR;
    };

    if (useMultiple) {
      int res;
      for (VPackValueLength idx = 0; idx < slice.length(); ++idx) {
        res = workOnOneNode(slice.at(idx), idx);
        if (res != TRI_ERROR_NO_ERROR) {
          // Is early abortion correct?
          return res;
        }
      }
    } else {
      int res = workOnOneNode(slice, 0);
      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }
    }

    // We sorted the shards correctly.

    auto body = std::make_shared<std::string>();
    for (auto const& it : shardMap) {
      if (!useMultiple) {
        TRI_ASSERT(it.second.size() == 1);
        body = std::make_shared<std::string>(std::move(slice.toJson()));
      } else {
        reqBuilder.clear();
        reqBuilder.openArray();
        for (auto const& idx : it.second) {
          reqBuilder.add(slice.at(idx));
        }
        reqBuilder.close();
        body = std::make_shared<std::string>(std::move(reqBuilder.slice().toJson()));
      }
      auto headersCopy =
          std::make_unique<std::map<std::string, std::string>>(*headers);
      cc->asyncRequest("", coordTransactionID, "shard:" + it.first,
                       arangodb::GeneralRequest::RequestType::DELETE_REQ,
                       baseUrl + StringUtils::urlEncode(it.first) + optsUrlPart,
                       body, headersCopy, nullptr, 60.0);
    }

    // Now listen to the results:
    if (!useMultiple) {
      auto res = cc->wait("", coordTransactionID, 0, "", 0.0);

      int commError = handleGeneralCommErrors(&res);
      if (commError != TRI_ERROR_NO_ERROR) {
        return commError;
      }

      responseCode = res.answer_code;
      TRI_ASSERT(res.answer != nullptr);
      auto parsedResult = res.answer->toVelocyPack(&VPackOptions::Defaults);
      resultBody.swap(parsedResult);
      return TRI_ERROR_NO_ERROR;
    }

    std::unordered_map<ShardID, std::shared_ptr<VPackBuilder>> resultMap;
    collectResultsFromAllShards<VPackValueLength>(
        shardMap, cc, coordTransactionID, errorCounter, resultMap);
    responseCode =
        (options.waitForSync ? GeneralResponse::ResponseCode::OK
                             : GeneralResponse::ResponseCode::ACCEPTED);
    mergeResults(reverseMapping, resultMap, resultBody);
    return TRI_ERROR_NO_ERROR;  // the cluster operation was OK, however,
                                // the DBserver could have reported an error.
  }

  // slowpath we do not know which server is responsible ask all of them.

  // We simply send the body to all shards and await their results.
  // As soon as we have the results we merge them in the following way:
  // For 1 .. slice.length()
  //    for res : allResults
  //      if res != NOT_FOUND => insert this result. skip other results
  //    end
  //    if (!skipped) => insert NOT_FOUND
 
  auto body = std::make_shared<std::string>(std::move(slice.toJson()));
  auto shardList = ci->getShardList(collid);
  for (auto const& shard : *shardList) {
    auto headersCopy =
        std::make_unique<std::map<std::string, std::string>>(*headers);
    cc->asyncRequest("", coordTransactionID, "shard:" + shard,
                     arangodb::GeneralRequest::RequestType::DELETE_REQ,
                     baseUrl + StringUtils::urlEncode(shard) + optsUrlPart,
                     body, headersCopy, nullptr, 60.0);
  }

  // Now listen to the results:
  if (!useMultiple) {
    // Only one can answer, we react a bit differently
    int count;
    int nrok = 0;
    for (count = (int)shardList->size(); count > 0; count--) {
      auto res = cc->wait("", coordTransactionID, 0, "", 0.0);
      if (res.status == CL_COMM_RECEIVED) {
        if (res.answer_code !=
                arangodb::GeneralResponse::ResponseCode::NOT_FOUND ||
            (nrok == 0 && count == 1)) {
          nrok++;
          responseCode = res.answer_code;
          TRI_ASSERT(res.answer != nullptr);
          auto parsedResult = res.answer->toVelocyPack(&VPackOptions::Defaults);
          resultBody.swap(parsedResult);
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

  // We select all results from all shards an merge them back again.
  std::vector<std::shared_ptr<VPackBuilder>> allResults;
  allResults.reserve(shardList->size());
  for (size_t i = 0; i < shardList->size(); ++i) {
    auto res = cc->wait("", coordTransactionID, 0, "", 0.0);
    int error = handleGeneralCommErrors(&res);
    if (error != TRI_ERROR_NO_ERROR) {
      // Cluster is in bad state. Just report. Drop other results.
      cc->drop("", coordTransactionID, 0, "");
      // Local data structores are automatically freed
      return error;
    }
    TRI_ASSERT(res.answer_code == arangodb::GeneralResponse::ResponseCode::OK);
    TRI_ASSERT(res.answer != nullptr);
    allResults.emplace_back(res.answer->toVelocyPack(&VPackOptions::Defaults));
    extractErrorCodes(res, errorCounter, false);
  }
  // If we get here we get exactly one result for every shard.
  TRI_ASSERT(allResults.size() == shardList->size());
  mergeResultsAllShards(allResults, resultBody, errorCounter, shardList->size());
  return TRI_ERROR_NO_ERROR;
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
                     arangodb::GeneralRequest::RequestType::PUT,
                     "/_db/" + StringUtils::urlEncode(dbname) +
                         "/_api/collection/" + p.first + "/truncate",
                     std::shared_ptr<std::string>(), headers, nullptr,
                     60.0);
  }
  // Now listen to the results:
  unsigned int count;
  unsigned int nrok = 0;
  for (count = (unsigned int)shards->size(); count > 0; count--) {
    auto res = cc->wait("", coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code == arangodb::GeneralResponse::ResponseCode::OK) {
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
    arangodb::GeneralResponse::ResponseCode& responseCode,
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
    headers->emplace("if-match", StringUtils::itoa(rev));
  }
  arangodb::GeneralRequest::RequestType reqType;
  if (generateDocument) {
    reqType = arangodb::GeneralRequest::RequestType::GET;
  } else {
    reqType = arangodb::GeneralRequest::RequestType::HEAD;
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

    int error = handleGeneralCommErrors(res.get());
    if (error != TRI_ERROR_NO_ERROR) {
      return error;
    }
    responseCode = static_cast<arangodb::GeneralResponse::ResponseCode>(
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
      if (res.answer_code != arangodb::GeneralResponse::ResponseCode::NOT_FOUND ||
          (nrok == 0 && count == 1)) {
        nrok++;
        responseCode = res.answer_code;
        resultHeaders = res.answer->headers();
        resultHeaders["content-length"] =
            StringUtils::itoa(res.answer->contentLength());
        resultBody = res.answer->body();
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
    std::unordered_map<std::string, std::shared_ptr<VPackBuffer<uint8_t>>>& result) {
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
                     arangodb::GeneralRequest::RequestType::PUT,
                     "/_db/" + StringUtils::urlEncode(dbname) +
                         "/_api/simple/lookup-by-keys",
                     bodyString, headersCopy, nullptr, 60.0);
  }
  // All requests send, now collect results.
  for (size_t i = 0; i < shardRequestMap.size(); ++i) {
    auto res = cc->wait("", coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_RECEIVED) {
      std::unique_ptr<TRI_json_t> resultBody(
          arangodb::basics::JsonHelper::fromString(res.answer->body()));

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
          TRI_json_t const* element = TRI_LookupArrayJson(documents, k);
          std::string id = arangodb::basics::JsonHelper::checkAndGetStringValue(
              element, TRI_VOC_ATTRIBUTE_ID);
          auto tmpBuilder = basics::JsonHelper::toVelocyPack(element);
          result.emplace(id, tmpBuilder->steal());
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
    arangodb::GeneralResponse::ResponseCode& responseCode,
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
                     arangodb::GeneralRequest::RequestType::GET,
                     "/_db/" + StringUtils::urlEncode(dbname) +
                         "/_api/document?collection=" + p.first + "&type=" +
                         StringUtils::urlEncode(returnType),
                     std::shared_ptr<std::string const>(), headers, nullptr,
                     3600.0);
  }
  // Now listen to the results:
  int count;
  responseCode = arangodb::GeneralResponse::ResponseCode::OK;
  contentType = "application/json; charset=utf-8";

  arangodb::basics::Json result(arangodb::basics::Json::Object);
  arangodb::basics::Json documents(arangodb::basics::Json::Array);

  for (count = (int)shards->size(); count > 0; count--) {
    auto res = cc->wait("", coordTransactionID, 0, "", 0.0);

    LOG(TRACE) << "Response status " << res.status;
    int error = handleGeneralCommErrors(&res);
    if (error != TRI_ERROR_NO_ERROR) {
      cc->drop("", coordTransactionID, 0, "");
      return error;
    }

    if (res.status == CL_COMM_ERROR || res.status == CL_COMM_DROPPED ||
        res.answer_code == arangodb::GeneralResponse::ResponseCode::NOT_FOUND) {
      cc->drop("", coordTransactionID, 0, "");
      return TRI_ERROR_INTERNAL;
    }

    std::unique_ptr<TRI_json_t> shardResult(
        TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, res.answer->body().c_str()));

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
    arangodb::GeneralResponse::ResponseCode& responseCode,
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
    arangodb::GeneralResponse::ResponseCode& responseCode,
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
                     arangodb::GeneralRequest::RequestType::PUT,
                     "/_db/" + StringUtils::urlEncode(dbname) + "/_api/edges/" +
                         p.first + queryParameters,
                     reqBodyString, headers, nullptr, 3600.0);
  }
  // Now listen to the results:
  int count;
  responseCode = arangodb::GeneralResponse::ResponseCode::OK;
  contentType = "application/json; charset=utf-8";
  size_t filtered = 0;
  size_t scannedIndex = 0;

  arangodb::basics::Json documents(arangodb::basics::Json::Array);

  for (count = (int)shards->size(); count > 0; count--) {
    auto res = cc->wait("", coordTransactionID, 0, "", 0.0);
    int error = handleGeneralCommErrors(&res);
    if (error != TRI_ERROR_NO_ERROR) {
      cc->drop("", coordTransactionID, 0, "");
      return error;
    }
    if (res.status == CL_COMM_ERROR || res.status == CL_COMM_DROPPED) {
      cc->drop("", coordTransactionID, 0, "");
      return TRI_ERROR_INTERNAL;
    }

    std::unique_ptr<TRI_json_t> shardResult(
        TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, res.answer->body().c_str()));

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
    VPackSlice const slice,
    arangodb::OperationOptions const& options, bool isPatch,
    std::unique_ptr<std::map<std::string, std::string>>& headers,
    arangodb::GeneralResponse::ResponseCode& responseCode,
    std::unordered_map<int, size_t>& errorCounter,
    std::shared_ptr<VPackBuilder>& resultBody) {
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
  //     attributes changed answer" in the fast path. In the first case
  //     we have to delegate to the slow path.
  //   isPatch == true     (this is an "update" operation)
  //     In this case we might or might not have all sharding attributes
  //     specified in the partial document given. If _key is the one and
  //     only sharding attribute, it is always given, if not all sharding
  //     attributes are explicitly given (at least as value `null`), we must
  //     assume that the fast path cannot be used. If all sharding attributes
  //     are given, we first try the fast path, but might, as above,
  //     have to use the slow path after all.

  ShardID shardID;

  std::unordered_map<ShardID, std::vector<VPackValueLength>> shardMap;
  std::vector<std::pair<ShardID, VPackValueLength>> reverseMapping;
  bool useMultiple = slice.isArray();

  int res = TRI_ERROR_NO_ERROR;
  bool canUseFastPath = true;
  if (useMultiple) {
    VPackValueLength length = slice.length();
    for (VPackValueLength idx = 0; idx < length; ++idx) {
      res = distributeBabyOnShards(shardMap, ci, collid, collinfo,
                                   reverseMapping, slice.at(idx), idx);
      if (res != TRI_ERROR_NO_ERROR) {
        if (!isPatch) {
          return res;
        }
        canUseFastPath = false;
        shardMap.clear();
        reverseMapping.clear();
        break;
      }
    }
  } else {
    res = distributeBabyOnShards(shardMap, ci, collid, collinfo, reverseMapping,
                                 slice, 0);
    if (res != TRI_ERROR_NO_ERROR) {
      if (!isPatch) {
        return res;
      }
      canUseFastPath = false;
    }
  }


  // Some stuff to prepare cluster-internal requests:

  std::string baseUrl = "/_db/" + StringUtils::urlEncode(dbname) + "/_api/document/";
  std::string optsUrlPart = std::string("?waitForSync=") + (options.waitForSync ? "true" : "false");
  optsUrlPart += std::string("&ignoreRevs=") + (options.ignoreRevs ? "true" : "false");

  arangodb::GeneralRequest::RequestType reqType;
  if (isPatch) {
    reqType = arangodb::GeneralRequest::RequestType::PATCH;
    if (!options.keepNull) {
      optsUrlPart += "&keepNull=false";
    }
    if (options.mergeObjects) {
      optsUrlPart += "&mergeObjects=true";
    } else {
      optsUrlPart += "&mergeObjects=false";
    }
  } else {
    reqType = arangodb::GeneralRequest::RequestType::PUT;
  }
  if (options.returnNew) {
    optsUrlPart += "&returnNew=true";
  }

  if (options.returnOld) {
    optsUrlPart += "&returnOld=true";
  }

  CoordTransactionID coordTransactionID = TRI_NewTickServer();
  if (canUseFastPath) {
    // All shard keys are known in all documents.
    // Contact all shards directly with the correct information.

    VPackBuilder reqBuilder;
    auto body = std::make_shared<std::string>();
    for (auto const& it : shardMap) {
      auto headersCopy =
          std::make_unique<std::map<std::string, std::string>>(*headers);
      if (!useMultiple) {
        TRI_ASSERT(it.second.size() == 1);
        body = std::make_shared<std::string>(std::move(slice.toJson()));

        // We send to single endpoint
        cc->asyncRequest("", coordTransactionID, "shard:" + it.first, reqType,
                         baseUrl + StringUtils::urlEncode(it.first) + "/" +
                             slice.get(TRI_VOC_ATTRIBUTE_KEY).copyString() +
                             optsUrlPart,
                         body, headersCopy, nullptr, 60.0);
      } else {
        reqBuilder.clear();
        reqBuilder.openArray();
        for (auto const& idx : it.second) {
          reqBuilder.add(slice.at(idx));
        }
        reqBuilder.close();
        body = std::make_shared<std::string>(std::move(reqBuilder.slice().toJson()));
        // We send to Babies endpoint
        cc->asyncRequest("", coordTransactionID, "shard:" + it.first, reqType,
            baseUrl + StringUtils::urlEncode(it.first) + optsUrlPart,
            body, headersCopy, nullptr, 60.0);
      }
    }

    // Now listen to the results:
    if (!useMultiple) {
      auto res = cc->wait("", coordTransactionID, 0, "", 0.0);

      int commError = handleGeneralCommErrors(&res);
      if (commError != TRI_ERROR_NO_ERROR) {
        return commError;
      }

      responseCode = res.answer_code;
      TRI_ASSERT(res.answer != nullptr);
      auto parsedResult = res.answer->toVelocyPack(&VPackOptions::Defaults);
      resultBody.swap(parsedResult);
      return TRI_ERROR_NO_ERROR;
    }

    std::unordered_map<ShardID, std::shared_ptr<VPackBuilder>> resultMap;
    collectResultsFromAllShards<VPackValueLength>(
        shardMap, cc, coordTransactionID, errorCounter, resultMap);

    responseCode =
        (options.waitForSync ? GeneralResponse::ResponseCode::OK
                             : GeneralResponse::ResponseCode::ACCEPTED);

    mergeResults(reverseMapping, resultMap, resultBody);

    // the cluster operation was OK, however,
    // the DBserver could have reported an error.
    return TRI_ERROR_NO_ERROR;
  }

  // Not all shard keys are known in all documents.
  // We contact all shards with the complete body and ignore NOT_FOUND

  auto body = std::make_shared<std::string>(std::move(slice.toJson()));
  auto shardList = ci->getShardList(collid);
  if (!useMultiple) {
    for (auto const& shard : *shardList) {
      auto headersCopy =
          std::make_unique<std::map<std::string, std::string>>(*headers);
      cc->asyncRequest("", coordTransactionID, "shard:" + shard, reqType,
                       baseUrl + StringUtils::urlEncode(shard) + "/" +
                           slice.get(TRI_VOC_ATTRIBUTE_KEY).copyString() +
                           optsUrlPart,
                       body, headersCopy, nullptr, 60.0);
    }
  } else {
    for (auto const& shard : *shardList) {
      auto headersCopy =
          std::make_unique<std::map<std::string, std::string>>(*headers);
      cc->asyncRequest("", coordTransactionID, "shard:" + shard, reqType,
                       baseUrl + StringUtils::urlEncode(shard) + optsUrlPart,
                       body, headersCopy, nullptr, 60.0);
    }
  }

  // Now listen to the results:
  if (!useMultiple) {
    // Only one can answer, we react a bit differently
    int count;
    int nrok = 0;
    for (count = (int)shardList->size(); count > 0; count--) {
      auto res = cc->wait("", coordTransactionID, 0, "", 0.0);
      if (res.status == CL_COMM_RECEIVED) {
        if (res.answer_code !=
                arangodb::GeneralResponse::ResponseCode::NOT_FOUND ||
            (nrok == 0 && count == 1)) {
          nrok++;
          responseCode = res.answer_code;
          TRI_ASSERT(res.answer != nullptr);
          auto parsedResult = res.answer->toVelocyPack(&VPackOptions::Defaults);
          resultBody.swap(parsedResult);
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

  // We select all results from all shards an merge them back again.
  std::vector<std::shared_ptr<VPackBuilder>> allResults;
  allResults.reserve(shardList->size());
  for (size_t i = 0; i < shardList->size(); ++i) {
    auto res = cc->wait("", coordTransactionID, 0, "", 0.0);
    int error = handleGeneralCommErrors(&res);
    if (error != TRI_ERROR_NO_ERROR) {
      // Cluster is in bad state. Just report. Drop other results.
      cc->drop("", coordTransactionID, 0, "");
      // Local data structores are automatically freed
      return error;
    }
    TRI_ASSERT(res.answer_code == arangodb::GeneralResponse::ResponseCode::OK);
    TRI_ASSERT(res.answer != nullptr);
    allResults.emplace_back(res.answer->toVelocyPack(&VPackOptions::Defaults));
    extractErrorCodes(res, errorCounter, false);
  }
  // If we get here we get exactly one result for every shard.
  TRI_ASSERT(allResults.size() == shardList->size());
  mergeResultsAllShards(allResults, resultBody, errorCounter, shardList->size());
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
                     arangodb::GeneralRequest::RequestType::PUT, url, body,
                     headers, nullptr, 120.0);
  }

  // Now listen to the results:
  int count;
  int nrok = 0;
  for (count = (int)DBservers.size(); count > 0; count--) {
    auto res = cc->wait("", coordTransactionID, 0, "", 0.0);
    if (res.status == CL_COMM_RECEIVED) {
      if (res.answer_code == arangodb::GeneralResponse::ResponseCode::OK) {
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
