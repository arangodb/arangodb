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

#include "RocksDBEdgeIndex.h"
#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringRef.h"
#include "Basics/fasthash.h"
#include "Basics/hashes.h"
#include "Indexes/IndexLookupContext.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Utils/CollectionNameResolver.h"
#include "Transaction/Context.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

/// @brief hard-coded vector of the index attributes
/// note that the attribute names must be hard-coded here to avoid an init-order
/// fiasco with StaticStrings::FromString etc.
static std::vector<std::vector<arangodb::basics::AttributeName>> const
    IndexAttributes{{arangodb::basics::AttributeName("_from", false)},
                    {arangodb::basics::AttributeName("_to", false)}};

RocksDBEdgeIndexIterator::RocksDBEdgeIndexIterator(LogicalCollection* collection, transaction::Methods* trx,
                                     ManagedDocumentResult* mmdr,
                                     arangodb::RocksDBEdgeIndex const* index,
                                     std::unique_ptr<VPackBuilder>& keys)
    : IndexIterator(collection, trx, mmdr, index),
      _keys(keys.get()),
      _iterator(_keys->slice()) {
  keys.release();  // now we have ownership for _keys
}

RocksDBEdgeIndexIterator::~RocksDBEdgeIndexIterator() {
  if (_keys != nullptr) {
    // return the VPackBuilder to the transaction context
    _trx->transactionContextPtr()->returnBuilder(_keys.release());
  }
}


bool RocksDBEdgeIndexIterator::next(TokenCallback const& cb, size_t limit) {
  THROW_ARANGO_NOT_IMPLEMENTED();
}

void RocksDBEdgeIndexIterator::reset() {
  THROW_ARANGO_NOT_IMPLEMENTED();
}
  
RocksDBEdgeIndex::RocksDBEdgeIndex(TRI_idx_iid_t iid, arangodb::LogicalCollection* collection)
    : Index(iid, collection,
            std::vector<std::vector<arangodb::basics::AttributeName>>(
                {{arangodb::basics::AttributeName(StaticStrings::FromString,
                                                  false)},
                 {arangodb::basics::AttributeName(StaticStrings::ToString,
                                                  false)}}),
            false, false) {
  TRI_ASSERT(iid != 0);
}

RocksDBEdgeIndex::~RocksDBEdgeIndex() {}

/// @brief return a selectivity estimate for the index
double RocksDBEdgeIndex::selectivityEstimate(arangodb::StringRef const* attribute) const {
  THROW_ARANGO_NOT_IMPLEMENTED();
  return 0.0;
}

/// @brief return the memory usage for the index
size_t RocksDBEdgeIndex::memory() const {
  THROW_ARANGO_NOT_IMPLEMENTED();
  return 0;
}

/// @brief return a VelocyPack representation of the index
void RocksDBEdgeIndex::toVelocyPack(VPackBuilder& builder, bool withFigures) const {
  Index::toVelocyPack(builder, withFigures);

  // hard-coded
  builder.add("unique", VPackValue(false));
  builder.add("sparse", VPackValue(false));
}

/// @brief return a VelocyPack representation of the index figures
void RocksDBEdgeIndex::toVelocyPackFigures(VPackBuilder& builder) const {
  Index::toVelocyPackFigures(builder);
  // TODO
  THROW_ARANGO_NOT_IMPLEMENTED();
}

int RocksDBEdgeIndex::insert(transaction::Methods* trx, TRI_voc_rid_t revisionId,
                      VPackSlice const& doc, bool isRollback) {
  THROW_ARANGO_NOT_IMPLEMENTED();
  return TRI_ERROR_NO_ERROR;
}

int RocksDBEdgeIndex::remove(transaction::Methods* trx, TRI_voc_rid_t revisionId,
                      VPackSlice const& doc, bool isRollback) {
  THROW_ARANGO_NOT_IMPLEMENTED();
  return TRI_ERROR_NO_ERROR;
}

/// @brief unload the index data from memory
int RocksDBEdgeIndex::unload() {
  // nothing to do here
  return TRI_ERROR_NO_ERROR;
}

/// @brief provides a size hint for the edge index
int RocksDBEdgeIndex::sizeHint(transaction::Methods* trx, size_t size) {
  // nothing to do here
  return TRI_ERROR_NO_ERROR;
}

/// @brief checks whether the index supports the condition
bool RocksDBEdgeIndex::supportsFilterCondition(
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    size_t& estimatedItems, double& estimatedCost) const {
  SimpleAttributeEqualityMatcher matcher(IndexAttributes);
  return matcher.matchOne(this, node, reference, itemsInIndex, estimatedItems,
                          estimatedCost);
}

/// @brief creates an IndexIterator for the given Condition
IndexIterator* RocksDBEdgeIndex::iteratorForCondition(
    transaction::Methods* trx, 
    ManagedDocumentResult* mmdr,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, bool reverse) const {
  THROW_ARANGO_NOT_IMPLEMENTED();
  return nullptr;
}

/// @brief specializes the condition for use with the index
arangodb::aql::AstNode* RocksDBEdgeIndex::specializeCondition(
    arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference) const {
  SimpleAttributeEqualityMatcher matcher(IndexAttributes);
  return matcher.specializeOne(this, node, reference);
}

/// @brief Transform the list of search slices to search values.
///        This will multiply all IN entries and simply return all other
///        entries.
void RocksDBEdgeIndex::expandInSearchValues(VPackSlice const slice,
                                            VPackBuilder& builder) const {
  TRI_ASSERT(slice.isArray());
  builder.openArray();
  for (auto const& side : VPackArrayIterator(slice)) {
    if (side.isNull()) {
      builder.add(side);
    } else {
      TRI_ASSERT(side.isArray());
      builder.openArray();
      for (auto const& item : VPackArrayIterator(side)) {
        TRI_ASSERT(item.isObject());
        if (item.hasKey(StaticStrings::IndexEq)) {
          TRI_ASSERT(!item.hasKey(StaticStrings::IndexIn));
          builder.add(item);
        } else {
          TRI_ASSERT(item.hasKey(StaticStrings::IndexIn));
          VPackSlice list = item.get(StaticStrings::IndexIn);
          TRI_ASSERT(list.isArray());
          for (auto const& it : VPackArrayIterator(list)) {
            builder.openObject();
            builder.add(StaticStrings::IndexEq, it);
            builder.close();
          }
        }
      }
      builder.close();
    }
  }
  builder.close();
}
