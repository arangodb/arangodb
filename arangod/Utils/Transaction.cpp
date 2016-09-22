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

#include "Transaction.h"
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Condition.h"
#include "Aql/SortCondition.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/Timers.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerState.h"
#include "Indexes/EdgeIndex.h"
#include "Indexes/HashIndex.h"
#include "Indexes/PrimaryIndex.h"
#include "Indexes/SkiplistIndex.h"
#include "Logger/Logger.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/OperationCursor.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/TransactionContext.h"
#include "VocBase/DatafileHelper.h"
#include "VocBase/Ditch.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/MasterPointers.h"
#include "VocBase/ticks.h"
#include "Wal/LogfileManager.h"

#ifdef ARANGODB_ENABLE_ROCKSDB
#include "Indexes/RocksDBIndex.h"

#include <rocksdb/utilities/optimistic_transaction_db.h>
#include <rocksdb/utilities/transaction.h>
#endif

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
  
//////////////////////////////////////////////////////////////////////////////
/// @brief Get the field names of the used index
//////////////////////////////////////////////////////////////////////////////

std::vector<std::vector<std::string>> Transaction::IndexHandle::fieldNames() const {
  return _index->fieldNames();
}

//////////////////////////////////////////////////////////////////////////////
/// @brief Only required by traversal should be removed ASAP
//////////////////////////////////////////////////////////////////////////////

bool Transaction::IndexHandle::isEdgeIndex() const {
  return _index->type() == Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief IndexHandle getter method
//////////////////////////////////////////////////////////////////////////////

std::shared_ptr<arangodb::Index> Transaction::IndexHandle::getIndex() const {
  return _index;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief IndexHandle toVelocyPack method passthrough
//////////////////////////////////////////////////////////////////////////////

void Transaction::IndexHandle::toVelocyPack(
    arangodb::velocypack::Builder& builder,
    bool withFigures) const {
  _index->toVelocyPack(builder, withFigures);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief tests if the given index supports the sort condition
//////////////////////////////////////////////////////////////////////////////

static bool indexSupportsSort(Index const* idx, arangodb::aql::Variable const* reference,
                              arangodb::aql::SortCondition const* sortCondition,
                              size_t itemsInIndex,
                              double& estimatedCost,
                              size_t& coveredAttributes) {
  if (idx->isSorted() &&
      idx->supportsSortCondition(sortCondition, reference, itemsInIndex,
                                 estimatedCost, coveredAttributes)) {
    // index supports the sort condition
    return true;
  }

  // index does not support the sort condition
  if (itemsInIndex > 0) {
    estimatedCost = itemsInIndex * std::log2(static_cast<double>(itemsInIndex));
  } else {
    estimatedCost = 0.0;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Return an Operation Result that parses the error information returned
///        by the DBServer.
////////////////////////////////////////////////////////////////////////////////

static OperationResult DBServerResponseBad(std::shared_ptr<VPackBuilder> resultBody) {
  VPackSlice res = resultBody->slice();
  return OperationResult(
      arangodb::basics::VelocyPackHelper::getNumericValue<int>(
          res, "errorNum", TRI_ERROR_INTERNAL),
      arangodb::basics::VelocyPackHelper::getStringValue(
          res, "errorMessage", "JSON sent to DBserver was bad"));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Insert an errror reported instead of the new document
////////////////////////////////////////////////////////////////////////////////

static void createBabiesError(VPackBuilder& builder,
                              std::unordered_map<int, size_t>& countErrorCodes,
                              int errorCode,
                              bool silent) {
  if (!silent) {
    builder.openObject();
    builder.add("error", VPackValue(true));
    builder.add("errorNum", VPackValue(errorCode));
    builder.add("errorMessage", VPackValue(TRI_errno_string(errorCode)));
    builder.close();
  }

  auto it = countErrorCodes.find(errorCode);
  if (it == countErrorCodes.end()) {
    countErrorCodes.emplace(errorCode, 1);
  } else {
    it->second++;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sort ORs for the same attribute so they are in ascending value
/// order. this will only work if the condition is for a single attribute
/// the usedIndexes vector may also be re-sorted
////////////////////////////////////////////////////////////////////////////////

bool Transaction::sortOrs(arangodb::aql::Ast* ast,
                    arangodb::aql::AstNode* root,
                    arangodb::aql::Variable const* variable,
                    std::vector<arangodb::Transaction::IndexHandle>& usedIndexes) const {
  if (root == nullptr) {
    return true;
  }

  size_t const n = root->numMembers();

  if (n < 2) {
    return true;
  }

  if (n != usedIndexes.size()) {
    // sorting will break if the number of ORs is unequal to the number of
    // indexes
    // but we shouldn't have got here then
    TRI_ASSERT(false);
    return false;
  }

  typedef std::pair<arangodb::aql::AstNode*, arangodb::Transaction::IndexHandle> ConditionData;
  std::vector<ConditionData*> conditionData;

  auto cleanup = [&conditionData]() -> void {
    for (auto& it : conditionData) {
      delete it;
    }
  };

  TRI_DEFER(cleanup());

  std::vector<arangodb::aql::ConditionPart> parts;
  parts.reserve(n);

  for (size_t i = 0; i < n; ++i) {
    // sort the conditions of each AND
    auto sub = root->getMemberUnchecked(i);

    TRI_ASSERT(sub != nullptr &&
               sub->type ==
                   arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_AND);
    size_t const nAnd = sub->numMembers();

    if (nAnd != 1) {
      // we can't handle this one
      return false;
    }

    auto operand = sub->getMemberUnchecked(0);

    if (!operand->isComparisonOperator()) {
      return false;
    }

    if (operand->type == arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_NE ||
        operand->type == arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_NIN) {
      return false;
    }

    auto lhs = operand->getMember(0);
    auto rhs = operand->getMember(1);

    if (lhs->type == arangodb::aql::AstNodeType::NODE_TYPE_ATTRIBUTE_ACCESS) {
      std::pair<arangodb::aql::Variable const*, std::vector<arangodb::basics::AttributeName>>
          result;

      if (rhs->isConstant() && lhs->isAttributeAccessForVariable(result) &&
          result.first == variable &&
          (operand->type != arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_IN || rhs->isArray())) {
        // create the condition data struct on the heap
        auto data = std::make_unique<ConditionData>(sub, usedIndexes[i]);
        // push it into an owning vector
        conditionData.emplace_back(data.get());
        // vector is now responsible for data
        auto p = data.release();
        // also add the pointer to the (non-owning) parts vector
        parts.emplace_back(arangodb::aql::ConditionPart(
            result.first, result.second, operand,
            arangodb::aql::AttributeSideType::ATTRIBUTE_LEFT, p));
      }
    }

    if (rhs->type == arangodb::aql::AstNodeType::NODE_TYPE_ATTRIBUTE_ACCESS ||
        rhs->type == arangodb::aql::AstNodeType::NODE_TYPE_EXPANSION) {
      std::pair<arangodb::aql::Variable const*, std::vector<arangodb::basics::AttributeName>>
          result;

      if (lhs->isConstant() && rhs->isAttributeAccessForVariable(result) &&
          result.first == variable) {
        // create the condition data struct on the heap
        auto data = std::make_unique<ConditionData>(sub, usedIndexes[i]);
        // push it into an owning vector
        conditionData.emplace_back(data.get());
        // vector is now responsible for data
        auto p = data.release();
        // also add the pointer to the (non-owning) parts vector
        parts.emplace_back(arangodb::aql::ConditionPart(
            result.first, result.second, operand,
            arangodb::aql::AttributeSideType::ATTRIBUTE_RIGHT, p));
      }
    }
  }

  if (parts.size() != root->numMembers()) {
    return false;
  }

  // check if all parts use the same variable and attribute
  for (size_t i = 1; i < n; ++i) {
    auto& lhs = parts[i - 1];
    auto& rhs = parts[i];

    if (lhs.variable != rhs.variable ||
        lhs.attributeName != rhs.attributeName) {
      // oops, the different OR parts are on different variables or attributes
      return false;
    }
  }

  size_t previousIn = SIZE_MAX;

  for (size_t i = 0; i < n; ++i) {
    auto& p = parts[i];

    if (p.operatorType ==
            arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_IN &&
        p.valueNode->isArray()) {
      TRI_ASSERT(p.valueNode->isConstant());

      if (previousIn != SIZE_MAX) {
        // merge IN with IN
        TRI_ASSERT(previousIn < i);
        auto emptyArray = ast->createNodeArray();
        auto mergedIn = ast->createNodeUnionizedArray(
            parts[previousIn].valueNode, p.valueNode);
        parts[previousIn].valueNode = mergedIn;
        parts[i].valueNode = emptyArray;
        root->getMember(previousIn)->getMember(0)->changeMember(1, mergedIn);
        root->getMember(i)->getMember(0)->changeMember(1, emptyArray);
      } else {
        // note first IN
        previousIn = i;
      }
    }
  }

  // now sort all conditions by variable name, attribute name, attribute value
  std::sort(parts.begin(), parts.end(),
            [](arangodb::aql::ConditionPart const& lhs,
               arangodb::aql::ConditionPart const& rhs) -> bool {
              // compare variable names first
              auto res = lhs.variable->name.compare(rhs.variable->name);

              if (res != 0) {
                return res < 0;
              }

              // compare attribute names next
              res = lhs.attributeName.compare(rhs.attributeName);

              if (res != 0) {
                return res < 0;
              }

              // compare attribute values next
              auto ll = lhs.lowerBound();
              auto lr = rhs.lowerBound();

              if (ll == nullptr && lr != nullptr) {
                // left lower bound is not set but right
                return true;
              } else if (ll != nullptr && lr == nullptr) {
                // left lower bound is set but not right
                return false;
              }

              if (ll != nullptr && lr != nullptr) {
                // both lower bounds are set
                res = CompareAstNodes(ll, lr, true);

                if (res != 0) {
                  return res < 0;
                }
              }

              if (lhs.isLowerInclusive() && !rhs.isLowerInclusive()) {
                return true;
              }
              if (rhs.isLowerInclusive() && !lhs.isLowerInclusive()) {
                return false;
              }

              // all things equal
              return false;
            });

  TRI_ASSERT(parts.size() == conditionData.size());

  // clean up
  usedIndexes.clear();
  while (root->numMembers()) {
    root->removeMemberUnchecked(0);
  }

  // and rebuild
  for (size_t i = 0; i < n; ++i) {
    if (parts[i].operatorType == arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_IN &&
        parts[i].valueNode->isArray() &&
        parts[i].valueNode->numMembers() == 0) {
      // can optimize away empty IN array
      continue;
    }

    auto conditionData = static_cast<ConditionData*>(parts[i].data);
    root->addMember(conditionData->first);
    usedIndexes.emplace_back(conditionData->second);
  }

  return true;
}

std::pair<bool, bool> Transaction::findIndexHandleForAndNode(
    std::vector<std::shared_ptr<Index>> indexes, arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference,
    arangodb::aql::SortCondition const* sortCondition, size_t itemsInCollection,
    std::vector<Transaction::IndexHandle>& usedIndexes,
    arangodb::aql::AstNode*& specializedCondition,
    bool& isSparse) const {
  std::shared_ptr<Index> bestIndex;
  double bestCost = 0.0;
  bool bestSupportsFilter = false;
  bool bestSupportsSort = false;
  size_t coveredAttributes = 0;

  for (auto const& idx : indexes) {
    double filterCost = 0.0;
    double sortCost = 0.0;
    size_t itemsInIndex = itemsInCollection;

    bool supportsFilter = false;
    bool supportsSort = false;

    // check if the index supports the filter expression
    double estimatedCost;
    size_t estimatedItems;
    if (idx->supportsFilterCondition(node, reference, itemsInIndex,
                                     estimatedItems, estimatedCost)) {
      // index supports the filter condition
      filterCost = estimatedCost;
      // this reduces the number of items left
      itemsInIndex = estimatedItems;
      supportsFilter = true;
    } else {
      // index does not support the filter condition
      filterCost = itemsInIndex * 1.5;
    }

    bool const isOnlyAttributeAccess =
        (!sortCondition->isEmpty() && sortCondition->isOnlyAttributeAccess());

    if (sortCondition->isUnidirectional()) {
      // only go in here if we actually have a sort condition and it can in
      // general be supported by an index. for this, a sort condition must not
      // be empty, must consist only of attribute access, and all attributes
      // must be sorted in the direction
      if (indexSupportsSort(idx.get(), reference, sortCondition, itemsInIndex,
                            sortCost, coveredAttributes)) {
        supportsSort = true;
      }
    }

    if (!supportsSort && isOnlyAttributeAccess && node->isOnlyEqualityMatch()) {
      // index cannot be used for sorting, but the filter condition consists
      // only of equality lookups (==)
      // now check if the index fields are the same as the sort condition fields
      // e.g. FILTER c.value1 == 1 && c.value2 == 42 SORT c.value1, c.value2
      size_t coveredFields =
          sortCondition->coveredAttributes(reference, idx->fields());

      if (coveredFields == sortCondition->numAttributes() &&
          (idx->isSorted() ||
           idx->fields().size() == sortCondition->numAttributes())) {
        // no sorting needed
        sortCost = 0.0;
      }
    }

    if (!supportsFilter && !supportsSort) {
      continue;
    }

    double const totalCost = filterCost + sortCost;
    if (bestIndex == nullptr || totalCost < bestCost) {
      bestIndex = idx;
      bestCost = totalCost;
      bestSupportsFilter = supportsFilter;
      bestSupportsSort = supportsSort;
    }
  }

  if (bestIndex == nullptr) {
    return std::make_pair(false, false);
  }

  specializedCondition = bestIndex->specializeCondition(node, reference);

  usedIndexes.emplace_back(bestIndex);
  isSparse = bestIndex->sparse();

  return std::make_pair(bestSupportsFilter, bestSupportsSort);
}

bool Transaction::findIndexHandleForAndNode(
    std::vector<std::shared_ptr<Index>> indexes,
    arangodb::aql::AstNode*& node,
    arangodb::aql::Variable const* reference,
    size_t itemsInCollection,
    Transaction::IndexHandle& usedIndex) const {
  std::shared_ptr<Index> bestIndex;
  double bestCost = 0.0;

  for (auto const& idx : indexes) {
    double filterCost = 0.0;
    double sortCost = 0.0;
    size_t itemsInIndex = itemsInCollection;

    // check if the index supports the filter expression
    double estimatedCost;
    size_t estimatedItems;
    if (!idx->supportsFilterCondition(node, reference, itemsInIndex,
                                     estimatedItems, estimatedCost)) {
      continue;
    }
    // index supports the filter condition
    filterCost = estimatedCost;
    // this reduces the number of items left
    itemsInIndex = estimatedItems;

    double const totalCost = filterCost + sortCost;
    if (bestIndex == nullptr || totalCost < bestCost) {
      bestIndex = idx;
      bestCost = totalCost;
    }
  }

  if (bestIndex == nullptr) {
    return false;
  }

  node = bestIndex->specializeCondition(node, reference);

  usedIndex = IndexHandle(bestIndex);

  return true;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief if this pointer is set to an actual set, then for each request
/// sent to a shardId using the ClusterComm library, an X-Arango-Nolock
/// header is generated.
////////////////////////////////////////////////////////////////////////////////

thread_local std::unordered_set<std::string>* Transaction::_makeNolockHeaders =
    nullptr;
  
      
Transaction::Transaction(std::shared_ptr<TransactionContext> transactionContext)
    : _serverRole(ServerState::ROLE_UNDEFINED),
      _setupState(TRI_ERROR_NO_ERROR),
      _nestingLevel(0),
      _errorData(),
      _hints(0),
      _timeout(0.0),
      _waitForSync(false),
      _allowImplicitCollections(true),
      _isReal(true),
      _trx(nullptr),
      _vocbase(transactionContext->vocbase()),
      _resolver(nullptr),
      _transactionContext(transactionContext),
      _transactionContextPtr(transactionContext.get()) {
  TRI_ASSERT(_vocbase != nullptr);
  TRI_ASSERT(_transactionContext != nullptr);

  _serverRole = ServerState::instance()->getRole();
  if (ServerState::isCoordinator(_serverRole)) {
    _isReal = false;
  }

  this->setupTransaction();
}
   
////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the transaction
////////////////////////////////////////////////////////////////////////////////

Transaction::~Transaction() {
  if (_trx == nullptr) {
    return;
  }

  if (isEmbeddedTransaction()) {
    _trx->_nestingLevel--;
  } else {
    if (getStatus() == TRI_TRANSACTION_RUNNING) {
      // auto abort a running transaction
      this->abort();
    }

    // free the data associated with the transaction
    freeTransaction();
  }
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief return the names of all collections used in the transaction
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> Transaction::collectionNames() const {
  std::vector<std::string> result;
  result.reserve(_trx->_collections.size());

  for (auto& trxCollection : _trx->_collections) {
    if (trxCollection->_collection != nullptr) {
      result.emplace_back(trxCollection->_collection->name());
    }
  }

  return result;
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection name resolver
////////////////////////////////////////////////////////////////////////////////

CollectionNameResolver const* Transaction::resolver() {
  if (_resolver == nullptr) {
    _resolver = _transactionContext->getResolver();
    TRI_ASSERT(_resolver != nullptr);
  }
  return _resolver;
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief return the transaction collection for a document collection
////////////////////////////////////////////////////////////////////////////////

TRI_transaction_collection_t* Transaction::trxCollection(TRI_voc_cid_t cid) const {
  TRI_ASSERT(_trx != nullptr);
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);

  return TRI_GetCollectionTransaction(_trx, cid, TRI_TRANSACTION_READ);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief order a ditch for a collection
////////////////////////////////////////////////////////////////////////////////

DocumentDitch* Transaction::orderDitch(TRI_voc_cid_t cid) {
  TRI_ASSERT(_trx != nullptr);
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING ||
             getStatus() == TRI_TRANSACTION_CREATED);

  if (_ditchCache.cid == cid) {
    return _ditchCache.ditch;
  }

  TRI_transaction_collection_t* trxCollection = TRI_GetCollectionTransaction(_trx, cid, TRI_TRANSACTION_READ);

  if (trxCollection == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);    
  }

  TRI_ASSERT(trxCollection->_collection != nullptr);

  DocumentDitch* ditch = _transactionContext->orderDitch(trxCollection->_collection);

  if (ditch == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  _ditchCache.cid = cid;
  _ditchCache.ditch = ditch;

  return ditch;
}
  
//////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a ditch has been created for the collection
//////////////////////////////////////////////////////////////////////////////
  
bool Transaction::hasDitch(TRI_voc_cid_t cid) const {
  return (_transactionContext->ditch(cid) != nullptr);
}
  
//////////////////////////////////////////////////////////////////////////////
/// @brief get (or create) a rocksdb WriteTransaction
//////////////////////////////////////////////////////////////////////////////

#ifdef ARANGODB_ENABLE_ROCKSDB
rocksdb::Transaction* Transaction::rocksTransaction() {
  if (_trx->_rocksTransaction == nullptr) {
    _trx->_rocksTransaction = RocksDBFeature::instance()->db()->BeginTransaction(
      rocksdb::WriteOptions(), rocksdb::OptimisticTransactionOptions());
  }
  return _trx->_rocksTransaction;
}
#endif
  
////////////////////////////////////////////////////////////////////////////////
/// @brief extract the _key attribute from a slice
////////////////////////////////////////////////////////////////////////////////

StringRef Transaction::extractKeyPart(VPackSlice const slice) {
  // extract _key
  if (slice.isObject()) {
    VPackSlice k = slice.get(StaticStrings::KeyString);
    if (!k.isString()) {
      return StringRef(); // fail
    }
    return StringRef(k);
  } 
  if (slice.isString()) {
    StringRef key(slice);
    size_t pos = key.find('/');
    if (pos == std::string::npos) {
      return key;
    }
    return key.substr(pos + 1);
  } 
  return StringRef();
}

//////////////////////////////////////////////////////////////////////////////
/// @brief creates an id string from a custom _id value and the _key string
//////////////////////////////////////////////////////////////////////////////
        
std::string Transaction::makeIdFromCustom(CollectionNameResolver const* resolver,
                                          VPackSlice const& id, 
                                          VPackSlice const& key) {
  TRI_ASSERT(id.isCustom() && id.head() == 0xf3);
  TRI_ASSERT(key.isString());

  uint64_t cid = DatafileHelper::ReadNumber<uint64_t>(id.begin() + 1, sizeof(uint64_t));
  // create a buffer big enough for collection name + _key
  std::string buffer;
  buffer.reserve(TRI_COL_NAME_LENGTH + TRI_VOC_KEY_MAX_LENGTH + 2);
  buffer.append(resolver->getCollectionNameCluster(cid));
  buffer.append("/");

  VPackValueLength keyLength;
  char const* p = key.getString(keyLength);
  if (p == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid _key value");
  }
  buffer.append(p, static_cast<size_t>(keyLength));
  return buffer;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief extract the _id attribute from a slice, and convert it into a 
/// string
//////////////////////////////////////////////////////////////////////////////

std::string Transaction::extractIdString(VPackSlice slice) {
  return extractIdString(resolver(), slice, VPackSlice());
}

//////////////////////////////////////////////////////////////////////////////
/// @brief extract the _id attribute from a slice, and convert it into a 
/// string, static method
//////////////////////////////////////////////////////////////////////////////

std::string Transaction::extractIdString(CollectionNameResolver const* resolver,
                                         VPackSlice slice,
                                         VPackSlice const& base) {
  VPackSlice id;

  if (slice.isExternal()) {
    slice = slice.resolveExternal();
  }
   
  if (slice.isObject()) {
    // extract id attribute from object
    uint8_t const* p = slice.begin() + slice.findDataOffset(slice.head());
    if (*p == basics::VelocyPackHelper::KeyAttribute) {
      // skip over attribute name
      ++p;
      VPackSlice key = VPackSlice(p);
      // skip over attribute value
      p += key.byteSize();
      
      if (*p == basics::VelocyPackHelper::IdAttribute) {
        id = VPackSlice(p + 1);
        if (id.isCustom()) {
          // we should be pointing to a custom value now
          TRI_ASSERT(id.head() == 0xf3);
 
          return makeIdFromCustom(resolver, id, key);
        }
        if (id.isString()) {
          return id.copyString();
        }
      }
    }

    // in case the quick access above did not work out, use the slow path... 
    id = slice.get(StaticStrings::IdString);
  } else {
    id = slice;
  }
  
  if (id.isString()) {
    // already a string...
    return id.copyString();
  }
  if (!id.isCustom() || id.head() != 0xf3) {
    // invalid type for _id
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  // we now need to extract the _key attribute
  VPackSlice key;
  if (slice.isObject()) {
    key = slice.get(StaticStrings::KeyString);
  } else if (base.isObject()) {
    key = extractKeyFromDocument(base);
  } else if (base.isExternal()) {
    key = base.resolveExternal().get(StaticStrings::KeyString);
  }

  if (!key.isString()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }
        
  return makeIdFromCustom(resolver, id, key);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief quick access to the _key attribute in a database document
/// the document must have at least two attributes, and _key is supposed to
/// be the first one
//////////////////////////////////////////////////////////////////////////////

VPackSlice Transaction::extractKeyFromDocument(VPackSlice slice) {
  if (slice.isExternal()) {
    slice = slice.resolveExternal();
  }
  TRI_ASSERT(slice.isObject());

  if (slice.isEmptyObject()) {
    return VPackSlice();
  }
  // a regular document must have at least the three attributes 
  // _key, _id and _rev (in this order). _key must be the first attribute
  // however this method may also be called for remove markers, which only
  // have _key and _rev. therefore the only assertion that we can make
  // here is that the document at least has two attributes 

  uint8_t const* p = slice.begin() + slice.findDataOffset(slice.head());

  if (*p == basics::VelocyPackHelper::KeyAttribute) {
    // the + 1 is required so that we can skip over the attribute name
    // and point to the attribute value 
    return VPackSlice(p + 1); 
  }
  
  // fall back to the regular lookup method
  return slice.get(StaticStrings::KeyString); 
}

//////////////////////////////////////////////////////////////////////////////
/// @brief quick access to the _id attribute in a database document
/// the document must have at least two attributes, and _id is supposed to
/// be the second one
/// note that this may return a Slice of type Custom!
//////////////////////////////////////////////////////////////////////////////

VPackSlice Transaction::extractIdFromDocument(VPackSlice slice) {
  if (slice.isExternal()) {
    slice = slice.resolveExternal();
  }
  TRI_ASSERT(slice.isObject());
  
  if (slice.isEmptyObject()) {
    return VPackSlice();
  }
  // a regular document must have at least the three attributes 
  // _key, _id and _rev (in this order). _id must be the second attribute

  uint8_t const* p = slice.begin() + slice.findDataOffset(slice.head());

  if (*p == basics::VelocyPackHelper::KeyAttribute) {
    // skip over _key 
    ++p;
    // skip over _key value
    p += VPackSlice(p).byteSize();
    if (*p == basics::VelocyPackHelper::IdAttribute) {
      // the + 1 is required so that we can skip over the attribute name
      // and point to the attribute value 
      return VPackSlice(p + 1); 
    }
  }
  
  // fall back to the regular lookup method
  return slice.get(StaticStrings::IdString); 
}

//////////////////////////////////////////////////////////////////////////////
/// @brief quick access to the _from attribute in a database document
/// the document must have at least five attributes: _key, _id, _from, _to
/// and _rev (in this order)
//////////////////////////////////////////////////////////////////////////////

VPackSlice Transaction::extractFromFromDocument(VPackSlice slice) {
  if (slice.isExternal()) {
    slice = slice.resolveExternal();
  }
  TRI_ASSERT(slice.isObject());
  
  if (slice.isEmptyObject()) {
    return VPackSlice();
  }
  // this method must only be called on edges
  // this means we must have at least the attributes  _key, _id, _from, _to and _rev

  uint8_t const* p = slice.begin() + slice.findDataOffset(slice.head());
  VPackValueLength count = 0;

  while (*p <= basics::VelocyPackHelper::FromAttribute && ++count <= 3) {
    if (*p == basics::VelocyPackHelper::FromAttribute) {
      // the + 1 is required so that we can skip over the attribute name
      // and point to the attribute value 
      return VPackSlice(p + 1);
    }
    // skip over the attribute name
    ++p;
    // skip over the attribute value
    p += VPackSlice(p).byteSize();
  }

  // fall back to the regular lookup method
  return slice.get(StaticStrings::FromString); 
}

//////////////////////////////////////////////////////////////////////////////
/// @brief quick access to the _to attribute in a database document
/// the document must have at least five attributes: _key, _id, _from, _to
/// and _rev (in this order)
//////////////////////////////////////////////////////////////////////////////

VPackSlice Transaction::extractToFromDocument(VPackSlice slice) {
  if (slice.isExternal()) {
    slice = slice.resolveExternal();
  }
  
  if (slice.isEmptyObject()) {
    return VPackSlice();
  }
  // this method must only be called on edges
  // this means we must have at least the attributes  _key, _id, _from, _to and _rev

  uint8_t const* p = slice.begin() + slice.findDataOffset(slice.head());
  VPackValueLength count = 0;

  while (*p <= basics::VelocyPackHelper::ToAttribute && ++count <= 4) {
    if (*p == basics::VelocyPackHelper::ToAttribute) {
      // the + 1 is required so that we can skip over the attribute name
      // and point to the attribute value 
      return VPackSlice(p + 1);
    }
    // skip over the attribute name
    ++p;
    // skip over the attribute value
    p += VPackSlice(p).byteSize();
  }
  
  // fall back to the regular lookup method
  return slice.get(StaticStrings::ToString); 
}

//////////////////////////////////////////////////////////////////////////////
/// @brief extract _key and _rev from a document, in one go
/// this is an optimized version used when loading collections, WAL 
/// collection and compaction
//////////////////////////////////////////////////////////////////////////////
    
void Transaction::extractKeyAndRevFromDocument(VPackSlice slice, 
                                               VPackSlice& keySlice, 
                                               TRI_voc_rid_t& revisionId) {
  if (slice.isExternal()) {
    slice = slice.resolveExternal();
  }
  TRI_ASSERT(slice.isObject());
  TRI_ASSERT(slice.length() >= 2); 

  uint8_t const* p = slice.begin() + slice.findDataOffset(slice.head());
  VPackValueLength count = 0;
  bool foundKey = false;
  bool foundRev = false;

  while (*p <= basics::VelocyPackHelper::ToAttribute && ++count <= 5) {
    if (*p == basics::VelocyPackHelper::KeyAttribute) {
      keySlice = VPackSlice(p + 1);
      if (foundRev) {
        return;
      }
      foundKey = true;
    } else if (*p == basics::VelocyPackHelper::RevAttribute) {
      VPackSlice revSlice(p + 1);
      if (revSlice.isString()) {
        VPackValueLength l;
        char const* p = revSlice.getString(l);
        revisionId = TRI_StringToRid(p, l);
      } else if (revSlice.isNumber()) {
        revisionId = revSlice.getNumericValue<TRI_voc_rid_t>();
      }
      if (foundKey) {
        return;
      }
      foundRev = true;
    }
    // skip over the attribute name
    ++p;
    // skip over the attribute value
    p += VPackSlice(p).byteSize();
  }

  // fall back to regular lookup
  {
    keySlice = slice.get(StaticStrings::KeyString);    
    VPackValueLength l;
    char const* p = slice.get(StaticStrings::RevString).getString(l);
    revisionId = TRI_StringToRid(p, l);
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief extract _rev from a database document
//////////////////////////////////////////////////////////////////////////////
    
TRI_voc_rid_t Transaction::extractRevFromDocument(VPackSlice slice) {
  TRI_ASSERT(slice.isObject());
  TRI_ASSERT(slice.length() >= 2); 

  uint8_t const* p = slice.begin() + slice.findDataOffset(slice.head());
  VPackValueLength count = 0;

  while (*p <= basics::VelocyPackHelper::ToAttribute && ++count <= 5) {
    if (*p == basics::VelocyPackHelper::RevAttribute) {
      VPackSlice revSlice(p + 1);
      if (revSlice.isString()) {
        VPackValueLength l;
        char const* p = revSlice.getString(l);
        return TRI_StringToRid(p, l);
      } else if (revSlice.isNumber()) {
        return revSlice.getNumericValue<TRI_voc_rid_t>();
      }
      // invalid type for revision id
      return 0;
    }
    // skip over the attribute name
    ++p;
    // skip over the attribute value
    p += VPackSlice(p).byteSize();
  }

  TRI_ASSERT(false);
  return 0;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief build a VPack object with _id, _key and _rev, the result is
/// added to the builder in the argument as a single object.
//////////////////////////////////////////////////////////////////////////////

void Transaction::buildDocumentIdentity(LogicalCollection* collection,
                                        VPackBuilder& builder,
                                        TRI_voc_cid_t cid,
                                        StringRef const& key,
                                        VPackSlice const rid,
                                        VPackSlice const oldRid,
                                        TRI_doc_mptr_t const* oldMptr,
                                        TRI_doc_mptr_t const* newMptr) {
  builder.openObject();
  if (ServerState::isRunningInCluster(_serverRole)) {
    builder.add(StaticStrings::IdString, VPackValue(resolver()->getCollectionName(cid) + "/" + key.toString()));
  } else {
    builder.add(StaticStrings::IdString,
                VPackValue(collection->name() + "/" + key.toString()));
  }
  builder.add(StaticStrings::KeyString, VPackValuePair(key.data(), key.length(), VPackValueType::String));
  TRI_ASSERT(!rid.isNone());
  builder.add(StaticStrings::RevString, rid);
  if (!oldRid.isNone()) {
    builder.add("_oldRev", oldRid);
  }
  if (oldMptr != nullptr) {
    builder.add("old", VPackValue(oldMptr->vpack(), VPackValueType::External));
  }
  if (newMptr != nullptr) {
    builder.add("new", VPackValue(newMptr->vpack(), VPackValueType::External));
  }
  builder.close();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief begin the transaction
////////////////////////////////////////////////////////////////////////////////

int Transaction::begin() {
  if (_trx == nullptr) {
    return TRI_ERROR_TRANSACTION_INTERNAL;
  }

  if (_setupState != TRI_ERROR_NO_ERROR) {
    return _setupState;
  }

  if (!_isReal) {
    if (_nestingLevel == 0) {
      _trx->_status = TRI_TRANSACTION_RUNNING;
    }
    return TRI_ERROR_NO_ERROR;
  }

  int res = TRI_BeginTransaction(_trx, _hints, _nestingLevel);

  return res;
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief commit / finish the transaction
////////////////////////////////////////////////////////////////////////////////

int Transaction::commit() {
  if (_trx == nullptr || getStatus() != TRI_TRANSACTION_RUNNING) {
    // transaction not created or not running
    return TRI_ERROR_TRANSACTION_INTERNAL;
  }

  if (!_isReal) {
    if (_nestingLevel == 0) {
      _trx->_status = TRI_TRANSACTION_COMMITTED;
    }
    return TRI_ERROR_NO_ERROR;
  }

  int res = TRI_CommitTransaction(_trx, _nestingLevel);

  return res;
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief abort the transaction
////////////////////////////////////////////////////////////////////////////////

int Transaction::abort() {
  if (_trx == nullptr || getStatus() != TRI_TRANSACTION_RUNNING) {
    // transaction not created or not running
    return TRI_ERROR_TRANSACTION_INTERNAL;
  }

  if (!_isReal) {
    if (_nestingLevel == 0) {
      _trx->_status = TRI_TRANSACTION_ABORTED;
    }

    return TRI_ERROR_NO_ERROR;
  }

  int res = TRI_AbortTransaction(_trx, _nestingLevel);

  return res;
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief finish a transaction (commit or abort), based on the previous state
////////////////////////////////////////////////////////////////////////////////

int Transaction::finish(int errorNum) {
  if (errorNum == TRI_ERROR_NO_ERROR) {
    // there was no previous error, so we'll commit
    return this->commit();
  }
  
  // there was a previous error, so we'll abort
  this->abort();

  // return original error number
  return errorNum;
}

std::string Transaction::name(TRI_voc_cid_t cid) const {
  auto c = trxCollection(cid);
  TRI_ASSERT(c != nullptr);
  return c->_collection->name();
}


////////////////////////////////////////////////////////////////////////////////
/// @brief read any (random) document
////////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::any(std::string const& collectionName) {
  return any(collectionName, 0, 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read all master pointers, using skip and limit.
/// The resualt guarantees that all documents are contained exactly once
/// as long as the collection is not modified.
////////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::any(std::string const& collectionName,
                                 uint64_t skip, uint64_t limit) {
  if (ServerState::isCoordinator(_serverRole)) {
    return anyCoordinator(collectionName, skip, limit);
  }
  return anyLocal(collectionName, skip, limit);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetches documents in a collection in random order, coordinator
////////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::anyCoordinator(std::string const&, uint64_t,
                                            uint64_t) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetches documents in a collection in random order, local
////////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::anyLocal(std::string const& collectionName,
                                      uint64_t skip, uint64_t limit) {
  TRI_voc_cid_t cid = resolver()->getCollectionIdLocal(collectionName);

  if (cid == 0) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, "'%s'",
                                  collectionName.c_str());
  }
 
  orderDitch(cid); // will throw when it fails 
  
  int res = lock(trxCollection(cid), TRI_TRANSACTION_READ);

  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }
  
  VPackBuilder resultBuilder;
  resultBuilder.openArray();

  std::unique_ptr<OperationCursor> cursor =
      indexScan(collectionName, Transaction::CursorType::ANY, IndexHandle(), 
                {}, skip, limit, 1000, false);

  std::vector<TRI_doc_mptr_t*> result;
  while (cursor->hasMore()) {
    result.clear();
    cursor->getMoreMptr(result);
    for (auto const& mptr : result) {
      resultBuilder.add(VPackSlice(mptr->vpack()));
    }
  }

  resultBuilder.close();

  res = unlock(trxCollection(cid), TRI_TRANSACTION_READ);

  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }

  return OperationResult(resultBuilder.steal(),
                         _transactionContextPtr->orderCustomTypeHandler(), "",
                         TRI_ERROR_NO_ERROR, false);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to the transaction for read, at runtime
//////////////////////////////////////////////////////////////////////////////

TRI_voc_cid_t Transaction::addCollectionAtRuntime(std::string const& collectionName) {
  if (collectionName == _collectionCache.name && !collectionName.empty()) {
    return _collectionCache.cid;
  }

  auto t = dynamic_cast<SingleCollectionTransaction*>(this);
  if (t != nullptr) {
    TRI_voc_cid_t cid = t->cid();
    _collectionCache.cid = cid;
    _collectionCache.name = collectionName;
    return cid;
  } 
  
  auto cid = resolver()->getCollectionIdLocal(collectionName);

  if (cid == 0) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, "'%s'",
                                  collectionName.c_str());
  }
  addCollectionAtRuntime(cid, collectionName);
  _collectionCache.cid = cid;
  _collectionCache.name = collectionName;
  return cid;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief return the type of a collection
//////////////////////////////////////////////////////////////////////////////

bool Transaction::isEdgeCollection(std::string const& collectionName) {
  return getCollectionType(collectionName) == TRI_COL_TYPE_EDGE;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief return the type of a collection
//////////////////////////////////////////////////////////////////////////////

bool Transaction::isDocumentCollection(std::string const& collectionName) {
  return getCollectionType(collectionName) == TRI_COL_TYPE_DOCUMENT;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief return the type of a collection
//////////////////////////////////////////////////////////////////////////////
  
TRI_col_type_e Transaction::getCollectionType(std::string const& collectionName) {
  if (ServerState::isCoordinator(_serverRole)) {
    return resolver()->getCollectionTypeCluster(collectionName);
  }
  return resolver()->getCollectionType(collectionName);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief return the name of a collection
//////////////////////////////////////////////////////////////////////////////
  
std::string Transaction::collectionName(TRI_voc_cid_t cid) { 
  return resolver()->getCollectionName(cid);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief return the edge index handle of collection
//////////////////////////////////////////////////////////////////////////////

Transaction::IndexHandle Transaction::edgeIndexHandle(std::string const& collectionName) {
  if (!isEdgeCollection(collectionName)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
  }
  auto indexes = indexesForCollection(collectionName); 
  for (auto idx : indexes) {
    if (idx->type() == Index::TRI_IDX_TYPE_EDGE_INDEX) {
      return IndexHandle(idx);
    }
  }
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief Iterate over all elements of the collection.
//////////////////////////////////////////////////////////////////////////////

void Transaction::invokeOnAllElements(std::string const& collectionName,
                                      std::function<bool(TRI_doc_mptr_t const*)> callback) {
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);
  if (ServerState::isCoordinator(_serverRole)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  
  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName); 
  TRI_transaction_collection_t* trxCol = trxCollection(cid);
  LogicalCollection* document = documentCollection(trxCol);

  orderDitch(cid); // will throw when it fails

  int res = lock(trxCol, TRI_TRANSACTION_READ);
  
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  auto primaryIndex = document->primaryIndex();
  primaryIndex->invokeOnAllElements(callback);
  
  res = unlock(trxCol, TRI_TRANSACTION_READ);
  
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief return one document from a collection, fast path
///        If everything went well the result will contain the found document
///        (as an external on single_server) and this function will return TRI_ERROR_NO_ERROR.
///        If there was an error the code is returned and it is guaranteed
///        that result remains unmodified.
///        Does not care for revision handling!
//////////////////////////////////////////////////////////////////////////////

int Transaction::documentFastPath(std::string const& collectionName,
                                  VPackSlice const value,
                                  VPackBuilder& result,
                                  bool shouldLock) {
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);
  if (!value.isObject() && !value.isString()) {
    // must provide a document object or string
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  if (ServerState::isCoordinator(_serverRole)) {
    OperationOptions options; // use default configuration
    options.ignoreRevs = true;

    OperationResult opRes = documentCoordinator(collectionName, value, options);
    if (opRes.failed()) {
      return opRes.code;
    }
    result.add(opRes.slice());
    return TRI_ERROR_NO_ERROR;
  }

  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName); 
  LogicalCollection* collection = documentCollection(trxCollection(cid));

  orderDitch(cid); // will throw when it fails

  StringRef key(Transaction::extractKeyPart(value));
  if (key.empty()) {
    return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
  }

  TRI_doc_mptr_t mptr;
  int res = collection->read(
      this, key, &mptr,
      shouldLock && !isLocked(collection, TRI_TRANSACTION_READ));

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
  
  TRI_ASSERT(hasDitch(cid));

  TRI_ASSERT(mptr.vpack() != nullptr);
  result.addExternal(mptr.vpack());
  return TRI_ERROR_NO_ERROR;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief return one document from a collection, fast path
///        If everything went well the result will contain the found document
///        (as an external on single_server) and this function will return TRI_ERROR_NO_ERROR.
///        If there was an error the code is returned 
///        Does not care for revision handling!
///        Must only be called on a local server, not in cluster case!
//////////////////////////////////////////////////////////////////////////////

int Transaction::documentFastPathLocal(std::string const& collectionName,
                                       std::string const& key,
                                       TRI_doc_mptr_t* result) {
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);

  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName);
  LogicalCollection* collection = documentCollection(trxCollection(cid));

  orderDitch(cid);  // will throw when it fails

  if (key.empty()) {
    return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
  }

  int res = collection->read(this, key, result,
                             !isLocked(collection, TRI_TRANSACTION_READ));

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }
    
  TRI_ASSERT(hasDitch(cid));
  return TRI_ERROR_NO_ERROR;
}

/// @brief Create Cluster Communication result
OperationResult Transaction::clusterResult(
    rest::ResponseCode const& responseCode,
    std::shared_ptr<VPackBuilder> const& resultBody,
    std::unordered_map<int, size_t> errorCounter) const {
  int errorCode = TRI_ERROR_NO_ERROR;
  switch (responseCode) {
    case rest::ResponseCode::CONFLICT:
      errorCode = TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED;
      // Fall through
    case rest::ResponseCode::PRECONDITION_FAILED:
      if (errorCode == TRI_ERROR_NO_ERROR) {
        errorCode = TRI_ERROR_ARANGO_CONFLICT;
      }
      // Fall through
    case rest::ResponseCode::ACCEPTED:
    case rest::ResponseCode::CREATED:
      return OperationResult(
          resultBody->steal(), nullptr, "", errorCode,
          responseCode == rest::ResponseCode::CREATED,
          errorCounter);
    case rest::ResponseCode::BAD:
      return DBServerResponseBad(resultBody);
    case rest::ResponseCode::NOT_FOUND:
      return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
    default:
      return OperationResult(TRI_ERROR_INTERNAL);
  }
}


//////////////////////////////////////////////////////////////////////////////
/// @brief return one or multiple documents from a collection
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::document(std::string const& collectionName,
                                      VPackSlice const value,
                                      OperationOptions& options) {
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);

  if (!value.isObject() && !value.isArray()) {
    // must provide a document object or an array of documents
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  if (ServerState::isCoordinator(_serverRole)) {
    return documentCoordinator(collectionName, value, options);
  }

  return documentLocal(collectionName, value, options);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief read one or multiple documents in a collection, coordinator
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::documentCoordinator(std::string const& collectionName,
                                                 VPackSlice const value,
                                                 OperationOptions& options) {
  auto headers = std::make_unique<std::unordered_map<std::string, std::string>>();
  rest::ResponseCode responseCode;
  std::unordered_map<int, size_t> errorCounter;
  auto resultBody = std::make_shared<VPackBuilder>();

  if (!value.isArray()) {
    StringRef key(Transaction::extractKeyPart(value));
    if (key.empty()) {
      return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD);
    }
  }
  
  int res = arangodb::getDocumentOnCoordinator(
      _vocbase->name(), collectionName, value, options, headers, responseCode, errorCounter, resultBody);

  if (res == TRI_ERROR_NO_ERROR) {
    if (responseCode == rest::ResponseCode::OK ||
        responseCode == rest::ResponseCode::PRECONDITION_FAILED) {
      return OperationResult(resultBody->steal(), nullptr, "", 
          responseCode == rest::ResponseCode::OK ?
          TRI_ERROR_NO_ERROR : TRI_ERROR_ARANGO_CONFLICT, false, errorCounter);
    } else if (responseCode == rest::ResponseCode::NOT_FOUND) {
      return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
    } else {
      return OperationResult(TRI_ERROR_INTERNAL);
    }
  }
  return OperationResult(res);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief read one or multiple documents in a collection, local
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::documentLocal(std::string const& collectionName,
                                           VPackSlice const value,
                                           OperationOptions& options) {
  TIMER_START(TRANSACTION_DOCUMENT_LOCAL);
  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName); 
  LogicalCollection* collection = documentCollection(trxCollection(cid));

  if (!options.silent) {
    orderDitch(cid); // will throw when it fails
  }
 
  VPackBuilder resultBuilder;

  auto workOnOneDocument = [&](VPackSlice const value, bool isMultiple) -> int {
    TIMER_START(TRANSACTION_DOCUMENT_EXTRACT);

    StringRef key(Transaction::extractKeyPart(value));
    if (key.empty()) {
      return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
    }

    VPackSlice expectedRevision;
    if (!options.ignoreRevs) {
      expectedRevision = TRI_ExtractRevisionIdAsSlice(value);
    }
    
    TIMER_STOP(TRANSACTION_DOCUMENT_EXTRACT);

    TRI_doc_mptr_t mptr;
    TIMER_START(TRANSACTION_DOCUMENT_DOCUMENT_DOCUMENT);
    int res = collection->read(this, key, &mptr,
                               !isLocked(collection, TRI_TRANSACTION_READ));
    TIMER_STOP(TRANSACTION_DOCUMENT_DOCUMENT_DOCUMENT);

    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  
    TRI_ASSERT(hasDitch(cid));
  
    TRI_ASSERT(mptr.vpack() != nullptr);
    if (!expectedRevision.isNone()) {
      VPackSlice foundRevision = mptr.revisionIdAsSlice();
      if (expectedRevision != foundRevision) {
        if (!isMultiple) {
          // still return
          buildDocumentIdentity(collection, resultBuilder, cid, key,
                                foundRevision, VPackSlice(), nullptr, nullptr);
        }
        return TRI_ERROR_ARANGO_CONFLICT;
      }
    }
  
    if (!options.silent) {
      resultBuilder.addExternal(mptr.vpack());
    } else if (isMultiple) {
      resultBuilder.add(VPackSlice::nullSlice());
    }

    return TRI_ERROR_NO_ERROR;
  };

  TIMER_START(TRANSACTION_DOCUMENT_WORK_FOR_ONE);

  int res = TRI_ERROR_NO_ERROR;
  std::unordered_map<int, size_t> countErrorCodes;
  if (!value.isArray()) {
    res = workOnOneDocument(value, false);
  } else {
    VPackArrayBuilder guard(&resultBuilder);
    for (auto const& s : VPackArrayIterator(value)) {
      res = workOnOneDocument(s, true);
      if (res != TRI_ERROR_NO_ERROR) {
        createBabiesError(resultBuilder, countErrorCodes, res, options.silent);
      }
    }
    res = TRI_ERROR_NO_ERROR;
  }
  
  TIMER_STOP(TRANSACTION_DOCUMENT_WORK_FOR_ONE);
  
  TIMER_STOP(TRANSACTION_DOCUMENT_LOCAL);

  return OperationResult(resultBuilder.steal(), 
                         _transactionContextPtr->orderCustomTypeHandler(), "",
                         res, options.waitForSync, countErrorCodes); 
}

//////////////////////////////////////////////////////////////////////////////
/// @brief create one or multiple documents in a collection
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::insert(std::string const& collectionName,
                                    VPackSlice const value,
                                    OperationOptions const& options) {
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);

  if (!value.isObject() && !value.isArray()) {
    // must provide a document object or an array of documents
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  // Validate Edges
  OperationOptions optionsCopy = options;

  if (ServerState::isCoordinator(_serverRole)) {
    return insertCoordinator(collectionName, value, optionsCopy);
  }

  return insertLocal(collectionName, value, optionsCopy);
}
   
//////////////////////////////////////////////////////////////////////////////
/// @brief create one or multiple documents in a collection, coordinator
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
//////////////////////////////////////////////////////////////////////////////

#ifndef USE_ENTERPRISE
OperationResult Transaction::insertCoordinator(std::string const& collectionName,
                                               VPackSlice const value,
                                               OperationOptions& options) {

  rest::ResponseCode responseCode;

  std::unordered_map<int, size_t> errorCounter;
  auto resultBody = std::make_shared<VPackBuilder>();

  int res = arangodb::createDocumentOnCoordinator(
      _vocbase->name(), collectionName, options, value, responseCode,
      errorCounter, resultBody);

  if (res == TRI_ERROR_NO_ERROR) {
    if (responseCode == rest::ResponseCode::ACCEPTED ||
        responseCode == rest::ResponseCode::CREATED) {
      return OperationResult(
          resultBody->steal(), nullptr, "", TRI_ERROR_NO_ERROR,
          responseCode == rest::ResponseCode::CREATED, errorCounter);
    } else if (responseCode == rest::ResponseCode::PRECONDITION_FAILED) {
      return OperationResult(TRI_ERROR_ARANGO_CONFLICT);
    } else if (responseCode == rest::ResponseCode::BAD) {
      return DBServerResponseBad(resultBody);
    } else if (responseCode == rest::ResponseCode::NOT_FOUND) {
      return OperationResult(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    } else if (responseCode == rest::ResponseCode::CONFLICT) {
      return OperationResult(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED);
    } else {
      return OperationResult(TRI_ERROR_INTERNAL);
    }
  }
  return OperationResult(res);
}
#endif

//////////////////////////////////////////////////////////////////////////////
/// @brief create one or multiple documents in a collection, local
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::insertLocal(std::string const& collectionName,
                                         VPackSlice const value,
                                         OperationOptions& options) {
  TIMER_START(TRANSACTION_INSERT_LOCAL);
  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName); 
  LogicalCollection* collection = documentCollection(trxCollection(cid));

  // First see whether or not we have to do synchronous replication:
  std::shared_ptr<std::vector<ServerID> const> followers;
  bool doingSynchronousReplication = false;
  if (ServerState::isDBServer(_serverRole)) {
    // Now replicate the same operation on all followers:
    auto const& followerInfo = collection->followers();
    followers = followerInfo->get();
    doingSynchronousReplication = followers->size() > 0;
  }

  if (options.returnNew) {
    orderDitch(cid); // will throw when it fails 
  }

  VPackBuilder resultBuilder;
  TRI_voc_tick_t maxTick = 0;

  auto workForOneDocument = [&](VPackSlice const value) -> int {
    if (!value.isObject()) {
      return TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
    }

    TRI_doc_mptr_t mptr;
    TRI_voc_tick_t resultMarkerTick = 0;

    TIMER_START(TRANSACTION_INSERT_DOCUMENT_INSERT);
    int res = collection->insert(this, value, &mptr, options, resultMarkerTick,
                                 !isLocked(collection, TRI_TRANSACTION_WRITE));
    TIMER_STOP(TRANSACTION_INSERT_DOCUMENT_INSERT);

    if (resultMarkerTick > 0 && resultMarkerTick > maxTick) {
      maxTick = resultMarkerTick;
    }
    
    if (res != TRI_ERROR_NO_ERROR) {
      // Error reporting in the babies case is done outside of here,
      // in the single document case no body needs to be created at all.
      return res;
    }

    if (options.silent && !doingSynchronousReplication) {
      // no need to construct the result object
      return TRI_ERROR_NO_ERROR;
    }

    TRI_ASSERT(mptr.vpack() != nullptr);
    
    StringRef keyString(VPackSlice(mptr.vpack()).get(StaticStrings::KeyString));

    TIMER_START(TRANSACTION_INSERT_BUILD_DOCUMENT_IDENTITY);

    buildDocumentIdentity(collection, resultBuilder, cid, keyString, 
        mptr.revisionIdAsSlice(), VPackSlice(),
        nullptr, options.returnNew ? &mptr : nullptr);

    TIMER_STOP(TRANSACTION_INSERT_BUILD_DOCUMENT_IDENTITY);

    return TRI_ERROR_NO_ERROR;
  };

  TIMER_START(TRANSACTION_INSERT_WORK_FOR_ONE);

  int res = TRI_ERROR_NO_ERROR;
  bool const multiCase = value.isArray();
  std::unordered_map<int, size_t> countErrorCodes;
  if (multiCase) {
    VPackArrayBuilder b(&resultBuilder);
    for (auto const& s : VPackArrayIterator(value)) {
      res = workForOneDocument(s);
      if (res != TRI_ERROR_NO_ERROR) {
        createBabiesError(resultBuilder, countErrorCodes, res, options.silent);
      }
    }
    // With babies the reporting is handled in the body of the result
    res = TRI_ERROR_NO_ERROR;
  } else {
    res = workForOneDocument(value);
  }
  
  TIMER_STOP(TRANSACTION_INSERT_WORK_FOR_ONE);
 
  // wait for operation(s) to be synced to disk here 
  if (res == TRI_ERROR_NO_ERROR && options.waitForSync && maxTick > 0 && isSingleOperationTransaction()) {
    arangodb::wal::LogfileManager::instance()->slots()->waitForTick(maxTick);
  }
  

  if (doingSynchronousReplication && res == TRI_ERROR_NO_ERROR) {
    // In the multi babies case res is always TRI_ERROR_NO_ERROR if we
    // get here, in the single document case, we do not try to replicate
    // in case of an error.
 
    // Now replicate the good operations on all followers:
    std::string path
        = "/_db/" +
          arangodb::basics::StringUtils::urlEncode(_vocbase->name()) +
          "/_api/document/" +
          arangodb::basics::StringUtils::urlEncode(collection->name())
          + "?isRestore=true";

    VPackBuilder payload;

    auto doOneDoc = [&](VPackSlice const& doc, VPackSlice result) {
      VPackObjectBuilder guard(&payload);
      VPackSlice s = result.get(StaticStrings::KeyString);
      payload.add(StaticStrings::KeyString, s);
      s = result.get(StaticStrings::RevString);
      payload.add(StaticStrings::RevString, s);
      TRI_SanitizeObject(doc, payload);
    };

    VPackSlice ourResult = resultBuilder.slice();
    size_t count = 0;
    if (value.isArray()) {
      VPackArrayBuilder guard(&payload);
      VPackArrayIterator itValue(value);
      VPackArrayIterator itResult(ourResult);
      while (itValue.valid() && itResult.valid()) {
        TRI_ASSERT((*itResult).isObject());
        if (!(*itResult).hasKey("error")) {
          doOneDoc(itValue.value(), itResult.value());
          count++;
        }
        itValue.next();
        itResult.next();
      }
    } else {
      doOneDoc(value, ourResult);
      count++;
    }
    if (count > 0) {
      auto body = std::make_shared<std::string>();
      *body = payload.slice().toJson();

      // Now prepare the requests:
      std::vector<ClusterCommRequest> requests;
      for (auto const& f : *followers) {
        requests.emplace_back("server:" + f, 
                              arangodb::rest::RequestType::POST,
                              path, body);
      }
      auto cc = arangodb::ClusterComm::instance();
      size_t nrDone = 0;
      size_t nrGood = cc->performRequests(requests, TRX_FOLLOWER_TIMEOUT, 
                                          nrDone, Logger::REPLICATION);
      if (nrGood < followers->size()) {
        // we drop all followers that were not successful:
        for (size_t i = 0; i < followers->size(); ++i) {
          bool replicationWorked 
              = requests[i].done &&
                requests[i].result.status == CL_COMM_RECEIVED &&
                (requests[i].result.answer_code == 
                     rest::ResponseCode::ACCEPTED ||
                 requests[i].result.answer_code == 
                     rest::ResponseCode::CREATED);
          if (replicationWorked) {
            bool found;
            requests[i].result.answer->header(StaticStrings::ErrorCodes, found);
            replicationWorked = !found;
          }
          if (!replicationWorked) {
            auto const& followerInfo = collection->followers();
            followerInfo->remove((*followers)[i]);
            LOG_TOPIC(ERR, Logger::REPLICATION)
                << "insertLocal: dropping follower "
                << (*followers)[i] << " for shard " << collectionName;
          }
        }
      }
    }
  }
  
  if (doingSynchronousReplication && options.silent) {
    // We needed the results, but do not want to report:
    resultBuilder.clear();
  }
  
  TIMER_STOP(TRANSACTION_INSERT_LOCAL);

  return OperationResult(resultBuilder.steal(), nullptr, "", res,
                         options.waitForSync, countErrorCodes);
}
  
//////////////////////////////////////////////////////////////////////////////
/// @brief update/patch one or multiple documents in a collection
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::update(std::string const& collectionName,
                                    VPackSlice const newValue,
                                    OperationOptions const& options) {
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);

  if (!newValue.isObject() && !newValue.isArray()) {
    // must provide a document object or an array of documents
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  OperationOptions optionsCopy = options;

  if (ServerState::isCoordinator(_serverRole)) {
    return updateCoordinator(collectionName, newValue, optionsCopy);
  }

  return modifyLocal(collectionName, newValue, optionsCopy,
                     TRI_VOC_DOCUMENT_OPERATION_UPDATE);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief update one or multiple documents in a collection, coordinator
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
//////////////////////////////////////////////////////////////////////////////

#ifndef USE_ENTERPRISE
OperationResult Transaction::updateCoordinator(std::string const& collectionName,
                                               VPackSlice const newValue,
                                               OperationOptions& options) {

  auto headers = std::make_unique<std::unordered_map<std::string, std::string>>();
  rest::ResponseCode responseCode;
  std::unordered_map<int, size_t> errorCounter;
  auto resultBody = std::make_shared<VPackBuilder>();

  int res = arangodb::modifyDocumentOnCoordinator(
      _vocbase->name(), collectionName, newValue, options,
      true /* isPatch */, headers, responseCode, errorCounter,
      resultBody);

  if (res == TRI_ERROR_NO_ERROR) {
    int errorCode = TRI_ERROR_NO_ERROR;
    switch (responseCode) {
      case rest::ResponseCode::CONFLICT:
        errorCode = TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED;
        // Fall through
      case rest::ResponseCode::PRECONDITION_FAILED:
        if (errorCode == TRI_ERROR_NO_ERROR) {
          errorCode = TRI_ERROR_ARANGO_CONFLICT;
        }
        // Fall through
      case rest::ResponseCode::ACCEPTED:
      case rest::ResponseCode::CREATED:
        return OperationResult(
            resultBody->steal(), nullptr, "", errorCode,
            responseCode == rest::ResponseCode::CREATED,
            errorCounter);
      case rest::ResponseCode::BAD:
        return DBServerResponseBad(resultBody);
      case rest::ResponseCode::NOT_FOUND:
        return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
      default:
        return OperationResult(TRI_ERROR_INTERNAL);
    }
  }
  return OperationResult(res);
}
#endif

//////////////////////////////////////////////////////////////////////////////
/// @brief replace one or multiple documents in a collection
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::replace(std::string const& collectionName,
                                     VPackSlice const newValue,
                                     OperationOptions const& options) {
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);

  if (!newValue.isObject() && !newValue.isArray()) {
    // must provide a document object or an array of documents
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  OperationOptions optionsCopy = options;

  if (ServerState::isCoordinator(_serverRole)) {
    return replaceCoordinator(collectionName, newValue, optionsCopy);
  }

  return modifyLocal(collectionName, newValue, optionsCopy,
                     TRI_VOC_DOCUMENT_OPERATION_REPLACE);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief replace one or multiple documents in a collection, coordinator
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
//////////////////////////////////////////////////////////////////////////////

#ifndef USE_ENTERPRISE
OperationResult Transaction::replaceCoordinator(std::string const& collectionName,
                                                VPackSlice const newValue,
                                                OperationOptions& options) {
  auto headers = std::make_unique<std::unordered_map<std::string, std::string>>();
  rest::ResponseCode responseCode;
  std::unordered_map<int, size_t> errorCounter;
  auto resultBody = std::make_shared<VPackBuilder>();

  int res = arangodb::modifyDocumentOnCoordinator(
      _vocbase->name(), collectionName, newValue, options,
      false /* isPatch */, headers, responseCode, errorCounter,
      resultBody);

  if (res == TRI_ERROR_NO_ERROR) {
    int errorCode = TRI_ERROR_NO_ERROR;
    switch (responseCode) {
      case rest::ResponseCode::CONFLICT:
        errorCode = TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED;
      case rest::ResponseCode::PRECONDITION_FAILED:
        if (errorCode == TRI_ERROR_NO_ERROR) {
          errorCode = TRI_ERROR_ARANGO_CONFLICT;
        }
        // Fall through
      case rest::ResponseCode::ACCEPTED:
      case rest::ResponseCode::CREATED:
        return OperationResult(
            resultBody->steal(), nullptr, "", errorCode,
            responseCode == rest::ResponseCode::CREATED,
            errorCounter);
      case rest::ResponseCode::BAD:
        return DBServerResponseBad(resultBody);
      case rest::ResponseCode::NOT_FOUND:
        return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
      default:
        return OperationResult(TRI_ERROR_INTERNAL);
    }
  }
  return OperationResult(res);
}
#endif

//////////////////////////////////////////////////////////////////////////////
/// @brief replace one or multiple documents in a collection, local
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::modifyLocal(
    std::string const& collectionName,
    VPackSlice const newValue,
    OperationOptions& options,
    TRI_voc_document_operation_e operation) {

  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName); 
  LogicalCollection* collection = documentCollection(trxCollection(cid));
  
  if (options.returnOld || options.returnNew) {
    orderDitch(cid); // will throw when it fails 
  }

  // First see whether or not we have to do synchronous replication:
  std::shared_ptr<std::vector<ServerID> const> followers;
  bool doingSynchronousReplication = false;
  if (ServerState::isDBServer(_serverRole)) {
    // Now replicate the same operation on all followers:
    auto const& followerInfo = collection->followers();
    followers = followerInfo->get();
    doingSynchronousReplication = followers->size() > 0;
  }

  // Update/replace are a read and a write, let's get the write lock already
  // for the read operation:
  int res = lock(trxCollection(cid), TRI_TRANSACTION_WRITE);

  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }

  VPackBuilder resultBuilder;  // building the complete result
  TRI_voc_tick_t maxTick = 0;

  auto workForOneDocument = [&](VPackSlice const newVal, bool isBabies) -> int {
    if (!newVal.isObject()) {
      return TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
    }
    TRI_doc_mptr_t mptr;
    VPackSlice actualRevision;
    TRI_doc_mptr_t previous;
    TRI_voc_tick_t resultMarkerTick = 0;

    if (operation == TRI_VOC_DOCUMENT_OPERATION_REPLACE) {
      res = collection->replace(this, newVal, &mptr, options, resultMarkerTick, 
          !isLocked(collection, TRI_TRANSACTION_WRITE), actualRevision,
          previous);
    } else {
      res = collection->update(this, newVal, &mptr, options, resultMarkerTick,
          !isLocked(collection, TRI_TRANSACTION_WRITE), actualRevision,
          previous);
    }
    
    if (resultMarkerTick > 0 && resultMarkerTick > maxTick) {
      maxTick = resultMarkerTick;
    }

    if (res == TRI_ERROR_ARANGO_CONFLICT) {
      // still return 
      if ((!options.silent || doingSynchronousReplication) && !isBabies) {
        StringRef key(newVal.get(StaticStrings::KeyString));
        buildDocumentIdentity(collection, resultBuilder, cid, key, actualRevision,
                              VPackSlice(), 
                              options.returnOld ? &previous : nullptr, nullptr);
      }
      return TRI_ERROR_ARANGO_CONFLICT;
    } else if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }

    TRI_ASSERT(mptr.vpack() != nullptr);

    if (!options.silent || doingSynchronousReplication) {
      StringRef key(newVal.get(StaticStrings::KeyString));
      buildDocumentIdentity(collection, resultBuilder, cid, key, 
          mptr.revisionIdAsSlice(), actualRevision, 
          options.returnOld ? &previous : nullptr , 
          options.returnNew ? &mptr : nullptr);
    }
    return TRI_ERROR_NO_ERROR;
  };

  res = TRI_ERROR_NO_ERROR;
  bool multiCase = newValue.isArray();
  std::unordered_map<int, size_t> errorCounter;
  if (multiCase) {
    {
      VPackArrayBuilder guard(&resultBuilder);
      VPackArrayIterator it(newValue);
      while (it.valid()) {
        res = workForOneDocument(it.value(), true);
        if (res != TRI_ERROR_NO_ERROR) {
          createBabiesError(resultBuilder, errorCounter, res, options.silent);
        }
        ++it;
      }
    }
    res = TRI_ERROR_NO_ERROR;
  } else {
    res = workForOneDocument(newValue, false);
  }
  
  // wait for operation(s) to be synced to disk here 
  if (res == TRI_ERROR_NO_ERROR && options.waitForSync && maxTick > 0 && isSingleOperationTransaction()) {
    arangodb::wal::LogfileManager::instance()->slots()->waitForTick(maxTick);
  }

  if (doingSynchronousReplication && res == TRI_ERROR_NO_ERROR) {
    // In the multi babies case res is always TRI_ERROR_NO_ERROR if we
    // get here, in the single document case, we do not try to replicate
    // in case of an error.
 
    // Now replicate the good operations on all followers:
    auto cc = arangodb::ClusterComm::instance();

    std::string path
        = "/_db/" +
          arangodb::basics::StringUtils::urlEncode(_vocbase->name()) +
          "/_api/document/" +
          arangodb::basics::StringUtils::urlEncode(collection->name())
          + "?isRestore=true";

    VPackBuilder payload;

    auto doOneDoc = [&](VPackSlice const& doc, VPackSlice result) {
      VPackObjectBuilder guard(&payload);
      VPackSlice s = result.get(StaticStrings::KeyString);
      payload.add(StaticStrings::KeyString, s);
      s = result.get(StaticStrings::RevString);
      payload.add(StaticStrings::RevString, s);
      TRI_SanitizeObject(doc, payload);
    };

    VPackSlice ourResult = resultBuilder.slice();
    if (multiCase) {
      VPackArrayBuilder guard(&payload);
      VPackArrayIterator itValue(newValue);
      VPackArrayIterator itResult(ourResult);
      while (itValue.valid() && itResult.valid()) {
        TRI_ASSERT((*itResult).isObject());
        if (!(*itResult).hasKey("error")) {
          doOneDoc(itValue.value(), itResult.value());
        }
        itValue.next();
        itResult.next();
      }
    } else {
      VPackArrayBuilder guard(&payload);
      doOneDoc(newValue, ourResult);
    }
    auto body = std::make_shared<std::string>();
    *body = payload.slice().toJson();

    // Now prepare the requests:
    std::vector<ClusterCommRequest> requests;
    for (auto const& f : *followers) {
      requests.emplace_back("server:" + f, 
          operation == TRI_VOC_DOCUMENT_OPERATION_REPLACE ?
          arangodb::rest::RequestType::PUT :
          arangodb::rest::RequestType::PATCH,
          path, body);
    }
    size_t nrDone = 0;
    size_t nrGood = cc->performRequests(requests, TRX_FOLLOWER_TIMEOUT, nrDone,
                                        Logger::REPLICATION);
    if (nrGood < followers->size()) {
      // we drop all followers that were not successful:
      for (size_t i = 0; i < followers->size(); ++i) {
        bool replicationWorked 
            = requests[i].done &&
              requests[i].result.status == CL_COMM_RECEIVED &&
              (requests[i].result.answer_code == 
                   rest::ResponseCode::ACCEPTED ||
               requests[i].result.answer_code == 
                   rest::ResponseCode::OK);
        if (replicationWorked) {
          bool found;
          requests[i].result.answer->header(StaticStrings::ErrorCodes, found);
          replicationWorked = !found;
        }
        if (!replicationWorked) {
          auto const& followerInfo = collection->followers();
          followerInfo->remove((*followers)[i]);
          LOG_TOPIC(ERR, Logger::REPLICATION)
              << "modifyLocal: dropping follower "
              << (*followers)[i] << " for shard " << collectionName;
        }
      }
    }
  }
  
  if (doingSynchronousReplication && options.silent) {
    // We needed the results, but do not want to report:
    resultBuilder.clear();
  }

  return OperationResult(resultBuilder.steal(), nullptr, "", res,
                         options.waitForSync, errorCounter); 
}

//////////////////////////////////////////////////////////////////////////////
/// @brief remove one or multiple documents in a collection
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::remove(std::string const& collectionName,
                                    VPackSlice const value,
                                    OperationOptions const& options) {
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);

  if (!value.isObject() && !value.isArray() && !value.isString()) {
    // must provide a document object or an array of documents
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  OperationOptions optionsCopy = options;

  if (ServerState::isCoordinator(_serverRole)) {
    return removeCoordinator(collectionName, value, optionsCopy);
  }

  return removeLocal(collectionName, value, optionsCopy);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief remove one or multiple documents in a collection, coordinator
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
//////////////////////////////////////////////////////////////////////////////

#ifndef USE_ENTERPRISE
OperationResult Transaction::removeCoordinator(std::string const& collectionName,
                                               VPackSlice const value,
                                               OperationOptions& options) {

  rest::ResponseCode responseCode;
  std::unordered_map<int, size_t> errorCounter;
  auto resultBody = std::make_shared<VPackBuilder>();

  int res = arangodb::deleteDocumentOnCoordinator(
      _vocbase->name(), collectionName, value, options, responseCode,
      errorCounter, resultBody);

  if (res == TRI_ERROR_NO_ERROR) {
    if (responseCode == rest::ResponseCode::OK ||
        responseCode == rest::ResponseCode::ACCEPTED ||
        responseCode == rest::ResponseCode::PRECONDITION_FAILED) {
      return OperationResult(
          resultBody->steal(), nullptr, "",
          responseCode == rest::ResponseCode::PRECONDITION_FAILED
              ? TRI_ERROR_ARANGO_CONFLICT
              : TRI_ERROR_NO_ERROR,
          responseCode != rest::ResponseCode::ACCEPTED,
          errorCounter);
    } else if (responseCode == rest::ResponseCode::BAD) {
      return DBServerResponseBad(resultBody);
    } else if (responseCode == rest::ResponseCode::NOT_FOUND) {
      return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
    } else {
      return OperationResult(TRI_ERROR_INTERNAL);
    }
  }
  return OperationResult(res);
}
#endif

//////////////////////////////////////////////////////////////////////////////
/// @brief remove one or multiple documents in a collection, local
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::removeLocal(std::string const& collectionName,
                                         VPackSlice const value,
                                         OperationOptions& options) {
  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName); 
  LogicalCollection* collection = documentCollection(trxCollection(cid));
  
  if (options.returnOld) {
    orderDitch(cid); // will throw when it fails 
  }
 
  // First see whether or not we have to do synchronous replication:
  std::shared_ptr<std::vector<ServerID> const> followers;
  bool doingSynchronousReplication = false;
  if (ServerState::isDBServer(_serverRole)) {
    // Now replicate the same operation on all followers:
    auto const& followerInfo = collection->followers();
    followers = followerInfo->get();
    doingSynchronousReplication = followers->size() > 0;
  }

  VPackBuilder resultBuilder;
  TRI_voc_tick_t maxTick = 0;

  auto workOnOneDocument = [&](VPackSlice value, bool isBabies) -> int {
    VPackSlice actualRevision;
    TRI_doc_mptr_t previous;
    TransactionBuilderLeaser builder(this);
    StringRef key;
    if (value.isString()) {
      key = value;
      size_t pos = key.find('/');
      if (pos != std::string::npos) {
        key = key.substr(pos + 1);
        builder->add(VPackValuePair(key.data(), key.length(), VPackValueType::String));
        value = builder->slice();
      }
    } else if (value.isObject()) {
      VPackSlice keySlice = value.get(StaticStrings::KeyString);
      if (!keySlice.isString()) {
        return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
      }
      key = keySlice;
    } else {
      return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
    }

    TRI_voc_tick_t resultMarkerTick = 0;

    int res = collection->remove(this, value, options, resultMarkerTick,
                                 !isLocked(collection, TRI_TRANSACTION_WRITE),
                                 actualRevision, previous);

    if (resultMarkerTick > 0 && resultMarkerTick > maxTick) {
      maxTick = resultMarkerTick;
    }
    
    if (res != TRI_ERROR_NO_ERROR) {
      if (res == TRI_ERROR_ARANGO_CONFLICT && 
          (!options.silent || doingSynchronousReplication) && 
          !isBabies) {
        buildDocumentIdentity(collection, resultBuilder, cid, key,
                              actualRevision, VPackSlice(), 
                              options.returnOld ? &previous : nullptr, nullptr);
      }
      return res;
    }

    if (!options.silent || doingSynchronousReplication) {
      buildDocumentIdentity(collection, resultBuilder, cid, key,
                            actualRevision, VPackSlice(),
                            options.returnOld ? &previous : nullptr, nullptr);
    }

    return TRI_ERROR_NO_ERROR;
  };

  int res = TRI_ERROR_NO_ERROR;
  bool multiCase = value.isArray();
  std::unordered_map<int, size_t> countErrorCodes;
  if (multiCase) {
    VPackArrayBuilder guard(&resultBuilder);
    for (auto const& s : VPackArrayIterator(value)) {
      res = workOnOneDocument(s, true);
      if (res != TRI_ERROR_NO_ERROR) {
        createBabiesError(resultBuilder, countErrorCodes, res, options.silent);
      }
    }
    // With babies the reporting is handled somewhere else.
    res = TRI_ERROR_NO_ERROR;
  } else {
    res = workOnOneDocument(value, false);
  }
 
  // wait for operation(s) to be synced to disk here 
  if (res == TRI_ERROR_NO_ERROR && options.waitForSync && maxTick > 0 && isSingleOperationTransaction()) {
    arangodb::wal::LogfileManager::instance()->slots()->waitForTick(maxTick);
  }

  if (doingSynchronousReplication && res == TRI_ERROR_NO_ERROR) {
    // In the multi babies case res is always TRI_ERROR_NO_ERROR if we
    // get here, in the single document case, we do not try to replicate
    // in case of an error.
 
    // Now replicate the good operations on all followers:
    auto cc = arangodb::ClusterComm::instance();

    std::string path
        = "/_db/" +
          arangodb::basics::StringUtils::urlEncode(_vocbase->name()) +
          "/_api/document/" +
          arangodb::basics::StringUtils::urlEncode(collection->name())
          + "?isRestore=true";

    VPackBuilder payload;

    auto doOneDoc = [&](VPackSlice const& doc, VPackSlice result) {
      VPackObjectBuilder guard(&payload);
      VPackSlice s = result.get(StaticStrings::KeyString);
      payload.add(StaticStrings::KeyString, s);
      s = result.get(StaticStrings::RevString);
      payload.add(StaticStrings::RevString, s);
      TRI_SanitizeObject(doc, payload);
    };

    VPackSlice ourResult = resultBuilder.slice();
    if (value.isArray()) {
      VPackArrayBuilder guard(&payload);
      VPackArrayIterator itValue(value);
      VPackArrayIterator itResult(ourResult);
      while (itValue.valid() && itResult.valid()) {
        TRI_ASSERT((*itResult).isObject());
        if (!(*itResult).hasKey("error")) {
          doOneDoc(itValue.value(), itResult.value());
        }
        itValue.next();
        itResult.next();
      }
    } else {
      VPackArrayBuilder guard(&payload);
      doOneDoc(value, ourResult);
    }
    auto body = std::make_shared<std::string>();
    *body = payload.slice().toJson();

    // Now prepare the requests:
    std::vector<ClusterCommRequest> requests;
    for (auto const& f : *followers) {
      requests.emplace_back("server:" + f, 
                            arangodb::rest::RequestType::DELETE_REQ,
                            path, body);
    }
    size_t nrDone = 0;
    size_t nrGood = cc->performRequests(requests, TRX_FOLLOWER_TIMEOUT, nrDone,
                                        Logger::REPLICATION);
    if (nrGood < followers->size()) {
      // we drop all followers that were not successful:
      for (size_t i = 0; i < followers->size(); ++i) {
        bool replicationWorked 
            = requests[i].done &&
              requests[i].result.status == CL_COMM_RECEIVED &&
              (requests[i].result.answer_code == 
                   rest::ResponseCode::ACCEPTED ||
               requests[i].result.answer_code == 
                   rest::ResponseCode::OK);
        if (replicationWorked) {
          bool found;
          requests[i].result.answer->header(StaticStrings::ErrorCodes, found);
          replicationWorked = !found;
        }
        if (!replicationWorked) {
          auto const& followerInfo = collection->followers();
          followerInfo->remove((*followers)[i]);
          LOG_TOPIC(ERR, Logger::REPLICATION)
              << "removeLocal: dropping follower "
              << (*followers)[i] << " for shard " << collectionName;
        }
      }
    }
  }
  
  if (doingSynchronousReplication && options.silent) {
    // We needed the results, but do not want to report:
    resultBuilder.clear();
  }

  return OperationResult(resultBuilder.steal(), nullptr, "", res,
                         options.waitForSync, countErrorCodes); 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetches all documents in a collection
////////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::all(std::string const& collectionName,
                                 uint64_t skip, uint64_t limit,
                                 OperationOptions const& options) {
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);
  
  OperationOptions optionsCopy = options;

  if (ServerState::isCoordinator(_serverRole)) {
    return allCoordinator(collectionName, skip, limit, optionsCopy);
  }

  return allLocal(collectionName, skip, limit, optionsCopy);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetches all documents in a collection, coordinator
////////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::allCoordinator(std::string const& collectionName,
                                            uint64_t skip, uint64_t limit, 
                                            OperationOptions& options) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetches all documents in a collection, local
////////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::allLocal(std::string const& collectionName,
                                      uint64_t skip, uint64_t limit,
                                      OperationOptions& options) {
  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName); 
  
  orderDitch(cid); // will throw when it fails
  
  int res = lock(trxCollection(cid), TRI_TRANSACTION_READ);

  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }
  
  VPackBuilder resultBuilder;
  resultBuilder.openArray();

  std::unique_ptr<OperationCursor> cursor =
      indexScan(collectionName, Transaction::CursorType::ALL, IndexHandle(),
                {}, skip, limit, 1000, false);

  if (cursor->failed()) {
    return OperationResult(cursor->code);
  }

  std::vector<TRI_doc_mptr_t*> result;
  result.reserve(1000);
  while (cursor->hasMore()) {
    cursor->getMoreMptr(result, 1000);
    for (auto const& mptr : result) {
      resultBuilder.addExternal(mptr->vpack());
    }
  }

  resultBuilder.close();

  res = unlock(trxCollection(cid), TRI_TRANSACTION_READ);

  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }

  return OperationResult(resultBuilder.steal(),
                         _transactionContextPtr->orderCustomTypeHandler(), "",
                         TRI_ERROR_NO_ERROR, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove all documents in a collection
////////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::truncate(std::string const& collectionName,
                                      OperationOptions const& options) {
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);
  
  OperationOptions optionsCopy = options;

  if (ServerState::isCoordinator(_serverRole)) {
    return truncateCoordinator(collectionName, optionsCopy);
  }

  return truncateLocal(collectionName, optionsCopy);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove all documents in a collection, coordinator
////////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::truncateCoordinator(std::string const& collectionName,
                                                 OperationOptions& options) {
  return OperationResult(
      arangodb::truncateCollectionOnCoordinator(_vocbase->name(),
                                                collectionName));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove all documents in a collection, local
////////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::truncateLocal(std::string const& collectionName,
                                           OperationOptions& options) {
  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName); 
  
  orderDitch(cid); // will throw when it fails
  
  int res = lock(trxCollection(cid), TRI_TRANSACTION_WRITE);

  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }
 
  LogicalCollection* collection = documentCollection(trxCollection(cid));
  
  VPackBuilder keyBuilder;
  auto primaryIndex = collection->primaryIndex();

  options.ignoreRevs = true;

  TRI_voc_tick_t resultMarkerTick = 0;

  auto callback = [&](TRI_doc_mptr_t const* mptr) {
    VPackSlice actualRevision;
    TRI_doc_mptr_t previous;
    int res =
        collection->remove(this, VPackSlice(mptr->vpack()), options,
                           resultMarkerTick, false, actualRevision, previous);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    return true;
  };

  try {
    primaryIndex->invokeOnAllElementsForRemoval(callback);
  }
  catch (basics::Exception const& ex) {
    unlock(trxCollection(cid), TRI_TRANSACTION_WRITE);
    return OperationResult(ex.code());
  }
  
  // Now see whether or not we have to do synchronous replication:
  if (ServerState::isDBServer(_serverRole)) {
    std::shared_ptr<std::vector<ServerID> const> followers;
    // Now replicate the same operation on all followers:
    auto const& followerInfo = collection->followers();
    followers = followerInfo->get();
    if (followers->size() > 0) {

      // Now replicate the good operations on all followers:
      auto cc = arangodb::ClusterComm::instance();

      std::string path
          = "/_db/" +
            arangodb::basics::StringUtils::urlEncode(_vocbase->name()) +
            "/_api/collection/" + collectionName + "/truncate";

      auto body = std::make_shared<std::string>();

      // Now prepare the requests:
      std::vector<ClusterCommRequest> requests;
      for (auto const& f : *followers) {
        requests.emplace_back("server:" + f, 
                              arangodb::rest::RequestType::PUT,
                              path, body);
      }
      size_t nrDone = 0;
      size_t nrGood = cc->performRequests(requests, TRX_FOLLOWER_TIMEOUT,
                                          nrDone, Logger::REPLICATION);
      if (nrGood < followers->size()) {
        // we drop all followers that were not successful:
        for (size_t i = 0; i < followers->size(); ++i) {
          bool replicationWorked 
              = requests[i].done &&
                requests[i].result.status == CL_COMM_RECEIVED &&
                (requests[i].result.answer_code == 
                     rest::ResponseCode::ACCEPTED ||
                 requests[i].result.answer_code == 
                     rest::ResponseCode::OK);
          if (!replicationWorked) {
            auto const& followerInfo = collection->followers();
            followerInfo->remove((*followers)[i]);
            LOG_TOPIC(ERR, Logger::REPLICATION)
                << "truncateLocal: dropping follower "
                << (*followers)[i] << " for shard " << collectionName;
          }
        }
      }

    }
  }

  res = unlock(trxCollection(cid), TRI_TRANSACTION_WRITE);

  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }

  return OperationResult(TRI_ERROR_NO_ERROR);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief count the number of documents in a collection
////////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::count(std::string const& collectionName) {
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);

  if (ServerState::isCoordinator(_serverRole)) {
    return countCoordinator(collectionName);
  }

  return countLocal(collectionName);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief count the number of documents in a collection
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::countCoordinator(std::string const& collectionName) {
  uint64_t count = 0;
  int res = arangodb::countOnCoordinator(_vocbase->name(), collectionName, count);

  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }

  VPackBuilder resultBuilder;
  resultBuilder.add(VPackValue(count));

  return OperationResult(resultBuilder.steal(), nullptr, "", TRI_ERROR_NO_ERROR, false);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief count the number of documents in a collection
//////////////////////////////////////////////////////////////////////////////

OperationResult Transaction::countLocal(std::string const& collectionName) {
  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName); 
  
  int res = lock(trxCollection(cid), TRI_TRANSACTION_READ);

  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }
 
  // TODO Temporary until the move to LogicalCollection is completed
  LogicalCollection* collection = documentCollection(trxCollection(cid));

  uint64_t num = collection->numberDocuments();

  res = unlock(trxCollection(cid), TRI_TRANSACTION_READ);
  
  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }
  
  VPackBuilder resultBuilder;
  resultBuilder.add(VPackValue(num));

  return OperationResult(resultBuilder.steal(), nullptr, "", TRI_ERROR_NO_ERROR, false);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief Gets the best fitting index for an AQL condition.
/// note: the caller must have read-locked the underlying collection when
/// calling this method
//////////////////////////////////////////////////////////////////////////////

std::pair<bool, bool> Transaction::getBestIndexHandlesForFilterCondition(
    std::string const& collectionName, arangodb::aql::Ast* ast,
    arangodb::aql::AstNode* root,
    arangodb::aql::Variable const* reference,
    arangodb::aql::SortCondition const* sortCondition,
    size_t itemsInCollection,
    std::vector<IndexHandle>& usedIndexes,
    bool& isSorted) {
  // We can only start after DNF transformation
  TRI_ASSERT(root->type ==
             arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_OR);
  auto indexes = indexesForCollection(collectionName);

  bool canUseForFilter = (root->numMembers() > 0);
  bool canUseForSort = false;
  bool isSparse = false;

  for (size_t i = 0; i < root->numMembers(); ++i) {
    auto node = root->getMemberUnchecked(i);
    arangodb::aql::AstNode* specializedCondition = nullptr;
    auto canUseIndex = findIndexHandleForAndNode(
        indexes, node, reference, sortCondition, itemsInCollection, usedIndexes,
        specializedCondition, isSparse);

    if (canUseIndex.second && !canUseIndex.first) {
      // index can be used for sorting only
      // we need to abort further searching and only return one index
      TRI_ASSERT(!usedIndexes.empty());
      if (usedIndexes.size() > 1) {
        auto sortIndex = usedIndexes.back();

        usedIndexes.clear();
        usedIndexes.emplace_back(sortIndex);
      }

      TRI_ASSERT(usedIndexes.size() == 1);

      if (isSparse) {
        // cannot use a sparse index for sorting alone
        usedIndexes.clear();
      }
      return std::make_pair(false, !usedIndexes.empty());
    }

    canUseForFilter &= canUseIndex.first;
    canUseForSort |= canUseIndex.second;

    root->changeMember(i, specializedCondition);
  }

  if (canUseForFilter) {
    isSorted = sortOrs(ast, root, reference, usedIndexes);
  }

  // should always be true here. maybe not in the future in case a collection
  // has absolutely no indexes
  return std::make_pair(canUseForFilter, canUseForSort);
}


/// @brief Gets the best fitting index for one specific condition.
///        Difference to IndexHandles: Condition is only one NARY_AND
///        and the Condition stays unmodified. Also does not care for sorting
///        Returns false if no index could be found.

bool Transaction::getBestIndexHandleForFilterCondition(
    std::string const& collectionName, arangodb::aql::AstNode*& node,
    arangodb::aql::Variable const* reference, size_t itemsInCollection,
    IndexHandle& usedIndex) {
  // We can only start after DNF transformation and only a single AND
  TRI_ASSERT(node->type ==
             arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_NARY_AND);
  if (node->numMembers() == 0) {
    // Well no index can serve no condition.
    return false;
  }

  auto indexes = indexesForCollection(collectionName);

  // Const cast is save here. Giving computeSpecialisation == false
  // Makes sure node is NOT modified.
  return findIndexHandleForAndNode(
      indexes, node, reference,
      itemsInCollection, usedIndex);
}


//////////////////////////////////////////////////////////////////////////////
/// @brief Checks if the index supports the filter condition.
/// note: the caller must have read-locked the underlying collection when
/// calling this method
//////////////////////////////////////////////////////////////////////////////

bool Transaction::supportsFilterCondition(
    IndexHandle const& indexHandle,
    arangodb::aql::AstNode const* condition,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    size_t& estimatedItems, double& estimatedCost) {
 
  auto idx = indexHandle.getIndex(); 
  if (nullptr == idx) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The index id cannot be empty.");
  }

  return idx->supportsFilterCondition(
      condition, reference, itemsInIndex, estimatedItems, estimatedCost);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief Get the index features:
///        Returns the covered attributes, and sets the first bool value
///        to isSorted and the second bool value to isSparse
//////////////////////////////////////////////////////////////////////////////

std::vector<std::vector<arangodb::basics::AttributeName>>
Transaction::getIndexFeatures(IndexHandle const& indexHandle, bool& isSorted,
                              bool& isSparse) {
  
  auto idx = indexHandle.getIndex(); 
  if (nullptr == idx) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The index id cannot be empty.");
  }

  isSorted = idx->isSorted();
  isSparse = idx->sparse();
  return idx->fields();
}

//////////////////////////////////////////////////////////////////////////////
/// @brief Gets the best fitting index for an AQL sort condition
/// note: the caller must have read-locked the underlying collection when
/// calling this method
//////////////////////////////////////////////////////////////////////////////

std::pair<bool, bool> Transaction::getIndexForSortCondition(
    std::string const& collectionName,
    arangodb::aql::SortCondition const* sortCondition,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    std::vector<IndexHandle>& usedIndexes,
    size_t& coveredAttributes) {
  // We do not have a condition. But we have a sort!
  if (!sortCondition->isEmpty() && sortCondition->isOnlyAttributeAccess() &&
      sortCondition->isUnidirectional()) {
    double bestCost = 0.0;
    std::shared_ptr<Index> bestIndex;

    auto indexes = indexesForCollection(collectionName);

    for (auto const& idx : indexes) {
      if (idx->sparse()) {
        // a sparse index may exclude some documents, so it can't be used to
        // get a sorted view of the ENTIRE collection
        continue;
      }
      double sortCost = 0.0;
      size_t covered = 0;
      if (indexSupportsSort(idx.get(), reference, sortCondition, itemsInIndex,
                            sortCost, covered)) {
        if (bestIndex == nullptr || sortCost < bestCost) {
          bestCost = sortCost;
          bestIndex = idx;
          coveredAttributes = covered;
        }
      }
    }

    if (bestIndex != nullptr) {
      usedIndexes.emplace_back(bestIndex);
    }

    return std::make_pair(false, bestIndex != nullptr);
  }

  // No Index and no sort condition that
  // can be supported by an index.
  // Nothing to do here.
  return std::make_pair(false, false);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief factory for OperationCursor objects from AQL
/// note: the caller must have read-locked the underlying collection when
/// calling this method
//////////////////////////////////////////////////////////////////////////////

OperationCursor* Transaction::indexScanForCondition(
    IndexHandle const& indexId,
    arangodb::aql::AstNode const* condition, arangodb::aql::Variable const* var,
    uint64_t limit, uint64_t batchSize, bool reverse) {
  if (ServerState::isCoordinator(_serverRole)) {
    // The index scan is only available on DBServers and Single Server.
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
  }

  if (limit == 0) {
    // nothing to do
    return new OperationCursor(TRI_ERROR_NO_ERROR);
  }

  // data that we pass to the iterator
  IndexIteratorContext ctxt(_vocbase, resolver(), _serverRole);
 
  auto idx = indexId.getIndex();
  if (nullptr == idx) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The index id cannot be empty.");
  }

  // Now create the Iterator
  std::unique_ptr<IndexIterator> iterator(idx->iteratorForCondition(this, &ctxt, condition, var, reverse));

  if (iterator == nullptr) {
    // We could not create an ITERATOR and it did not throw an error itself
    return new OperationCursor(TRI_ERROR_OUT_OF_MEMORY);
  }

  return new OperationCursor(iterator.release(), limit, batchSize);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief factory for OperationCursor objects
/// note: the caller must have read-locked the underlying collection when
/// calling this method
//////////////////////////////////////////////////////////////////////////////

std::unique_ptr<OperationCursor> Transaction::indexScan(
    std::string const& collectionName, CursorType cursorType,
    IndexHandle const& indexId, VPackSlice const search, uint64_t skip,
    uint64_t limit, uint64_t batchSize, bool reverse) {
  // For now we assume indexId is the iid part of the index.

  if (ServerState::isCoordinator(_serverRole)) {
    // The index scan is only available on DBServers and Single Server.
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
  }

  if (limit == 0) {
    // nothing to do
    return std::make_unique<OperationCursor>(TRI_ERROR_NO_ERROR);
  }

  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName); 
  LogicalCollection* document = documentCollection(trxCollection(cid));
  
  orderDitch(cid); // will throw when it fails 

  std::unique_ptr<IndexIterator> iterator;

  switch (cursorType) {
    case CursorType::ANY: {
      // We do not need search values
      TRI_ASSERT(search.isNone());
      // We do not need an index either
      TRI_ASSERT(nullptr == indexId.getIndex());

      arangodb::PrimaryIndex* idx = document->primaryIndex();

      if (idx == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_ARANGO_INDEX_NOT_FOUND,
            "Could not find primary index in collection '" + collectionName + "'.");
      }

      iterator.reset(idx->anyIterator(this));
      break;
    }
    case CursorType::ALL: {
      // We do not need search values
      TRI_ASSERT(search.isNone());
      // We do not need an index either
      TRI_ASSERT(nullptr == indexId.getIndex());

      arangodb::PrimaryIndex* idx = document->primaryIndex();

      if (idx == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_ARANGO_INDEX_NOT_FOUND,
            "Could not find primary index in collection '" + collectionName + "'.");
      }

      iterator.reset(idx->allIterator(this, reverse));
      break;
    }
    case CursorType::INDEX: {
      auto idx = indexId.getIndex();
      if (nullptr == idx) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                       "The index id cannot be empty.");
      }
      // Normalize the search values
      // VPackBuilder expander;
      // idx->expandInSearchValues(search, expander);

      // Now collect the Iterator
      IndexIteratorContext ctxt(_vocbase, resolver(), _serverRole);
      iterator.reset(idx->iteratorForSlice(this, &ctxt, search, reverse));
    }
  }
  if (iterator == nullptr) {
    // We could not create an ITERATOR and it did not throw an error itself
    return std::make_unique<OperationCursor>(TRI_ERROR_OUT_OF_MEMORY);
  }

  uint64_t unused = 0;
  iterator->skip(skip, unused);

  return std::make_unique<OperationCursor>(iterator.release(), limit, batchSize);
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection
////////////////////////////////////////////////////////////////////////////////

arangodb::LogicalCollection* Transaction::documentCollection(
      TRI_transaction_collection_t const* trxCollection) const {
  TRI_ASSERT(_trx != nullptr);
  TRI_ASSERT(trxCollection != nullptr);
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);
  TRI_ASSERT(trxCollection->_collection != nullptr);

  return trxCollection->_collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection
////////////////////////////////////////////////////////////////////////////////

arangodb::LogicalCollection* Transaction::documentCollection(
      TRI_voc_cid_t cid) const {
  TRI_ASSERT(_trx != nullptr);
  TRI_ASSERT(getStatus() == TRI_TRANSACTION_RUNNING);
  
  auto trxCollection = TRI_GetCollectionTransaction(_trx, cid, TRI_TRANSACTION_READ);
  TRI_ASSERT(trxCollection->_collection != nullptr);
  return trxCollection->_collection;
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection by id, with the name supplied
////////////////////////////////////////////////////////////////////////////////

int Transaction::addCollection(TRI_voc_cid_t cid, char const* name,
                    TRI_transaction_type_e type) {
  int res = this->addCollection(cid, type);

  if (res != TRI_ERROR_NO_ERROR) {
    _errorData = std::string(name);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection by id, with the name supplied
////////////////////////////////////////////////////////////////////////////////

int Transaction::addCollection(TRI_voc_cid_t cid, std::string const& name,
                    TRI_transaction_type_e type) {
  return addCollection(cid, name.c_str(), type);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection by id
////////////////////////////////////////////////////////////////////////////////

int Transaction::addCollection(TRI_voc_cid_t cid, TRI_transaction_type_e type) {
  if (_trx == nullptr) {
    return registerError(TRI_ERROR_INTERNAL);
  }

  if (cid == 0) {
    // invalid cid
    return registerError(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }

  if (_setupState != TRI_ERROR_NO_ERROR) {
    return _setupState;
  }

  TRI_transaction_status_e const status = getStatus();

  if (status == TRI_TRANSACTION_COMMITTED ||
      status == TRI_TRANSACTION_ABORTED) {
    // transaction already finished?
    return registerError(TRI_ERROR_TRANSACTION_INTERNAL);
  }

  if (isEmbeddedTransaction()) {
   return addCollectionEmbedded(cid, type);
  } 

  return addCollectionToplevel(cid, type);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection by name
////////////////////////////////////////////////////////////////////////////////

int Transaction::addCollection(std::string const& name, TRI_transaction_type_e type) {
  if (_setupState != TRI_ERROR_NO_ERROR) {
    return _setupState;
  }

  return addCollection(this->resolver()->getCollectionId(name),
                       name.c_str(), type);
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief test if a collection is already locked
////////////////////////////////////////////////////////////////////////////////

bool Transaction::isLocked(LogicalCollection* document,
                TRI_transaction_type_e type) {
  if (_trx == nullptr || getStatus() != TRI_TRANSACTION_RUNNING) {
    return false;
  }

  TRI_transaction_collection_t* trxCollection =
      TRI_GetCollectionTransaction(_trx, document->cid(), type);
  TRI_ASSERT(trxCollection != nullptr);
  return TRI_IsLockedCollectionTransaction(trxCollection, type, _nestingLevel);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read- or write-lock a collection
////////////////////////////////////////////////////////////////////////////////

int Transaction::lock(TRI_transaction_collection_t* trxCollection,
           TRI_transaction_type_e type) {
  if (_trx == nullptr || getStatus() != TRI_TRANSACTION_RUNNING) {
    return TRI_ERROR_TRANSACTION_INTERNAL;
  }

  return TRI_LockCollectionTransaction(trxCollection, type, _nestingLevel);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read- or write-unlock a collection
////////////////////////////////////////////////////////////////////////////////

int Transaction::unlock(TRI_transaction_collection_t* trxCollection,
             TRI_transaction_type_e type) {
  if (_trx == nullptr || getStatus() != TRI_TRANSACTION_RUNNING) {
    return TRI_ERROR_TRANSACTION_INTERNAL;
  }

  return TRI_UnlockCollectionTransaction(trxCollection, type, _nestingLevel);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get list of indexes for a collection
////////////////////////////////////////////////////////////////////////////////

std::vector<std::shared_ptr<Index>> Transaction::indexesForCollection(
    std::string const& collectionName) {

  if (ServerState::isCoordinator(_serverRole)) {
    return indexesForCollectionCoordinator(collectionName);
  }
  // For a DBserver we use the local case.

  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName); 
  LogicalCollection* document = documentCollection(trxCollection(cid));
  return document->getIndexes();
}


/// @brief Lock all collections. Only works for selected sub-classes

int Transaction::lockCollections() {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

/// @brief Clone this transaction. Only works for selected sub-classes

Transaction* Transaction::clone() const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief Get all indexes for a collection name, coordinator case
//////////////////////////////////////////////////////////////////////////////

std::shared_ptr<Index> Transaction::indexForCollectionCoordinator(
    std::string const& name, std::string const& id) const {
  auto clusterInfo = arangodb::ClusterInfo::instance();
  auto collectionInfo =
      clusterInfo->getCollection(_vocbase->name(), name);

  if (collectionInfo.get() == nullptr) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_INTERNAL,
                                  "collection not found '%s' in database '%s'",
                                  name.c_str(), _vocbase->name().c_str());
  }

  auto idxs = collectionInfo->getIndexes();
  TRI_idx_iid_t iid = basics::StringUtils::uint64(id);
  for (auto const& it : idxs) {
    if (it->id() == iid) {
      return it;
    }
  }
  return nullptr;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief Get all indexes for a collection name, coordinator case
//////////////////////////////////////////////////////////////////////////////

std::vector<std::shared_ptr<Index>>
Transaction::indexesForCollectionCoordinator(std::string const& name) const {
  auto clusterInfo = arangodb::ClusterInfo::instance();
  auto collectionInfo =
      clusterInfo->getCollection(_vocbase->name(), name);

  if (collectionInfo.get() == nullptr) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_INTERNAL,
                                  "collection not found '%s' in database '%s'",
                                  name.c_str(), _vocbase->name().c_str());
  }
  return collectionInfo->getIndexes();
}

//////////////////////////////////////////////////////////////////////////////
/// @brief get the index by it's identifier. Will either throw or
///        return a valid index. nullptr is impossible.
//////////////////////////////////////////////////////////////////////////////

Transaction::IndexHandle Transaction::getIndexByIdentifier(
    std::string const& collectionName, std::string const& indexHandle) {

  if (ServerState::isCoordinator(_serverRole)) {
    if (indexHandle.empty()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "The index id cannot be empty.");
    }

    if (!arangodb::Index::validateId(indexHandle.c_str())) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_INDEX_HANDLE_BAD);
    }

    std::shared_ptr<Index> idx =
        indexForCollectionCoordinator(collectionName, indexHandle);

    if (idx == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_INDEX_NOT_FOUND,
                                     "Could not find index '" + indexHandle +
                                         "' in collection '" +
                                         collectionName + "'.");
    }

    // We have successfully found an index with the requested id.
    return IndexHandle(idx);
  }

  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName); 
  LogicalCollection* document = documentCollection(trxCollection(cid));

  if (indexHandle.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The index id cannot be empty.");
  }

  if (!arangodb::Index::validateId(indexHandle.c_str())) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_INDEX_HANDLE_BAD);
  }
  TRI_idx_iid_t iid = arangodb::basics::StringUtils::uint64(indexHandle);
  std::shared_ptr<arangodb::Index> idx = document->lookupIndex(iid);

  if (idx == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_INDEX_NOT_FOUND,
                                   "Could not find index '" + indexHandle +
                                       "' in collection '" +
                                       collectionName + "'.");
  }
  
  // We have successfully found an index with the requested id.
  return IndexHandle(idx);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to an embedded transaction
////////////////////////////////////////////////////////////////////////////////

int Transaction::addCollectionEmbedded(TRI_voc_cid_t cid, TRI_transaction_type_e type) {
  TRI_ASSERT(_trx != nullptr);

  int res = TRI_AddCollectionTransaction(_trx, cid, type, _nestingLevel,
                                         false, _allowImplicitCollections);

  if (res != TRI_ERROR_NO_ERROR) {
    if (res == TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION) {
      // special error message to indicate which collection was undeclared
      THROW_ARANGO_EXCEPTION_MESSAGE(res, std::string(TRI_errno_string(res)) + ": " + resolver()->getCollectionNameCluster(cid) + " [" + TRI_TransactionTypeGetStr(type) + "]");
    }
    return registerError(res);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to a top-level transaction
////////////////////////////////////////////////////////////////////////////////

int Transaction::addCollectionToplevel(TRI_voc_cid_t cid, TRI_transaction_type_e type) {
  TRI_ASSERT(_trx != nullptr);

  int res;

  if (getStatus() != TRI_TRANSACTION_CREATED) {
    // transaction already started?
    res = TRI_ERROR_TRANSACTION_INTERNAL;
  } else {
    res = TRI_AddCollectionTransaction(_trx, cid, type, _nestingLevel, false,
                                       _allowImplicitCollections);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    if (res == TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION) {
      // special error message to indicate which collection was undeclared
      THROW_ARANGO_EXCEPTION_MESSAGE(res, std::string(TRI_errno_string(res)) + ": " + resolver()->getCollectionNameCluster(cid) + " [" + TRI_TransactionTypeGetStr(type) + "]");
    }
    registerError(res);
  }

  return res;
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief initialize the transaction
/// this will first check if the transaction is embedded in a parent
/// transaction. if not, it will create a transaction of its own
////////////////////////////////////////////////////////////////////////////////

int Transaction::setupTransaction() {
  // check in the context if we are running embedded
  _trx = this->_transactionContext->getParentTransaction();

  if (_trx != nullptr) {
    // yes, we are embedded
    _setupState = setupEmbedded();
    _allowImplicitCollections = _trx->_allowImplicit;
  } else {
    // non-embedded
    _setupState = setupToplevel();
  }

  // this may well be TRI_ERROR_NO_ERROR...
  return _setupState;
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief set up an embedded transaction
////////////////////////////////////////////////////////////////////////////////

int Transaction::setupEmbedded() {
  TRI_ASSERT(_nestingLevel == 0);

  _nestingLevel = ++_trx->_nestingLevel;

  if (!this->_transactionContext->isEmbeddable()) {
    // we are embedded but this is disallowed...
    return TRI_ERROR_TRANSACTION_NESTED;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set up a top-level transaction
////////////////////////////////////////////////////////////////////////////////

int Transaction::setupToplevel() {
  TRI_ASSERT(_nestingLevel == 0);

  // we are not embedded. now start our own transaction
  try {
    _trx = new TRI_transaction_t(_vocbase, _timeout, _waitForSync);
  } catch (...) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  TRI_ASSERT(_trx != nullptr);

  // register the transaction in the context
  return this->_transactionContext->registerTransaction(_trx);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free transaction
////////////////////////////////////////////////////////////////////////////////

void Transaction::freeTransaction() {
  TRI_ASSERT(!isEmbeddedTransaction());

  if (_trx != nullptr) {
    auto id = _trx->_id;
    bool hasFailedOperations = _trx->hasFailedOperations();
    delete _trx;
    _trx = nullptr;
      
    // store result
    _transactionContext->storeTransactionResult(id, hasFailedOperations);
    _transactionContext->unregisterTransaction();
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief constructor, leases a StringBuffer
//////////////////////////////////////////////////////////////////////////////

StringBufferLeaser::StringBufferLeaser(arangodb::Transaction* trx) 
      : _transactionContext(trx->transactionContextPtr()),
        _stringBuffer(_transactionContext->leaseStringBuffer(32)) {
}

//////////////////////////////////////////////////////////////////////////////
/// @brief constructor, leases a StringBuffer
//////////////////////////////////////////////////////////////////////////////

StringBufferLeaser::StringBufferLeaser(arangodb::TransactionContext* transactionContext) 
      : _transactionContext(transactionContext), 
        _stringBuffer(_transactionContext->leaseStringBuffer(32)) {
}

//////////////////////////////////////////////////////////////////////////////
/// @brief destructor, returns a StringBuffer
//////////////////////////////////////////////////////////////////////////////

StringBufferLeaser::~StringBufferLeaser() { 
  _transactionContext->returnStringBuffer(_stringBuffer);
}
  
//////////////////////////////////////////////////////////////////////////////
/// @brief constructor, leases a builder
//////////////////////////////////////////////////////////////////////////////

TransactionBuilderLeaser::TransactionBuilderLeaser(arangodb::Transaction* trx) 
      : _transactionContext(trx->transactionContextPtr()), 
        _builder(_transactionContext->leaseBuilder()) {
  TRI_ASSERT(_builder != nullptr);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief constructor, leases a builder
//////////////////////////////////////////////////////////////////////////////

TransactionBuilderLeaser::TransactionBuilderLeaser(arangodb::TransactionContext* transactionContext) 
      : _transactionContext(transactionContext), 
        _builder(_transactionContext->leaseBuilder()) {
  TRI_ASSERT(_builder != nullptr);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief destructor, returns a builder
//////////////////////////////////////////////////////////////////////////////

TransactionBuilderLeaser::~TransactionBuilderLeaser() { 
  if (_builder != nullptr) {
    _transactionContext->returnBuilder(_builder); 
  }
}

