////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
  #undef NOEXCEPT
#endif

#include "search/scorers.hpp"

#include "AqlHelper.h"
#include "IResearchFeature.h"
#include "IResearchOrderFactory.h"
#include "VelocyPackHelper.h"
#include "Aql/AstNode.h"
#include "Aql/Function.h"
#include "Aql/SortCondition.h"

// ----------------------------------------------------------------------------
// --SECTION--                                        OrderFactory dependencies
// ----------------------------------------------------------------------------

NS_LOCAL

arangodb::aql::AstNode const EMPTY_ARGS(arangodb::aql::NODE_TYPE_ARRAY);

bool validateFuncArgs(
    arangodb::aql::AstNode const* args,
    arangodb::aql::Variable const& ref
) {
  if (!args || arangodb::aql::NODE_TYPE_ARRAY != args->type) {
    return false;
  }

  size_t const size = args->numMembers();

  if (size < 1) {
    return false; // invalid args
  }

  // 1st argument has to be reference to `ref`
  auto const* arg0 = args->getMemberUnchecked(0);

  if (!arg0
      || arangodb::aql::NODE_TYPE_REFERENCE != arg0->type
      || reinterpret_cast<void const*>(&ref) != arg0->getData()) {
    return false;
  }

  for (size_t i = 1, size = args->numMembers(); i < size; ++i) {
    auto const* arg = args->getMemberUnchecked(i);

    if (!arg || !arg->isDeterministic()) {
      // we don't support non-deterministic arguments for scorers
      return false;
    }
  }

  return true;
}

bool makeScorer(
    irs::sort::ptr& scorer,
    irs::string_ref const& name,
    arangodb::aql::AstNode const& args,
    arangodb::iresearch::QueryContext const& ctx
) {
  TRI_ASSERT(!args.numMembers() || arangodb::iresearch::findReference(*args.getMember(0), *ctx.ref));

  switch (args.numMembers()) {
    case 0:
      break;
    case 1: {
      // ArangoDB, for API consistency, only supports scorers configurable via jSON
      scorer = irs::scorers::get(name, irs::text_format::json, irs::string_ref::NIL);

      if (!scorer) {
        // ArangoDB, for API consistency, only supports scorers configurable via jSON
        scorer = irs::scorers::get(name, irs::text_format::json, "[]"); // pass arg as json array
      }
    } break;
    default: { // fall through
      arangodb::velocypack::Builder builder;
      arangodb::iresearch::ScopedAqlValue arg;

      builder.openArray();

      for (size_t i = 1, count = args.numMembers(); i < count; ++i) {
        auto const* argNode = args.getMemberUnchecked(i);

        if (!argNode) {
          return false; // invalid arg
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
      scorer = irs::scorers::get(name, irs::text_format::json, builder.toJson()); // pass arg as json
    }
  }

  return bool(scorer);
}

bool fromFCall(
    irs::sort::ptr* scorer,
    irs::string_ref const& scorerName,
    arangodb::aql::AstNode const* args,
    arangodb::iresearch::QueryContext const& ctx
) {
  if (!validateFuncArgs(args, *ctx.ref)) {
    // invalid arguments
    return false;
  }

  if (!scorer) {
    // cheap shallow check
    // ArangoDB, for API consistency, only supports scorers configurable via jSON
    return irs::scorers::exists(scorerName, irs::text_format::json);
  }

  // we don't support non-constant arguments for scorers now, if it
  // will change ensure that proper `ExpressionContext` set in `ctx`

  return makeScorer(*scorer, scorerName, *args, ctx);
}

bool nameFromFCall(
    std::string& scorerName,
    arangodb::aql::AstNode const& node
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_FCALL == node.type);
  auto* fn = static_cast<arangodb::aql::Function*>(node.getData());

  if (!fn
      || 1 != node.numMembers()
      || !arangodb::iresearch::isScorer(*fn)) {
    return false; // no function
  }

  scorerName = fn->name;

  // convert name to lower case
  std::transform(
    scorerName.begin(), scorerName.end(), scorerName.begin(), ::tolower
  );

  return true;
}

bool fromFCall(
    irs::sort::ptr* scorer,
    arangodb::aql::AstNode const& node,
    arangodb::iresearch::QueryContext const& ctx
) {
  std::string scorerName;

  if (!nameFromFCall(scorerName, node)) {
    return false;
  }

  return fromFCall(
    scorer,
    scorerName,
    node.getMemberUnchecked(0),
    ctx
  );
}

bool nameFromFCallUser(
    irs::string_ref& scorerName,
    arangodb::aql::AstNode const& node) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_FCALL_USER == node.type);

  if (arangodb::aql::VALUE_TYPE_STRING != node.value.type
      || 1 != node.numMembers()) {
    return false; // no function name
  }

  return arangodb::iresearch::parseValue(scorerName, node);
}

bool fromFCallUser(
    irs::sort::ptr* scorer,
    arangodb::aql::AstNode const& node,
    arangodb::iresearch::QueryContext const& ctx
) {
  irs::string_ref scorerName;

  if (!nameFromFCallUser(scorerName, node)) {
    return false;
  }

  return fromFCall(
    scorer,
    scorerName,
    node.getMemberUnchecked(0),
    ctx
  );
}

NS_END

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

// ----------------------------------------------------------------------------
// --SECTION--                                      OrderFactory implementation
// ----------------------------------------------------------------------------

/*static*/ bool OrderFactory::scorer(
    irs::sort::ptr* scorer,
    arangodb::aql::AstNode const& node,
    arangodb::iresearch::QueryContext const& ctx
) {
  switch (node.type) {
    case arangodb::aql::NODE_TYPE_FCALL: // function call
      return fromFCall(scorer, node, ctx);
    case arangodb::aql::NODE_TYPE_FCALL_USER: // user function call
      return fromFCallUser(scorer, node, ctx);
    default:
      // IResearch does not support any
      // expressions except function calls
      return false;
  }
}

/*static*/ bool OrderFactory::comparer(
    irs::sort::ptr* comparer,
    arangodb::aql::AstNode const& node
) {
  std::string buf;
  irs::string_ref scorerName;

  switch (node.type) {
    case arangodb::aql::NODE_TYPE_FCALL: { // function call
      if (!nameFromFCall(buf, node)) {
        return false;
      }

      scorerName = buf;
    } break;
    case arangodb::aql::NODE_TYPE_FCALL_USER: { // user function call
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
    return irs::scorers::exists(scorerName, irs::text_format::json);
  }

  // create scorer with default arguments
  // ArangoDB, for API consistency, only supports scorers configurable via jSON
  *comparer = irs::scorers::get(scorerName, irs::text_format::json, irs::string_ref::NIL);

  return bool(*comparer);
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
