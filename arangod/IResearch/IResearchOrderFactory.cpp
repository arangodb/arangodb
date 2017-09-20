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

#include "IResearchOrderFactory.h"

#include "AstHelper.h"
#include "AttributeScorer.h"
#include "IResearchAttributes.h"
#include "VelocyPackHelper.h"

#include "Aql/AstNode.h"
#include "Aql/Function.h"
#include "Aql/SortCondition.h"

// ----------------------------------------------------------------------------
// --SECTION--                                        OrderFactory dependencies
// ----------------------------------------------------------------------------

NS_LOCAL

class Sort: public irs::sort {
 public:
  Sort(
      irs::sort::ptr&& impl,
      arangodb::iresearch::attribute::AttributePath& attrPath
  ): irs::sort(impl->type()),
     _attrPath(attrPath),
     _impl(std::move(impl)) {
    TRI_ASSERT(_impl);
    irs::sort::reverse(_impl->reverse()); // ensure full equivalence for tests
  }

  virtual ~Sort() { }

  static irs::sort::ptr make(
      irs::sort::ptr&& impl,
      arangodb::iresearch::attribute::AttributePath& attrPath
  ) {
    PTR_NAMED(Sort, ptr, std::move(impl), attrPath);
    return ptr;
  }

  virtual prepared::ptr prepare() const override {
    auto ptr = _impl->prepare();

    if (!ptr) {
      return nullptr;
    }

    auto* attr = ptr->attributes().get<arangodb::iresearch::attribute::AttributePath>();

    if (attr) {
      *attr = &_attrPath; // set attribute path if requested
    }

    return ptr;
  }

  void reverse(bool rev) { TRI_ASSERT(false); } // must initialize impl before construction wrapper

 private:
  arangodb::iresearch::attribute::AttributePath& _attrPath;
  irs::sort::ptr _impl;
};

bool appendAttributePath(
    arangodb::velocypack::Builder* builder,
    arangodb::aql::AstNode const*& head,
    arangodb::aql::AstNode const* node
) {
  if (!node) {
    return false;
  }

  if (!builder) {
    struct {
      bool operator()() { return false; } // do not support [*]
      bool operator()(int64_t value) { return value >= 0; }
      bool operator()(irs::string_ref const& value) { return !value.null(); }
    } visitor;

    return arangodb::iresearch::visitAttributePath(head, *node, visitor);
  }

  struct VisitorType{
    arangodb::velocypack::Builder& builder;
    VisitorType(arangodb::velocypack::Builder& vBuilder): builder(vBuilder) {
      builder.openArray();
    }
    ~VisitorType() { builder.close(); }
    bool operator()() { return false; } // do not support [*]
    bool operator()(int64_t value) {
      builder.add(arangodb::velocypack::Value(value));
      return value >= 0;
    }
    bool operator()(irs::string_ref const& value) {
      builder.add(arangodb::iresearch::toValuePair(value));
      return !value.null();
    }
  } visitor(*builder);

  return arangodb::iresearch::visitAttributePath(head, *node, visitor);
}

bool validateOutputVariable(arangodb::aql::AstNode const* node) {
  // referencing chain node structure:
  // NODE_TYPE_ATTRIBUTE_ACCESS->NODE_TYPE_ATTRIBUTE_ACCESS->NODE_TYPE_REFERENCE
  // for SORT should only expect that the the node is a reference to a variable
  return node
    && arangodb::aql::NODE_TYPE_REFERENCE == node->type;
}

bool fromFCall(
    arangodb::iresearch::OrderFactory::OrderContext* ctx,
    irs::string_ref const& name,
    arangodb::aql::AstNode const& args,
    bool reverse,
    arangodb::iresearch::IResearchViewMeta const& meta
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_ARRAY == args.type);
  arangodb::velocypack::Builder* attrPath = nullptr;
  arangodb::iresearch::attribute::AttributePath* attrPtr = nullptr;
  arangodb::aql::AstNode const* head;
  irs::sort::ptr scorer;

  if (ctx) {
    auto attr = arangodb::iresearch::attribute::AttributePath::make();

    #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      attrPtr = dynamic_cast<arangodb::iresearch::attribute::AttributePath*>(attr.get());
    #else
      attrPtr = static_cast<arangodb::iresearch::attribute::AttributePath*>(attr.get());
    #endif

    if (!attrPtr) {
      return false; // failure to create attribute path coiner attribute
    }

    attrPath = &(attrPtr->value);
    ctx->attributes.emplace_back(std::move(attr));
  }

  if (!args.numMembers()
      || !appendAttributePath(attrPath, head, args.getMemberUnchecked(0))
      || !(validateOutputVariable(head)
           || (arangodb::aql::NODE_TYPE_VALUE == head->type // allow pure string attribute path
               && args.getMemberUnchecked(0) == head)
          )
     ) {
    return false; // expected to see an attribute path as the first argument
  }

  switch (args.numMembers()) {
   case 1:
    scorer = irs::scorers::get(name, irs::string_ref::nil);

    if (!scorer) {
      scorer = irs::scorers::get(name, "{}"); // pass arg as json
    }

    break;
   case 2: {
    auto* arg = args.getMemberUnchecked(1);

    if (arg
        && arangodb::aql::NODE_TYPE_VALUE == arg->type
        && arangodb::aql::VALUE_TYPE_STRING == arg->value.type) {
      irs::string_ref value(arg->getStringValue(), arg->getStringLength());

      scorer = irs::scorers::get(name, value);

      if (scorer) {
        break;
      }
    }
   }
   // fall through
   default: {
    arangodb::velocypack::Builder builder;

    builder.openArray();

    for (size_t i = 1, count = args.numMembers(); i < count; ++i) {
      auto* arg = args.getMemberUnchecked(i);

      if (!arg) {
        return false; // invalid arg
      }

      arg->toVelocyPackValue(builder);
    }

    builder.close();
    scorer = irs::scorers::get(name, builder.toJson()); // pass arg as json
   }
  }

  if (!scorer) {
    return false; // failed find scorer
  }

  if (ctx) {
    scorer->reverse(reverse);
    ctx->order.add<Sort>(std::move(scorer), *attrPtr);
  }

  return true;
}

bool fromFCall(
    arangodb::iresearch::OrderFactory::OrderContext* ctx,
    arangodb::aql::AstNode const& node,
    bool reverse,
    arangodb::iresearch::IResearchViewMeta const& meta
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_FCALL == node.type);
  auto* fn = static_cast<arangodb::aql::Function*>(node.getData());

  if (!fn || 1 != node.numMembers()) {
    return false; // no function
  }

  auto* args = node.getMemberUnchecked(0);

  if (!args || arangodb::aql::NODE_TYPE_ARRAY != args->type) {
    return false; // invalid args
  }

  auto& name = fn->name;
  std::string scorerName(name);

  // convert name to lower case
  std::transform(scorerName.begin(), scorerName.end(), scorerName.begin(), ::tolower);

  return fromFCall(ctx, scorerName, *args, reverse, meta);
}

bool fromFCallUser(
    arangodb::iresearch::OrderFactory::OrderContext* ctx,
    arangodb::aql::AstNode const& node,
    bool reverse,
    arangodb::iresearch::IResearchViewMeta const& meta
) {
  TRI_ASSERT(arangodb::aql::NODE_TYPE_FCALL_USER == node.type);

  if (arangodb::aql::VALUE_TYPE_STRING != node.value.type
      || 1 != node.numMembers()) {
    return false; // no function name
  }

  irs::string_ref name(node.getStringValue(), node.getStringLength());
  auto* args = node.getMemberUnchecked(0);

  if (!args || arangodb::aql::NODE_TYPE_ARRAY != args->type) {
    return false; // invalid args
  }

  return fromFCall(ctx, name, *args, reverse, meta);
}

bool fromValue(
    arangodb::iresearch::OrderFactory::OrderContext* ctx,
    arangodb::aql::AstNode const& node,
    bool reverse,
    arangodb::iresearch::IResearchViewMeta const& meta
) {
  TRI_ASSERT(
    arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS == node.type
    || arangodb::aql::NODE_TYPE_INDEXED_ACCESS == node.type
    || arangodb::aql::NODE_TYPE_VALUE == node.type
  );

  arangodb::aql::AstNode args(arangodb::aql::NODE_TYPE_ARRAY);

  args.addMember(&node); // simulate argument array AST structure of an FCall

  return fromFCall(
    ctx,
    arangodb::iresearch::AttributeScorer::type().name(),
    args,
    reverse,
    meta
  );
}

NS_END

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

// ----------------------------------------------------------------------------
// --SECTION--                                      OrderFactory implementation
// ----------------------------------------------------------------------------

/*static*/ bool OrderFactory::order(
  arangodb::iresearch::OrderFactory::OrderContext* ctx,
  arangodb::aql::SortCondition const& node,
  IResearchViewMeta const& meta
) {
  for (size_t i = 0, count = node.numAttributes(); i < count; ++i) {
    auto field = node.field(i);
    auto* variable = std::get<0>(field);
    auto* expression = std::get<1>(field);
    auto ascending = std::get<2>(field);
    UNUSED(variable);

    if (!expression) {
      return false;
    }

    bool result;

    switch (expression->type) {
      case arangodb::aql::NODE_TYPE_FCALL: // function call
        result = fromFCall(ctx, *expression, !ascending, meta);
        break;
      case arangodb::aql::NODE_TYPE_FCALL_USER: // user function call
        result = fromFCallUser(ctx, *expression, !ascending, meta);
        break;
      case arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS: // fall through
      case arangodb::aql::NODE_TYPE_INDEXED_ACCESS: // fall through
      case arangodb::aql::NODE_TYPE_VALUE:
        result = fromValue(ctx, *expression, !ascending, meta);
        break;
      default:
        result = false;
    }

    if (!result) {
      return false;
    }
  }

  return true;
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
