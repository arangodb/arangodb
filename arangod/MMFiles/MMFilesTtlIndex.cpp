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

#include "MMFilesTtlIndex.h"
#include "Basics/FloatingPoint.h"
#include "Basics/StaticStrings.h"
#include "Transaction/Helpers.h"

#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

MMFilesTtlIndex::MMFilesTtlIndex(
    TRI_idx_iid_t iid,
    arangodb::LogicalCollection& collection,
    arangodb::velocypack::Slice const& info
)
    : MMFilesSkiplistIndex(iid, collection, info),
      _expireAfter(info.get(StaticStrings::IndexExpireAfter).getNumericValue<double>()) {

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // ttl index must always be non-unique, but sparse
  TRI_ASSERT(!info.get(StaticStrings::IndexUnique).getBool()); 
  TRI_ASSERT(info.get(StaticStrings::IndexSparse).getBool()); 
#endif
}

MMFilesTtlIndex::~MMFilesTtlIndex() {}

/// @brief Test if this index matches the definition
bool MMFilesTtlIndex::matchesDefinition(VPackSlice const& info) const {
  // call compare method of parent first
  if (!MMFilesSkiplistIndex::matchesDefinition(info)) {
    return false;
  }
  // compare our own attribute, "expireAfter"
  TRI_ASSERT(info.isObject());
  double const expireAfter = info.get(StaticStrings::IndexExpireAfter).getNumber<double>();
  return FloatingPoint<double>{expireAfter}.AlmostEquals(FloatingPoint<double>{_expireAfter});
}

void MMFilesTtlIndex::toVelocyPack(arangodb::velocypack::Builder& builder,
                                   std::underlying_type<Index::Serialize>::type flags) const {
  builder.openObject();
  Index::toVelocyPack(builder, flags);
  builder.add(
    arangodb::StaticStrings::IndexUnique,
    arangodb::velocypack::Value(_unique)
  );
  builder.add(
    arangodb::StaticStrings::IndexSparse,
    arangodb::velocypack::Value(_sparse)
  );
  builder.add(StaticStrings::IndexExpireAfter, VPackValue(_expireAfter));
  builder.close();
}

/// @brief inserts a document into the index
Result MMFilesTtlIndex::insert(transaction::Methods& trx, 
                               LocalDocumentId const& documentId,
                               velocypack::Slice const& doc,
                               Index::OperationMode mode) {
  double timestamp = getTimestamp(doc);
  if (timestamp < 0) {
    // index attribute not present or invalid. nothing to do 
    return Result();
  }
  transaction::BuilderLeaser leased(&trx);
  leased->openObject();
  leased->add(getAttribute(), VPackValue(timestamp));
  leased->close(); 
  return MMFilesSkiplistIndex::insert(trx, documentId, leased->slice(), mode);
}

/// @brief removes a document from the index
Result MMFilesTtlIndex::remove(transaction::Methods& trx, 
                               LocalDocumentId const& documentId,
                               velocypack::Slice const& doc,
                               Index::OperationMode mode) {
  double timestamp = getTimestamp(doc);
  if (timestamp < 0) {
    // index attribute not present or invalid. nothing to do 
    return Result();
  }
  transaction::BuilderLeaser leased(&trx);
  leased->openObject();
  leased->add(getAttribute(), VPackValue(timestamp));
  leased->close(); 
  return MMFilesSkiplistIndex::remove(trx, documentId, leased->slice(), mode);
}
 
double MMFilesTtlIndex::getTimestamp(arangodb::velocypack::Slice const& doc) const {
  return Index::getTimestamp(doc, getAttribute());
}
