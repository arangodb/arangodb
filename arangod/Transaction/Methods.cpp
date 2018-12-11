////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/NumberUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/encoding.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/FollowerInfo.h"
#include "Cluster/ReplicationTimeoutFeature.h"
#include "Cluster/ServerState.h"
#include "ClusterEngine/ClusterEngine.h"
#include "Indexes/Index.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Options.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/ExecContext.h"
#include "Utils/Events.h"
#include "Utils/OperationCursor.h"
#include "Utils/OperationOptions.h"
#include "VocBase/KeyLockInfo.h"
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

enum class ReplicationType {
  NONE, LEADER, FOLLOWER
};

// wrap vector inside a static function to ensure proper initialization order
std::vector<arangodb::transaction::Methods::DataSourceRegistrationCallback>& getDataSourceRegistrationCallbacks() {
  static std::vector<arangodb::transaction::Methods::DataSourceRegistrationCallback> callbacks;

  return callbacks;
}

/// @return the status change callbacks stored in state
///         or nullptr if none and !create
std::vector<arangodb::transaction::Methods::StatusChangeCallback const*>* getStatusChangeCallbacks(
    arangodb::TransactionState& state,
    bool create = false
) {
  struct CookieType: public arangodb::TransactionState::Cookie {
    std::vector<arangodb::transaction::Methods::StatusChangeCallback const*> _callbacks;
  };

  static const int key = 0; // arbitrary location in memory, common for all

  // TODO FIXME find a better way to look up a ViewState
  #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto* cookie = dynamic_cast<CookieType*>(state.cookie(&key));
  #else
    auto* cookie = static_cast<CookieType*>(state.cookie(&key));
  #endif

  if (!cookie && create) {
    auto ptr = std::make_unique<CookieType>();

    cookie = ptr.get();
    state.cookie(&key, std::move(ptr));
  }

  return cookie ? &(cookie->_callbacks) : nullptr;
}

/// @brief notify callbacks of association of 'cid' with this TransactionState
/// @note done separately from addCollection() to avoid creating a
///       TransactionCollection instance for virtual entities, e.g. View
arangodb::Result applyDataSourceRegistrationCallbacks(
    LogicalDataSource& dataSource,
    arangodb::transaction::Methods& trx
) {
  for (auto& callback: getDataSourceRegistrationCallbacks()) {
    TRI_ASSERT(callback); // addDataSourceRegistrationCallback(...) ensures valid

    try {
      auto res = callback(dataSource, trx);

      if (res.fail()) {
        return res;
      }
    } catch (...) {
      return arangodb::Result(TRI_ERROR_INTERNAL);
    }
  }

  return arangodb::Result();
}

/// @brief notify callbacks of association of 'cid' with this TransactionState
/// @note done separately from addCollection() to avoid creating a
///       TransactionCollection instance for virtual entities, e.g. View
void applyStatusChangeCallbacks(
    arangodb::transaction::Methods& trx,
    arangodb::transaction::Status status
) noexcept {
  TRI_ASSERT(
    arangodb::transaction::Status::ABORTED == status
    || arangodb::transaction::Status::COMMITTED == status
    || arangodb::transaction::Status::RUNNING == status
  );
  TRI_ASSERT(
    !trx.state() // for embeded transactions status is not always updated
    || (trx.state()->isTopLevelTransaction() && trx.state()->status() == status)
    || (!trx.state()->isTopLevelTransaction()
        && arangodb::transaction::Status::RUNNING == trx.state()->status()
       )
  );

  auto* state = trx.state();

  if (!state) {
    return; // nothing to apply
  }

  auto* callbacks = getStatusChangeCallbacks(*state);

  if (!callbacks) {
    return; // no callbacks to apply
  }

  // no need to lock since transactions are single-threaded
  for (auto& callback: *callbacks) {
    TRI_ASSERT(callback); // addStatusChangeCallback(...) ensures valid

    try {
      (*callback)(trx, status);
    } catch (...) {
      // we must not propagate exceptions from here
    }
  }
}

static void throwCollectionNotFound(char const* name) {
  if (name == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
      std::string(TRI_errno_string(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) +
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

/// @brief Insert an error reported instead of the new document
static void createBabiesError(VPackBuilder& builder,
                              std::unordered_map<int, size_t>& countErrorCodes,
                              Result error, bool silent) {
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

static OperationResult emptyResult(OperationOptions const& options) {
  VPackBuilder resultBuilder;
  resultBuilder.openArray();
  resultBuilder.close();
  return OperationResult(Result(), resultBuilder.steal(), nullptr, options);
}
}  // namespace

/*static*/ void transaction::Methods::addDataSourceRegistrationCallback(
    DataSourceRegistrationCallback const& callback
) {
  if (callback) {
    getDataSourceRegistrationCallbacks().emplace_back(callback);
  }
}

bool transaction::Methods::addStatusChangeCallback(
    StatusChangeCallback const* callback
) {
  if (!callback || !*callback) {
    return true; // nothing to call back
  } else if (!_state) {
    return false; // nothing to add to
  }

  auto* statusChangeCallbacks = getStatusChangeCallbacks(*_state, true);

  TRI_ASSERT(nullptr != statusChangeCallbacks); // 'create' was specified

  // no need to lock since transactions are single-threaded
  statusChangeCallbacks->emplace_back(callback);

  return true;
}

bool transaction::Methods::removeStatusChangeCallback(
    StatusChangeCallback const* callback
) {
  if (!callback || !*callback) {
    return true; // nothing to call back
  } else if (!_state) {
    return false; // nothing to add to
  }

  auto* statusChangeCallbacks = getStatusChangeCallbacks(*_state, false);
  if (statusChangeCallbacks) {
    auto it = std::find(statusChangeCallbacks->begin(), statusChangeCallbacks->end(), callback);
    TRI_ASSERT(it != statusChangeCallbacks->end());
    if (ADB_LIKELY(it != statusChangeCallbacks->end())) {
      statusChangeCallbacks->erase(it);
    }
  }
  return true;
}


/*static*/ void transaction::Methods::clearDataSourceRegistrationCallbacks() {
  getDataSourceRegistrationCallbacks().clear();
}

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
    arangodb::velocypack::Builder& builder,
    std::underlying_type<Index::Serialize>::type flags) const {
  _index->toVelocyPack(builder, flags);
}

TRI_vocbase_t& transaction::Methods::vocbase() const {
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

        // must edit nodes in place; TODO change so we can replace with copy

        auto n1 = root->getMember(previousIn)->getMember(0);
        TEMPORARILY_UNLOCK_NODE(n1);
        n1->changeMember(1, mergedIn);

        auto n2 = root->getMember(i)->getMember(0);
        {
          TEMPORARILY_UNLOCK_NODE(n2);
          n2->changeMember(1, emptyArray);
        }
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

  std::unordered_set<std::string> seenIndexConditions;

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
    bool isUnique = true;

    if (!usedIndexes.empty()) {
      // try to find duplicate condition parts, and only return each
      // unique condition part once
      try {
        std::string conditionString = conditionData->first->toString() + " - " + std::to_string(conditionData->second.getIndex()->id());
        isUnique = seenIndexConditions.emplace(std::move(conditionString)).second;
          // we already saw the same combination of index & condition
          // don't add it again
      } catch (...) {
        // condition stringification may fail. in this case, we simply carry own
        // without simplifying the condition
      }
    }

    if (isUnique) {
      root->addMember(conditionData->first);
      usedIndexes.emplace_back(conditionData->second);
    }
  }

  return true;
}

std::pair<bool, bool> transaction::Methods::findIndexHandleForAndNode(
    std::vector<std::shared_ptr<Index>> const& indexes, arangodb::aql::AstNode* node,
    arangodb::aql::Variable const* reference,
    arangodb::aql::SortCondition const* sortCondition, size_t itemsInCollection,
    std::vector<transaction::Methods::IndexHandle>& usedIndexes,
    arangodb::aql::AstNode*& specializedCondition, bool& isSparse) const {
  std::shared_ptr<Index> bestIndex;
  double bestCost = 0.0;
  bool bestSupportsFilter = false;
  bool bestSupportsSort = false;

  for (auto const& idx : indexes) {
    double filterCost = 0.0;
    double sortCost = 0.0;
    size_t itemsInIndex = itemsInCollection;

    bool supportsFilter = false;
    bool supportsSort = false;

    // check if the index supports the filter expression
    double estimatedCost;
    size_t estimatedItems;
    if (idx->supportsFilterCondition(indexes, node, reference, itemsInIndex,
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
      size_t coveredAttributes = 0;
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

    double totalCost = filterCost;
    if (!sortCondition->isEmpty()) {
      // only take into account the costs for sorting if there is actually something to sort
      totalCost += sortCost;
    }

    LOG_TOPIC(TRACE, Logger::FIXME) << "looking at index: " << idx.get() << ", isSorted: " << idx->isSorted() << ", isSparse: " << idx->sparse() << ", fields: " << idx->fields().size() << ", supportsFilter: " << supportsFilter << ", supportsSort: " << supportsSort << ", filterCost: " << filterCost << ", sortCost: " << sortCost << ", totalCost: " << totalCost << ", isOnlyAttributeAccess: " << isOnlyAttributeAccess << ", isUnidirectional: " << sortCondition->isUnidirectional() << ", isOnlyEqualityMatch: " << node->isOnlyEqualityMatch() << ", itemsInIndex: " << itemsInIndex;

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
    std::vector<std::shared_ptr<Index>> const& indexes, arangodb::aql::AstNode*& node,
    arangodb::aql::Variable const* reference, size_t itemsInCollection,
    transaction::Methods::IndexHandle& usedIndex) const {
  std::shared_ptr<Index> bestIndex;
  double bestCost = 0.0;

  for (auto const& idx : indexes) {
    size_t itemsInIndex = itemsInCollection;

    // check if the index supports the filter expression
    double estimatedCost;
    size_t estimatedItems;
    bool supportsFilter = idx->supportsFilterCondition(indexes, node, reference, itemsInIndex,
                                                       estimatedItems, estimatedCost);

    // enable the following line to see index candidates considered with their
    // abilities and scores
    LOG_TOPIC(TRACE, Logger::FIXME) << "looking at index: " << idx.get() << ", isSorted: " << idx->isSorted() << ", isSparse: " << idx->sparse() << ", fields: " << idx->fields().size() << ", supportsFilter: " << supportsFilter << ", estimatedCost: " << estimatedCost << ", estimatedItems: " << estimatedItems << ", itemsInIndex: " << itemsInIndex << ", selectivity: " << (idx->hasSelectivityEstimate() ? idx->selectivityEstimate() : -1.0) << ", node: " << node;

    if (!supportsFilter) {
      continue;
    }

    // index supports the filter condition

    // this reduces the number of items left
    itemsInIndex = estimatedItems;

    if (bestIndex == nullptr || estimatedCost < bestCost) {
      bestIndex = idx;
      bestCost = estimatedCost;
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

static bool findRefusal(std::vector<ClusterCommRequest> const& requests) {
  for (auto const& it : requests) {
    if (it.done &&
        it.result.status == CL_COMM_RECEIVED &&
        it.result.answer_code == rest::ResponseCode::NOT_ACCEPTABLE) {
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

  // brief initialize the transaction
  // this will first check if the transaction is embedded in a parent
  // transaction. if not, it will create a transaction of its own
  // check in the context if we are running embedded
  TransactionState* parent = _transactionContextPtr->getParentTransaction();

  if (parent != nullptr) { // yes, we are embedded
    if (!_transactionContextPtr->isEmbeddable()) {
      // we are embedded but this is disallowed...
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TRANSACTION_NESTED);
    }

    _state = parent;

    TRI_ASSERT(_state != nullptr);
    _state->increaseNesting();
  } else { // non-embedded
    // now start our own transaction
    StorageEngine* engine = EngineSelectorFeature::ENGINE;

    _state = engine->createTransactionState(
       _transactionContextPtr->vocbase(), options
    ).release();
    TRI_ASSERT(_state != nullptr);
    TRI_ASSERT(_state->isTopLevelTransaction());

    // register the transaction in the context
    _transactionContextPtr->registerTransaction(_state);
  }

  TRI_ASSERT(_state != nullptr);
}

/// @brief create the transaction, used to be UserTransaction
transaction::Methods::Methods(std::shared_ptr<transaction::Context> const& ctx,
                              std::vector<std::string> const& readCollections,
                              std::vector<std::string> const& writeCollections,
                              std::vector<std::string> const& exclusiveCollections,
                              transaction::Options const& options)
    : transaction::Methods(ctx, options) {
  addHint(transaction::Hints::Hint::LOCK_ENTIRELY);

  for (auto const& it : exclusiveCollections) {
    addCollection(it, AccessMode::Type::EXCLUSIVE);
  }
  for (auto const& it : writeCollections) {
    addCollection(it, AccessMode::Type::WRITE);
  }
  for (auto const& it : readCollections) {
    addCollection(it, AccessMode::Type::READ);
  }
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
  return &(_transactionContextPtr->resolver());
}

/// @brief return the transaction collection for a document collection
TransactionCollection* transaction::Methods::trxCollection(
    TRI_voc_cid_t cid, AccessMode::Type type) const {
  TRI_ASSERT(_state != nullptr);
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING ||
             _state->status() == transaction::Status::CREATED);
  return _state->collection(cid, type);
}

/// @brief order a ditch for a collection
void transaction::Methods::pinData(TRI_voc_cid_t cid) {
  TRI_ASSERT(_state != nullptr);
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING ||
             _state->status() == transaction::Status::CREATED);

  TransactionCollection* trxColl = trxCollection(cid, AccessMode::Type::READ);
  if (trxColl == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "unable to determine transaction collection");
  }

  TRI_ASSERT(trxColl->collection() != nullptr);
  _transactionContextPtr->pinData(trxColl->collection());
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
  } else {
    auto res = _state->beginTransaction(_localHints);

    if (res.fail()) {
      return res;
    }
  }

  applyStatusChangeCallbacks(*this, Status::RUNNING);

  return Result();
}

/// @brief commit / finish the transaction
Result transaction::Methods::commit() {
  TRI_IF_FAILURE("TransactionCommitFail") {
    return Result(TRI_ERROR_DEBUG);
  }

  if (_state == nullptr || _state->status() != transaction::Status::RUNNING) {
    // transaction not created or not running
    return Result(TRI_ERROR_TRANSACTION_INTERNAL, "transaction not running on commit");
  }

  ExecContext const* exe = ExecContext::CURRENT;
  if (exe != nullptr && !_state->isReadOnlyTransaction()) {
    bool cancelRW = ServerState::readOnly() && !exe->isSuperuser();
    if (exe->isCanceled() || cancelRW) {
      return Result(TRI_ERROR_ARANGO_READ_ONLY, "server is in read-only mode");
    }
  }

  if (_state->isCoordinator()) {
    if (_state->isTopLevelTransaction()) {
      _state->updateStatus(transaction::Status::COMMITTED);
    }
  } else {
    auto res = _state->commitTransaction(this);

    if (res.fail()) {
      return res;
    }
  }

  applyStatusChangeCallbacks(*this, Status::COMMITTED);

  return Result();
}

/// @brief abort the transaction
Result transaction::Methods::abort() {
  if (_state == nullptr || _state->status() != transaction::Status::RUNNING) {
    // transaction not created or not running
    return Result(TRI_ERROR_TRANSACTION_INTERNAL, "transaction not running on abort");
  }

  if (_state->isCoordinator()) {
    if (_state->isTopLevelTransaction()) {
      _state->updateStatus(transaction::Status::ABORTED);
    }
  } else {
    auto res = _state->abortTransaction(this);

    if (res.fail()) {
      return res;
    }
  }

  applyStatusChangeCallbacks(*this, Status::ABORTED);

  return Result();
}

/// @brief finish a transaction (commit or abort), based on the previous state
Result transaction::Methods::finish(int errorNum) {
  return finish(Result(errorNum));
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

/// @brief return the transaction id
TRI_voc_tid_t transaction::Methods::tid() const {
  TRI_ASSERT(_state != nullptr);
  return _state->id();
}

std::string transaction::Methods::name(TRI_voc_cid_t cid) const {
  auto c = trxCollection(cid);
  if (c == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }
  return c->collectionName();
}

/// @brief read all master pointers, using skip and limit.
/// The resualt guarantees that all documents are contained exactly once
/// as long as the collection is not modified.
OperationResult transaction::Methods::any(std::string const& collectionName) {
  if (_state->isCoordinator()) {
    return anyCoordinator(collectionName);
  }
  return anyLocal(collectionName);
}

/// @brief fetches documents in a collection in random order, coordinator
OperationResult transaction::Methods::anyCoordinator(std::string const&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

/// @brief fetches documents in a collection in random order, local
OperationResult transaction::Methods::anyLocal(
    std::string const& collectionName) {
  TRI_voc_cid_t cid = resolver()->getCollectionIdLocal(collectionName);

  if (cid == 0) {
    throwCollectionNotFound(collectionName.c_str());
  }

  pinData(cid);  // will throw when it fails

  VPackBuilder resultBuilder;
  resultBuilder.openArray();

  Result lockResult = lockRecursive(cid, AccessMode::Type::READ);

  if (!lockResult.ok() && !lockResult.is(TRI_ERROR_LOCKED)) {
    return OperationResult(lockResult);
  }

  std::unique_ptr<OperationCursor> cursor =
      indexScan(collectionName, transaction::Methods::CursorType::ANY);

  cursor->nextDocument([&resultBuilder](LocalDocumentId const& token, VPackSlice slice) {
    resultBuilder.add(slice);
  }, 1);

  if (lockResult.is(TRI_ERROR_LOCKED)) {
    Result res = unlockRecursive(cid, AccessMode::Type::READ);

    if (res.fail()) {
      return OperationResult(res);
    }
  }

  resultBuilder.close();

  return OperationResult(Result(), resultBuilder.steal(), _transactionContextPtr->orderCustomTypeHandler());
}

TRI_voc_cid_t transaction::Methods::addCollectionAtRuntime(
    TRI_voc_cid_t cid, std::string const& cname,
    AccessMode::Type type) {
  auto collection = trxCollection(cid);

  if (collection == nullptr) {
    int res = _state->addCollection(cid, cname, type, _state->nestingLevel(), true);

    if (res != TRI_ERROR_NO_ERROR) {
      if (res == TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION) {
        // special error message to indicate which collection was undeclared
        THROW_ARANGO_EXCEPTION_MESSAGE(
            res, std::string(TRI_errno_string(res)) + ": " + cname +
                     " [" + AccessMode::typeString(type) + "]");
      }
      THROW_ARANGO_EXCEPTION(res);
    }

    auto dataSource = resolver()->getDataSource(cid);

    if (!dataSource) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    }

    auto result = applyDataSourceRegistrationCallbacks(*dataSource, *this);

    if (!result.ok()) {
      THROW_ARANGO_EXCEPTION(result.errorNumber());
    }

    _state->ensureCollections(_state->nestingLevel());
    collection = trxCollection(cid);

    if (collection == nullptr) {
      throwCollectionNotFound(cname.c_str());
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
bool transaction::Methods::isEdgeCollection(std::string const& collectionName) const {
  return getCollectionType(collectionName) == TRI_COL_TYPE_EDGE;
}

/// @brief return the type of a collection
bool transaction::Methods::isDocumentCollection(
    std::string const& collectionName) const {
  return getCollectionType(collectionName) == TRI_COL_TYPE_DOCUMENT;
}

/// @brief return the type of a collection
TRI_col_type_e transaction::Methods::getCollectionType(
    std::string const& collectionName) const {
  auto collection = resolver()->getCollection(collectionName);

  return collection ? collection->type() : TRI_COL_TYPE_UNKNOWN;
}

/// @brief Iterate over all elements of the collection.
void transaction::Methods::invokeOnAllElements(
    std::string const& collectionName,
    std::function<bool(LocalDocumentId const&)> callback) {
  TRI_ASSERT(_state != nullptr && _state->status() == transaction::Status::RUNNING);
  if (_state == nullptr || _state->status() != transaction::Status::RUNNING) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_TRANSACTION_INTERNAL);
  } else if (_state->isCoordinator()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName);
  TransactionCollection* trxCol = trxCollection(cid, AccessMode::Type::READ);
  LogicalCollection* collection = documentCollection(trxCol);
  TRI_ASSERT(collection != nullptr);
  _transactionContextPtr->pinData(collection);

  Result lockResult = trxCol->lockRecursive(AccessMode::Type::READ, _state->nestingLevel());
  if (!lockResult.ok() && !lockResult.is(TRI_ERROR_LOCKED)) {
    THROW_ARANGO_EXCEPTION(lockResult);
  }

  TRI_ASSERT(isLocked(collection, AccessMode::Type::READ));

  collection->invokeOnAllElements(this, callback);

  if (lockResult.is(TRI_ERROR_LOCKED)) {
    Result res = trxCol->unlockRecursive(AccessMode::Type::READ, _state->nestingLevel());

    if (res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
    }
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
    if (opRes.fail()) {
      return opRes.result;
    }
    result.add(opRes.slice());
    return Result();
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
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
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

static OperationResult errorCodeFromClusterResult(std::shared_ptr<VPackBuilder> const& resultBody,
                                                  int defaultErrorCode) {
  // read the error number from the response and use it if present
  if (resultBody != nullptr) {
    VPackSlice slice = resultBody->slice();
    if (slice.isObject()) {
      VPackSlice num = slice.get(StaticStrings::ErrorNum);
      VPackSlice msg = slice.get(StaticStrings::ErrorMessage);
      if (num.isNumber()) {
        if (msg.isString()) {
          // found an error number and an error message, so let's use it!
          return OperationResult(Result(num.getNumericValue<int>(), msg.copyString()));
        }
        // we found an error number, so let's use it!
        return OperationResult(num.getNumericValue<int>());
      }
    }
  }

  return OperationResult(defaultErrorCode);
}

/// @brief Create Cluster Communication result for document
OperationResult transaction::Methods::clusterResultDocument(
    rest::ResponseCode const& responseCode,
    std::shared_ptr<VPackBuilder> const& resultBody,
    std::unordered_map<int, size_t> const& errorCounter) const {
  switch (responseCode) {
    case rest::ResponseCode::OK:
    case rest::ResponseCode::PRECONDITION_FAILED:
      return OperationResult(Result(responseCode == rest::ResponseCode::OK
                                 ? TRI_ERROR_NO_ERROR
                                 : TRI_ERROR_ARANGO_CONFLICT), resultBody->steal(), nullptr, OperationOptions{}, errorCounter);
    case rest::ResponseCode::NOT_FOUND:
      return errorCodeFromClusterResult(resultBody, TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
    default:
      return errorCodeFromClusterResult(resultBody, TRI_ERROR_INTERNAL);
  }
}

/// @brief Create Cluster Communication result for insert
OperationResult transaction::Methods::clusterResultInsert(
    rest::ResponseCode const& responseCode,
    std::shared_ptr<VPackBuilder> const& resultBody,
    OperationOptions const& options,
    std::unordered_map<int, size_t> const& errorCounter) const {
  switch (responseCode) {
    case rest::ResponseCode::ACCEPTED:
    case rest::ResponseCode::CREATED: {
      OperationOptions copy = options;
      copy.waitForSync = (responseCode == rest::ResponseCode::CREATED); // wait for sync is abused herea
                                                                        // operationResult should get a return code.
      return OperationResult(Result(), resultBody->steal(), nullptr, copy, errorCounter);
    }
    case rest::ResponseCode::PRECONDITION_FAILED:
      return errorCodeFromClusterResult(resultBody, TRI_ERROR_ARANGO_CONFLICT);
    case rest::ResponseCode::BAD:
      return errorCodeFromClusterResult(resultBody, TRI_ERROR_INTERNAL);
    case rest::ResponseCode::NOT_FOUND:
      return errorCodeFromClusterResult(resultBody, TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    case rest::ResponseCode::CONFLICT:
      return errorCodeFromClusterResult(resultBody, TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED);
    default:
      return errorCodeFromClusterResult(resultBody, TRI_ERROR_INTERNAL);
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
    case rest::ResponseCode::CREATED: {
      OperationOptions options;
      options.waitForSync = (responseCode == rest::ResponseCode::CREATED);
      return OperationResult(Result(errorCode), resultBody->steal(), nullptr, options, errorCounter);
    }
    case rest::ResponseCode::BAD:
      return errorCodeFromClusterResult(resultBody, TRI_ERROR_INTERNAL);
    case rest::ResponseCode::NOT_FOUND:
      return errorCodeFromClusterResult(resultBody, TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
    default:
      return errorCodeFromClusterResult(resultBody, TRI_ERROR_INTERNAL);
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
    case rest::ResponseCode::PRECONDITION_FAILED: {
      OperationOptions options;
      options.waitForSync = (responseCode != rest::ResponseCode::ACCEPTED);
      return OperationResult(
          Result(responseCode == rest::ResponseCode::PRECONDITION_FAILED
              ? TRI_ERROR_ARANGO_CONFLICT
              : TRI_ERROR_NO_ERROR),
          resultBody->steal(), nullptr,
          options, errorCounter);
    }
    case rest::ResponseCode::BAD:
      return errorCodeFromClusterResult(resultBody, TRI_ERROR_INTERNAL);
    case rest::ResponseCode::NOT_FOUND:
      return errorCodeFromClusterResult(resultBody, TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
    default:
      return errorCodeFromClusterResult(resultBody, TRI_ERROR_INTERNAL);
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
    vocbase().name(),
    collectionName,
    *this,
    value,
    options,
    responseCode,
    errorCounter,
    resultBody
  );

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
  ManagedDocumentResult result;

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

    result.clear();

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
    for (VPackSlice s : VPackArrayIterator(value)) {
      res = workForOneDocument(s, true);
      if (res.fail()) {
        createBabiesError(resultBuilder, countErrorCodes, res, options.silent);
      }
    }
    res = TRI_ERROR_NO_ERROR;
  }

  return OperationResult(std::move(res), resultBuilder.steal(),
                         _transactionContextPtr->orderCustomTypeHandler(),
                         options, countErrorCodes);
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
    return emptyResult(options);
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

  Result res = arangodb::createDocumentOnCoordinator(
      vocbase().name(), collectionName, *this, options, value, responseCode,
      errorCounter, resultBody);

  if (res.ok()) {
    return clusterResultInsert(responseCode, resultBody, options, errorCounter);
  }
  return OperationResult(res, options);
}
#endif

/// @brief choose a timeout for synchronous replication, based on the
/// number of documents we ship over
static double chooseTimeout(size_t count, size_t totalBytes) {
  // We usually assume that a server can process at least 2500 documents
  // per second (this is a low estimate), and use a low limit of 0.5s
  // and a high timeout of 120s
  double timeout = count / 2500.0;

  // Really big documents need additional adjustment. Using total size
  // of all messages to handle worst case scenario of constrained resource
  // processing all
  timeout += (totalBytes / 4096) * ReplicationTimeoutFeature::timeoutPer4k;

  if (timeout < ReplicationTimeoutFeature::lowerLimit) {
    return ReplicationTimeoutFeature::lowerLimit * ReplicationTimeoutFeature::timeoutFactor;
  }
  return (std::min)(120.0, timeout) * ReplicationTimeoutFeature::timeoutFactor;
}

/// @brief create one or multiple documents in a collection, local
/// the single-document variant of this operation will either succeed or,
/// if it fails, clean up after itself
OperationResult transaction::Methods::insertLocal(
    std::string const& collectionName, VPackSlice const value,
    OperationOptions& options) {
  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName);
  LogicalCollection* collection = documentCollection(trxCollection(cid));

  bool const needsLock = !isLocked(collection, AccessMode::Type::WRITE);

  // If we maybe will overwrite, we cannot do single document operations, thus:
  // options.overwrite => !needsLock
  TRI_ASSERT(!options.overwrite || !needsLock);

  bool const isMMFiles = EngineSelectorFeature::isMMFiles();

  // Assert my assumption that we don't have a lock only with mmfiles single
  // document operations.

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  {
    bool const isMock = EngineSelectorFeature::ENGINE->typeName() == "Mock";
    if (!isMock) {
      // needsLock => isMMFiles
      // needsLock => !value.isArray()
      // needsLock => _localHints.has(Hints::Hint::SINGLE_OPERATION))
      // However, due to nested transactions, there are mmfiles single
      // operations that already have a lock.
      TRI_ASSERT(!needsLock || isMMFiles);
      TRI_ASSERT(!needsLock || !value.isArray());
      TRI_ASSERT(!needsLock || _localHints.has(Hints::Hint::SINGLE_OPERATION));
    }
  }
#endif

  // If we are
  // - not on a single server (i.e. maybe replicating),
  // - using the MMFiles storage engine, and
  // - doing a single document operation,
  // we have to:
  // - Get the list of followers during the time span we actually do hold a
  //   collection level lock. This is to avoid races with the replication where
  //   a follower may otherwise be added between the actual document operation
  //   and the point where we get our copy of the followers, regardless of the
  //   latter happens before or after the document operation.

  // Note that getting the followers this way also doesn't do any harm in other
  // cases, except for babies because it would be done multiple times. Thus this
  // bool.
  // I suppose alternatively we could also do it via the updateFollowers
  // callback and set updateFollowers to nullptr afterwards, so we only do it
  // once.
  bool const needsToGetFollowersUnderLock = needsLock && _state->isDBServer();

  std::shared_ptr<std::vector<ServerID> const> followers;

  std::function<Result(void)> updateFollowers = nullptr;

  if (needsToGetFollowersUnderLock) {
    auto const& followerInfo = *collection->followers();

    updateFollowers = [&followerInfo, &followers]() -> Result {
      TRI_ASSERT(followers == nullptr);
      followers = followerInfo.get();

      return Result{};
    };
  } else if (_state->isDBServer()) {
    TRI_ASSERT(followers == nullptr);
    followers = collection->followers()->get();
  }

  // we may need to lock individual keys here so we can ensure that even with concurrent
  // operations on the same keys we have the same order of data application on leader
  // and followers
  KeyLockInfo keyLockInfo;

  ReplicationType replicationType = ReplicationType::NONE;
  if (_state->isDBServer()) {
    // Block operation early if we are not supposed to perform it:
    auto const& followerInfo = collection->followers();
    std::string theLeader = followerInfo->getLeader();
    if (theLeader.empty()) {
      if (!options.isSynchronousReplicationFrom.empty()) {
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION, options);
      }

      replicationType = ReplicationType::LEADER;
      if (isMMFiles && needsLock) {
        keyLockInfo.shouldLock = true;
      }
      // We cannot be silent if we may have to replicate later.
      // If we need to get the followers under the single document operation's
      // lock, we don't know yet if we will have followers later and thus cannot
      // be silent.
      // Otherwise, if we already know the followers to replicate to, we can
      // just check if they're empty.
      if (needsToGetFollowersUnderLock || keyLockInfo.shouldLock || !followers->empty()) {
        options.silent = false;
      }
    } else {  // we are a follower following theLeader
      replicationType = ReplicationType::FOLLOWER;
      if (options.isSynchronousReplicationFrom.empty()) {
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED, options);
      }
      if (options.isSynchronousReplicationFrom != theLeader) {
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_FOLLOWER_REFUSES_OPERATION, options);
      }
    }
  } // isDBServer - early block

  if (options.returnOld || options.returnNew) {
    pinData(cid);  // will throw when it fails
  }

  VPackBuilder resultBuilder;
  ManagedDocumentResult documentResult;
  TRI_voc_tick_t maxTick = 0;

  auto workForOneDocument = [&](VPackSlice const value) -> Result {
    if (!value.isObject()) {
      return Result(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
    }

    TRI_voc_tick_t resultMarkerTick = 0;
    TRI_voc_rid_t revisionId = 0;
    documentResult.clear();

    // insert with overwrite may NOT be a single document operation, as we
    // possibly need to do two separate operations (insert and replace).
    TRI_ASSERT(!(options.overwrite && needsLock));

    TRI_ASSERT(needsLock == !isLocked(collection, AccessMode::Type::WRITE));
    Result res = collection->insert(this, value, documentResult, options,
                                    resultMarkerTick, needsLock, revisionId,
                                    &keyLockInfo, updateFollowers);

    TRI_voc_rid_t previousRevisionId = 0;
    ManagedDocumentResult previousDocumentResult; // return OLD

    if (options.overwrite && res.is(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED)) {
      // RepSert Case - unique_constraint violated -> maxTick has not changed -> try replace
      resultMarkerTick = 0;
      // If we're overwriting, we already have a lock. Therefore we also don't
      // need to get the followers under the lock.
      TRI_ASSERT(!needsLock);
      TRI_ASSERT(!needsToGetFollowersUnderLock);
      TRI_ASSERT(updateFollowers == nullptr);
      res = collection->replace(this, value, documentResult, options,
                                resultMarkerTick, false, previousRevisionId,
                                previousDocumentResult, nullptr);
      if (res.ok() && !options.silent) {
        // If we are silent, then revisionId will not be looked at further
        // down. In the silent case, documentResult is empty, so nobody
        // must actually look at it!
        revisionId = TRI_ExtractRevisionId(VPackSlice(documentResult.vpack()));
      }
    }

    if (resultMarkerTick > 0 && resultMarkerTick > maxTick) {
      maxTick = resultMarkerTick;
    }

    if (res.fail()) {
      // Error reporting in the babies case is done outside of here,
      // in the single document case no body needs to be created at all.
      return res;
    }

    if (!options.silent) {
      TRI_ASSERT(!documentResult.empty());

      StringRef keyString(transaction::helpers::extractKeyFromDocument(VPackSlice(documentResult.vpack())));

      bool showReplaced = false;
      if (options.returnOld && previousRevisionId) {
        showReplaced = true;
        TRI_ASSERT(!previousDocumentResult.empty());
      }

      buildDocumentIdentity(collection, resultBuilder,
                            cid, keyString, revisionId, previousRevisionId,
                            showReplaced ? &previousDocumentResult : nullptr,
                            options.returnNew ? &documentResult : nullptr);
    }
    return Result();
  };

  Result res;
  std::unordered_map<int, size_t> countErrorCodes;
  if (value.isArray()) {
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

  if (res.ok() && replicationType == ReplicationType::LEADER) {
    TRI_ASSERT(collection != nullptr);
    TRI_ASSERT(followers != nullptr);

    // In the multi babies case res is always TRI_ERROR_NO_ERROR if we
    // get here, in the single document case, we do not try to replicate
    // in case of an error.

    // Now replicate the good operations on all followers:
    res = replicateOperations(*collection, followers, options, value,
                              TRI_VOC_DOCUMENT_OPERATION_INSERT, resultBuilder);

    if (!res.ok()) {
      return OperationResult{std::move(res), options};
    }
  }

  // wait for operation(s) to be synced to disk here. On rocksdb maxTick == 0
  if (res.ok() && options.waitForSync && maxTick > 0 &&
      isSingleOperationTransaction()) {
    EngineSelectorFeature::ENGINE->waitForSyncTick(maxTick);
  }

  if (options.silent) {
    // We needed the results, but do not want to report:
    resultBuilder.clear();
  }

  return OperationResult(std::move(res), resultBuilder.steal(), nullptr, options, countErrorCodes);
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
    return emptyResult(options);
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
    vocbase().name(),
    collectionName,
    *this,
    newValue,
    options,
    true /* isPatch */,
    headers,
    responseCode,
    errorCounter,
    resultBody
  );

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
    return emptyResult(options);
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
    vocbase().name(),
    collectionName,
    *this,
    newValue,
    options,
    false /* isPatch */,
    headers,
    responseCode,
    errorCounter,
    resultBody
  );

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

  bool const needsLock = !isLocked(collection, AccessMode::Type::WRITE);

  // Assert my assumption that we don't have a lock only with mmfiles single
  // document operations.


#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  {
    bool const isMMFiles = EngineSelectorFeature::isMMFiles();
    bool const isMock = EngineSelectorFeature::ENGINE->typeName() == "Mock";
    if (!isMock) {
      // needsLock => isMMFiles
      // needsLock => !newValue.isArray()
      // needsLock => _localHints.has(Hints::Hint::SINGLE_OPERATION))
      // However, due to nested transactions, there are mmfiles single
      // operations that already have a lock.
      TRI_ASSERT(!needsLock || isMMFiles);
      TRI_ASSERT(!needsLock || !newValue.isArray());
      TRI_ASSERT(!needsLock || _localHints.has(Hints::Hint::SINGLE_OPERATION));
    }
  }
#endif

  // If we are
  // - not on a single server (i.e. maybe replicating),
  // - using the MMFiles storage engine, and
  // - doing a single document operation,
  // we have to:
  // - Get the list of followers during the time span we actually do hold a
  //   collection level lock. This is to avoid races with the replication where
  //   a follower may otherwise be added between the actual document operation
  //   and the point where we get our copy of the followers, regardless of the
  //   latter happens before or after the document operation.
  // In update/replace we do NOT have to get document level locks as in insert
  // or remove, as we still hold a lock during the replication in this case.
  bool const needsToGetFollowersUnderLock = needsLock && _state->isDBServer();

  std::shared_ptr<std::vector<ServerID> const> followers;

  std::function<Result(void)> updateFollowers = nullptr;

  if (needsToGetFollowersUnderLock) {
    auto const& followerInfo = *collection->followers();

    updateFollowers = [&followerInfo, &followers]() -> Result {
      TRI_ASSERT(followers == nullptr);
      followers = followerInfo.get();

      return Result{};
    };
  } else if (_state->isDBServer()) {
    TRI_ASSERT(followers == nullptr);
    followers = collection->followers()->get();
  }

  ReplicationType replicationType = ReplicationType::NONE;
  if (_state->isDBServer()) {
    // Block operation early if we are not supposed to perform it:
    auto const& followerInfo = collection->followers();
    std::string theLeader = followerInfo->getLeader();
    if (theLeader.empty()) {
      if (!options.isSynchronousReplicationFrom.empty()) {
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION);
      }

      replicationType = ReplicationType::LEADER;
      // We cannot be silent if we may have to replicate later.
      // If we need to get the followers under the single document operation's
      // lock, we don't know yet if we will have followers later and thus cannot
      // be silent.
      // Otherwise, if we already know the followers to replicate to, we can
      // just check if they're empty.
      if (needsToGetFollowersUnderLock || !followers->empty()) {
        options.silent = false;
      }
    } else {  // we are a follower following theLeader
      replicationType = ReplicationType::FOLLOWER;
      if (options.isSynchronousReplicationFrom.empty()) {
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED);
      }
      if (options.isSynchronousReplicationFrom != theLeader) {
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_FOLLOWER_REFUSES_OPERATION);
      }
    }
  } // isDBServer - early block

  if (options.returnOld || options.returnNew) {
    pinData(cid);  // will throw when it fails
  }

  // Update/replace are a read and a write, let's get the write lock already
  // for the read operation:
  Result lockResult = lockRecursive(cid, AccessMode::Type::WRITE);

  if (!lockResult.ok() && !lockResult.is(TRI_ERROR_LOCKED)) {
    return OperationResult(lockResult);
  }
  // Iff we didn't have a lock before, we got one now.
  TRI_ASSERT(needsLock == lockResult.is(TRI_ERROR_LOCKED));

  VPackBuilder resultBuilder;  // building the complete result
  TRI_voc_tick_t maxTick = 0;
  ManagedDocumentResult previous;
  ManagedDocumentResult result;

  // lambda //////////////
  auto workForOneDocument = [this, &operation, &options, &maxTick, &collection,
                             &resultBuilder, &cid, &previous, &result](VPackSlice const newVal,
                                                   bool isBabies) -> Result {
    Result res;
    if (!newVal.isObject()) {
      res.reset(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
      return res;
    }

    TRI_voc_rid_t actualRevision = 0;
    TRI_voc_tick_t resultMarkerTick = 0;
    result.clear();
    previous.clear();

    // replace and update are two operations each, thus this can and must not be
    // single document operations. We need to have a lock here already.
    TRI_ASSERT(isLocked(collection, AccessMode::Type::WRITE));

    if (operation == TRI_VOC_DOCUMENT_OPERATION_REPLACE) {
      res = collection->replace(this, newVal, result, options, resultMarkerTick,
                                false, actualRevision, previous, nullptr);
    } else {
      res = collection->update(this, newVal, result, options, resultMarkerTick,
                               false, actualRevision, previous, nullptr);
    }

    if (resultMarkerTick > 0 && resultMarkerTick > maxTick) {
      maxTick = resultMarkerTick;
    }

    if (res.fail()) {
      if (res.is(TRI_ERROR_ARANGO_CONFLICT) && !isBabies) {
        StringRef key(newVal.get(StaticStrings::KeyString));
        buildDocumentIdentity(collection, resultBuilder, cid, key,
                              actualRevision, 0,
                              options.returnOld ? &previous : nullptr, nullptr);
      }
      return res;
    }

    if (!options.silent) {
      TRI_ASSERT(!previous.empty());
      TRI_ASSERT(!result.empty());
      StringRef key(newVal.get(StaticStrings::KeyString));
      buildDocumentIdentity(collection, resultBuilder, cid, key,
                            TRI_ExtractRevisionId(VPackSlice(result.vpack())),
                            actualRevision,
                            options.returnOld ? &previous : nullptr,
                            options.returnNew ? &result : nullptr);
    }

    return res;  // must be ok!
  }; // workForOneDocument
  ///////////////////////

  bool multiCase = newValue.isArray();
  std::unordered_map<int, size_t> errorCounter;
  Result res;
  if (multiCase) {
    {
      VPackArrayBuilder guard(&resultBuilder);
      VPackArrayIterator it(newValue);
      while (it.valid()) {
        res = workForOneDocument(it.value(), true);
        if (res.fail()) {
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

  if (res.ok() && replicationType == ReplicationType::LEADER) {

    // We still hold a lock here, because this is update/replace and we're
    // therefore not doing single document operations. But if we didn't hold it
    // at the beginning of the method the followers may not be up-to-date.
    TRI_ASSERT(isLocked(collection, AccessMode::Type::WRITE));
    if (needsToGetFollowersUnderLock) {
      followers = collection->followers()->get();
    }

    TRI_ASSERT(collection != nullptr);
    TRI_ASSERT(followers != nullptr);

    // In the multi babies case res is always TRI_ERROR_NO_ERROR if we
    // get here, in the single document case, we do not try to replicate
    // in case of an error.

    // Now replicate the good operations on all followers:
    res = replicateOperations(*collection, followers, options, newValue, operation,
                          resultBuilder);

    if (!res.ok()) {
      return OperationResult{std::move(res), options};
    }
  }

  // wait for operation(s) to be synced to disk here. On rocksdb maxTick == 0
  if (res.ok() && options.waitForSync && maxTick > 0 &&
      isSingleOperationTransaction()) {
    EngineSelectorFeature::ENGINE->waitForSyncTick(maxTick);
  }

  if (options.silent) {
    // We needed the results, but do not want to report:
    resultBuilder.clear();
  }

  return OperationResult(std::move(res), resultBuilder.steal(), nullptr, options, errorCounter);
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
    return emptyResult(options);
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
    vocbase().name(),
    collectionName,
    *this,
    value,
    options,
    responseCode,
    errorCounter,
    resultBody
  );

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

  bool const needsLock = !isLocked(collection, AccessMode::Type::WRITE);
  bool const isMMFiles = EngineSelectorFeature::isMMFiles();

  // Assert my assumption that we don't have a lock only with mmfiles single
  // document operations.

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  {
    bool const isMock = EngineSelectorFeature::ENGINE->typeName() == "Mock";
    if (!isMock) {
      // needsLock => isMMFiles
      // needsLock => !value.isArray()
      // needsLock => _localHints.has(Hints::Hint::SINGLE_OPERATION))
      // However, due to nested transactions, there are mmfiles single
      // operations that already have a lock.
      TRI_ASSERT(!needsLock || isMMFiles);
      TRI_ASSERT(!needsLock || !value.isArray());
      TRI_ASSERT(!needsLock || _localHints.has(Hints::Hint::SINGLE_OPERATION));
    }
  }
#endif

  // If we are
  // - not on a single server (i.e. maybe replicating),
  // - using the MMFiles storage engine, and
  // - doing a single document operation,
  // we have to:
  // - Get the list of followers during the time span we actually do hold a
  //   collection level lock. This is to avoid races with the replication where
  //   a follower may otherwise be added between the actual document operation
  //   and the point where we get our copy of the followers, regardless of the
  //   latter happens before or after the document operation.
  bool const needsToGetFollowersUnderLock = needsLock && _state->isDBServer();

  std::shared_ptr<std::vector<ServerID> const> followers;

  std::function<Result(void)> updateFollowers = nullptr;

  if (needsToGetFollowersUnderLock) {
    auto const& followerInfo = *collection->followers();

    updateFollowers = [&followerInfo, &followers]() -> Result {
      TRI_ASSERT(followers == nullptr);
      followers = followerInfo.get();

      return Result{};
    };
  } else if (_state->isDBServer()) {
    TRI_ASSERT(followers == nullptr);
    followers = collection->followers()->get();
  }

  // we may need to lock individual keys here so we can ensure that even with concurrent
  // operations on the same keys we have the same order of data application on leader
  // and followers
  KeyLockInfo keyLockInfo;

  ReplicationType replicationType = ReplicationType::NONE;
  if (_state->isDBServer()) {
    // Block operation early if we are not supposed to perform it:
    auto const& followerInfo = collection->followers();
    std::string theLeader = followerInfo->getLeader();
    if (theLeader.empty()) {
      if (!options.isSynchronousReplicationFrom.empty()) {
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION);
      }

      replicationType = ReplicationType::LEADER;
      if (isMMFiles && needsLock) {
        keyLockInfo.shouldLock = true;
      }
      // We cannot be silent if we may have to replicate later.
      // If we need to get the followers under the single document operation's
      // lock, we don't know yet if we will have followers later and thus cannot
      // be silent.
      // Otherwise, if we already know the followers to replicate to, we can
      // just check if they're empty.
      if (needsToGetFollowersUnderLock || !followers->empty()) {
        options.silent = false;
      }
    } else {  // we are a follower following theLeader
      replicationType = ReplicationType::FOLLOWER;
      if (options.isSynchronousReplicationFrom.empty()) {
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED);
      }
      if (options.isSynchronousReplicationFrom != theLeader) {
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_FOLLOWER_REFUSES_OPERATION);
      }
    }
  } // isDBServer - early block

  if (options.returnOld) {
    pinData(cid);  // will throw when it fails
  }

  VPackBuilder resultBuilder;
  ManagedDocumentResult previous;
  TRI_voc_tick_t maxTick = 0;

  auto workForOneDocument = [&](VPackSlice value, bool isBabies) -> Result {
    TRI_voc_rid_t actualRevision = 0;
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
    previous.clear();

    TRI_ASSERT(needsLock == !isLocked(collection, AccessMode::Type::WRITE));

    auto res = collection->remove(
      *this,
      value,
      options,
      resultMarkerTick,
      needsLock,
      actualRevision,
      previous,
      &keyLockInfo,
      updateFollowers
    );

    if (resultMarkerTick > 0 && resultMarkerTick > maxTick) {
      maxTick = resultMarkerTick;
    }

    if (res.fail()) {
      if (res.is(TRI_ERROR_ARANGO_CONFLICT) && !isBabies) {
        buildDocumentIdentity(collection, resultBuilder, cid, key,
                              actualRevision, 0,
                              options.returnOld ? &previous : nullptr, nullptr);
      }
      return res;
    }

    TRI_ASSERT(!previous.empty());
    if (!options.silent) {
      buildDocumentIdentity(collection, resultBuilder, cid, key, actualRevision,
                            0, options.returnOld ? &previous : nullptr, nullptr);
    }

    return Result();
  };

  Result res;
  std::unordered_map<int, size_t> countErrorCodes;
  if (value.isArray()) {
    VPackArrayBuilder guard(&resultBuilder);
    for (auto const& s : VPackArrayIterator(value)) {
      res = workForOneDocument(s, true);
      if (res.fail()) {
        createBabiesError(resultBuilder, countErrorCodes, res, options.silent);
      }
    }
    // With babies the reporting is handled somewhere else.
    res = Result(TRI_ERROR_NO_ERROR);
  } else {
    res = workForOneDocument(value, false);
  }

  if (res.ok() && replicationType == ReplicationType::LEADER) {
    TRI_ASSERT(collection != nullptr);
    TRI_ASSERT(followers != nullptr);
    // Now replicate the same operation on all followers:

    // In the multi babies case res is always TRI_ERROR_NO_ERROR if we
    // get here, in the single document case, we do not try to replicate
    // in case of an error.

    // Now replicate the good operations on all followers:
    res = replicateOperations(*collection, followers, options, value,
                              TRI_VOC_DOCUMENT_OPERATION_REMOVE, resultBuilder);

    if (!res.ok()) {
      return OperationResult{std::move(res), options};
    }
  }

  // wait for operation(s) to be synced to disk here. On rocksdb maxTick == 0
  if (res.ok() && options.waitForSync && maxTick > 0 &&
      isSingleOperationTransaction()) {
    EngineSelectorFeature::ENGINE->waitForSyncTick(maxTick);
  }

  if (options.silent) {
    // We needed the results, but do not want to report:
    resultBuilder.clear();
  }

  return OperationResult(std::move(res), resultBuilder.steal(), nullptr, options, countErrorCodes);
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

  VPackBuilder resultBuilder;
  resultBuilder.openArray();

  Result lockResult = lockRecursive(cid, AccessMode::Type::READ);

  if (!lockResult.ok() && !lockResult.is(TRI_ERROR_LOCKED)) {
    return OperationResult(lockResult);
  }

  std::unique_ptr<OperationCursor> cursor =
      indexScan(collectionName, transaction::Methods::CursorType::ALL);

  if (cursor->fail()) {
    return OperationResult(cursor->code);
  }

  auto cb = [&resultBuilder](LocalDocumentId const& token, VPackSlice slice) {
    resultBuilder.add(slice);
  };
  cursor->allDocuments(cb, 1000);

  if (lockResult.is(TRI_ERROR_LOCKED)) {
    Result res = unlockRecursive(cid, AccessMode::Type::READ);

    if (res.ok()) {
      return OperationResult(res);
    }
  }

  resultBuilder.close();

  return OperationResult(Result(), resultBuilder.steal(), _transactionContextPtr->orderCustomTypeHandler());
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

  events::TruncateCollection(collectionName, result.errorNumber());
  return result;
}

/// @brief remove all documents in a collection, coordinator
#ifndef USE_ENTERPRISE
OperationResult transaction::Methods::truncateCoordinator(
    std::string const& collectionName, OperationOptions& options) {
  return OperationResult(arangodb::truncateCollectionOnCoordinator(
    vocbase().name(), collectionName)
  );
}
#endif

/// @brief remove all documents in a collection, local
OperationResult transaction::Methods::truncateLocal(
    std::string const& collectionName, OperationOptions& options) {
  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName);

  LogicalCollection* collection = documentCollection(trxCollection(cid));

  std::shared_ptr<std::vector<ServerID> const> followers;

  ReplicationType replicationType = ReplicationType::NONE;
  if (_state->isDBServer()) {
    // Block operation early if we are not supposed to perform it:
    auto const& followerInfo = collection->followers();
    std::string theLeader = followerInfo->getLeader();
    if (theLeader.empty()) {
      if (!options.isSynchronousReplicationFrom.empty()) {
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION);
      }

      // fetch followers
      followers = followerInfo->get();
      if (followers->size() > 0) {
        replicationType = ReplicationType::LEADER;
        options.silent = false;
      }
    } else {  // we are a follower following theLeader
      replicationType = ReplicationType::FOLLOWER;
      if (options.isSynchronousReplicationFrom.empty()) {
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED);
      }
      if (options.isSynchronousReplicationFrom != theLeader) {
        return OperationResult(TRI_ERROR_CLUSTER_SHARD_FOLLOWER_REFUSES_OPERATION);
      }
    }
  } // isDBServer - early block

  pinData(cid);  // will throw when it fails

  Result lockResult = lockRecursive(cid, AccessMode::Type::WRITE);

  if (!lockResult.ok() && !lockResult.is(TRI_ERROR_LOCKED)) {
    return OperationResult(lockResult);
  }

  TRI_ASSERT(isLocked(collection, AccessMode::Type::WRITE));

  auto res = collection->truncate(*this, options);;

  if (res.fail()) {
    if (lockResult.is(TRI_ERROR_LOCKED)) {
      unlockRecursive(cid, AccessMode::Type::WRITE);
    }

    return OperationResult(res);
  }

  // Now see whether or not we have to do synchronous replication:
  if (replicationType == ReplicationType::LEADER) {
    TRI_ASSERT(followers != nullptr);

    // Now replicate the good operations on all followers:
    auto cc = arangodb::ClusterComm::instance();

    if (cc != nullptr) {
      // nullptr only happens on controlled shutdown
      std::string path =
          "/_db/" +
          arangodb::basics::StringUtils::urlEncode(vocbase().name()) +
          "/_api/collection/" +
          arangodb::basics::StringUtils::urlEncode(collectionName) +
          "/truncate?isSynchronousReplication=" +
          ServerState::instance()->getId();
      auto body = std::make_shared<std::string>();

      // Now prepare the requests:
      std::vector<ClusterCommRequest> requests;
      requests.reserve(followers->size());

      for (auto const& f : *followers) {
        requests.emplace_back("server:" + f, arangodb::rest::RequestType::PUT,
                              path, body);
      }

      size_t nrDone = 0;
      cc->performRequests(requests, 120.0, nrDone, Logger::REPLICATION, false);
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

  if (lockResult.is(TRI_ERROR_LOCKED)) {
    res = unlockRecursive(cid, AccessMode::Type::WRITE);
  }

  return OperationResult(res);
}

/// @brief count the number of documents in a collection
OperationResult transaction::Methods::count(std::string const& collectionName,
                                            transaction::CountType type) {
  TRI_ASSERT(_state->status() == transaction::Status::RUNNING);

  if (_state->isCoordinator()) {
    return countCoordinator(collectionName, type);
  }

  if (type == CountType::Detailed) {
    // we are a single-server... we cannot provide detailed per-shard counts,
    // so just downgrade the request to a normal request
    type = CountType::Normal;
  }

  return countLocal(collectionName, type);
}

#ifndef USE_ENTERPRISE
/// @brief count the number of documents in a collection
OperationResult transaction::Methods::countCoordinator(
    std::string const& collectionName, transaction::CountType type) {

  ClusterInfo* ci = ClusterInfo::instance();
  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return OperationResult(TRI_ERROR_SHUTTING_DOWN);
  }

  // First determine the collection ID from the name:
  auto collinfo = ci->getCollectionNT(vocbase().name(), collectionName);
  if (collinfo == nullptr) {
    return OperationResult(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  return countCoordinatorHelper(collinfo, collectionName, type);
}

#endif

OperationResult transaction::Methods::countCoordinatorHelper(
    std::shared_ptr<LogicalCollection> const& collinfo, std::string const& collectionName, transaction::CountType type) {
  TRI_ASSERT(collinfo != nullptr);
  auto& cache = collinfo->countCache();

  int64_t documents = CountCache::NotPopulated;
  if (type == transaction::CountType::ForceCache) {
    // always return from the cache, regardless what's in it
    documents = cache.get();
  } else if (type == transaction::CountType::TryCache) {
    documents = cache.get(CountCache::Ttl);
  }

  if (documents == CountCache::NotPopulated) {
    // no cache hit, or detailed results requested
    std::vector<std::pair<std::string, uint64_t>> count;
    auto res = arangodb::countOnCoordinator(
      vocbase().name(), collectionName, *this, count
    );

    if (res != TRI_ERROR_NO_ERROR) {
      return OperationResult(res);
    }

    int64_t total = 0;
    OperationResult opRes = buildCountResult(count, type, total);
    cache.store(total);
    return opRes;
  }

  // cache hit!
  TRI_ASSERT(documents >= 0);
  TRI_ASSERT(type != transaction::CountType::Detailed);

  // return number from cache
  VPackBuilder resultBuilder;
  resultBuilder.add(VPackValue(documents));
  return OperationResult(Result(), resultBuilder.buffer(), nullptr);
}

/// @brief count the number of documents in a collection
OperationResult transaction::Methods::countLocal(
    std::string const& collectionName, transaction::CountType type) {
  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName);
  LogicalCollection* collection = documentCollection(trxCollection(cid));

  Result lockResult = lockRecursive(cid, AccessMode::Type::READ);

  if (!lockResult.ok() && !lockResult.is(TRI_ERROR_LOCKED)) {
    return OperationResult(lockResult);
  }

  TRI_ASSERT(isLocked(collection, AccessMode::Type::READ));

  uint64_t num = collection->numberDocuments(this, type);

  if (lockResult.is(TRI_ERROR_LOCKED)) {
    Result res = unlockRecursive(cid, AccessMode::Type::READ);

    if (res.fail()) {
      return OperationResult(res);
    }
  }

  VPackBuilder resultBuilder;
  resultBuilder.add(VPackValue(num));

  return OperationResult(Result(), resultBuilder.steal(), nullptr);
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

  // must edit root in place; TODO change so we can replace with copy
  TEMPORARILY_UNLOCK_NODE(root);

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

  // Const cast is save here. Giving computeSpecialization == false
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

  return idx->supportsFilterCondition(std::vector<std::shared_ptr<Index>>(), condition, reference, itemsInIndex,
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
    IndexIteratorOptions const& opts) {
  if (_state->isCoordinator()) {
    // The index scan is only available on DBServers and Single Server.
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
  }

  auto idx = indexId.getIndex();
  if (nullptr == idx) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The index id cannot be empty.");
  }

  // Now create the Iterator
  std::unique_ptr<IndexIterator> iterator(
      idx->iteratorForCondition(this, mmdr, condition, var, opts));

  if (iterator == nullptr) {
    // We could not create an ITERATOR and it did not throw an error itself
    return new OperationCursor(TRI_ERROR_OUT_OF_MEMORY);
  }

  return new OperationCursor(iterator.release(), defaultBatchSize());
}

/// @brief factory for OperationCursor objects
/// note: the caller must have read-locked the underlying collection when
/// calling this method
std::unique_ptr<OperationCursor> transaction::Methods::indexScan(
    std::string const& collectionName, CursorType cursorType) {
  // For now we assume indexId is the iid part of the index.

  if (_state->isCoordinator()) {
    // The index scan is only available on DBServers and Single Server.
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER);
  }

  TRI_voc_cid_t cid = addCollectionAtRuntime(collectionName);
  TransactionCollection* trxColl = trxCollection(cid);
  if (trxColl == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL, "unable to determine transaction collection");
  }
  LogicalCollection* logical = documentCollection(trxColl);
  TRI_ASSERT(logical != nullptr);

  // will throw when it fails
  _transactionContextPtr->pinData(logical);

  std::unique_ptr<IndexIterator> iterator = nullptr;
  switch (cursorType) {
    case CursorType::ANY: {
      iterator = logical->getAnyIterator(this);
      break;
    }
    case CursorType::ALL: {
      iterator = logical->getAllIterator(this);
      break;
    }
  }
  if (iterator == nullptr) {
    // We could not create an ITERATOR and it did not throw an error itself
    return std::make_unique<OperationCursor>(TRI_ERROR_OUT_OF_MEMORY);
  }

  return std::make_unique<OperationCursor>(iterator.release(),
                                           defaultBatchSize());
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

  auto trxColl = trxCollection(cid, AccessMode::Type::READ);
  if (trxColl == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "could not find collection");
  }

  TRI_ASSERT(trxColl != nullptr);
  TRI_ASSERT(trxColl->collection() != nullptr);
  return trxColl->collection();
}

/// @brief add a collection by id, with the name supplied
Result transaction::Methods::addCollection(TRI_voc_cid_t cid, std::string const& cname,
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

  if (_state->isTopLevelTransaction()
      && status != transaction::Status::CREATED) {
    // transaction already started?
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_TRANSACTION_INTERNAL,
      "cannot add collection to a previously started top-level transaction"
    );
  }

  if (cid == 0) {
    // invalid cid
    throwCollectionNotFound(cname.c_str());
  }

  auto addCollection = [this, &cname, type](TRI_voc_cid_t cid)->void {
    auto res =
      _state->addCollection(cid, cname, type, _state->nestingLevel(), false);

    if (TRI_ERROR_NO_ERROR == res) {
      return;
    }

    if (TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION == res) {
      // special error message to indicate which collection was undeclared
      THROW_ARANGO_EXCEPTION_MESSAGE(
        res,
        std::string(TRI_errno_string(res)) + ": " + cname
        + " [" + AccessMode::typeString(type) + "]"
      );
    }

    if (TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == res) {
      throwCollectionNotFound(cname.c_str());
    }

    THROW_ARANGO_EXCEPTION(res);
  };

  Result res;
  bool visited = false;
  std::function<bool(LogicalCollection&)> visitor(
    [this, &addCollection, &res, cid, &visited](LogicalCollection& col)->bool {
      addCollection(col.id()); // will throw on error
      res = applyDataSourceRegistrationCallbacks(col, *this);
      visited |= cid == col.id();

      return res.ok(); // add the remaining collections (or break on error)
    }
  );

  if (!resolver()->visitCollections(visitor, cid) || res.fail()) {
    // trigger exception as per the original behaviour (tests depend on this)
    if (res.ok() && !visited) {
      addCollection(cid); // will throw on error
    }

    return res.ok() ? Result(TRI_ERROR_INTERNAL) : res; // return first error
  }

  // skip provided 'cid' if it was already done by the visitor
  if (visited) {
    return res;
  }

  auto dataSource = resolver()->getDataSource(cid);

  return dataSource
    ? applyDataSourceRegistrationCallbacks(*dataSource, *this)
    : Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)
    ;
}

/// @brief add a collection by name
Result transaction::Methods::addCollection(std::string const& name,
                                           AccessMode::Type type) {
  return addCollection(resolver()->getCollectionId(name), name, type);
}

bool transaction::Methods::isLockedShard(std::string const& shardName) const {
  return _state->isLockedShard(shardName);
}

void transaction::Methods::setLockedShard(std::string const& shardName) {
  _state->setLockedShard(shardName);
}

void transaction::Methods::setLockedShards(std::unordered_set<std::string> const& lockedShards) {
  _state->setLockedShards(lockedShards);
}

/// @brief test if a collection is already locked
bool transaction::Methods::isLocked(LogicalCollection* document,
                                    AccessMode::Type type) const {
  if (_state == nullptr || _state->status() != transaction::Status::RUNNING) {
    return false;
  }
  if (_state->hasHint(Hints::Hint::LOCK_NEVER)) {
    // In the lock never case we have made sure that
    // some other process holds this lock.
    // So we can lie here and report that it actually
    // is locked!
    return true;
  }

  TransactionCollection* trxColl = trxCollection(document->id(), type);
  TRI_ASSERT(trxColl != nullptr);
  return trxColl->isLocked(type, _state->nestingLevel());
}

/// @brief read- or write-lock a collection
Result transaction::Methods::lockRecursive(TRI_voc_cid_t cid,
                                           AccessMode::Type type) {
  if (_state == nullptr || _state->status() != transaction::Status::RUNNING) {
    return Result(TRI_ERROR_TRANSACTION_INTERNAL, "transaction not running on lock");
  }
  TransactionCollection* trxColl = trxCollection(cid, type);
  TRI_ASSERT(trxColl != nullptr);
  return Result(trxColl->lockRecursive(type, _state->nestingLevel()));
}

/// @brief read- or write-unlock a collection
Result transaction::Methods::unlockRecursive(TRI_voc_cid_t cid,
                                             AccessMode::Type type) {
  if (_state == nullptr || _state->status() != transaction::Status::RUNNING) {
    return Result(TRI_ERROR_TRANSACTION_INTERNAL, "transaction not running on unlock");
  }
  TransactionCollection* trxColl = trxCollection(cid, type);
  TRI_ASSERT(trxColl != nullptr);
  return Result(trxColl->unlockRecursive(type, _state->nestingLevel()));
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
  auto collectionInfo = clusterInfo->getCollection(vocbase().name(), name);
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
  auto clusterInfo = arangodb::ClusterInfo::instance();
  auto collection = clusterInfo->getCollection(vocbase().name(), name);

  // update selectivity estimates if they were expired
  collection->clusterIndexEstimates(true);
  return collection->getIndexes();
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

Result transaction::Methods::resolveId(char const* handle, size_t length,
                                       std::shared_ptr<LogicalCollection>& collection, char const*& key,
                                       size_t& outLength) {
  char const* p = static_cast<char const*>(
      memchr(handle, TRI_DOCUMENT_HANDLE_SEPARATOR_CHR, length));

  if (p == nullptr || *p == '\0') {
    return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
  }

  std::string const name(handle, p - handle);
  collection = resolver()->getCollectionStructCluster(name);

  if (collection == nullptr) {
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  key = p + 1;
  outLength = length - (key - handle);

  return TRI_ERROR_NO_ERROR;
}

// Unified replication of operations. May be inserts (with or without
// overwrite), removes, or modifies (updates/replaces).
Result Methods::replicateOperations(
    LogicalCollection const& collection,
    std::shared_ptr<const std::vector<std::string>> const& followers,
    OperationOptions const& options, VPackSlice const value,
    TRI_voc_document_operation_e const operation, VPackBuilder& resultBuilder) {
  TRI_ASSERT(followers != nullptr);

  if (followers->empty()) {
    return Result{};
  }

  // nullptr only happens on controlled shutdown
  auto cc = arangodb::ClusterComm::instance();
  if (cc == nullptr) {
    return Result{};
  };

  // path and requestType are different for insert/remove/modify.

  std::stringstream pathStream;
  pathStream << "/_db/"
             << arangodb::basics::StringUtils::urlEncode(vocbase().name())
             << "/_api/document/"
             << arangodb::basics::StringUtils::urlEncode(collection.name());
  if (operation != TRI_VOC_DOCUMENT_OPERATION_INSERT && !value.isArray()) {
    TRI_ASSERT(value.isObject());
    TRI_ASSERT(value.hasKey(StaticStrings::KeyString));
    pathStream << "/" << value.get(StaticStrings::KeyString).copyString();
  }
  pathStream << "?isRestore=true&isSynchronousReplication="
             << ServerState::instance()->getId() << "&"
             << StaticStrings::SilentString << "=true";

  arangodb::rest::RequestType requestType = RequestType::ILLEGAL;

  switch (operation) {
    case TRI_VOC_DOCUMENT_OPERATION_INSERT:
      requestType = arangodb::rest::RequestType::POST;
      pathStream << "&" << StaticStrings::OverWrite << "="
                 << (options.overwrite ? "true" : "false");
      break;
    case TRI_VOC_DOCUMENT_OPERATION_UPDATE:
      requestType = arangodb::rest::RequestType::PATCH;
      break;
    case TRI_VOC_DOCUMENT_OPERATION_REPLACE:
      requestType = arangodb::rest::RequestType::PUT;
      break;
    case TRI_VOC_DOCUMENT_OPERATION_REMOVE:
      requestType = arangodb::rest::RequestType::DELETE_REQ;
      break;
    case TRI_VOC_DOCUMENT_OPERATION_UNKNOWN:
    default:
      TRI_ASSERT(false);
  }

  std::string const path {pathStream.str()};

  transaction::BuilderLeaser payload(this);

  auto doOneDoc = [&](VPackSlice const& doc, VPackSlice result) {
    VPackObjectBuilder guard(payload.get());
    VPackSlice s = result.get(StaticStrings::KeyString);
    payload->add(StaticStrings::KeyString, s);
    s = result.get(StaticStrings::RevString);
    payload->add(StaticStrings::RevString, s);
    if (operation != TRI_VOC_DOCUMENT_OPERATION_REMOVE) {
      TRI_SanitizeObject(doc, *payload.get());
    }
  };

  VPackSlice ourResult = resultBuilder.slice();
  size_t count = 0;
  if (value.isArray()) {
    VPackArrayBuilder guard(payload.get());
    VPackArrayIterator itValue(value);
    VPackArrayIterator itResult(ourResult);
    while (itValue.valid() && itResult.valid()) {
      TRI_ASSERT((*itResult).isObject());
      if (!(*itResult).hasKey(StaticStrings::Error)) {
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

  if (count == 0) {
    // nothing to do
    return Result{};
  }

  auto body = std::make_shared<std::string>();
  *body = payload->slice().toJson();

  // Now prepare the requests:
  std::vector<ClusterCommRequest> requests;
  requests.reserve(followers->size());

  for (auto const& f : *followers) {
    requests.emplace_back("server:" + f, requestType, path, body);
  }

  double const timeout = chooseTimeout(count, body->size() * followers->size());

  size_t nrDone = 0;

  cc->performRequests(requests,
                      timeout,
                      nrDone, Logger::REPLICATION, false);
  // If any would-be-follower refused to follow there are two possiblities:
  // (1) there is a new leader in the meantime, or
  // (2) the follower was restarted and forgot that it is a follower.
  // Unfortunately, we cannot know which is the case.
  // In case (1) case we must not allow
  // this operation to succeed, since the new leader is now responsible.
  // In case (2) we at least have to drop the follower such that it
  // resyncs and we can be sure that it is in sync again.
  // Therefore, we drop the follower here (just in case), and refuse to
  // return with a refusal error (note that we use the follower version,
  // since we have lost leadership):

  // We drop all followers that were not successful:
  for (size_t i = 0; i < followers->size(); ++i) {
    bool replicationWorked =
      requests[i].done &&
      requests[i].result.status == CL_COMM_RECEIVED &&
      (requests[i].result.answer_code == rest::ResponseCode::ACCEPTED ||
       requests[i].result.answer_code == rest::ResponseCode::CREATED ||
       requests[i].result.answer_code == rest::ResponseCode::OK);
    if (replicationWorked) {
      bool found;
      requests[i].result.answer->header(StaticStrings::ErrorCodes, found);
      replicationWorked = !found;
    }
    if (!replicationWorked) {
      auto const& followerInfo = collection.followers();
      if (followerInfo->remove((*followers)[i])) {
        LOG_TOPIC(WARN, Logger::REPLICATION)
          << "synchronous replication: dropping follower " << (*followers)[i]
          << " for shard " << collection.name();
      } else {
        LOG_TOPIC(ERR, Logger::REPLICATION)
          << "synchronous replication: could not drop follower "
          << (*followers)[i] << " for shard " << collection.name();
        THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_COULD_NOT_DROP_FOLLOWER);
      }
    }
  }

  if (findRefusal(requests)) {
    return Result{TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED};
  }

  return Result{};
}
