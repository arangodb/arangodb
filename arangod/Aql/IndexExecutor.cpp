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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "IndexExecutor.h"

#include "Aql/AqlValue.h"
#include "Aql/Collection.h"
#include "Aql/DocumentProducingHelper.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/Query.h"
#include "Aql/SingleRowFetcher.h"
#include "Basics/Common.h"
#include "Cluster/ServerState.h"
#include "ExecutorExpressionContext.h"
#include "Transaction/Methods.h"
#include "Utils/OperationCursor.h"
#include "V8/v8-globals.h"

#include <lib/Logger/LogMacros.h>

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

namespace {
/// resolve constant attribute accesses
static void resolveFCallConstAttributes(AstNode* fcall) {
  TRI_ASSERT(fcall->type == NODE_TYPE_FCALL);
  TRI_ASSERT(fcall->numMembers() == 1);
  AstNode* array = fcall->getMemberUnchecked(0);
  for (size_t x = 0; x < array->numMembers(); x++) {
    AstNode* child = array->getMemberUnchecked(x);
    if (child->type == NODE_TYPE_ATTRIBUTE_ACCESS && child->isConstant()) {
      child = const_cast<AstNode*>(Ast::resolveConstAttributeAccess(child));
      array->changeMember(x, child);
    }
  }
}
}  // namespace

IndexExecutorInfos::IndexExecutorInfos(
    RegisterId outputRegister, RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
    std::unordered_set<RegisterId> registersToClear, ExecutionEngine* engine,
    Collection const* collection, Variable const* outVariable, bool produceResult,
    std::vector<std::string> const& projections, transaction::Methods* trxPtr,
    std::vector<size_t> const& coveringIndexAttributePositions,
    bool allowCoveringIndexOptimization, bool useRawDocumentPointers,
    std::vector<std::unique_ptr<NonConstExpression>>&& nonConstExpression,
    std::vector<Variable const*>&& expInVars, std::vector<RegisterId>&& expInRegs,
    bool hasV8Expression, AstNode const* condition,
    std::vector<transaction::Methods::IndexHandle> indexes, Ast* ast)
    : ExecutorInfos(make_shared_unordered_set(),
                    make_shared_unordered_set({outputRegister}), nrInputRegisters,
                    nrOutputRegisters, std::move(registersToClear)),
      _indexes(indexes),
      _condition(condition),
      _ast(ast),
      _hasMultipleExpansions(false),
      _isLastIndex(false),
      _outputRegisterId(outputRegister),
      _engine(engine),
      _collection(collection),
      _expInVars(std::move(expInVars)),
      _expInRegs(std::move(expInRegs)),
      _outVariable(outVariable),
      _projections(projections),
      _trxPtr(trxPtr),
      _coveringIndexAttributePositions(coveringIndexAttributePositions),
      _allowCoveringIndexOptimization(allowCoveringIndexOptimization),
      _useRawDocumentPointers(useRawDocumentPointers),
      _nonConstExpression(nonConstExpression),
      _produceResult(produceResult),
      _hasV8Expression(hasV8Expression) {}

IndexExecutor::IndexExecutor(Fetcher& fetcher, Infos& infos)
    : _infos(infos),
      _fetcher(fetcher),
      _input(ExecutionState::HASMORE, InputAqlItemRow{CreateInvalidInputRowHint{}}) {
  TRI_ASSERT(!_infos.getIndexes().empty());

  if (_infos.getCondition() != nullptr) {
    // fix const attribute accesses, e.g. { "a": 1 }.a
    for (size_t i = 0; i < _infos.getCondition()->numMembers(); ++i) {
      auto andCond = _infos.getCondition()->getMemberUnchecked(i);
      for (size_t j = 0; j < andCond->numMembers(); ++j) {
        auto leaf = andCond->getMemberUnchecked(j);

        // geo index condition i.e. GEO_CONTAINS, GEO_INTERSECTS
        if (leaf->type == NODE_TYPE_FCALL) {
          ::resolveFCallConstAttributes(leaf);
          continue;  //
        } else if (leaf->numMembers() != 2) {
          continue;  // Otherwise we only support binary conditions
        }

        TRI_ASSERT(leaf->numMembers() == 2);
        AstNode* lhs = leaf->getMemberUnchecked(0);
        AstNode* rhs = leaf->getMemberUnchecked(1);
        if (lhs->type == NODE_TYPE_ATTRIBUTE_ACCESS && lhs->isConstant()) {
          lhs = const_cast<AstNode*>(Ast::resolveConstAttributeAccess(lhs));
          leaf->changeMember(0, lhs);
        }
        if (rhs->type == NODE_TYPE_ATTRIBUTE_ACCESS && rhs->isConstant()) {
          rhs = const_cast<AstNode*>(Ast::resolveConstAttributeAccess(rhs));
          leaf->changeMember(1, rhs);
        }
        // geo index condition i.e. `GEO_DISTANCE(x, y) <= d`
        if (lhs->type == NODE_TYPE_FCALL) {
          ::resolveFCallConstAttributes(lhs);
        }
      }
    }
  }

  // count how many attributes in the index are expanded (array index)
  // if more than a single attribute, we always need to deduplicate the
  // result later on
  for (auto const& it : _infos.getIndexes()) {
    size_t expansions = 0;
    auto idx = it.getIndex();
    auto const& fields = idx->fields();
    for (size_t i = 0; i < fields.size(); ++i) {
      if (idx->isAttributeExpanded(i)) {
        ++expansions;
        if (expansions > 1 || i > 0) {
          infos.setHasMultipleExpansions(true);
          ;
          break;
        }
      }
    }
  }

  this->setProducingFunction(
      buildCallback(_documentProducer, _infos.getOutVariable(),
                    _infos.getProduceResult(), _infos.getProjections(),
                    _infos.getTrxPtr(), _infos.getCoveringIndexAttributePositions(),
                    _infos.getAllowCoveringIndexOptimization(),
                    _infos.getUseRawDocumentPointers()));
};

IndexExecutor::~IndexExecutor() = default;

void IndexExecutor::executeExpressions(InputAqlItemRow& input, OutputAqlItemRow& output) {
  TRI_ASSERT(_infos.getCondition() != nullptr);
  TRI_ASSERT(!_infos.getNonConstExpressions().empty());

  // The following are needed to evaluate expressions with local data from
  // the current incoming item:

  auto ast = _infos.getAst();
  AstNode* condition = const_cast<AstNode*>(_infos.getCondition());

  // modify the existing node in place
  TEMPORARILY_UNLOCK_NODE(condition);

  Query* query = _infos.getEngine()->getQuery();

  for (size_t posInExpressions = 0; posInExpressions < _infos.getNonConstExpressions().size(); ++posInExpressions) {
    NonConstExpression* toReplace = _infos.getNonConstExpressions()[posInExpressions].get();
    auto exp = toReplace->expression.get();

    bool mustDestroy;
    ExecutorExpressionContext ctx(query, input, _infos.getExpInVars(), _infos.getExpInRegs());
    AqlValue a = exp->execute(_infos.getTrxPtr(), &ctx, mustDestroy);
    AqlValueGuard guard(a, mustDestroy);

    AqlValueMaterializer materializer(_infos.getTrxPtr());
    VPackSlice slice = materializer.slice(a, false);
    AstNode* evaluatedNode = ast->nodeFromVPack(slice, true);

    AstNode* tmp = condition;
    for (size_t x = 0; x < toReplace->indexPath.size(); x++) {
      size_t idx = toReplace->indexPath[x];
      AstNode* old = tmp->getMember(idx);
      // modify the node in place
      TEMPORARILY_UNLOCK_NODE(tmp);
      if (x + 1 < toReplace->indexPath.size()) {
        AstNode* cpy = old;
        tmp->changeMember(idx, cpy);
        tmp = cpy;
      } else {
        // insert the actual expression value
        tmp->changeMember(idx, evaluatedNode);
      }
    }
  }
}

std::pair<ExecutionState, IndexStats> IndexExecutor::produceRow(OutputAqlItemRow& output) {
  TRI_IF_FAILURE("IndexExecutor::produceRow") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  ExecutionState state;
  IndexStats stats{};
  InputAqlItemRow input{CreateInvalidInputRowHint{}};

  while (true) {
    std::tie(state, input) = _input;

    if (state == ExecutionState::WAITING) {
      return {state, stats};
    }

    if (!input) {
      TRI_ASSERT(state == ExecutionState::DONE);
      return {state, stats};
    }
    TRI_ASSERT(input.isInitialized());

    // LOGIC HERE
    IndexIterator::DocumentCallback callback;
    size_t const nrInRegs = _infos.numberOfInputRegisters();

    if (_infos.getIndexes().size() > 1 || _infos.hasMultipleExpansions()) {
      // Activate uniqueness checks
      callback = [this, &input, &output](LocalDocumentId const& token, VPackSlice slice) {
          if (!_infos.isLastIndex()) {
            // insert & check for duplicates in one go
            if (!_alreadyReturned.insert(token.id()).second) {
              // Document already in list. Skip this
              return;
            }
          } else {
            // only check for duplicates
            if (_alreadyReturned.find(token.id()) != _alreadyReturned.end()) {
              // Document found, skip
              return;
            }
          }

          _documentProducer(input, output, slice, _infos.getOutputRegisterId());
      };
    } else {
      // No uniqueness checks
      callback = [this, &input, &output](LocalDocumentId const&, VPackSlice slice) {
          _documentProducer(input, output, slice, _infos.getOutputRegisterId());
      };
    }


    return {ExecutionState::HASMORE, stats};

    TRI_ASSERT(state == ExecutionState::HASMORE);
  }
}
