////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "ReduceExtractionToProjection.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/QueryContext.h"
#include "Aql/Condition.h"
#include "Indexes/Index.h"
#include "Containers/FlatHashSet.h"
#include "VocBase/LogicalCollection.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Aql/Optimizer.h"

namespace arangodb::aql {
namespace {

static constexpr std::initializer_list<ExecutionNode::NodeType>
    reduceExtractionToProjectionTypes{ExecutionNode::ENUMERATE_COLLECTION,
                                      ExecutionNode::INDEX};

}  // namespace
// simplify an EnumerationCollectionNode that fetches an entire document to a
// projection of this document
void reduceExtractionToProjectionRule(Optimizer* opt,
                                      std::unique_ptr<ExecutionPlan> plan,
                                      OptimizerRule const& rule) {
  // These are all the nodes where we start traversing (including all
  // subqueries)
  containers::SmallVector<ExecutionNode*, 8> nodes;

  plan->findNodesOfType(nodes, reduceExtractionToProjectionTypes, true);

  bool modified = false;
  VarSet vars;
  containers::FlatHashSet<AttributeNamePath> attributes;

  for (auto n : nodes) {
    // isDeterministic is false for EnumerateCollectionNodes when the "random"
    // flag is set.
    bool const isRandomOrder =
        (n->getType() == ExecutionNode::ENUMERATE_COLLECTION &&
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
            if (idx->type() != IndexType::Primary &&
                idx->type() != IndexType::Hash &&
                idx->type() != IndexType::Skiplist &&
                idx->type() != IndexType::Persistent) {
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
          if (idx->type() == IndexType::Primary) {
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
          TRI_ASSERT(picked->type() == IndexType::Primary);
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
}  // namespace arangodb::aql
