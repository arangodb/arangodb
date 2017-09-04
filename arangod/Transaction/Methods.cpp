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

#include "Methods.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Condition.h"
#include "Aql/SortCondition.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/encoding.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/FollowerInfo.h"
#include "Cluster/ServerState.h"
#include "Indexes/Index.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Options.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/Events.h"
#include "Utils/OperationCursor.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/Methods/Indexes.h"
#include "VocBase/ticks.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::transaction;
using namespace arangodb::transaction::helpers;

namespace {

static void throwCollectionNotFound(char const* name) {
  if (name == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
      std::string(TRI_errno_string(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND)) +
          ": " + name);
}

/// @brief tests if the given index supports the sort condition
static bool indexSupportsSort(Index const* idx,
                              arangodb::aql::Variable const* reference,
                              arangodb::aql::SortCondition const* sortCondition,
                              size_t itemsInIndex, double& estimatedCost,
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

/// @brief Return an Operation Result that parses the error information returned
///        by the DBServer.
static OperationResult dbServerResponseBad(
    std::shared_ptr<VPackBuilder> resultBody) {
  VPackSlice res = resultBody->slice();
  return OperationResult(
      arangodb::basics::VelocyPackHelper::getNumericValue<int>(
          res, "errorNum", TRI_ERROR_INTERNAL),
      arangodb::basics::VelocyPackHelper::getStringValue(
          res, "errorMessage", "JSON sent to DBserver was bad"));
}

/// @brief Insert an error reported instead of the new document
static void createBabiesError(VPackBuilder& builder,
                              std::unordered_map<int, size_t>& countErrorCodes,
                              Result error, bool silent) {
  if (!silent) {
    builder.openObject();
    builder.add("error", VPackValue(true));
    builder.add("errorNum", VPackValue(error.errorNumber()));
    builder.add("errorMessage", VPackValue(error.errorMessage()));
    builder.close();
  }

  auto it = countErrorCodes.find(error.errorNumber());
  if (it == countErrorCodes.end()) {
    countErrorCodes.emplace(error.errorNumber(), 1);
  } else {
    it->second++;
  }
}

static OperationResult emptyResult(bool waitForSync) {
  VPackBuilder resultBuilder;
  resultBuilder.openArray();
  resultBuilder.close();
  std::unordered_map<int, size_t> errorCounter;
  return OperationResult(resultBuilder.steal(), nullptr, "", TRI_ERROR_NO_ERROR,
                         waitForSync, errorCounter);
}
}  // namespace

/// @brief Get the field names of the used index
std::vector<std::vector<std::string>>
transaction::Methods::IndexHandle::fieldNames() const {
  return _index->fieldNames();
}

/// @brief IndexHandle getter method
std::shared_ptr<arangodb::Index> transaction::Methods::IndexHandle::getIndex()
    const {
  return _index;
}

/// @brief IndexHandle toVelocyPack method passthrough
void transaction::Methods::IndexHandle::toVelocyPack(
    arangodb::velocypack::Builder& builder, bool withFigures) const {
  _index->toVelocyPack(builder, withFigures, false);
}

TRI_vocbase_t* transaction::Methods::vocbase() const {
  return _state->vocbase();
}

/// @brief whether or not the transaction consists of a single operation only
bool transaction::Methods::isSingleOperationTransaction() const {
  return _state->isSingleOperation();
}

/// @brief get the status of the transaction
transaction::Status transaction::Methods::status() const {
  return _state->status();
}

/// @brief sort ORs for the same attribute so they are in ascending value
/// order. this will only work if the condition is for a single attribute
/// the usedIndexes vector may also be re-sorted
bool transaction::Methods::sortOrs(
    arangodb::aql::Ast* ast, arangodb::aql::AstNode* root,
    arangodb::aql::Variable const* variable,
    std::vector<transaction::Methods::IndexHandle>& usedIndexes) {
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

  typedef std::pair<arangodb::aql::AstNode*, transaction::Methods::IndexHandle>
      ConditionData;
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

    if (operand->type ==
            arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_NE ||
        operand->type ==
            arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_NIN) {
      return false;
    }

    auto lhs = operand->getMember(0);
    auto rhs = operand->getMember(1);

    if (lhs->type == arangodb::aql::AstNodeType::NODE_TYPE_ATTRIBUTE_ACCESS) {
      std::pair<arangodb::aql::Variable const*,
                std::vector<arangodb::basics::AttributeName>>
          result;

      if (rhs->isConstant() && lhs->isAttributeAccessForVariable(result) &&
          result.first == variable &&
          (operand->type !=
               arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_IN ||
           rhs->isArray())) {
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
      std::pair<arangodb::aql::Variable const*,
                std::vector<arangodb::basics::AttributeName>>
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
    if (parts[i].operatorType ==
            arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_IN &&
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

std::pair<bool, bool> transaction::Methods::findIndexHandleForAndNode(
    std::vector<std::shared_ptr<Index>> indexes, arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference,
    arangodb::aql::SortCondition const* sortCondition, size_t itemsInCollection,
    std::vector<transaction::Methods::IndexHandle>& usedIndexes,
    arangodb::aql::AstNode*& specializedCondition, bool& isSparse) const {
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

bool transaction::Methods::findIndexHandleForAndNode(
    std::vector<std::shared_ptr<Index>> indexes, arangodb::aql::AstNode*& node,
    arangodb::aql::Variable const* reference, size_t itemsInCollection,
    transaction::Methods::IndexHandle& usedIndex) const {
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

/// @brief Find out if any of the given requests has ended in a refusal

static bool findRefusal(std::vector<ClusterCommRequest>& requests) {
  for (size_t i = 0; i < requests.size(); ++i) {
    if (requests[i].done &&
        requests[i].result.status == CL_COMM_RECEIVED &&
        requests[i].result.answer_code == rest::ResponseCode::NOT_ACCEPTABLE) {
      return true;
    }
  }
  return false;
}

transaction::Methods::Methods(
    std::shared_ptr<transaction::Context> const& transactionContext,
    transaction::Options const& options)
    : _state(nullptr),
      _transactionContext(transactionContext),
      _transactionContextPtr(transactionContext.get()) {
  TRI_ASSERT(_transactionContextPtr != nullptr);

  TRI_vocbase_t* vocbase = _transactionContextPtr->vocbase();

  // brief initialize the transaction
  // this will first check if the transaction is embedded in a parent
  // transaction. if not, it will create a transaction of its own
  // check in the context if we are running embedded
  TransactionState* parent = _transactionContextPtr->getParentTransaction();

  if (parent != nullptr) {
    // yes, we are embedded
    setupEmbedded(vocbase);
  } else {
    // non-embedded
    setupToplevel(vocbase, options);
  }

  TRI_ASSERT(_state != nullptr);
}

/// @brief destroy the transaction
transaction::Methods::~Methods() {
  if (_state->isEmbeddedTransaction()) {
    _state->decreaseNesting();
  } else {
    if (_state->status() == transaction::Status::RUNNING) {
      // auto abort a running transaction
      try {
        this->abort();
        TRI_ASSERT(_state->status() != transaction::Status::RUNNING);
      } catch (...) {
        // must never throw because we are in a dtor
      }
    }

    // free the state associated with the transaction
    TRI_ASSERT(_state->status() != transaction::Status::RUNNING);
    // store result
    _transactionContextPtr->storeTransactionResult(
        _state->id(), _state->hasFailedOperations());
    _transactionContextPtr->unregisterTransaction();

    delete _state;
    _state = nullptr;
  }
}

/// @brief return the collection name resolver
CollectionNameResolver const* transaction::Methods::resolver() const {
  return _transactionContextPtr->getResolver();
}

/// @brief return the transaction collection for a document collection
TransactionCollection* transaction::Methods::trxCollection(
    TRI_voc_cid_t cid) const {
  TRI_ASSERT(_state != nullptr);
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  return _state->collection(cid, AccessMode::Type::READ);
}

/// @brief order a ditch for a collection
void transaction::Methods::pinData(TRI_voc_cid_t cid) {
  TRI_ASSERT(_state != nullptr);
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING ||
             _state->status() == transaction::Status::CREATED);

  TransactionCollection* trxCollection =
      _state->collection(cid, AccessMode::Type::READ);

  if (trxCollection == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "unable to determine transaction collection");
  }

  TRI_ASSERT(trxCollection->collection() != nullptr);

  _transactionContextPtr->pinData(trxCollection->collection());
}

/// @brief whether or not a ditch has been created for the collection
bool transaction::Methods::isPinned(TRI_voc_cid_t cid) const {
  return _transactionContextPtr->isPinned(cid);
}

/// @brief extract the _id attribute from a slice, and convert it into a
/// string
std::string transaction::Methods::extractIdString(VPackSlice slice) {
  return transaction::helpers::extractIdString(resolver(), slice, VPackSlice());
}

/// @brief build a VPack object with _id, _key and _rev, the result is
/// added to the builder in the argument as a single object.
void transaction::Methods::buildDocumentIdentity(
    LogicalCollection* collection, VPackBuilder& builder, TRI_voc_cid_t cid,
    StringRef const& key, TRI_voc_rid_t rid, TRI_voc_rid_t oldRid,
    ManagedDocumentResult const* oldDoc, ManagedDocumentResult const* newDoc) {
  std::string temp; // TODO: pass a string into this function
  temp.reserve(64);

  if (_state->isRunningInCluster()) {
    std::string resolved = resolver()->getCollectionNameCluster(cid);
#ifdef USE_ENTERPRISE
    if (resolved.compare(0, 7, "_local_") == 0) {
      resolved.erase(0, 7);
    } else if (resolved.compare(0, 6, "_from_") == 0) {
      resolved.erase(0, 6);
    } else if (resolved.compare(0, 4, "_to_") == 0) {
      resolved.erase(0, 4);
    }
#endif
    // build collection name
    temp.append(resolved);
  } else {
    // build collection name
    temp.append(collection->name());
  }

  // append / and key part
  temp.push_back('/');
  temp.append(key.begin(), key.size());

  builder.openObject();
  builder.add(StaticStrings::IdString, VPackValue(temp));

  builder.add(StaticStrings::KeyString,
              VPackValuePair(key.data(), key.length(), VPackValueType::String));

  TRI_ASSERT(rid != 0);
  builder.add(StaticStrings::RevString, VPackValue(TRI_RidToString(rid)));

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
  builder.close();
}

/// @brief begin the transaction
Result transaction::Methods::begin() {
  if (_state == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "invalid transaction state");
  }

  if (_state->isCoordinator()) {
    if (_state->isTopLevelTransaction()) {
      _state->updateStatus(transaction::Status::RUNNING);
    }
    return TRI_ERROR_NO_ERROR;
  }

  return _state->beginTransaction(_localHints);
}

/// @brief commit / finish the transaction
Result transaction::Methods::commit() {
  if (_state == nullptr || _state->status() != transaction::Status::RUNNING) {
    // transaction not created or not running
    return TRI_ERROR_TRANSACTION_INTERNAL;
  }

  CallbackInvoker invoker(this);

  if (_state->isCoordinator()) {
    if (_state->isTopLevelTransaction()) {
      _state->updateStatus(transaction::Status::COMMITTED);
    }
    return TRI_ERROR_NO_ERROR;
  }

  return _state->commitTransaction(this);
}

/// @brief abort the transaction
Result transaction::Methods::abort() {
  if (_state == nullptr || _state->status() != transaction::Status::RUNNING) {
    // transaction not created or not running
    return TRI_ERROR_TRANSACTION_INTERNAL;
  }

  CallbackInvoker invoker(this);

  if (_state->isCoordinator()) {
    if (_state->isTopLevelTransaction()) {
      _state->updateStatus(transaction::Status::ABORTED);
    }

    return TRI_ERROR_NO_ERROR;
  }

  return _state->abortTransaction(this);
}

/// @brief finish a transaction (commit or abort), based on the previous state
Result transaction::Methods::finish(int errorNum) {
  if (errorNum == TRI_ERROR_NO_ERROR) {
    // there was no previous error, so we'll commit
    return this->commit();
  }

  // there was a previous error, so we'll abort
  this->abort();

  // return original error number
  return errorNum;
}

/// @brief finish a transaction (commit or abort), based on the previous state
Result transaction::Methods::finish(Result const& res) {
  if (res.ok()) {
    // there was no previous error, so we'll commit
    return this->commit();
  }

  // there was a previous error, so we'll abort
  this->abort();

  // return original error
  return res;
}

std::string transaction::Methods::name(TRI_voc_cid_t cid) const {
  auto c = trxCollection(cid);
  TRI_ASSERT(c != nullptr);
  return c->collectionName();
}

/// @brief read all master pointers, using skip and limit.
/// The resualt guarantees that all documents are contained exactly once
/// as long as the collection is not modified.
OperationResult transaction::Methods::any(std::string const& collectionName,
                                          uint64_t skip, uint64_t limit) {
  if (_state->isCoordinator()) {
    return anyCoordinator(collectionName, skip, limit);
  }
  return anyLocal(collectionName, skip, limit);
}

/// @brief fetches documents in a collection in random order, coordinator
OperationResult transaction::Methods::anyCoordinator(std::string const&,
                                                     uint64_t, uint64_t) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

/// @brief fetches documents in a collection in random order, local
OperationResult transaction::Methods::anyLocal(
    std::string const& collectionName, uint64_t skip, uint64_t limit) {
  TRI_voc_cid_t cid = resolver()->getCollectionIdLocal(collectionName);

  if (cid == 0) {
    throwCollectionNotFound(collectionName.c_str());
  }

  pinData(cid);  // will throw when it fails

  Result res = lock(trxCollection(cid), AccessMode::Type::READ);

  if (!res.ok()) {
    return OperationResult(res);
  }

  VPackBuilder resultBuilder;
  resultBuilder.openArray();

  ManagedDocumentResult mmdr;
  std::unique_ptr<OperationCursor> cursor =
      indexScan(collectionName, transaction::Methods::CursorType::ANY, &mmdr,
                skip, limit, 1000, false);

  cursor->allDocuments([&resultBuilder](DocumentIdentifierToken const& token, VPackSlice slice) {
    resultBuilder.add(slice);
  });

  resultBuilder.close();

  res = unlock(trxCollection(cid), AccessMode::Type::READ);

  if (!res.ok()) {
    return OperationResult(res);
  }

  return OperationResult(resultBuilder.steal(),
                         _transactionContextPtr->orderCustomTypeHandler(), "",
                         TRI_ERROR_NO_ERROR, false);
}

TRI_voc_cid_t transaction::Methods::addCollectionAtRuntime(
    TRI_voc_cid_t cid, std::string const& collectionName,
    AccessMode::Type type) {
  auto collection = trxCollection(cid);

  if (collection == nullptr) {
    int res = _state->addCollection(cid, type, _state->nestingLevel(), true);

    if (res != TRI_ERROR_NO_ERROR) {
      if (res == TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION) {
        // special error message to indicate which collection was undeclared
        THROW_ARANGO_EXCEPTION_MESSAGE(
            res, std::string(TRI_errno_string(res)) + ": " + collectionName +
                     " [" + AccessMode::typeString(type) + "]");
      }
      THROW_ARANGO_EXCEPTION(res);
    }
    _state->ensureCollections(_state->nestingLevel());
    collection = trxCollection(cid);

    if (collection == nullptr) {
      throwCollectionNotFound(collectionName.c_str());
    }
  }
  TRI_ASSERT(collection != nullptr);
  return cid;
}

/// @brief add a collection to the transaction for read, at runtime
TRI_voc_cid_t transaction::Methods::addCollectionAtRuntime(
    std::string const& collectionName) {
  if (collectionName == _collectionCache.name && !collectionName.empty()) {
    return _collectionCache.cid;
  }

  auto cid = resolver()->getCollectionIdLocal(collectionName);

  if (cid == 0) {
    throwCollectionNotFound(collectionName.c_str());
  }
  addCollectionAtRuntime(cid, collectionName);
  _collectionCache.cid = cid;
  _collectionCache.name = collectionName;
  return cid;
}

/// @brief return the type of a collection
bool transaction::Methods::isEdgeCollection(std::string const& collectionName) {
  return getCollectionType(collectionName) == TRI_COL_TYPE_EDGE;
}

/// @brief return the type of a collection
bool transaction::Methods::isDocumentCollection(
    std::string const& collectionName) {
  return getCollectionType(collectionName) == TRI_COL_TYPE_DOCUMENT;
}

/// @brief return the type of a collection
TRI_col_type_e transaction::Methods::getCollectionType(
    std::string const& collectionName) {
  if (_state->isCoordinator()) {
    return resolver()->getCollectionTypeCluster(collectionName);
  }
  return resolver()->getCollectionType(collectionName);
}

/// @brief return the name of a collection
std::string transaction::Methods::collectionName(TRI_voc_cid_t cid) {
  return resolver()->getCollectionName(cid);
}

/// @brief Iterate over all elements of the collection.
void transaction::Methods::invokeOnAllElements(
    std::string const& collectionName,
    std::function<bool(DocumentIdentifierToken const&)> callback) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);
  if (_state->isCoordinator()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName);
  TransactionCollection* trxCol = trxCollection(cid);
  LogicalCollection* logical = documentCollection(trxCol);

  pinData(cid);  // will throw when it fails

  Result res = lock(trxCol, AccessMode::Type::READ);

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  logical->invokeOnAllElements(this, callback);

  res = unlock(trxCol, AccessMode::Type::READ);

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

/// @brief return one document from a collection, fast path
///        If everything went well the result will contain the found document
///        (as an external on single_server) and this function will return
///        TRI_ERROR_NO_ERROR.
///        If there was an error the code is returned and it is guaranteed
///        that result remains unmodified.
///        Does not care for revision handling!
Result transaction::Methods::documentFastPath(std::string const& collectionName,
                                              ManagedDocumentResult* mmdr,
                                              VPackSlice const value,
                                              VPackBuilder& result,
                                              bool shouldLock) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);
  if (!value.isObject() && !value.isString()) {
    // must provide a document object or string
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  if (_state->isCoordinator()) {
    OperationOptions options;  // use default configuration
    options.ignoreRevs = true;

    OperationResult opRes = documentCoordinator(collectionName, value, options);
    if (opRes.failed()) {
      return opRes.code;
    }
    result.add(opRes.slice());
    return Result(TRI_ERROR_NO_ERROR);
  }

  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName);
  LogicalCollection* collection = documentCollection(trxCollection(cid));

  pinData(cid);  // will throw when it fails

  StringRef key(transaction::helpers::extractKeyPart(value));
  if (key.empty()) {
    return Result(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  std::unique_ptr<ManagedDocumentResult> tmp;
  if (mmdr == nullptr) {
    tmp.reset(new ManagedDocumentResult);
    mmdr = tmp.get();
  }

  TRI_ASSERT(mmdr != nullptr);

  Result res = collection->read(
      this, key, *mmdr,
      shouldLock && !isLocked(collection, AccessMode::Type::READ));

  if (res.fail()) {
    return res;
  }

  TRI_ASSERT(isPinned(cid));

  mmdr->addToBuilder(result, true);
  return Result(TRI_ERROR_NO_ERROR);
}

/// @brief return one document from a collection, fast path
///        If everything went well the result will contain the found document
///        (as an external on single_server) and this function will return
///        TRI_ERROR_NO_ERROR.
///        If there was an error the code is returned
///        Does not care for revision handling!
///        Must only be called on a local server, not in cluster case!
Result transaction::Methods::documentFastPathLocal(
    std::string const& collectionName, StringRef const& key,
    ManagedDocumentResult& result, bool shouldLock) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName);
  TransactionCollection* trxColl = trxCollection(cid);
  LogicalCollection* collection = documentCollection(trxColl);
  TRI_ASSERT(collection != nullptr);
  _transactionContextPtr->pinData(collection); // will throw when it fails

  if (key.empty()) {
    return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
  }

  bool isLocked = trxColl->isLocked(AccessMode::Type::READ,
                                    _state->nestingLevel());
  Result res = collection->read(this, key, result, shouldLock && !isLocked);
  TRI_ASSERT(res.fail() || isPinned(cid));
  return res;
}

/// @brief Create Cluster Communication result for document
OperationResult transaction::Methods::clusterResultDocument(
    rest::ResponseCode const& responseCode,
    std::shared_ptr<VPackBuilder> const& resultBody,
    std::unordered_map<int, size_t> const& errorCounter) const {
  switch (responseCode) {
    case rest::ResponseCode::OK:
    case rest::ResponseCode::PRECONDITION_FAILED:
      return OperationResult(resultBody->steal(), nullptr, "",
                             responseCode == rest::ResponseCode::OK
                                 ? TRI_ERROR_NO_ERROR
                                 : TRI_ERROR_ARANGO_CONFLICT,
                             false, errorCounter);
    case rest::ResponseCode::NOT_FOUND:
      return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
    default:
      return OperationResult(TRI_ERROR_INTERNAL);
  }
}

/// @brief Create Cluster Communication result for insert
OperationResult transaction::Methods::clusterResultInsert(
    rest::ResponseCode const& responseCode,
    std::shared_ptr<VPackBuilder> const& resultBody,
    std::unordered_map<int, size_t> const& errorCounter) const {
  switch (responseCode) {
    case rest::ResponseCode::ACCEPTED:
    case rest::ResponseCode::CREATED:
      return OperationResult(
          resultBody->steal(), nullptr, "", TRI_ERROR_NO_ERROR,
          responseCode == rest::ResponseCode::CREATED, errorCounter);
    case rest::ResponseCode::PRECONDITION_FAILED:
      return OperationResult(TRI_ERROR_ARANGO_CONFLICT);
    case rest::ResponseCode::BAD:
      return dbServerResponseBad(resultBody);
    case rest::ResponseCode::NOT_FOUND:
      return OperationResult(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    case rest::ResponseCode::CONFLICT:
      return OperationResult(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED);
    default:
      return OperationResult(TRI_ERROR_INTERNAL);
  }
}

/// @brief Create Cluster Communication result for modify
OperationResult transaction::Methods::clusterResultModify(
    rest::ResponseCode const& responseCode,
    std::shared_ptr<VPackBuilder> const& resultBody,
    std::unordered_map<int, size_t> const& errorCounter) const {
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
      return OperationResult(resultBody->steal(), nullptr, "", errorCode,
                             responseCode == rest::ResponseCode::CREATED,
                             errorCounter);
    case rest::ResponseCode::BAD:
      return dbServerResponseBad(resultBody);
    case rest::ResponseCode::NOT_FOUND:
      return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
    default:
      return OperationResult(TRI_ERROR_INTERNAL);
  }
}

/// @brief Helper create a Cluster Communication remove result
OperationResult transaction::Methods::clusterResultRemove(
    rest::ResponseCode const& responseCode,
    std::shared_ptr<VPackBuilder> const& resultBody,
    std::unordered_map<int, size_t> const& errorCounter) const {
  switch (responseCode) {
    case rest::ResponseCode::OK:
    case rest::ResponseCode::ACCEPTED:
    case rest::ResponseCode::PRECONDITION_FAILED:
      return OperationResult(
          resultBody->steal(), nullptr, "",
          responseCode == rest::ResponseCode::PRECONDITION_FAILED
              ? TRI_ERROR_ARANGO_CONFLICT
              : TRI_ERROR_NO_ERROR,
          responseCode != rest::ResponseCode::ACCEPTED, errorCounter);
    case rest::ResponseCode::BAD:
      return dbServerResponseBad(resultBody);
    case rest::ResponseCode::NOT_FOUND:
      return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
    default:
      return OperationResult(TRI_ERROR_INTERNAL);
  }
}

/// @brief return one or multiple documents from a collection
OperationResult transaction::Methods::document(
    std::string const& collectionName, VPackSlice const value,
    OperationOptions& options) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  if (!value.isObject() && !value.isArray()) {
    // must provide a document object or an array of documents
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  if (_state->isCoordinator()) {
    return documentCoordinator(collectionName, value, options);
  }

  return documentLocal(collectionName, value, options);
}

/// @brief read one or multiple documents in a collection, coordinator
#ifndef USE_ENTERPRISE
OperationResult transaction::Methods::documentCoordinator(
    std::string const& collectionName, VPackSlice const value,
    OperationOptions& options) {
  auto headers =
      std::make_unique<std::unordered_map<std::string, std::string>>();
  rest::ResponseCode responseCode;
  std::unordered_map<int, size_t> errorCounter;
  auto resultBody = std::make_shared<VPackBuilder>();

  if (!value.isArray()) {
    StringRef key(transaction::helpers::extractKeyPart(value));
    if (key.empty()) {
      return OperationResult(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD);
    }
  }

  int res = arangodb::getDocumentOnCoordinator(
      databaseName(), collectionName, value, options, headers, responseCode,
      errorCounter, resultBody);

  if (res == TRI_ERROR_NO_ERROR) {
    return clusterResultDocument(responseCode, resultBody, errorCounter);
  }
  return OperationResult(res);
}
#endif

/// @brief read one or multiple documents in a collection, local
OperationResult transaction::Methods::documentLocal(
    std::string const& collectionName, VPackSlice const value,
    OperationOptions& options) {
  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName);
  LogicalCollection* collection = documentCollection(trxCollection(cid));

  if (!options.silent) {
    pinData(cid);  // will throw when it fails
  }

  VPackBuilder resultBuilder;

  auto workForOneDocument = [&](VPackSlice const value,
                                bool isMultiple) -> Result {
    StringRef key(transaction::helpers::extractKeyPart(value));
    if (key.empty()) {
      return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
    }

    TRI_voc_rid_t expectedRevision = 0;
    if (!options.ignoreRevs && value.isObject()) {
      expectedRevision = TRI_ExtractRevisionId(value);
    }

    ManagedDocumentResult result;
    Result res = collection->read(
      this, key, result, !isLocked(collection, AccessMode::Type::READ));

    if (res.fail()) {
      return res;
    }

    TRI_ASSERT(isPinned(cid));

    if (expectedRevision != 0) {
      TRI_voc_rid_t foundRevision =
          transaction::helpers::extractRevFromDocument(
              VPackSlice(result.vpack()));
      if (expectedRevision != foundRevision) {
        if (!isMultiple) {
          // still return
          buildDocumentIdentity(collection, resultBuilder, cid, key,
                                foundRevision, 0, nullptr, nullptr);
        }
        return TRI_ERROR_ARANGO_CONFLICT;
      }
    }

    if (!options.silent) {
      result.addToBuilder(resultBuilder, true);
    } else if (isMultiple) {
      resultBuilder.add(VPackSlice::nullSlice());
    }

    return TRI_ERROR_NO_ERROR;
  };

  Result res(TRI_ERROR_NO_ERROR);
  std::unordered_map<int, size_t> countErrorCodes;
  if (!value.isArray()) {
    res = workForOneDocument(value, false);
  } else {
    VPackArrayBuilder guard(&resultBuilder);
    for (auto const& s : VPackArrayIterator(value)) {
      res = workForOneDocument(s, true);
      if (res.fail()) {
        createBabiesError(resultBuilder, countErrorCodes, res, options.silent);
      }
    }
    res = TRI_ERROR_NO_ERROR;
  }

  return OperationResult(resultBuilder.steal(),
                         _transactionContextPtr->orderCustomTypeHandler(),
                         res.errorMessage(), res.errorNumber(),
                         options.waitForSync, countErrorCodes);
}

/// @brief create one or multiple documents in a collection
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
OperationResult transaction::Methods::insert(std::string const& collectionName,
                                             VPackSlice const value,
                                             OperationOptions const& options) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  if (!value.isObject() && !value.isArray()) {
    // must provide a document object or an array of documents
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }
  if (value.isArray() && value.length() == 0) {
    return emptyResult(options.waitForSync);
  }

  // Validate Edges
  OperationOptions optionsCopy = options;

  if (_state->isCoordinator()) {
    return insertCoordinator(collectionName, value, optionsCopy);
  }

  return insertLocal(collectionName, value, optionsCopy);
}

/// @brief create one or multiple documents in a collection, coordinator
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
#ifndef USE_ENTERPRISE
OperationResult transaction::Methods::insertCoordinator(
    std::string const& collectionName, VPackSlice const value,
    OperationOptions& options) {
  rest::ResponseCode responseCode;

  std::unordered_map<int, size_t> errorCounter;
  auto resultBody = std::make_shared<VPackBuilder>();

  int res = arangodb::createDocumentOnCoordinator(
      databaseName(), collectionName, options, value, responseCode,
      errorCounter, resultBody);

  if (res == TRI_ERROR_NO_ERROR) {
    return clusterResultInsert(responseCode, resultBody, errorCounter);
  }
  return OperationResult(res);
}
#endif

/// @brief choose a timeout for synchronous replication, based on the
/// number of documents we ship over
static double chooseTimeout(size_t count) {
  static bool timeoutQueried = false;
  static double timeoutFactor = 1.0;
  static double lowerLimit = 0.5;
  if (!timeoutQueried) {
    // Multithreading is no problem here because these static variables
    // are only ever set once in the lifetime of the server.
    auto feature = application_features::ApplicationServer::getFeature<ClusterFeature>("Cluster");
    timeoutFactor = feature->syncReplTimeoutFactor();
    timeoutQueried = true;
    auto feature2 = application_features::ApplicationServer::getFeature<EngineSelectorFeature>("EngineSelector");
    if (feature2->engineName() == arangodb::RocksDBEngine::EngineName) {
      lowerLimit = 1.0;
    }
  }

  // We usually assume that a server can process at least 2500 documents
  // per second (this is a low estimate), and use a low limit of 0.5s
  // and a high timeout of 120s
  double timeout = static_cast<double>(count / 2500);
  if (timeout < lowerLimit) {
    return lowerLimit * timeoutFactor;
  } else if (timeout > 120) {
    return 120.0 * timeoutFactor;
  } else {
    return timeout * timeoutFactor;
  }
}

/// @brief create one or multiple documents in a collection, local
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
OperationResult transaction::Methods::insertLocal(
    std::string const& collectionName, VPackSlice const value,
    OperationOptions& options) {
  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName);
  LogicalCollection* collection = documentCollection(trxCollection(cid));

  bool isFollower = false;
  if (_state->isDBServer()) {
    // Block operation early if we are not supposed to perform it:
    std::string theLeader = collection->followers()->getLeader();
    if (theLeader.empty()) {
      if (!options.isSynchronousReplicationFrom.empty()) {
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION);
      }
    } else {  // we are a follower following theLeader
      isFollower = true;
      if (options.isSynchronousReplicationFrom.empty()) {
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED);
      }
      if (options.isSynchronousReplicationFrom != theLeader) {
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_FOLLOWER_REFUSES_OPERATION);
      }
    }
  }

  if (options.returnNew) {
    pinData(cid);  // will throw when it fails
  }

  VPackBuilder resultBuilder;
  TRI_voc_tick_t maxTick = 0;

  auto workForOneDocument = [&](VPackSlice const value) -> Result {
    if (!value.isObject()) {
      return TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
    }

    ManagedDocumentResult result;
    TRI_voc_tick_t resultMarkerTick = 0;

    Result res =
        collection->insert(this, value, result, options, resultMarkerTick,
                           !isLocked(collection, AccessMode::Type::WRITE));

    if (resultMarkerTick > 0 && resultMarkerTick > maxTick) {
      maxTick = resultMarkerTick;
    }

    if (!res.ok()) {
      // Error reporting in the babies case is done outside of here,
      // in the single document case no body needs to be created at all.
      return res;
    }

    TRI_ASSERT(!result.empty());

    StringRef keyString(transaction::helpers::extractKeyFromDocument(
        VPackSlice(result.vpack())));

    buildDocumentIdentity(collection, resultBuilder, cid, keyString,
                          transaction::helpers::extractRevFromDocument(
                              VPackSlice(result.vpack())),
                          0, nullptr, options.returnNew ? &result : nullptr);

    return TRI_ERROR_NO_ERROR;
  };

  Result res = TRI_ERROR_NO_ERROR;
  bool const multiCase = value.isArray();
  std::unordered_map<int, size_t> countErrorCodes;
  if (multiCase) {
    VPackArrayBuilder b(&resultBuilder);
    for (auto const& s : VPackArrayIterator(value)) {
      res = workForOneDocument(s);
      if (res.fail()) {
        createBabiesError(resultBuilder, countErrorCodes, res, options.silent);
      }
    }
    // With babies the reporting is handled in the body of the result
    res = Result(TRI_ERROR_NO_ERROR);
  } else {
    res = workForOneDocument(value);
  }

  // wait for operation(s) to be synced to disk here. On rocksdb maxTick == 0
  if (res.ok() && options.waitForSync && maxTick > 0 &&
      isSingleOperationTransaction()) {
    EngineSelectorFeature::ENGINE->waitForSync(maxTick);
  }

  if (res.ok() && _state->isDBServer()) {
    // Now replicate the same operation on all followers:
    auto const& followerInfo = collection->followers();
    std::shared_ptr<std::vector<ServerID> const> followers = followerInfo->get();
    // Now see whether or not we have to do synchronous replication:
    bool doingSynchronousReplication = !isFollower && followers->size() > 0;

    if (doingSynchronousReplication) {
      // In the multi babies case res is always TRI_ERROR_NO_ERROR if we
      // get here, in the single document case, we do not try to replicate
      // in case of an error.

      // Now replicate the good operations on all followers:
      std::string path =
        "/_db/" + arangodb::basics::StringUtils::urlEncode(databaseName()) +
        "/_api/document/" +
        arangodb::basics::StringUtils::urlEncode(collection->name()) +
        "?isRestore=true&isSynchronousReplication=" +
        ServerState::instance()->getId();

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
          requests.emplace_back("server:" + f, arangodb::rest::RequestType::POST,
              path, body);
        }
        auto cc = arangodb::ClusterComm::instance();
        if (cc != nullptr) {
          // nullptr only happens on controlled shutdown
          size_t nrDone = 0;
          size_t nrGood = cc->performRequests(requests, chooseTimeout(count),
                                              nrDone, Logger::REPLICATION, false);
          if (nrGood < followers->size()) {
            // If any would-be-follower refused to follow there must be a
            // new leader in the meantime, in this case we must not allow
            // this operation to succeed, we simply return with a refusal
            // error (note that we use the follower version, since we have
            // lost leadership):
            if (findRefusal(requests)) {
              return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED);
            }

            // Otherwise we drop all followers that were not successful:
            for (size_t i = 0; i < followers->size(); ++i) {
              bool replicationWorked =
                requests[i].done &&
                requests[i].result.status == CL_COMM_RECEIVED &&
                (requests[i].result.answer_code ==
                 rest::ResponseCode::ACCEPTED ||
                 requests[i].result.answer_code == rest::ResponseCode::CREATED);
              if (replicationWorked) {
                bool found;
                requests[i].result.answer->header(StaticStrings::ErrorCodes,
                    found);
                replicationWorked = !found;
              }
              if (!replicationWorked) {
                auto const& followerInfo = collection->followers();
                if (followerInfo->remove((*followers)[i])) {
                  LOG_TOPIC(WARN, Logger::REPLICATION)
                    << "insertLocal: dropping follower " << (*followers)[i]
                    << " for shard " << collectionName;
                } else {
                  LOG_TOPIC(ERR, Logger::REPLICATION)
                    << "insertLocal: could not drop follower "
                    << (*followers)[i] << " for shard " << collectionName;
                  THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_COULD_NOT_DROP_FOLLOWER);
                }
              }
            }
          }
        }
      }
    }
  }

  if (options.silent) {
    // We needed the results, but do not want to report:
    resultBuilder.clear();
  }

  return OperationResult(resultBuilder.steal(), nullptr, res.errorMessage(),
                         res.errorNumber(), options.waitForSync,
                         countErrorCodes);
}

/// @brief update/patch one or multiple documents in a collection
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
OperationResult transaction::Methods::update(std::string const& collectionName,
                                             VPackSlice const newValue,
                                             OperationOptions const& options) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  if (!newValue.isObject() && !newValue.isArray()) {
    // must provide a document object or an array of documents
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }
  if (newValue.isArray() && newValue.length() == 0) {
    return emptyResult(options.waitForSync);
  }

  OperationOptions optionsCopy = options;

  if (_state->isCoordinator()) {
    return updateCoordinator(collectionName, newValue, optionsCopy);
  }

  return modifyLocal(collectionName, newValue, optionsCopy,
                     TRI_VOC_DOCUMENT_OPERATION_UPDATE);
}

/// @brief update one or multiple documents in a collection, coordinator
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
#ifndef USE_ENTERPRISE
OperationResult transaction::Methods::updateCoordinator(
    std::string const& collectionName, VPackSlice const newValue,
    OperationOptions& options) {
  auto headers =
      std::make_unique<std::unordered_map<std::string, std::string>>();
  rest::ResponseCode responseCode;
  std::unordered_map<int, size_t> errorCounter;
  auto resultBody = std::make_shared<VPackBuilder>();

  int res = arangodb::modifyDocumentOnCoordinator(
      databaseName(), collectionName, newValue, options, true /* isPatch */,
      headers, responseCode, errorCounter, resultBody);

  if (res == TRI_ERROR_NO_ERROR) {
    return clusterResultModify(responseCode, resultBody, errorCounter);
  }
  return OperationResult(res);
}
#endif

/// @brief replace one or multiple documents in a collection
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
OperationResult transaction::Methods::replace(std::string const& collectionName,
                                              VPackSlice const newValue,
                                              OperationOptions const& options) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  if (!newValue.isObject() && !newValue.isArray()) {
    // must provide a document object or an array of documents
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }
  if (newValue.isArray() && newValue.length() == 0) {
    return emptyResult(options.waitForSync);
  }

  OperationOptions optionsCopy = options;

  if (_state->isCoordinator()) {
    return replaceCoordinator(collectionName, newValue, optionsCopy);
  }

  return modifyLocal(collectionName, newValue, optionsCopy,
                     TRI_VOC_DOCUMENT_OPERATION_REPLACE);
}

/// @brief replace one or multiple documents in a collection, coordinator
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
#ifndef USE_ENTERPRISE
OperationResult transaction::Methods::replaceCoordinator(
    std::string const& collectionName, VPackSlice const newValue,
    OperationOptions& options) {
  auto headers =
      std::make_unique<std::unordered_map<std::string, std::string>>();
  rest::ResponseCode responseCode;
  std::unordered_map<int, size_t> errorCounter;
  auto resultBody = std::make_shared<VPackBuilder>();

  int res = arangodb::modifyDocumentOnCoordinator(
      databaseName(), collectionName, newValue, options, false /* isPatch */,
      headers, responseCode, errorCounter, resultBody);

  if (res == TRI_ERROR_NO_ERROR) {
    return clusterResultModify(responseCode, resultBody, errorCounter);
  }
  return OperationResult(res);
}
#endif

/// @brief replace one or multiple documents in a collection, local
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
OperationResult transaction::Methods::modifyLocal(
    std::string const& collectionName, VPackSlice const newValue,
    OperationOptions& options, TRI_voc_document_operation_e operation) {
  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName);
  LogicalCollection* collection = documentCollection(trxCollection(cid));

  bool isFollower = false;
  if (_state->isDBServer()) {
    // Block operation early if we are not supposed to perform it:
    std::string theLeader = collection->followers()->getLeader();
    if (theLeader.empty()) {
      if (!options.isSynchronousReplicationFrom.empty()) {
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION);
      }
    } else {  // we are a follower following theLeader
      isFollower = true;
      if (options.isSynchronousReplicationFrom.empty()) {
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED);
      }
      if (options.isSynchronousReplicationFrom != theLeader) {
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_FOLLOWER_REFUSES_OPERATION);
      }
    }
  }

  if (options.returnOld || options.returnNew) {
    pinData(cid);  // will throw when it fails
  }

  // Update/replace are a read and a write, let's get the write lock already
  // for the read operation:
  Result res = lock(trxCollection(cid), AccessMode::Type::WRITE);

  if (!res.ok()) {
    return OperationResult(res);
  }

  VPackBuilder resultBuilder;  // building the complete result
  TRI_voc_tick_t maxTick = 0;

  // lambda //////////////
  auto workForOneDocument = [this, &operation, &options, &maxTick, &collection,
                             &resultBuilder, &cid](VPackSlice const newVal,
                                                   bool isBabies) -> Result {
    Result res;
    if (!newVal.isObject()) {
      res.reset(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
      return res;
    }

    ManagedDocumentResult result;
    TRI_voc_rid_t actualRevision = 0;
    ManagedDocumentResult previous;
    TRI_voc_tick_t resultMarkerTick = 0;

    if (operation == TRI_VOC_DOCUMENT_OPERATION_REPLACE) {
      res = collection->replace(this, newVal, result, options, resultMarkerTick,
                                !isLocked(collection, AccessMode::Type::WRITE),
                                actualRevision, previous);
    } else {
      res = collection->update(this, newVal, result, options, resultMarkerTick,
                               !isLocked(collection, AccessMode::Type::WRITE),
                               actualRevision, previous);
    }

    if (resultMarkerTick > 0 && resultMarkerTick > maxTick) {
      maxTick = resultMarkerTick;
    }

    if (res.errorNumber() == TRI_ERROR_ARANGO_CONFLICT) {
      // still return
      if (!isBabies) {
        StringRef key(newVal.get(StaticStrings::KeyString));
        buildDocumentIdentity(collection, resultBuilder, cid, key,
                              actualRevision, 0,
                              options.returnOld ? &previous : nullptr, nullptr);
      }
      return res;
    } else if (!res.ok()) {
      return res;
    }

    TRI_ASSERT(!result.empty());
    TRI_ASSERT(!previous.empty());

    StringRef key(newVal.get(StaticStrings::KeyString));
    buildDocumentIdentity(collection, resultBuilder, cid, key,
                          TRI_ExtractRevisionId(VPackSlice(result.vpack())),
                          actualRevision,
                          options.returnOld ? &previous : nullptr,
                          options.returnNew ? &result : nullptr);

    return res;  // must be ok!
  };
  ///////////////////////

  bool multiCase = newValue.isArray();
  std::unordered_map<int, size_t> errorCounter;
  if (multiCase) {
    {
      VPackArrayBuilder guard(&resultBuilder);
      VPackArrayIterator it(newValue);
      while (it.valid()) {
        res = workForOneDocument(it.value(), true);
        if (!res.ok()) {
          createBabiesError(resultBuilder, errorCounter, res.errorNumber(),
                            options.silent);
        }
        ++it;
      }
    }
    res.reset();
  } else {
    res = workForOneDocument(newValue, false);
  }

  // wait for operation(s) to be synced to disk here. On rocksdb maxTick == 0
  if (res.ok() && options.waitForSync && maxTick > 0 &&
      isSingleOperationTransaction()) {
    EngineSelectorFeature::ENGINE->waitForSync(maxTick);
  }

  // Now see whether or not we have to do synchronous replication:
  if (res.ok() && _state->isDBServer()) {
    // Now replicate the same operation on all followers:
    auto const& followerInfo = collection->followers();
    std::shared_ptr<std::vector<ServerID> const> followers = followerInfo->get();
    bool doingSynchronousReplication = !isFollower && followers->size() > 0;

    if (doingSynchronousReplication) {
      // In the multi babies case res is always TRI_ERROR_NO_ERROR if we
      // get here, in the single document case, we do not try to replicate
      // in case of an error.

      // Now replicate the good operations on all followers:
      auto cc = arangodb::ClusterComm::instance();
      if (cc != nullptr) {
        // nullptr only happens on controlled shutdown
        std::string path =
          "/_db/" + arangodb::basics::StringUtils::urlEncode(databaseName()) +
          "/_api/document/" +
          arangodb::basics::StringUtils::urlEncode(collection->name()) +
          "?isRestore=true&isSynchronousReplication=" +
          ServerState::instance()->getId();

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
        if (multiCase) {
          VPackArrayBuilder guard(&payload);
          VPackArrayIterator itValue(newValue);
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
          VPackArrayBuilder guard(&payload);
          doOneDoc(newValue, ourResult);
          count++;
        }
        if (count > 0) {
          auto body = std::make_shared<std::string>();
          *body = payload.slice().toJson();

          // Now prepare the requests:
          std::vector<ClusterCommRequest> requests;
          for (auto const& f : *followers) {
            requests.emplace_back("server:" + f,
                operation == TRI_VOC_DOCUMENT_OPERATION_REPLACE
                ? arangodb::rest::RequestType::PUT
                : arangodb::rest::RequestType::PATCH,
                path, body);
          }
          size_t nrDone = 0;
          size_t nrGood = cc->performRequests(requests, chooseTimeout(count),
                                              nrDone, Logger::REPLICATION, false);
          if (nrGood < followers->size()) {
            // If any would-be-follower refused to follow there must be a
            // new leader in the meantime, in this case we must not allow
            // this operation to succeed, we simply return with a refusal
            // error (note that we use the follower version, since we have
            // lost leadership):
            if (findRefusal(requests)) {
              return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED);
            }

            // Otherwise we drop all followers that were not successful:
            for (size_t i = 0; i < followers->size(); ++i) {
              bool replicationWorked =
                requests[i].done &&
                requests[i].result.status == CL_COMM_RECEIVED &&
                (requests[i].result.answer_code ==
                 rest::ResponseCode::ACCEPTED ||
                 requests[i].result.answer_code == rest::ResponseCode::OK);
              if (replicationWorked) {
                bool found;
                requests[i].result.answer->header(StaticStrings::ErrorCodes,
                    found);
                replicationWorked = !found;
              }
              if (!replicationWorked) {
                auto const& followerInfo = collection->followers();
                if (followerInfo->remove((*followers)[i])) {
                  LOG_TOPIC(WARN, Logger::REPLICATION)
                    << "modifyLocal: dropping follower " << (*followers)[i]
                    << " for shard " << collectionName;
                } else {
                  LOG_TOPIC(ERR, Logger::REPLICATION)
                    << "modifyLocal: could not drop follower "
                    << (*followers)[i] << " for shard " << collectionName;
                  THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_COULD_NOT_DROP_FOLLOWER);
                }
                LOG_TOPIC(ERR, Logger::REPLICATION)
                  << "modifyLocal: dropping follower " << (*followers)[i]
                  << " for shard " << collectionName;
              }
            }
          }
        }
      }
    }
  }

  if (options.silent) {
    // We needed the results, but do not want to report:
    resultBuilder.clear();
  }

  return OperationResult(resultBuilder.steal(), nullptr, "", res.errorNumber(),
                         options.waitForSync, errorCounter);
}

/// @brief remove one or multiple documents in a collection
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
OperationResult transaction::Methods::remove(std::string const& collectionName,
                                             VPackSlice const value,
                                             OperationOptions const& options) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  if (!value.isObject() && !value.isArray() && !value.isString()) {
    // must provide a document object or an array of documents
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }
  if (value.isArray() && value.length() == 0) {
    return emptyResult(options.waitForSync);
  }

  OperationOptions optionsCopy = options;

  if (_state->isCoordinator()) {
    return removeCoordinator(collectionName, value, optionsCopy);
  }

  return removeLocal(collectionName, value, optionsCopy);
}

/// @brief remove one or multiple documents in a collection, coordinator
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
#ifndef USE_ENTERPRISE
OperationResult transaction::Methods::removeCoordinator(
    std::string const& collectionName, VPackSlice const value,
    OperationOptions& options) {
  rest::ResponseCode responseCode;
  std::unordered_map<int, size_t> errorCounter;
  auto resultBody = std::make_shared<VPackBuilder>();

  int res = arangodb::deleteDocumentOnCoordinator(
      databaseName(), collectionName, value, options, responseCode,
      errorCounter, resultBody);

  if (res == TRI_ERROR_NO_ERROR) {
    return clusterResultRemove(responseCode, resultBody, errorCounter);
  }
  return OperationResult(res);
}
#endif

/// @brief remove one or multiple documents in a collection, local
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
OperationResult transaction::Methods::removeLocal(
    std::string const& collectionName, VPackSlice const value,
    OperationOptions& options) {
  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName);
  LogicalCollection* collection = documentCollection(trxCollection(cid));

  bool isFollower = false;
  if (_state->isDBServer()) {
    // Block operation early if we are not supposed to perform it:
    std::string theLeader = collection->followers()->getLeader();
    if (theLeader.empty()) {
      if (!options.isSynchronousReplicationFrom.empty()) {
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION);
      }
    } else {  // we are a follower following theLeader
      isFollower = true;
      if (options.isSynchronousReplicationFrom.empty()) {
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED);
      }
      if (options.isSynchronousReplicationFrom != theLeader) {
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_FOLLOWER_REFUSES_OPERATION);
      }
    }
  }

  if (options.returnOld) {
    pinData(cid);  // will throw when it fails
  }

  VPackBuilder resultBuilder;
  TRI_voc_tick_t maxTick = 0;

  auto workForOneDocument = [&](VPackSlice value, bool isBabies) -> Result {
    TRI_voc_rid_t actualRevision = 0;
    ManagedDocumentResult previous;
    transaction::BuilderLeaser builder(this);
    StringRef key;
    if (value.isString()) {
      key = value;
      size_t pos = key.find('/');
      if (pos != std::string::npos) {
        key = key.substr(pos + 1);
        builder->add(
            VPackValuePair(key.data(), key.length(), VPackValueType::String));
        value = builder->slice();
      }
    } else if (value.isObject()) {
      VPackSlice keySlice = value.get(StaticStrings::KeyString);
      if (!keySlice.isString()) {
        return Result(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
      }
      key = keySlice;
    } else {
      return Result(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
    }

    TRI_voc_tick_t resultMarkerTick = 0;

    Result res = collection->remove(this, value, options, resultMarkerTick,
                                 !isLocked(collection, AccessMode::Type::WRITE),
                                 actualRevision, previous);

    if (resultMarkerTick > 0 && resultMarkerTick > maxTick) {
      maxTick = resultMarkerTick;
    }

    if (!res.ok()) {
      if (res.errorNumber() == TRI_ERROR_ARANGO_CONFLICT && !isBabies) {
        buildDocumentIdentity(collection, resultBuilder, cid, key,
                              actualRevision, 0,
                              options.returnOld ? &previous : nullptr, nullptr);
      }
      return res;
    }

    TRI_ASSERT(!previous.empty());
    buildDocumentIdentity(collection, resultBuilder, cid, key, actualRevision,
                          0, options.returnOld ? &previous : nullptr, nullptr);

    return Result(TRI_ERROR_NO_ERROR);
  };

  Result res(TRI_ERROR_NO_ERROR);
  bool multiCase = value.isArray();
  std::unordered_map<int, size_t> countErrorCodes;
  if (multiCase) {
    VPackArrayBuilder guard(&resultBuilder);
    for (auto const& s : VPackArrayIterator(value)) {
      res = workForOneDocument(s, true);
      if (!res.ok()) {
        createBabiesError(resultBuilder, countErrorCodes, res, options.silent);
      }
    }
    // With babies the reporting is handled somewhere else.
    res = Result(TRI_ERROR_NO_ERROR);
  } else {
    res = workForOneDocument(value, false);
  }

  // wait for operation(s) to be synced to disk here. On rocksdb maxTick == 0
  if (res.ok() && options.waitForSync && maxTick > 0 &&
      isSingleOperationTransaction()) {
    EngineSelectorFeature::ENGINE->waitForSync(maxTick);
  }

  // Now see whether or not we have to do synchronous replication:
  if (res.ok() && _state->isDBServer()) {
    // Now replicate the same operation on all followers:
    auto const& followerInfo = collection->followers();
    std::shared_ptr<std::vector<ServerID> const> followers = followerInfo->get();
    bool doingSynchronousReplication = !isFollower && followers->size() > 0;

    if (doingSynchronousReplication) {
      // In the multi babies case res is always TRI_ERROR_NO_ERROR if we
      // get here, in the single document case, we do not try to replicate
      // in case of an error.

      // Now replicate the good operations on all followers:
      auto cc = arangodb::ClusterComm::instance();
      if (cc != nullptr) {
        // nullptr only happens on controled shutdown

        std::string path =
          "/_db/" + arangodb::basics::StringUtils::urlEncode(databaseName()) +
          "/_api/document/" +
          arangodb::basics::StringUtils::urlEncode(collection->name()) +
          "?isRestore=true&isSynchronousReplication=" +
          ServerState::instance()->getId();

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
          VPackArrayBuilder guard(&payload);
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
                arangodb::rest::RequestType::DELETE_REQ, path,
                body);
          }
          size_t nrDone = 0;
          size_t nrGood = cc->performRequests(requests, chooseTimeout(count),
                                              nrDone, Logger::REPLICATION, false);
          if (nrGood < followers->size()) {
            // If any would-be-follower refused to follow there must be a
            // new leader in the meantime, in this case we must not allow
            // this operation to succeed, we simply return with a refusal
            // error (note that we use the follower version, since we have
            // lost leadership):
            if (findRefusal(requests)) {
              return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED);
            }

            // we drop all followers that were not successful:
            for (size_t i = 0; i < followers->size(); ++i) {
              bool replicationWorked =
                requests[i].done &&
                requests[i].result.status == CL_COMM_RECEIVED &&
                (requests[i].result.answer_code ==
                 rest::ResponseCode::ACCEPTED ||
                 requests[i].result.answer_code == rest::ResponseCode::OK);
              if (replicationWorked) {
                bool found;
                requests[i].result.answer->header(StaticStrings::ErrorCodes,
                    found);
                replicationWorked = !found;
              }
              if (!replicationWorked) {
                auto const& followerInfo = collection->followers();
                if (followerInfo->remove((*followers)[i])) {
                  LOG_TOPIC(WARN, Logger::REPLICATION)
                    << "removeLocal: dropping follower " << (*followers)[i]
                    << " for shard " << collectionName;
                } else {
                  LOG_TOPIC(ERR, Logger::REPLICATION)
                    << "removeLocal: could not drop follower "
                    << (*followers)[i] << " for shard " << collectionName;
                  THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_COULD_NOT_DROP_FOLLOWER);
                }
              }
            }
          }
        }
      }
    }
  }

  if (options.silent) {
    // We needed the results, but do not want to report:
    resultBuilder.clear();
  }

  return OperationResult(resultBuilder.steal(), nullptr, res.errorMessage(),
                         res.errorNumber(), options.waitForSync,
                         countErrorCodes);
}

/// @brief fetches all documents in a collection
OperationResult transaction::Methods::all(std::string const& collectionName,
                                          uint64_t skip, uint64_t limit,
                                          OperationOptions const& options) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  OperationOptions optionsCopy = options;

  if (_state->isCoordinator()) {
    return allCoordinator(collectionName, skip, limit, optionsCopy);
  }

  return allLocal(collectionName, skip, limit, optionsCopy);
}

/// @brief fetches all documents in a collection, coordinator
OperationResult transaction::Methods::allCoordinator(
    std::string const& collectionName, uint64_t skip, uint64_t limit,
    OperationOptions& options) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

/// @brief fetches all documents in a collection, local
OperationResult transaction::Methods::allLocal(
    std::string const& collectionName, uint64_t skip, uint64_t limit,
    OperationOptions& options) {
  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName);

  pinData(cid);  // will throw when it fails

  Result res = lock(trxCollection(cid), AccessMode::Type::READ);

  if (!res.ok()) {
    return OperationResult(res);
  }

  VPackBuilder resultBuilder;
  resultBuilder.openArray();

  ManagedDocumentResult mmdr;
  std::unique_ptr<OperationCursor> cursor =
      indexScan(collectionName, transaction::Methods::CursorType::ALL, &mmdr,
                skip, limit, 1000, false);

  if (cursor->failed()) {
    return OperationResult(cursor->code);
  }

  auto cb = [&resultBuilder](DocumentIdentifierToken const& token, VPackSlice slice) {
    resultBuilder.add(slice);
  };
  cursor->allDocuments(cb);

  resultBuilder.close();

  res = unlock(trxCollection(cid), AccessMode::Type::READ);

  if (res.ok()) {
    return OperationResult(res);
  }

  return OperationResult(resultBuilder.steal(),
                         _transactionContextPtr->orderCustomTypeHandler(), "",
                         TRI_ERROR_NO_ERROR, false);
}

/// @brief remove all documents in a collection
OperationResult transaction::Methods::truncate(
    std::string const& collectionName, OperationOptions const& options) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  OperationOptions optionsCopy = options;
  OperationResult result;

  if (_state->isCoordinator()) {
    result = truncateCoordinator(collectionName, optionsCopy);
  } else {
    result = truncateLocal(collectionName, optionsCopy);
  }

  events::TruncateCollection(collectionName, result.code);
  return result;
}

/// @brief remove all documents in a collection, coordinator
#ifndef USE_ENTERPRISE
OperationResult transaction::Methods::truncateCoordinator(
    std::string const& collectionName, OperationOptions& options) {
  return OperationResult(arangodb::truncateCollectionOnCoordinator(
      databaseName(), collectionName));
}
#endif

/// @brief remove all documents in a collection, local
OperationResult transaction::Methods::truncateLocal(
    std::string const& collectionName, OperationOptions& options) {
  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName);

  LogicalCollection* collection = documentCollection(trxCollection(cid));

  bool isFollower = false;
  if (_state->isDBServer()) {
    // Block operation early if we are not supposed to perform it:
    std::string theLeader = collection->followers()->getLeader();
    if (theLeader.empty()) {
      if (!options.isSynchronousReplicationFrom.empty()) {
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION);
      }
    } else {  // we are a follower following theLeader
      isFollower = true;
      if (options.isSynchronousReplicationFrom.empty()) {
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED);
      }
      if (options.isSynchronousReplicationFrom != theLeader) {
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_FOLLOWER_REFUSES_OPERATION);
      }
    }
  }

  pinData(cid);  // will throw when it fails

  Result res = lock(trxCollection(cid), AccessMode::Type::WRITE);

  if (!res.ok()) {
    return OperationResult(res);
  }

  try {
    collection->truncate(this, options);
  } catch (basics::Exception const& ex) {
    unlock(trxCollection(cid), AccessMode::Type::WRITE);
    return OperationResult(ex.code(), ex.what());
  }

  // Now see whether or not we have to do synchronous replication:
  if (_state->isDBServer()) {
    // Now replicate the same operation on all followers:
    auto const& followerInfo = collection->followers();
    std::shared_ptr<std::vector<ServerID> const> followers = followerInfo->get();
    if (!isFollower && followers->size() > 0) {
      // Now replicate the good operations on all followers:
      auto cc = arangodb::ClusterComm::instance();
      if (cc != nullptr) {
        // nullptr only happens on controlled shutdown
        std::string path =
            "/_db/" + arangodb::basics::StringUtils::urlEncode(databaseName()) +
            "/_api/collection/" +
            arangodb::basics::StringUtils::urlEncode(collectionName) +
            "/truncate?isSynchronousReplication=" +
            ServerState::instance()->getId();

        auto body = std::make_shared<std::string>();

        // Now prepare the requests:
        std::vector<ClusterCommRequest> requests;
        for (auto const& f : *followers) {
          requests.emplace_back("server:" + f, arangodb::rest::RequestType::PUT,
                                path, body);
        }
        size_t nrDone = 0;
        size_t nrGood = cc->performRequests(requests, TRX_FOLLOWER_TIMEOUT,
                                            nrDone, Logger::REPLICATION, false);
        if (nrGood < followers->size()) {
          // If any would-be-follower refused to follow there must be a
          // new leader in the meantime, in this case we must not allow
          // this operation to succeed, we simply return with a refusal
          // error (note that we use the follower version, since we have
          // lost leadership):
          if (findRefusal(requests)) {
            return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED);
          }
          // we drop all followers that were not successful:
          for (size_t i = 0; i < followers->size(); ++i) {
            bool replicationWorked =
                requests[i].done &&
                requests[i].result.status == CL_COMM_RECEIVED &&
                (requests[i].result.answer_code ==
                     rest::ResponseCode::ACCEPTED ||
                 requests[i].result.answer_code == rest::ResponseCode::OK);
            if (!replicationWorked) {
              auto const& followerInfo = collection->followers();
              if (followerInfo->remove((*followers)[i])) {
                LOG_TOPIC(WARN, Logger::REPLICATION)
                    << "truncateLocal: dropping follower " << (*followers)[i]
                    << " for shard " << collectionName;
              } else {
                LOG_TOPIC(ERR, Logger::REPLICATION)
                    << "truncateLocal: could not drop follower "
                    << (*followers)[i] << " for shard " << collectionName;
                THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_COULD_NOT_DROP_FOLLOWER);
              }
            }
          }
        }
      }
    }
  }

  res = unlock(trxCollection(cid), AccessMode::Type::WRITE);

  return OperationResult(res);
}

/// @brief count the number of documents in a collection
OperationResult transaction::Methods::count(std::string const& collectionName,
                                            bool aggregate) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  if (_state->isCoordinator()) {
    return countCoordinator(collectionName, aggregate);
  }

  return countLocal(collectionName);
}

/// @brief count the number of documents in a collection
#ifndef USE_ENTERPRISE
OperationResult transaction::Methods::countCoordinator(
    std::string const& collectionName, bool aggregate) {
  std::vector<std::pair<std::string, uint64_t>> count;
  int res = arangodb::countOnCoordinator(databaseName(), collectionName, count);

  if (res != TRI_ERROR_NO_ERROR) {
    return OperationResult(res);
  }

  return buildCountResult(count, aggregate);
}
#endif

/// @brief count the number of documents in a collection
OperationResult transaction::Methods::countLocal(
    std::string const& collectionName) {
  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName);
  LogicalCollection* collection = documentCollection(trxCollection(cid));

  Result res = lock(trxCollection(cid), AccessMode::Type::READ);

  if (!res.ok()) {
    return OperationResult(res);
  }

  uint64_t num = collection->numberDocuments(this);

  res = unlock(trxCollection(cid), AccessMode::Type::READ);

  if (!res.ok()) {
    return OperationResult(res);
  }

  VPackBuilder resultBuilder;
  resultBuilder.add(VPackValue(num));

  return OperationResult(resultBuilder.steal(), nullptr, "", TRI_ERROR_NO_ERROR,
                         false);
}

/// @brief Gets the best fitting index for an AQL condition.
/// note: the caller must have read-locked the underlying collection when
/// calling this method
std::pair<bool, bool>
transaction::Methods::getBestIndexHandlesForFilterCondition(
    std::string const& collectionName, arangodb::aql::Ast* ast,
    arangodb::aql::AstNode* root, arangodb::aql::Variable const* reference,
    arangodb::aql::SortCondition const* sortCondition, size_t itemsInCollection,
    std::vector<IndexHandle>& usedIndexes, bool& isSorted) {
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

bool transaction::Methods::getBestIndexHandleForFilterCondition(
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
  return findIndexHandleForAndNode(indexes, node, reference, itemsInCollection,
                                   usedIndex);
}

/// @brief Checks if the index supports the filter condition.
/// note: the caller must have read-locked the underlying collection when
/// calling this method
bool transaction::Methods::supportsFilterCondition(
    IndexHandle const& indexHandle, arangodb::aql::AstNode const* condition,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    size_t& estimatedItems, double& estimatedCost) {
  auto idx = indexHandle.getIndex();
  if (nullptr == idx) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The index id cannot be empty.");
  }

  return idx->supportsFilterCondition(condition, reference, itemsInIndex,
                                      estimatedItems, estimatedCost);
}

/// @brief Get the index features:
///        Returns the covered attributes, and sets the first bool value
///        to isSorted and the second bool value to isSparse
std::vector<std::vector<arangodb::basics::AttributeName>>
transaction::Methods::getIndexFeatures(IndexHandle const& indexHandle,
                                       bool& isSorted, bool& isSparse) {
  auto idx = indexHandle.getIndex();
  if (nullptr == idx) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The index id cannot be empty.");
  }

  isSorted = idx->isSorted();
  isSparse = idx->sparse();
  return idx->fields();
}

/// @brief Gets the best fitting index for an AQL sort condition
/// note: the caller must have read-locked the underlying collection when
/// calling this method
std::pair<bool, bool> transaction::Methods::getIndexForSortCondition(
    std::string const& collectionName,
    arangodb::aql::SortCondition const* sortCondition,
    arangodb::aql::Variable const* reference, size_t itemsInIndex,
    std::vector<IndexHandle>& usedIndexes, size_t& coveredAttributes) {
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

/// @brief factory for OperationCursor objects from AQL
/// note: the caller must have read-locked the underlying collection when
/// calling this method
OperationCursor* transaction::Methods::indexScanForCondition(
    IndexHandle const& indexId, arangodb::aql::AstNode const* condition,
    arangodb::aql::Variable const* var, ManagedDocumentResult* mmdr,
    uint64_t limit, uint64_t batchSize, bool reverse) {
  if (_state->isCoordinator()) {
    // The index scan is only available on DBServers and Single Server.
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
  }

  if (limit == 0) {
    // nothing to do
    return new OperationCursor(TRI_ERROR_NO_ERROR);
  }

  auto idx = indexId.getIndex();
  if (nullptr == idx) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The index id cannot be empty.");
  }

  // Now create the Iterator
  std::unique_ptr<IndexIterator> iterator(
      idx->iteratorForCondition(this, mmdr, condition, var, reverse));

  if (iterator == nullptr) {
    // We could not create an ITERATOR and it did not throw an error itself
    return new OperationCursor(TRI_ERROR_OUT_OF_MEMORY);
  }

  return new OperationCursor(iterator.release(), limit, batchSize);
}

/// @brief factory for OperationCursor objects
/// note: the caller must have read-locked the underlying collection when
/// calling this method
std::unique_ptr<OperationCursor> transaction::Methods::indexScan(
    std::string const& collectionName, CursorType cursorType,
    ManagedDocumentResult* mmdr, uint64_t skip, uint64_t limit,
    uint64_t batchSize, bool reverse) {
  // For now we assume indexId is the iid part of the index.

  if (_state->isCoordinator()) {
    // The index scan is only available on DBServers and Single Server.
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
  }

  if (limit == 0) {
    // nothing to do
    return std::make_unique<OperationCursor>(TRI_ERROR_NO_ERROR);
  }

  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName);
  LogicalCollection* logical = documentCollection(trxCollection(cid));

  pinData(cid);  // will throw when it fails

  std::unique_ptr<IndexIterator> iterator = nullptr;

  switch (cursorType) {
    case CursorType::ANY: {
      iterator = logical->getAnyIterator(this, mmdr);
      break;
    }
    case CursorType::ALL: {
      iterator = logical->getAllIterator(this, mmdr, reverse);
      break;
    }
  }
  if (iterator == nullptr) {
    // We could not create an ITERATOR and it did not throw an error itself
    return std::make_unique<OperationCursor>(TRI_ERROR_OUT_OF_MEMORY);
  }

  if (skip > 0) {
    uint64_t unused = 0;
    iterator->skip(skip, unused);
  }

  return std::make_unique<OperationCursor>(iterator.release(), limit,
                                           batchSize);
}

/// @brief return the collection
arangodb::LogicalCollection* transaction::Methods::documentCollection(
    TransactionCollection const* trxCollection) const {
  TRI_ASSERT(_state != nullptr);
  TRI_ASSERT(trxCollection != nullptr);
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);
  TRI_ASSERT(trxCollection->collection() != nullptr);

  return trxCollection->collection();
}

/// @brief return the collection
arangodb::LogicalCollection* transaction::Methods::documentCollection(
    TRI_voc_cid_t cid) const {
  TRI_ASSERT(_state != nullptr);
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  auto trxCollection = _state->collection(cid, AccessMode::Type::READ);

  if (trxCollection == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "could not find collection");
  }

  TRI_ASSERT(trxCollection != nullptr);
  TRI_ASSERT(trxCollection->collection() != nullptr);
  return trxCollection->collection();
}

/// @brief add a collection by id, with the name supplied
Result transaction::Methods::addCollection(TRI_voc_cid_t cid, char const* name,
                                           AccessMode::Type type) {
  if (_state == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "cannot add collection without state");
  }

  Status const status = _state->status();

  if (status == transaction::Status::COMMITTED ||
      status == transaction::Status::ABORTED) {
    // transaction already finished?
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "cannot add collection to committed or aborted transaction");
  }

  if (cid == 0) {
    // invalid cid
    throwCollectionNotFound(name);
  }

  if (_state->isEmbeddedTransaction()) {
    return addCollectionEmbedded(cid, name, type);
  }

  return addCollectionToplevel(cid, name, type);
}

/// @brief add a collection by id, with the name supplied
Result transaction::Methods::addCollection(TRI_voc_cid_t cid,
                                           std::string const& name,
                                           AccessMode::Type type) {
  return addCollection(cid, name.c_str(), type);
}

/// @brief add a collection by id
Result transaction::Methods::addCollection(TRI_voc_cid_t cid,
                                           AccessMode::Type type) {
  return addCollection(cid, nullptr, type);
}

/// @brief add a collection by name
Result transaction::Methods::addCollection(std::string const& name,
                                           AccessMode::Type type) {
  return addCollection(resolver()->getCollectionId(name), name.c_str(), type);
}

/// @brief test if a collection is already locked
bool transaction::Methods::isLocked(LogicalCollection* document,
                                    AccessMode::Type type) const {
  if (_state == nullptr || _state->status() != transaction::Status::RUNNING) {
    return false;
  }

  TransactionCollection* trxCollection =
      _state->collection(document->cid(), type);

  TRI_ASSERT(trxCollection != nullptr);
  return trxCollection->isLocked(type, _state->nestingLevel());
}

/// @brief read- or write-lock a collection
Result transaction::Methods::lock(TransactionCollection* trxCollection,
                                  AccessMode::Type type) {
  if (_state == nullptr || _state->status() != transaction::Status::RUNNING) {
    return TRI_ERROR_TRANSACTION_INTERNAL;
  }

  return trxCollection->lock(type, _state->nestingLevel());
}

/// @brief read- or write-unlock a collection
Result transaction::Methods::unlock(TransactionCollection* trxCollection,
                                    AccessMode::Type type) {
  if (_state == nullptr || _state->status() != transaction::Status::RUNNING) {
    return TRI_ERROR_TRANSACTION_INTERNAL;
  }

  return trxCollection->unlock(type, _state->nestingLevel());
}

/// @brief get list of indexes for a collection
std::vector<std::shared_ptr<Index>> transaction::Methods::indexesForCollection(
    std::string const& collectionName) {
  if (_state->isCoordinator()) {
    return indexesForCollectionCoordinator(collectionName);
  }
  // For a DBserver we use the local case.

  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName);
  LogicalCollection* document = documentCollection(trxCollection(cid));
  return document->getIndexes();
}

/// @brief Lock all collections. Only works for selected sub-classes
int transaction::Methods::lockCollections() {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

/// @brief Clone this transaction. Only works for selected sub-classes
transaction::Methods* transaction::Methods::clone(transaction::Options const&) const {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

/// @brief Get all indexes for a collection name, coordinator case
std::shared_ptr<Index> transaction::Methods::indexForCollectionCoordinator(
    std::string const& name, std::string const& id) const {
  auto clusterInfo = arangodb::ClusterInfo::instance();
  auto collectionInfo = clusterInfo->getCollection(databaseName(), name);

  auto idxs = collectionInfo->getIndexes();
  TRI_idx_iid_t iid = basics::StringUtils::uint64(id);
  for (auto const& it : idxs) {
    if (it->id() == iid) {
      return it;
    }
  }
  return nullptr;
}

/// @brief Get all indexes for a collection name, coordinator case
std::vector<std::shared_ptr<Index>>
transaction::Methods::indexesForCollectionCoordinator(
    std::string const& name) const {

  auto dbname = databaseName();
  auto clusterInfo = arangodb::ClusterInfo::instance();
  std::shared_ptr<LogicalCollection> collection = clusterInfo->getCollection(databaseName(), name);
  std::vector<std::shared_ptr<Index>> indexes = collection->getIndexes();

  collection->clusterIndexEstimates(); // update estiamtes in logical collection
  // push updated values into indexes
  for(auto i : indexes){
    i->updateClusterEstimate();
  }

  return indexes;
}

/// @brief get the index by it's identifier. Will either throw or
///        return a valid index. nullptr is impossible.
transaction::Methods::IndexHandle transaction::Methods::getIndexByIdentifier(
    std::string const& collectionName, std::string const& indexHandle) {
  if (_state->isCoordinator()) {
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
                                         "' in collection '" + collectionName +
                                         "'.");
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
                                       "' in collection '" + collectionName +
                                       "'.");
  }

  // We have successfully found an index with the requested id.
  return IndexHandle(idx);
}

/// @brief add a collection to an embedded transaction
Result transaction::Methods::addCollectionEmbedded(TRI_voc_cid_t cid,
                                                   char const* name,
                                                   AccessMode::Type type) {
  TRI_ASSERT(_state != nullptr);

  int res = _state->addCollection(cid, type, _state->nestingLevel(), false);

  if (res != TRI_ERROR_NO_ERROR) {
    if (res == TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION) {
      // special error message to indicate which collection was undeclared
      THROW_ARANGO_EXCEPTION_MESSAGE(
          res, std::string(TRI_errno_string(res)) + ": " +
                   resolver()->getCollectionNameCluster(cid) + " [" +
                   AccessMode::typeString(type) + "]");
    } else if (res == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
      throwCollectionNotFound(name);
    }
    THROW_ARANGO_EXCEPTION(res);
  }

  return res;
}

/// @brief add a collection to a top-level transaction
Result transaction::Methods::addCollectionToplevel(TRI_voc_cid_t cid,
                                                   char const* name,
                                                   AccessMode::Type type) {
  TRI_ASSERT(_state != nullptr);

  int res;

  if (_state->status() != transaction::Status::CREATED) {
    // transaction already started?
    res = TRI_ERROR_TRANSACTION_INTERNAL;
  } else {
    res = _state->addCollection(cid, type, _state->nestingLevel(), false);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    if (res == TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION) {
      // special error message to indicate which collection was undeclared
      THROW_ARANGO_EXCEPTION_MESSAGE(
          res, std::string(TRI_errno_string(res)) + ": " +
                   resolver()->getCollectionNameCluster(cid) + " [" +
                   AccessMode::typeString(type) + "]");
    } else if (res == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
      throwCollectionNotFound(name);
    }
    THROW_ARANGO_EXCEPTION(res);
  }

  return res;
}

/// @brief set up an embedded transaction
void transaction::Methods::setupEmbedded(TRI_vocbase_t*) {
  if (!_transactionContextPtr->isEmbeddable()) {
    // we are embedded but this is disallowed...
    THROW_ARANGO_EXCEPTION(TRI_ERROR_TRANSACTION_NESTED);
  }

  _state = _transactionContextPtr->getParentTransaction();

  TRI_ASSERT(_state != nullptr);
  _state->increaseNesting();
}

/// @brief set up a top-level transaction
void transaction::Methods::setupToplevel(TRI_vocbase_t* vocbase, transaction::Options const& options) {
  // we are not embedded. now start our own transaction
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  _state = engine->createTransactionState(vocbase, options);

  TRI_ASSERT(_state != nullptr);

  // register the transaction in the context
  _transactionContextPtr->registerTransaction(_state);
}

Result transaction::Methods::resolveId(char const* handle, size_t length,
                                       TRI_voc_cid_t& cid, char const*& key,
                                       size_t& outLength) {
  char const* p = static_cast<char const*>(
      memchr(handle, TRI_DOCUMENT_HANDLE_SEPARATOR_CHR, length));

  if (p == nullptr || *p == '\0') {
    return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
  }

  if (*handle >= '0' && *handle <= '9') {
    cid = arangodb::basics::StringUtils::uint64(handle, p - handle);
  } else {
    std::string const name(handle, p - handle);
    cid = resolver()->getCollectionIdCluster(name);
  }

  if (cid == 0) {
    return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }

  key = p + 1;
  outLength = length - (key - handle);

  return TRI_ERROR_NO_ERROR;
}

/// @brief invoke a callback method when a transaction has finished
void transaction::CallbackInvoker::invoke() noexcept {
  if (!_trx->_onFinish) {
    return;
  }

  try {
    _trx->_onFinish(_trx);
  } catch (...) {
    // we must not propagate exceptions from here
  }
}
