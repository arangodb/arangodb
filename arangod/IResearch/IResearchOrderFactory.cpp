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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchOrderFactory.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/IResearchViewNode.h"
#include "Aql/Expression.h"
#include "Aql/Function.h"
#include "Aql/SortCondition.h"
#include "Aql/types.h"
#include "Basics/Exceptions.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchFilterContext.h"
#include "IResearch/VelocyPackHelper.h"

#include <search/scorers.hpp>

namespace {

using namespace arangodb;

bool makeScorer(irs::Scorer::ptr& scorer, std::string_view name,
                aql::AstNode const& args,
                arangodb::iresearch::QueryContext const& ctx) {
  TRI_ASSERT(!args.numMembers() || !ctx.ref ||
             arangodb::iresearch::findReference(*args.getMember(0), *ctx.ref));

  switch (args.numMembers()) {
    case 0:
      break;
    case 1: {
      // ArangoDB, for API consistency, only supports scorers configurable via
      // jSON
      scorer = irs::scorers::get(name, irs::type<irs::text_format::json>::get(),
                                 std::string_view{}, false);

      if (!scorer) {
        // ArangoDB, for API consistency, only supports scorers configurable via
        // jSON
        scorer = irs::scorers::get(
            name, irs::type<irs::text_format::json>::get(), "[]", false);
      }
    } break;
    default: {
      velocypack::Builder builder;
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

      // ArangoDB, for API consistency, only supports scorers configurable via
      // jSON
      scorer = irs::scorers::get(name, irs::type<irs::text_format::json>::get(),
                                 builder.toJson(), false);
    }
  }

  return bool(scorer);
}

bool fromFCall(irs::Scorer::ptr* scorer, std::string_view scorerName,
               aql::AstNode const* args,
               arangodb::iresearch::QueryContext const& ctx) {
  auto const* ref = arangodb::iresearch::getSearchFuncRef(args);

  if (ref != ctx.ref) {
    // invalid arguments
    return false;
  }

  if (!scorer) {
    // cheap shallow check
    // ArangoDB, for API consistency, only supports scorers configurable via
    // jSON
    return irs::scorers::exists(
        scorerName, irs::type<irs::text_format::json>::get(), false);
  }

  // we don't support non-constant arguments for scorers now, if it
  // will change ensure that proper `ExpressionContext` set in `ctx`

  return makeScorer(*scorer, scorerName, *args, ctx);
}

bool nameFromFCall(std::string& scorerName, aql::AstNode const& node) {
  TRI_ASSERT(aql::NODE_TYPE_FCALL == node.type);
  auto* fn = static_cast<aql::Function*>(node.getData());

  if (!fn || 1 != node.numMembers() || !arangodb::iresearch::isScorer(*fn)) {
    return false;  // no function
  }

  scorerName = fn->name;

  // convert name to lower case
  std::transform(scorerName.begin(), scorerName.end(), scorerName.begin(),
                 ::tolower);

  return true;
}

bool fromFCall(irs::Scorer::ptr* scorer, aql::AstNode const& node,
               arangodb::iresearch::QueryContext const& ctx) {
  std::string scorerName;

  if (!nameFromFCall(scorerName, node)) {
    return false;
  }

  return fromFCall(scorer, scorerName, node.getMemberUnchecked(0), ctx);
}

bool nameFromFCallUser(std::string_view& scorerName, aql::AstNode const& node) {
  TRI_ASSERT(aql::NODE_TYPE_FCALL_USER == node.type);

  if (aql::VALUE_TYPE_STRING != node.value.type || 1 != node.numMembers()) {
    return false;  // no function name
  }

  return arangodb::iresearch::parseValue(scorerName, node);
}

bool fromFCallUser(irs::Scorer::ptr* scorer, aql::AstNode const& node,
                   arangodb::iresearch::QueryContext const& ctx) {
  std::string_view scorerName;

  if (!nameFromFCallUser(scorerName, node)) {
    return false;
  }

  return fromFCall(scorer, scorerName, node.getMemberUnchecked(0), ctx);
}

}  // namespace

namespace arangodb::iresearch::order_factory {

aql::Variable const* refFromScorer(aql::AstNode const& node) {
  if (aql::NODE_TYPE_FCALL != node.type &&
      aql::NODE_TYPE_FCALL_USER != node.type) {
    return nullptr;
  }

  auto* ref = getSearchFuncRef(node.getMember(0));

  if (!ref) {
    // invalid arguments or reference
    return nullptr;
  }

  QueryContext const ctx{.ref = ref};

  if (!order_factory::scorer(nullptr, node, ctx)) {
    // not a scorer function
    return nullptr;
  }

  return ref;
}

bool scorer(irs::Scorer::ptr* scorer, aql::AstNode const& node,
            QueryContext const& ctx) {
  switch (node.type) {
    case aql::NODE_TYPE_FCALL:  // function call
      return fromFCall(scorer, node, ctx);
    case aql::NODE_TYPE_FCALL_USER:  // user function call
      return fromFCallUser(scorer, node, ctx);
    default:
      // IResearch does not support any
      // expressions except function calls
      return false;
  }
}

bool comparer(irs::Scorer::ptr* comparer, aql::AstNode const& node) {
  std::string buf;
  std::string_view scorerName;

  switch (node.type) {
    case aql::NODE_TYPE_FCALL: {  // function call
      if (!nameFromFCall(buf, node)) {
        return false;
      }

      scorerName = buf;
    } break;
    case aql::NODE_TYPE_FCALL_USER: {  // user function call
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
    // ArangoDB, for API consistency, only supports scorers configurable via
    // jSON
    return irs::scorers::exists(
        scorerName, irs::type<irs::text_format::json>::get(), false);
  }

  // create scorer with default arguments
  // ArangoDB, for API consistency, only supports scorers configurable via jSON
  *comparer = irs::scorers::get(  // get scorer
      scorerName, irs::type<irs::text_format::json>::get(), std::string_view{},
      false  // args
  );

  return bool(*comparer);
}

}  // namespace arangodb::iresearch::order_factory
