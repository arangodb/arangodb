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

#include "RocksDBPrimaryIndex.h"
#include "Aql/AstNode.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Transaction/Context.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

/// @brief hard-coded vector of the index attributes
/// note that the attribute names must be hard-coded here to avoid an init-order
/// fiasco with StaticStrings::FromString etc.
static std::vector<std::vector<arangodb::basics::AttributeName>> const IndexAttributes
    {{arangodb::basics::AttributeName("_id", false)},
     {arangodb::basics::AttributeName("_key", false)}};

RocksDBPrimaryIndexIterator::RocksDBPrimaryIndexIterator(LogicalCollection* collection,
                       transaction::Methods* trx, 
                       ManagedDocumentResult* mmdr,
                       RocksDBPrimaryIndex const* index,
                       std::unique_ptr<VPackBuilder>& keys)
    : IndexIterator(collection, trx, mmdr, index),
      _index(index), 
      _keys(keys.get()), 
      _iterator(_keys->slice()) {

  keys.release(); // now we have ownership for _keys
  TRI_ASSERT(_keys->slice().isArray());
}

RocksDBPrimaryIndexIterator::~RocksDBPrimaryIndexIterator() {
  if (_keys != nullptr) {
    // return the VPackBuilder to the transaction context 
    _trx->transactionContextPtr()->returnBuilder(_keys.release());
  }
}

bool RocksDBPrimaryIndexIterator::next(TokenCallback const& cb, size_t limit) {
  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return false;
}

void RocksDBPrimaryIndexIterator::reset() { _iterator.reset(); }
  
RocksDBAllIndexIterator::RocksDBAllIndexIterator(LogicalCollection* collection,
                   transaction::Methods* trx, 
                   ManagedDocumentResult* mmdr,
                   RocksDBPrimaryIndex const* index,
                   bool reverse)
    : IndexIterator(collection, trx, mmdr, index), _reverse(reverse), _total(0) {}

bool RocksDBAllIndexIterator::next(TokenCallback const& cb, size_t limit) {
  // TODO
  return false;
}

void RocksDBAllIndexIterator::reset() { 
  // TODO
}
  
RocksDBPrimaryIndex::RocksDBPrimaryIndex(arangodb::LogicalCollection* collection)
    : RocksDBIndex(0, collection,
            std::vector<std::vector<arangodb::basics::AttributeName>>(
                {{arangodb::basics::AttributeName(StaticStrings::KeyString, false)}}),
            true, false) {}

RocksDBPrimaryIndex::~RocksDBPrimaryIndex() {}

/// @brief return the number of documents from the index
size_t RocksDBPrimaryIndex::size() const { 
  // TODO
  return 0;
}

/// @brief return the memory usage of the index
size_t RocksDBPrimaryIndex::memory() const {
  return 0; // TODO 
}

/// @brief return a VelocyPack representation of the index
void RocksDBPrimaryIndex::toVelocyPack(VPackBuilder& builder, bool withFigures) const {
  Index::toVelocyPack(builder, withFigures);
  // hard-coded
  builder.add("unique", VPackValue(true));
  builder.add("sparse", VPackValue(false));
}

/// @brief return a VelocyPack representation of the index figures
void RocksDBPrimaryIndex::toVelocyPackFigures(VPackBuilder& builder) const {
  Index::toVelocyPackFigures(builder);
  // TODO: implement
}

int RocksDBPrimaryIndex::insert(transaction::Methods*, TRI_voc_rid_t, VPackSlice const&, bool) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "insert() called for primary index";
#endif
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "insert() called for primary index");
}

int RocksDBPrimaryIndex::remove(transaction::Methods*, TRI_voc_rid_t, VPackSlice const&, bool) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "remove() called for primary index";
#endif
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "remove() called for primary index");
}

/// @brief unload the index data from memory
int RocksDBPrimaryIndex::unload() {
  // nothing to do
  return TRI_ERROR_NO_ERROR;
}

/// @brief checks whether the index supports the condition
bool RocksDBPrimaryIndex::supportsFilterCondition(
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    size_t& estimatedItems, double& estimatedCost) const {

  SimpleAttributeEqualityMatcher matcher(IndexAttributes);
  return matcher.matchOne(this, node, reference, itemsInIndex, estimatedItems,
                          estimatedCost);
}

/// @brief creates an IndexIterator for the given Condition
IndexIterator* RocksDBPrimaryIndex::iteratorForCondition(
    transaction::Methods* trx, 
    ManagedDocumentResult* mmdr,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, bool reverse) const {

  THROW_ARANGO_NOT_YET_IMPLEMENTED();
  return nullptr;
}

/// @brief specializes the condition for use with the index
arangodb::aql::AstNode* RocksDBPrimaryIndex::specializeCondition(
    arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference) const {

  SimpleAttributeEqualityMatcher matcher(IndexAttributes);
  return matcher.specializeOne(this, node, reference);
}

/// @brief request an iterator over all elements in the index in
///        a sequential order.
IndexIterator* RocksDBPrimaryIndex::allIterator(transaction::Methods* trx,
                                         ManagedDocumentResult* mmdr,
                                         bool reverse) const {
  return new RocksDBAllIndexIterator(_collection, trx, mmdr, this, reverse);
}
