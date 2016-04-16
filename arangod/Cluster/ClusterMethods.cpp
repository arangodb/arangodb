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


  ShardID shardID;
  bool userSpecifiedKey = false;
  std::string _key = "";

  if (!node.isObject()) {
    // We have invalid input at this point.
    // However we can work with the other babies.
    // This is for compatibility with single server
    // We just asign it to any shard and pretend the user has given a key
    std::shared_ptr<std::vector<ShardID>> shards = ci->getShardList(collid);
    shardID = shards->at(0);
    userSpecifiedKey = true;
  } else {

    // Sort out the _key attribute:
    // The user is allowed to specify _key, provided that _key is the one
    // and only sharding attribute, because in this case we can delegate
    // the responsibility to make _key attributes unique to the responsible
    // shard. Otherwise, we ensure uniqueness here and now by taking a
    // cluster-wide unique number. Note that we only know the sharding
    // attributes a bit further down the line when we have determined
    // the responsible shard.

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
  }

  // We found the responsible shard. Add it to the list.
  auto it = shardMap.find(shardID);
  if (it == shardMap.end()) {
    std::vector<std::pair<VPackValueLength, std::string>> counter(
        {{index, _key}});
    shardMap.emplace(shardID, counter);
    reverseMapping.emplace_back(shardID, 0);
  } else {
    it->second.emplace_back(index, _key);
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
    std::unordered_map<ShardID, std::shared_ptr<VPackBuilder>>& resultMap,
    GeneralResponse::ResponseCode& responseCode) {
  size_t count;
  // If none of the shards responds we return a SERVER_ERROR;
  responseCode = GeneralResponse::ResponseCode::SERVER_ERROR;
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
        responseCode = res.answer_code;
      }
    }
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
      shardMap, cc, coordTransactionID, errorCounter, resultMap, responseCode);

  responseCode =
      (options.waitForSync ? GeneralResponse::ResponseCode::CREATED
                           : GeneralResponse::ResponseCode::ACCEPTED);
  mergeResults(reverseMapping, resultMap, resultBody);

  // the cluster operation was OK, however,
  // the DBserver could have reported an error.
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
      ShardID shardID;
      if (_key.empty()) {
        // We have invalid input at this point.
        // However we can work with the other babies.
        // This is for compatibility with single server
        // We just asign it to any shard and pretend the user has given a key
        std::shared_ptr<std::vector<ShardID>> shards = ci->getShardList(collid);
        shardID = shards->at(0);
      } else {
        // Now find the responsible shard:
        bool usesDefaultShardingAttributes;
        int error = ci->getResponsibleShard(
            collid, arangodb::basics::VelocyPackHelper::EmptyObjectValue(), true,
            shardID, usesDefaultShardingAttributes, _key);

        if (error == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
          return TRI_ERROR_CLUSTER_SHARD_GONE;
        }
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
        shardMap, cc, coordTransactionID, errorCounter, resultMap, responseCode);
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
  // If no server responds we return 500
  responseCode = GeneralResponse::ResponseCode::SERVER_ERROR;
  for (size_t i = 0; i < shardList->size(); ++i) {
    auto res = cc->wait("", coordTransactionID, 0, "", 0.0);
    int error = handleGeneralCommErrors(&res);
    if (error != TRI_ERROR_NO_ERROR) {
      // Cluster is in bad state. Just report. Drop other results.
      cc->drop("", coordTransactionID, 0, "");
      // Local data structores are automatically freed
      return error;
    }
    if (res.answer_code == GeneralResponse::ResponseCode::OK ||
        res.answer_code == GeneralResponse::ResponseCode::ACCEPTED) {
      responseCode = res.answer_code;
    }
    TRI_ASSERT(res.answer != nullptr);
    allResults.emplace_back(res.answer->toVelocyPack(&VPackOptions::Defaults));
    extractErrorCodes(res, errorCounter, false);
  }
  // If we get here we get exactly one result for every shard.
  TRI_ASSERT(allResults.size() == shardList->size());
  mergeResultsAllShards(allResults, resultBody, errorCounter, static_cast<size_t>(slice.length()));
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
    VPackSlice const slice, OperationOptions const& options,
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

  // If _key is the one and only sharding attribute, we can do this quickly,
  // because we can easily determine which shard is responsible for the
  // document. Otherwise we have to contact all shards and ask them to
  // delete the document. All but one will not know it.
  // Now find the responsible shard(s)
  
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
      canUseFastPath = false;
    }
  }

  // Some stuff to prepare cluster-internal requests:

  std::string baseUrl = "/_db/" + StringUtils::urlEncode(dbname) + "/_api/document/";
  std::string optsUrlPart = std::string("?ignoreRevs=") + (options.ignoreRevs ? "true" : "false");
 
  arangodb::GeneralRequest::RequestType reqType;
  if (!useMultiple) {
    if (options.silent) {
      reqType = arangodb::GeneralRequest::RequestType::HEAD;
    } else {
      reqType = arangodb::GeneralRequest::RequestType::GET;
    }
  } else {
    reqType = arangodb::GeneralRequest::RequestType::PUT;
    if (options.silent) {
      optsUrlPart += std::string("&silent=true");
    }
    optsUrlPart += std::string("&onlyget=true");
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
        if (!options.ignoreRevs && slice.hasKey(TRI_VOC_ATTRIBUTE_REV)) {
          headersCopy->emplace("if-match", slice.get(TRI_VOC_ATTRIBUTE_REV).copyString());
        }

        // We send to single endpoint
        cc->asyncRequest(
            "", coordTransactionID, "shard:" + it.first, reqType,
            baseUrl + StringUtils::urlEncode(it.first) + "/" +
                StringUtils::urlEncode(
                    slice.get(TRI_VOC_ATTRIBUTE_KEY).copyString()) +
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
        shardMap, cc, coordTransactionID, errorCounter, resultMap, responseCode);

    mergeResults(reverseMapping, resultMap, resultBody);

    // the cluster operation was OK, however,
    // the DBserver could have reported an error.
    return TRI_ERROR_NO_ERROR;
  }

  // Not all shard keys are known in all documents.
  // We contact all shards with the complete body and ignore NOT_FOUND

  auto shardList = ci->getShardList(collid);
  if (!useMultiple) {

    if (!options.ignoreRevs && slice.hasKey(TRI_VOC_ATTRIBUTE_REV)) {
      headers->emplace("if-match", slice.get(TRI_VOC_ATTRIBUTE_REV).copyString());
    }
    for (auto const& shard : *shardList) {
      auto headersCopy =
          std::make_unique<std::map<std::string, std::string>>(*headers);
      cc->asyncRequest("", coordTransactionID, "shard:" + shard, reqType,
                       baseUrl + StringUtils::urlEncode(shard) + "/" +
                           StringUtils::urlEncode(
                               slice.get(TRI_VOC_ATTRIBUTE_KEY).copyString()) +
                           optsUrlPart,
                       nullptr, headersCopy, nullptr, 60.0);
    }
  } else {
    auto body = std::make_shared<std::string>(std::move(slice.toJson()));
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
  // If no server responds we return 500
  responseCode = GeneralResponse::ResponseCode::SERVER_ERROR;
  for (size_t i = 0; i < shardList->size(); ++i) {
    auto res = cc->wait("", coordTransactionID, 0, "", 0.0);
    int error = handleGeneralCommErrors(&res);
    if (error != TRI_ERROR_NO_ERROR) {
      // Cluster is in bad state. Just report. Drop other results.
      cc->drop("", coordTransactionID, 0, "");
      // Local data structores are automatically freed
      return error;
    }
    if (res.answer_code == GeneralResponse::ResponseCode::OK ||
        res.answer_code == GeneralResponse::ResponseCode::ACCEPTED) {
      responseCode = res.answer_code;
    }
    TRI_ASSERT(res.answer != nullptr);
    allResults.emplace_back(res.answer->toVelocyPack(&VPackOptions::Defaults));
    extractErrorCodes(res, errorCounter, false);
  }
  // If we get here we get exactly one result for every shard.
  TRI_ASSERT(allResults.size() == shardList->size());
  mergeResultsAllShards(allResults, resultBody, errorCounter, static_cast<size_t>(slice.length()));
  return TRI_ERROR_NO_ERROR;
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
  VPackBuilder bodyBuilder;
  for (auto const& shard : shardRequestMap) {
    std::unique_ptr<std::map<std::string, std::string>> headersCopy(
        new std::map<std::string, std::string>(*headers));
    bodyBuilder.clear();
    bodyBuilder.openObject();
    bodyBuilder.add("collection", VPackValue(shard.first));
    bodyBuilder.add("keys", VPackValue(VPackValueType::Array));
    for (auto const& key : shard.second) {
      bodyBuilder.add(VPackValue(key));
    }
    bodyBuilder.close(); // keys
    if (!expressions.empty()) {
      bodyBuilder.add("filter", VPackValue(VPackValueType::Array));
      for (auto const& e : expressions) {
        e->toVelocyPack(bodyBuilder);
      }
    }
    bodyBuilder.close(); // filter
    bodyBuilder.close(); // Object

    auto bodyString = std::make_shared<std::string>(bodyBuilder.toJson());

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

      std::shared_ptr<VPackBuilder> resultBody = res.answer->toVelocyPack(&VPackOptions::Defaults);
      VPackSlice resSlice = resultBody->slice();

      if (!resSlice.isObject()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL, "Received an invalid result in cluster.");
      }
      bool isError = arangodb::basics::VelocyPackHelper::getBooleanValue(
          resSlice, "error", false);
      if (isError) {
        int errorNum = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
            resSlice, "errorNum", TRI_ERROR_INTERNAL);
        std::string message =
            arangodb::basics::VelocyPackHelper::getStringValue(
                resSlice, "errorMessage", "");
        THROW_ARANGO_EXCEPTION_MESSAGE(errorNum, message);
      }
      VPackSlice documents = resSlice.get("documents");
      if (!documents.isArray()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL, "Received an invalid result in cluster.");
      }
      for (auto const& element : VPackArrayIterator(documents)) {
        std::string id = arangodb::basics::VelocyPackHelper::getStringValue(
            element, TRI_VOC_ATTRIBUTE_ID, "");
        VPackBuilder tmp;
        tmp.add(element);
        result.emplace(id, tmp.steal());
      }
      VPackSlice filtered = resSlice.get("filtered");
      if (filtered.isArray()) {
        for (auto const& element : VPackArrayIterator(filtered)) {
          if (element.isString()) {
            std::string id = element.copyString();
            documentIds.erase(id);
          }
        }
      }
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get all edges on coordinator using a Traverser Filter
////////////////////////////////////////////////////////////////////////////////

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
        shardMap, cc, coordTransactionID, errorCounter, resultMap, responseCode);

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

  responseCode = GeneralResponse::ResponseCode::SERVER_ERROR;
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
    if (res.answer_code == GeneralResponse::ResponseCode::OK ||
        res.answer_code == GeneralResponse::ResponseCode::ACCEPTED) {
      responseCode = res.answer_code;
    }
    TRI_ASSERT(res.answer != nullptr);
    allResults.emplace_back(res.answer->toVelocyPack(&VPackOptions::Defaults));
    extractErrorCodes(res, errorCounter, false);
  }
  // If we get here we get exactly one result for every shard.
  TRI_ASSERT(allResults.size() == shardList->size());
  mergeResultsAllShards(allResults, resultBody, errorCounter, static_cast<size_t>(slice.length()));
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
