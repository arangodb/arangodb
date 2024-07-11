////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "RocksDBOptimizerRules.h"

#include "Aql/Ast.h"
#include "Aql/AttributeNamePath.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/LimitNode.h"
#include "Aql/ExecutionNode/SortNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/IndexHint.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRule.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/Query.h"
#include "Basics/StaticStrings.h"
#include "Containers/FlatHashSet.h"
#include "Indexes/Index.h"
#include "StorageEngine/PhysicalCollection.h"
#include "VocBase/LogicalCollection.h"

#include <absl/strings/str_cat.h>

#include <initializer_list>

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

namespace {

std::initializer_list<ExecutionNode::NodeType> const
    reduceExtractionToProjectionTypes = {ExecutionNode::ENUMERATE_COLLECTION,
                                         ExecutionNode::INDEX};

}  // namespace

void RocksDBOptimizerRules::registerResources(OptimizerRulesFeature& feature) {
  // simplify an EnumerationCollectionNode that fetches an entire document to a
  // projection of this document
  feature.registerRule(
      "reduce-extraction-to-projection", reduceExtractionToProjectionRule,
      OptimizerRule::reduceExtractionToProjectionRule,
      OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled),
      R"(Modify `EnumerationCollectionNode` and `IndexNode` that would have
extracted entire documents to only return a projection of each document.

Projections are limited to at most 5 different document attributes by default.
The maximum number of projected attributes can optionally be adjusted by
setting the `maxProjections` hint for an AQL `FOR` operation since
ArangoDB 3.9.1.)");

  // remove SORT RAND() LIMIT 1 if appropriate
  feature.registerRule(
      "remove-sort-rand-limit-1", removeSortRandRule,
      OptimizerRule::removeSortRandRule,
      OptimizerRule::makeFlags(OptimizerRule::Flags::CanBeDisabled),
      R"(Remove `SORT RAND() LIMIT 1` constructs by moving the random iteration
into `EnumerateCollectionNode`.

The RocksDB storage engine doesn't allow to seek random documents efficiently.
This optimization picks a pseudo-random document based on a limited number of
seeks within the collection's key range, selecting a random start key in the
key range, and then going a few steps before or after that.)");
}

// simplify an EnumerationCollectionNode that fetches an entire document to a
// projection of this document
void RocksDBOptimizerRules::reduceExtractionToProjectionRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  // These are all the nodes where we start traversing (including all
  // subqueries)
  containers::SmallVector<ExecutionNode*, 8> nodes;

  plan->findNodesOfType(nodes, ::reduceExtractionToProjectionTypes, true);

  bool modified = false;
  VarSet vars;
  containers::FlatHashSet<aql::AttributeNamePath> attributes;

  for (auto n : nodes) {
    // isDeterministic is false for EnumerateCollectionNodes when the "random"
    // flag is set.
    bool const isRandomOrder =
        (n->getType() == EN::ENUMERATE_COLLECTION &&
         !ExecutionNode::castTo<EnumerateCollectionNode*>(n)
              ->isDeterministic());

    DocumentProducingNode* e = dynamic_cast<DocumentProducingNode*>(n);
    if (e == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL, "cannot convert node to DocumentProducingNode");
    }

    attributes.clear();
    bool foundProjections = aql::utils::findProjections(
        n, e->outVariable(), /*expectedAttribute*/ "",
        /*excludeStartNodeFilterCondition*/ false, attributes);

    if (foundProjections && !attributes.empty() &&
        attributes.size() <= e->maxProjections()) {
      Projections projections(std::move(attributes));

      if (n->getType() == ExecutionNode::ENUMERATE_COLLECTION &&
          !isRandomOrder) {
        // the node is still an EnumerateCollection... now check if we should
        // turn it into an index scan
        EnumerateCollectionNode const* en =
            ExecutionNode::castTo<EnumerateCollectionNode const*>(n);
        auto const& hint = en->hint();

        // now check all indexes if they cover the projection
        if (!hint.isDisabled()) {
          std::vector<std::shared_ptr<Index>> indexes;

          auto& trx = plan->getAst()->query().trxForOptimization();
          if (!trx.isInaccessibleCollection(
                  en->collection()->getCollection()->name())) {
            indexes = en->collection()
                          ->getCollection()
                          ->getPhysical()
                          ->getReadyIndexes();
          }

          std::shared_ptr<Index> picked;
          auto selectIndexIfPossible =
              [&picked,
               &projections](std::shared_ptr<Index> const& idx) -> bool {
            if (idx->inProgress()) {
              // index is currently being built
              return false;
            }
            if (!idx->covers(projections)) {
              // index doesn't cover the projection
              return false;
            }
            if (idx->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX &&
                idx->type() != Index::IndexType::TRI_IDX_TYPE_HASH_INDEX &&
                idx->type() != Index::IndexType::TRI_IDX_TYPE_SKIPLIST_INDEX &&
                idx->type() !=
                    Index::IndexType::TRI_IDX_TYPE_PERSISTENT_INDEX) {
              // only the above index types are supported
              return false;
            }

            if (idx->sparse()) {
              // we cannot safely substitute a full collection scan with a
              // sparse index scan, as the sparse index may be missing some
              // documents
              return false;
            }

            picked = idx;
            return true;
          };

          bool forced = false;
          if (hint.isSimple()) {
            forced = hint.isForced();
            for (std::string const& hinted : hint.candidateIndexes()) {
              auto idx = en->collection()->getCollection()->lookupIndex(hinted);
              if (idx && selectIndexIfPossible(idx)) {
                TRI_ASSERT(picked != nullptr);
                break;
              }
            }
            if (forced && !picked) {
              THROW_ARANGO_EXCEPTION_MESSAGE(
                  TRI_ERROR_QUERY_FORCED_INDEX_HINT_UNUSABLE,
                  absl::StrCat("could not use index hint to serve query; ",
                               hint.toString()));
            }
          }

          if (!picked && !forced) {
            for (auto const& idx : indexes) {
              if (selectIndexIfPossible(idx)) {
                TRI_ASSERT(picked != nullptr);
                break;
              }
            }
          }

          if (picked != nullptr) {
            TRI_ASSERT(!picked->inProgress());
            // turn the EnumerateCollection node into an IndexNode now
            auto condition = std::make_unique<Condition>(plan->getAst());
            condition->normalize(plan.get());
            IndexIteratorOptions opts;
            opts.useCache = false;
            // we have already proven that we can use the covering index
            // optimization, so force it - if we wouldn't force it here it would
            // mean that for a FILTER-less query we would be a lot less
            // efficient for some indexes
            auto inode = plan->createNode<IndexNode>(
                plan.get(), plan->nextId(), en->collection(), en->outVariable(),
                std::vector<transaction::Methods::IndexHandle>{picked},
                false,  // here we are not using inverted index so for sure no
                        // "whole" coverage
                std::move(condition), opts);
            en->CollectionAccessingNode::cloneInto(*inode);
            en->DocumentProducingNode::cloneInto(plan.get(), *inode);
            plan->replaceNode(n, inode);

            if (en->isRestricted()) {
              inode->restrictToShard(en->restrictedShard());
            }
            // copy over specialization data from smart-joins rule
            inode->setPrototype(en->prototypeCollection(),
                                en->prototypeOutVariable());
            n = inode;
            // need to update e, because it is used later
            e = dynamic_cast<DocumentProducingNode*>(n);
            if (e == nullptr) {
              THROW_ARANGO_EXCEPTION_MESSAGE(
                  TRI_ERROR_INTERNAL,
                  "cannot convert node to DocumentProducingNode");
            }
          }
        }  // index selection
      }

      if (n->getType() == ExecutionNode::INDEX) {
        // need to update covering index support in an IndexNode
        ExecutionNode::castTo<IndexNode*>(n)->setProjections(
            std::move(projections));
      } else {
        // store projections in DocumentProducingNode
        e->setProjections(std::move(projections));
      }

      modified = true;
    } else if (foundProjections && attributes.empty() &&
               n->getType() == ExecutionNode::ENUMERATE_COLLECTION &&
               !isRandomOrder) {
      // replace collection access with primary index access (which can be
      // faster given the fact that keys and values are stored together in
      // RocksDB, but average values are much bigger in the documents column
      // family than in the primary index colum family. thus in disk-bound
      // workloads scanning the documents via the primary index should be faster
      EnumerateCollectionNode* en =
          ExecutionNode::castTo<EnumerateCollectionNode*>(n);
      auto const& hint = en->hint();

      if (!hint.isDisabled()) {
        std::shared_ptr<Index> picked;
        std::vector<std::shared_ptr<Index>> indexes;

        auto& trx = plan->getAst()->query().trxForOptimization();
        if (!trx.isInaccessibleCollection(
                en->collection()->getCollection()->name())) {
          indexes = en->collection()
                        ->getCollection()
                        ->getPhysical()
                        ->getReadyIndexes();
        }

        auto selectIndexIfPossible =
            [&picked](std::shared_ptr<Index> const& idx) -> bool {
          if (idx->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX) {
            TRI_ASSERT(!idx->inProgress());
            picked = idx;
            return true;
          }
          return false;
        };

        bool forced = false;
        if (hint.isSimple()) {
          forced = hint.isForced();
          for (std::string const& hinted : hint.candidateIndexes()) {
            auto idx = en->collection()->getCollection()->lookupIndex(hinted);
            if (idx && selectIndexIfPossible(idx)) {
              TRI_ASSERT(picked != nullptr);
              break;
            }
          }
          if (forced && !picked) {
            THROW_ARANGO_EXCEPTION_MESSAGE(
                TRI_ERROR_QUERY_FORCED_INDEX_HINT_UNUSABLE,
                absl::StrCat("could not use index hint to serve query; ",
                             hint.toString()));
          }
        }

        if (!picked && !forced) {
          for (auto const& idx : indexes) {
            if (selectIndexIfPossible(idx)) {
              TRI_ASSERT(picked != nullptr);
              break;
            }
          }
        }

        if (picked != nullptr) {
          TRI_ASSERT(picked->type() ==
                     Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX);
          TRI_ASSERT(!picked->inProgress());
          IndexIteratorOptions opts;
          opts.useCache = false;
          auto condition = std::make_unique<Condition>(plan->getAst());
          condition->normalize(plan.get());
          auto inode = plan->createNode<IndexNode>(
              plan.get(), plan->nextId(), en->collection(), en->outVariable(),
              std::vector<transaction::Methods::IndexHandle>{picked},
              false,  // here we are not using inverted index so for sure no
                      // "whole" coverage
              std::move(condition), opts);
          plan->replaceNode(n, inode);
          en->CollectionAccessingNode::cloneInto(*inode);
          en->DocumentProducingNode::cloneInto(plan.get(), *inode);

          n = inode;

          modified = true;
        }
      }  // index selection
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

/// @brief remove SORT RAND() if appropriate
void RocksDBOptimizerRules::removeSortRandRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, EN::SORT, true);

  bool modified = false;

  for (auto const& n : nodes) {
    auto node = ExecutionNode::castTo<SortNode*>(n);
    auto const& elements = node->elements();
    if (elements.size() != 1) {
      // we're looking for "SORT RAND()", which has just one sort criterion
      continue;
    }

    auto const variable = elements[0].var;
    TRI_ASSERT(variable != nullptr);

    auto setter = plan->getVarSetBy(variable->id);

    if (setter == nullptr || setter->getType() != EN::CALCULATION) {
      continue;
    }

    auto cn = ExecutionNode::castTo<CalculationNode*>(setter);
    auto const expression = cn->expression();

    if (expression == nullptr || expression->node() == nullptr ||
        expression->node()->type != NODE_TYPE_FCALL) {
      // not the right type of node
      continue;
    }

    auto funcNode = expression->node();
    auto func = static_cast<Function const*>(funcNode->getData());

    // we're looking for "RAND()", which is a function call
    // with an empty parameters array
    if (func->name != "RAND" || funcNode->numMembers() != 1 ||
        funcNode->getMember(0)->numMembers() != 0) {
      continue;
    }

    // now we're sure we got SORT RAND() !

    // we found what we were looking for!
    // now check if the dependencies qualify
    if (!n->hasDependency()) {
      break;
    }

    auto current = n->getFirstDependency();
    ExecutionNode* collectionNode = nullptr;

    while (current != nullptr) {
      switch (current->getType()) {
        case EN::SORT:
        case EN::COLLECT:
        case EN::FILTER:
        case EN::SUBQUERY:
        case EN::ENUMERATE_LIST:
        case EN::TRAVERSAL:
        case EN::SHORTEST_PATH:
        case EN::INDEX:
        case EN::ENUMERATE_IRESEARCH_VIEW: {
          // if we found another SortNode, a CollectNode, FilterNode, a
          // SubqueryNode, an EnumerateListNode, a TraversalNode or an IndexNode
          // this means we cannot apply our optimization
          collectionNode = nullptr;
          current = nullptr;
          continue;  // this will exit the while loop
        }

        case EN::ENUMERATE_COLLECTION: {
          if (collectionNode == nullptr) {
            // note this node
            collectionNode = current;
            break;
          } else {
            // we already found another collection node before. this means we
            // should not apply our optimization
            collectionNode = nullptr;
            current = nullptr;
            continue;  // this will exit the while loop
          }
          // cannot get here
          TRI_ASSERT(false);
        }

        default: {
          // ignore all other nodes
        }
      }

      if (!current->hasDependency()) {
        break;
      }

      current = current->getFirstDependency();
    }

    if (collectionNode == nullptr || !node->hasParent()) {
      continue;  // skip
    }

    current = node->getFirstParent();
    bool valid = false;
    if (current->getType() == EN::LIMIT) {
      LimitNode* ln = ExecutionNode::castTo<LimitNode*>(current);
      if (ln->limit() == 1 && ln->offset() == 0) {
        valid = true;
      }
    }

    if (valid) {
      // we found a node to modify!
      TRI_ASSERT(collectionNode->getType() == EN::ENUMERATE_COLLECTION);
      // set the random iteration flag for the EnumerateCollectionNode
      ExecutionNode::castTo<EnumerateCollectionNode*>(collectionNode)
          ->setRandom();

      // remove the SortNode and the CalculationNode
      plan->unlinkNode(n);
      plan->unlinkNode(setter);
      modified = true;
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}
