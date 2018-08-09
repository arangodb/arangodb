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

#include "BatchResponses.h"
#include <lib/Basics/StaticStrings.h>

using namespace arangodb;

// TODO either remove this in Methods.cpp, or make it a shared function
static OperationResult emptyResult(OperationOptions const& options) {
  VPackBuilder resultBuilder;
  resultBuilder.openArray();
  resultBuilder.close();
  return OperationResult(Result(), resultBuilder.steal(), nullptr, options);
}

// TODO either remove this in Methods.cpp, or make it a shared function
/// @brief Insert an error reported instead of the new document
static void createBabiesError(VPackBuilder& builder,
                              std::unordered_map<int, size_t>& countErrorCodes,
                              Result const& error, bool silent) {
  if (!silent) {
    builder.openObject();
    builder.add(StaticStrings::Error, VPackValue(true));
    builder.add(StaticStrings::ErrorNum, VPackValue(error.errorNumber()));
    builder.add(StaticStrings::ErrorMessage, VPackValue(error.errorMessage()));
    builder.close();
  }

  auto it = countErrorCodes.find(error.errorNumber());
  if (it == countErrorCodes.end()) {
    countErrorCodes.emplace(error.errorNumber(), 1);
  } else {
    it->second++;
  }
}

static void buildResultDocumentInObject(VPackBuilder& builder,
                                        std::string const& collection,
                                        std::string const& key,
                                        TRI_voc_rid_t rid, TRI_voc_rid_t oldRid,
                                        ManagedDocumentResult const* oldDoc,
                                        ManagedDocumentResult const* newDoc) {
  std::string id;
  id.reserve(collection.size() + key.size() + 1);
  id.append(collection);
  // append / and key part
  id.push_back('/');
  id.append(key);

  builder.add(StaticStrings::IdString, VPackValue(id));
  builder.add(StaticStrings::KeyString, VPackValue(key));

  char ridBuffer[21];
  builder.add(StaticStrings::RevString, TRI_RidToValuePair(rid, &ridBuffer[0]));

  if (oldRid != 0) {
    builder.add("_oldRev", VPackValue(TRI_RidToString(oldRid)));
  }
  if (oldDoc != nullptr) {
    builder.add(VPackValue("old"));
    oldDoc->addToBuilder(builder, true);
  }
  if (newDoc != nullptr) {
    builder.add(VPackValue("new"));
    newDoc->addToBuilder(builder, true);
  }
}

static void buildResultDocument(VPackBuilder& builder,
                                std::string const& collection,
                                std::string const& key, TRI_voc_rid_t rid,
                                TRI_voc_rid_t oldRid,
                                ManagedDocumentResult const* oldDoc,
                                ManagedDocumentResult const* newDoc) {
  builder.openObject();
  buildResultDocumentInObject(builder, collection, key, rid, oldRid, oldDoc,
                              newDoc);
  builder.close();
}

OperationResult batch::RemoveResponse::moveToOperationResult(
    OperationOptions const& options, bool const isBabies) {
  // !isBabies => data.size() == 1
  TRI_ASSERT(isBabies || data.size() == 1);

  if (data.empty()) {
    return emptyResult(options);
  }

  VPackBuilder resultBuilder;
  Result totalResult;
  std::unordered_map<int, size_t> errorCounter{};

  if (!isBabies) {
    TRI_ASSERT(data.size() == 1);
    SingleDocRemoveResponse& response = data.front();
    TRI_DEFER(data.pop_front());

    if (!options.silent && (response.result.ok() ||
                            response.result.is(TRI_ERROR_ARANGO_CONFLICT))) {
      TRI_ASSERT(response.old != nullptr);
      buildResultDocument(resultBuilder, collection, response.key, response.rid,
                          0, options.returnOld ? response.old.get() : nullptr,
                          nullptr);
    }

    totalResult = std::move(response.result);
  } else {
    TRI_ASSERT(!data.empty());

    VPackArrayBuilder guard(&resultBuilder);

    while (!data.empty()) {
      SingleDocRemoveResponse& response = data.front();
      TRI_DEFER(data.pop_front());

      if (response.result.ok()) {
        TRI_ASSERT(response.old != nullptr);

        if (!options.silent) {
          buildResultDocument(
              resultBuilder, collection, response.key, response.rid, 0,
              options.returnOld ? response.old.get() : nullptr, nullptr);
        }

      } else {
        createBabiesError(resultBuilder, errorCounter, response.result,
                          options.silent);
      }

      totalResult = std::move(response.result);
    }
  }

  TRI_ASSERT(data.empty());

  TRI_ASSERT(resultBuilder.isClosed());

  return OperationResult{std::move(totalResult), resultBuilder.steal(), nullptr,
                         options, errorCounter};
}
