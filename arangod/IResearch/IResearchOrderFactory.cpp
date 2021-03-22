////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

// otherwise define conflict between 3rdParty\date\include\date\date.h and 3rdParty\iresearch\core\shared.hpp
#if defined(_MSC_VER)
#include "date/date.h"
#endif

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionNode.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/SortCondition.h"
#include "Basics/fasthash.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchOrderFactory.h"
#include "IResearch/VelocyPackHelper.h"

#include <search/scorers.hpp>


// ----------------------------------------------------------------------------
// --SECTION--                                        OrderFactory dependencies
// ----------------------------------------------------------------------------

namespace {

arangodb::aql::AstNode const EMPTY_ARGS(arangodb::aql::NODE_TYPE_ARRAY);

// checks a specified args to be deterministic
// and retuns reference to a loop variable
arangodb::aql::Variable const* getScorerRef(arangodb::aql::AstNode const* args) noexcept {
  if (!args || arangodb::aql::NODE_TYPE_ARRAY != args->type) {
    return nullptr;
  }

  size_t const size = args->numMembers();

  if (size < 1) {
    return nullptr;  // invalid args
  }

  // 1st argument has to be reference to `ref`
  auto const* arg0 = args->getMemberUnchecked(0);

  if (!arg0 || arangodb::aql::NODE_TYPE_REFERENCE != arg0->type) {
    return nullptr;
  }

  for (size_t i = 1, size = args->numMembers(); i < size; ++i) {
    auto const* arg = args->getMemberUnchecked(i);

    if (!arg || !arg->isDeterministic()) {
      // we don't support non-deterministic arguments for scorers
      return nullptr;
    }
  }

  return reinterpret_cast<arangodb::aql::Variable const*>(arg0->getData());
}

bool makeScorer(irs::sort::ptr& scorer, irs::string_ref const& name,
                arangodb::aql::AstNode const& args,
                arangodb::iresearch::QueryContext const& ctx) {
  TRI_ASSERT(!args.numMembers() ||
             arangodb::iresearch::findReference(*args.getMember(0), *ctx.ref));

  switch (args.numMembers()) {
    case 0:
      break;
    case 1: {
      // ArangoDB, for API consistency, only supports scorers configurable via jSON
      scorer = irs::scorers::get( // get scorer
        name, irs::type<irs::text_format::json>::get(), irs::string_ref::NIL, false // args
      );

      if (!scorer) {
        // ArangoDB, for API consistency, only supports scorers configurable via jSON
        scorer = irs::scorers::get(name, irs::type<irs::text_format::json>::get(), "[]", false); // pass arg as json array
      }
    } break;
    default: {  // fall through
      arangodb::velocypack::Builder builder;
      arangodb::iresearch::ScopedAqlValue arg;

      builder.openArray();

      for (size_t i = 1, count = args.numMembers(); i < count; ++i) {
        auto const* argNode = args.getMemberUnchecked(i);

        if (!argNode) {
          return false;  // invalid arg
        }

        arg.reset(*argNode);

        if (!arg.execute(ctx)) {
          // failed to execute value
          return false;
        }

        arg.toVelocyPack(builder);
      }

      builder.close();

      // ArangoDB, for API consistency, only supports scorers configurable via jSON
      scorer = irs::scorers::get( // get scorer
        name, irs::type<irs::text_format::json>::get(), builder.toJson(), false // pass arg as json
      );
    }
  }

  return bool(scorer);
}

bool fromFCall(irs::sort::ptr* scorer, irs::string_ref const& scorerName,
               arangodb::aql::AstNode const* args,
               arangodb::iresearch::QueryContext const& ctx) {
  auto const* ref = getScorerRef(args);

  if (ref != ctx.ref) {
    // invalid arguments
    return false;
  }

  if (!scorer) {
    // cheap shallow check
    // ArangoDB, for API consistency, only supports scorers configurable via jSON
    return irs::scorers::exists(scorerName, irs::type<irs::text_format::json>::get(), false);
  }

  // we don't support non-constant arguments for scorers now, if it
  // will change ensure that proper `ExpressionContext` set in `ctx`

  return makeScorer(*scorer, scorerName, *args, ctx);
}

bool nameFromFCall(std::string& scorerName, arangodb::aql::AstNode const& node) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_FCALL == node.type);
  auto* fn = static_cast<arangodb::aql::Function*>(node.getData());

  if (!fn || 1 != node.numMembers() || !arangodb::iresearch::isScorer(*fn)) {
    return false;  // no function
  }

  scorerName = fn->name;

  // convert name to lower case
  std::transform(scorerName.begin(), scorerName.end(), scorerName.begin(), ::tolower);

  return true;
}

bool fromFCall(irs::sort::ptr* scorer, arangodb::aql::AstNode const& node,
               arangodb::iresearch::QueryContext const& ctx) {
  std::string scorerName;

  if (!nameFromFCall(scorerName, node)) {
    return false;
  }

  return fromFCall(scorer, scorerName, node.getMemberUnchecked(0), ctx);
}

bool nameFromFCallUser(irs::string_ref& scorerName, arangodb::aql::AstNode const& node) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_FCALL_USER == node.type);

  if (arangodb::aql::VALUE_TYPE_STRING != node.value.type || 1 != node.numMembers()) {
    return false;  // no function name
  }

  return arangodb::iresearch::parseValue(scorerName, node);
}

bool fromFCallUser(irs::sort::ptr* scorer, arangodb::aql::AstNode const& node,
                   arangodb::iresearch::QueryContext const& ctx) {
  irs::string_ref scorerName;

  if (!nameFromFCallUser(scorerName, node)) {
    return false;
  }

  return fromFCall(scorer, scorerName, node.getMemberUnchecked(0), ctx);
}


arangodb::aql::Variable const* refFromScorer(arangodb::aql::AstNode const& node) {
  if (arangodb::aql::NODE_TYPE_FCALL != node.type && arangodb::aql::NODE_TYPE_FCALL_USER != node.type) {
    return nullptr;
  }

  auto* ref = getScorerRef(node.getMember(0));

  if (!ref) {
    // invalid arguments or reference
    return nullptr;
  }

  arangodb::iresearch::QueryContext const ctx{
    nullptr, nullptr, nullptr,
    nullptr, nullptr, ref
  };

  if (!arangodb::iresearch::OrderFactory::scorer(nullptr, node, ctx)) {
    // not a scorer function
    return nullptr;
  }

  return ref;
}

////////////////////////////////////////////////////////////////////////////////
/// @return true if a specified node contains at least one scorer
////////////////////////////////////////////////////////////////////////////////
bool hasScorer(arangodb::aql::AstNode const& root) {
  auto visitor = [](arangodb::aql::AstNode const& node) {
    return nullptr == refFromScorer(node);
  };

  return !arangodb::iresearch::visit<true>(root, visitor);
}

}  // namespace

namespace arangodb {
namespace iresearch {

// ----------------------------------------------------------------------------
// --SECTION--                                    ScorerReplacer implementation
// ----------------------------------------------------------------------------

void ScorerReplacer::replace(aql::CalculationNode& node) {
  if (!node.expression()) {
    return;
  }

  auto& expr = *node.expression();
  auto* ast = expr.ast();

  if (!expr.ast()) {
    // ast is not set
    return;
  }

  auto* exprNode = expr.nodeForModification();

  if (!exprNode) {
    // node is not set
    return;
  }

  auto replaceScorers = [this, ast](aql::AstNode* node) -> aql::AstNode* {
    TRI_ASSERT(node); // ensured by 'Ast::traverseAndModify(...)'

    auto* ref = refFromScorer(*node);

    if (!ref) {
      // not a scorer
      return node;
    }

    HashedScorer const key(ref, node);

    auto it = _dedup.find(key);

    if (it == _dedup.end()) {
      // create variable
      auto* var = ast->variables()->createTemporaryVariable();

      it = _dedup.try_emplace(key, var).first;
    }

    return ast->createNodeReference(it->second);
  };

  // Try to modify root node of the expression
  auto newNode = replaceScorers(exprNode);

  if (exprNode != newNode) {
    // simple expression, e.g LET x = BM25(d)
    expr.replaceNode(newNode);
  } else if (hasScorer(*exprNode)) {
    auto* exprClone = exprNode->clone(ast);
    aql::Ast::traverseAndModify(exprClone, replaceScorers);
    expr.replaceNode(exprClone);
  }
}

void ScorerReplacer::extract(aql::Variable const& var, std::vector<Scorer>& scorers) {
  for (auto it = _dedup.begin(), end = _dedup.end(); it != end;) {
    if (it->first.var == &var) {
      scorers.emplace_back(it->second, it->first.node);
      it = _dedup.erase(it);
    } else {
      ++it;
    }
  }
}

// ----------------------------------------------------------------------------
// --SECTION--                                      OrderFactory implementation
// ----------------------------------------------------------------------------

/*static*/ bool OrderFactory::scorer(irs::sort::ptr* scorer,
                                     arangodb::aql::AstNode const& node,
                                     arangodb::iresearch::QueryContext const& ctx) {
  switch (node.type) {
    case arangodb::aql::NODE_TYPE_FCALL:  // function call
      return fromFCall(scorer, node, ctx);
    case arangodb::aql::NODE_TYPE_FCALL_USER:  // user function call
      return fromFCallUser(scorer, node, ctx);
    default:
      // IResearch does not support any
      // expressions except function calls
      return false;
  }
}

/*static*/ bool OrderFactory::comparer(irs::sort::ptr* comparer,
                                       arangodb::aql::AstNode const& node) {
  std::string buf;
  irs::string_ref scorerName;

  switch (node.type) {
    case arangodb::aql::NODE_TYPE_FCALL: {  // function call
      if (!nameFromFCall(buf, node)) {
        return false;
      }

      scorerName = buf;
    } break;
    case arangodb::aql::NODE_TYPE_FCALL_USER: {  // user function call
      if (!nameFromFCallUser(scorerName, node)) {
        return false;
      }
    } break;
    default:
      // IResearch does not support any
      // expressions except function calls
      return false;
  }

  if (!comparer) {
    // cheap shallow check
    // ArangoDB, for API consistency, only supports scorers configurable via jSON
    return irs::scorers::exists(scorerName, irs::type<irs::text_format::json>::get(), false);
  }

  // create scorer with default arguments
  // ArangoDB, for API consistency, only supports scorers configurable via jSON
  *comparer = irs::scorers::get( // get scorer
    scorerName, irs::type<irs::text_format::json>::get(), irs::string_ref::NIL, false // args
  );

  return bool(*comparer);
}

}  // namespace iresearch
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION-- END-OF-FILE
// -----------------------------------------------------------------------------
