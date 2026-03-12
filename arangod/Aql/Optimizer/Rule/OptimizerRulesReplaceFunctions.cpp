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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "Aql/AqlFunctionsInternalCache.h"
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionNode/IndexNode.h"
#include "Aql/ExecutionNode/LimitNode.h"
#include "Aql/ExecutionNode/ReturnNode.h"
#include "Aql/ExecutionNode/SingletonNode.h"
#include "Aql/ExecutionNode/SortNode.h"
#include "Aql/ExecutionNode/SubqueryNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/IndexHint.h"
#include "Aql/Optimizer.h"
#include "Aql/OptimizerRules.h"
#include "Aql/Query.h"
#include "Aql/SortElement.h"
#include "Aql/Variable.h"
#include "Basics/AttributeNameParser.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/SupervisedBuffer.h"
#include "Basics/VelocyPackHelper.h"
#include "Containers/SmallVector.h"
#include "Indexes/Index.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"

using namespace arangodb;
using namespace arangodb::aql;
using EN = arangodb::aql::ExecutionNode;

namespace {

// FULLTEXT(collection, "attribute", "search", 100 /*limit*/[, "distance name"])
struct FulltextParams {
  std::string collection;
  std::string attribute;
  AstNode* limit = nullptr;

  explicit FulltextParams(AstNode const* node) {
    TRI_ASSERT(node->type == AstNodeType::NODE_TYPE_FCALL);
    ast::FunctionCallNode fcall(node);
    auto args = fcall.getArguments().getElements();
    TRI_ASSERT(args.size() >= 2);
    if (args[0]->isStringValue()) {
      collection = args[0]->getString();
    }
    if (args[1]->isStringValue()) {
      attribute = args[1]->getString();
    }
    if (args.size() > 3) {
      limit = args[3];
    }
  }
};

AstNode* getAstNode(CalculationNode* c) noexcept {
  return c->expression()->nodeForModification();
}

Function* getFunction(AstNode const* ast) noexcept {
  if (ast->type == AstNodeType::NODE_TYPE_FCALL) {
    ast::FunctionCallNode fcall(ast);
    return fcall.getFunction();
  }
  return nullptr;
}

AstNode* createSubqueryWithLimit(ExecutionPlan* plan, ExecutionNode* node,
                                 ExecutionNode* first, ExecutionNode* last,
                                 Variable* lastOutVariable, AstNode* limit) {
  // Creates a subquery of the following form:
  //
  //    singleton
  //        |
  //      first
  //        |
  //       ...
  //        |
  //       last
  //        |
  //     [limit]
  //        |
  //      return
  //
  // The subquery is then injected into the plan before the given `node`
  // This function returns an `AstNode*` of type reference to the subquery's
  // `outVariable` that can be used to replace the expression (or only a
  // part) of a `CalculationNode`.
  //
  if (limit && !(limit->isIntValue() || limit->isNullValue())) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
        "limit parameter has wrong type");
  }

  auto* ast = plan->getAst();

  /// singleton
  ExecutionNode* eSingleton = plan->createNode<SingletonNode>(
      plan, plan->nextId(), ast->bindParameterVariables());

  /// return
  /// link output of index with the return node
  ExecutionNode* eReturn =
      plan->createNode<ReturnNode>(plan, plan->nextId(), lastOutVariable);

  /// link nodes together
  first->addDependency(eSingleton);
  eReturn->addDependency(last);

  /// add optional limit node
  if (limit && !limit->isNullValue()) {
    ExecutionNode* eLimit = plan->createNode<LimitNode>(
        plan, plan->nextId(), 0 /*offset*/, limit->getIntValue());
    plan->insertAfter(last, eLimit);  // inject into plan
  }

  /// create subquery
  Variable* subqueryOutVariable = ast->variables()->createTemporaryVariable();
  ExecutionNode* eSubquery = plan->registerSubquery(
      new SubqueryNode(plan, plan->nextId(), eReturn, subqueryOutVariable));

  plan->insertBefore(node, eSubquery);

  // return reference to outVariable
  return ast->createNodeReference(subqueryOutVariable);
}

AstNode* replaceFullText(AstNode* funAstNode, ExecutionNode* calcNode,
                         ExecutionPlan* plan) {
  auto* ast = plan->getAst();
  QueryContext& query = ast->query();

  TRI_ASSERT(funAstNode->type == NODE_TYPE_FCALL);
  FulltextParams params(funAstNode);

  /// index
  //  we create this first as creation of this node is more
  //  likely to fail than the creation of other nodes

  //  index - part 1 - figure out index to use
  std::shared_ptr<arangodb::Index> index;
  std::vector<basics::AttributeName> field;
  TRI_ParseAttributeString(params.attribute, field, false);

  aql::Collection* coll = query.collections().get(params.collection);
  if (!coll) {
    coll = addCollectionToQuery(query, params.collection, "FULLTEXT");
  }

  for (auto& idx : coll->indexes()) {
    if (idx->type() ==
        arangodb::Index::IndexType::TRI_IDX_TYPE_FULLTEXT_INDEX) {
      if (basics::AttributeName::isIdentical(
              idx->fields()[0], field, false /*ignore expansion in last?!*/)) {
        index = idx;
        break;
      }
    }
  }

  if (!index) {  // not found or error
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_FULLTEXT_INDEX_MISSING,
                                  params.collection.c_str());
  }

  // index part 2 - get remaining vars required for index creation
  auto* aqlCollection =
      aql::addCollectionToQuery(query, params.collection, "FULLTEXT");
  auto condition = std::make_unique<Condition>(ast);
  condition->andCombine(funAstNode);
  condition->normalize(plan);
  // create a fresh out variable
  Variable* indexOutVariable = ast->variables()->createTemporaryVariable();

  ExecutionNode* eIndex = plan->createNode<IndexNode>(
      plan, plan->nextId(), aqlCollection, indexOutVariable,
      std::vector<transaction::Methods::IndexHandle>{
          transaction::Methods::IndexHandle{index}},
      false,  // here we are not using inverted index so for sure
              // no "whole" coverage
      std::move(condition), IndexIteratorOptions());

  //// wrap plan part into subquery
  return createSubqueryWithLimit(plan, calcNode, eIndex, eIndex,
                                 indexOutVariable, params.limit);
}

}  // namespace

//! @brief replace legacy JS Functions with pure AQL
void arangodb::aql::replaceNearWithinFulltextRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  bool modified = false;

  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, ExecutionNode::CALCULATION, true);

  for (auto const& node : nodes) {
    auto visitor = [&modified, &node, &plan](AstNode* astnode) {
      auto* fun = getFunction(
          astnode);  // if fun != nullptr -> astnode->type NODE_TYPE_FCALL
      if (fun) {
        AstNode* replacement = nullptr;
        if (fun->name == "FULLTEXT") {
          replacement = replaceFullText(astnode, node, plan.get());
          TRI_ASSERT(replacement);
        }

        if (replacement) {
          modified = true;
          return replacement;
        }
      }

      return astnode;
    };

    CalculationNode* calc = ExecutionNode::castTo<CalculationNode*>(node);
    auto* original = getAstNode(calc);
    auto* replacement = Ast::traverseAndModify(original, visitor);

    // replace root node if it was modified
    // TraverseAndModify has no access to roots parent
    if (replacement != original) {
      calc->expression()->replaceNode(replacement);
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}

void arangodb::aql::replaceLikeWithRangeRule(
    Optimizer* opt, std::unique_ptr<ExecutionPlan> plan,
    OptimizerRule const& rule) {
  bool modified = false;

  containers::SmallVector<ExecutionNode*, 8> nodes;
  plan->findNodesOfType(nodes, ExecutionNode::CALCULATION, true);

  for (auto node : nodes) {
    auto visitor = [&modified, &plan](AstNode* node) {
      auto* func = getFunction(node);
      if (func != nullptr && func->name == "LIKE") {
        // optimize a LIKE(x, y) into a plain x == y or a range scan in case the
        // search is case-sensitive and the pattern is either a full match or a
        // left-most prefix.
        // this is desirable in 99.999% of all cases, but would be incompatible
        // for search terms that are non-strings. LIKE(1, '1') would behave
        // differently when executed via the AQL LIKE function than would be 1
        // == '1'. for left-most prefix searches (e.g. LIKE(text, 'abc%')) we
        // need to determine the upper bound for the range scan. We use the
        // originally supplied string for the upper bound and append a \uFFFF
        // character to it, which compares higher than other characters.
        bool caseInsensitive = false;  // this is the default behavior of LIKE
        ast::FunctionCallNode likeFcall(node);
        auto args = likeFcall.getArguments().getElements();
        TRI_ASSERT(args.size() >= 2);
        if (args.size() >= 3) {
          caseInsensitive =
              true;  // we have 3 arguments, set case-sensitive to false now
          auto caseArg = args[2];
          if (caseArg->isConstant()) {
            // ok, we can figure out at compile time if the parameter is true or
            // false
            caseInsensitive = caseArg->isTrue();
          }
        }

        auto patternArg = args[1];

        if (!caseInsensitive && patternArg->isStringValue() &&
            args[0]->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
          AstNode const* sub = args[0];
          while (sub != nullptr && sub->type == NODE_TYPE_ATTRIBUTE_ACCESS) {
            ast::AttributeAccessNode attrAccess(sub);
            sub = attrAccess.getObject();
          }
          if (sub == nullptr || sub->type != NODE_TYPE_REFERENCE) {
            return node;
          }
          ast::ReferenceNode ref(sub);
          auto setter = plan->getVarSetBy(ref.getVariable()->id);
          if (setter == nullptr ||
              setter->getType() != EN::ENUMERATE_COLLECTION) {
            // setter could be a view. for views we do not want to change the
            // LIKE function invocation because it might result in a
            // pessimization
            return node;
          }
          auto cn =
              ExecutionNode::castTo<EnumerateCollectionNode const*>(setter);
          auto const& hint = cn->hint();
          if (hint.isDisabled()) {
            // no index should be used. no need for the optimization
            return node;
          }
          if (hint.isSimple()) {
            // we have an index hint
            Collection const* c = cn->collection();

            // check if any of the indexes suggested in the index hint is
            // an inverted index. if so, we disable the optimization
            for (auto const& name : hint.candidateIndexes()) {
              auto idx = c->getCollection()->lookupIndex(name);
              if (idx != nullptr &&
                  idx->type() == Index::TRI_IDX_TYPE_INVERTED_INDEX) {
                // usage of an inverted index -> prevent optimization
                return node;
              }
            }
          }

          // we can go ahead with the optimization

          // optimization only possible for case-sensitive LIKE
          std::string unescapedPattern;
          auto [wildcardFound, wildcardIsLastChar] =
              AqlFunctionsInternalCache::inspectLikePattern(
                  unescapedPattern, patternArg->getStringView());

          if (!wildcardFound) {
            TRI_ASSERT(!wildcardIsLastChar);

            // can turn LIKE into ==
            modified = true;
            Ast* ast = plan->getAst();

            char const* p = ast->resources().registerString(
                unescapedPattern.data(), unescapedPattern.size());
            AstNode* pattern =
                ast->createNodeValueString(p, unescapedPattern.size());

            return ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ,
                                                 args[0], pattern);
          }

          if (!unescapedPattern.empty()) {
            // can turn LIKE into >= && <=
            modified = true;
            Ast* ast = plan->getAst();

            char const* p = ast->resources().registerString(
                unescapedPattern.data(), unescapedPattern.size());
            AstNode* pattern =
                ast->createNodeValueString(p, unescapedPattern.size());
            AstNode* lhs = ast->createNodeBinaryOperator(
                NODE_TYPE_OPERATOR_BINARY_GE, args[0], pattern);

            // add a new end character \uFFFF that is expected to sort "higher"
            // than anything else (note: \xef\xbf\xbf is equivalent to \uFFFF).
            constexpr std::string_view upper = "\xef\xbf\xbf";
            unescapedPattern.append(upper);
            p = ast->resources().registerString(unescapedPattern.data(),
                                                unescapedPattern.size());
            pattern = ast->createNodeValueString(p, unescapedPattern.size());
            AstNode* rhs = ast->createNodeBinaryOperator(
                NODE_TYPE_OPERATOR_BINARY_LT, args[0], pattern);

            AstNode* op = ast->createNodeBinaryOperator(
                NODE_TYPE_OPERATOR_BINARY_AND, lhs, rhs);
            // add >= && <=, but keep LIKE in place to properly handle case
            return ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND,
                                                 op, node);
          }
        }
      }

      return node;
    };

    CalculationNode* calc = ExecutionNode::castTo<CalculationNode*>(node);
    auto* original = getAstNode(calc);
    auto* replacement = Ast::traverseAndModify(original, visitor);

    // replace root node if it was modified
    // TraverseAndModify has no access to roots parent
    if (replacement != original) {
      calc->expression()->replaceNode(replacement);
    }
  }

  opt->addPlan(std::move(plan), rule, modified);
}
