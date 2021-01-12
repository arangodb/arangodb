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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "AttributeAccessor.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/AttributeNamePath.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Variable.h"
#include "Basics/StaticStrings.h"
#include "Utils/CollectionNameResolver.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace {

class AttributeAccessorKey final : public AttributeAccessor {
 public:
  explicit AttributeAccessorKey(Variable const* variable)
      : AttributeAccessor(variable) {}

  AqlValue get(CollectionNameResolver const&, ExpressionContext const* context, bool& mustDestroy) override {
    AqlValue const& value = context->getVariableValue(_variable, false, mustDestroy);
    return value.getKeyAttribute(mustDestroy, true);
  }
};

class AttributeAccessorId final : public AttributeAccessor {
 public:
  explicit AttributeAccessorId(Variable const* variable)
      : AttributeAccessor(variable) {}

  AqlValue get(CollectionNameResolver const& resolver, ExpressionContext const* context, bool& mustDestroy) override {
    AqlValue const& value = context->getVariableValue(_variable, false, mustDestroy);
    return value.getIdAttribute(resolver, mustDestroy, true);
  }
};

class AttributeAccessorFrom final : public AttributeAccessor {
 public:
  explicit AttributeAccessorFrom(Variable const* variable)
      : AttributeAccessor(variable) {}

  AqlValue get(CollectionNameResolver const&, ExpressionContext const* context, bool& mustDestroy) override {
    AqlValue const& value = context->getVariableValue(_variable, false, mustDestroy);
    return value.getFromAttribute(mustDestroy, true);
  }
};

class AttributeAccessorTo final : public AttributeAccessor {
 public:
  explicit AttributeAccessorTo(Variable const* variable)
      : AttributeAccessor(variable) {}

  AqlValue get(CollectionNameResolver const&, ExpressionContext const* context, bool& mustDestroy) override {
    AqlValue const& value = context->getVariableValue(_variable, false, mustDestroy);
    return value.getToAttribute(mustDestroy, true);
  }
};

class AttributeAccessorSingle final : public AttributeAccessor {
 public:
  explicit AttributeAccessorSingle(Variable const* variable, std::string const& attributeName)
      : AttributeAccessor(variable), _attributeName(attributeName) {}

  AqlValue get(CollectionNameResolver const& resolver, ExpressionContext const* context, bool& mustDestroy) override {
    AqlValue const& value = context->getVariableValue(_variable, false, mustDestroy);
    // use optimized version for single attribute (e.g. variable.attr)
    return value.get(resolver, _attributeName, mustDestroy, true);
  }

 private:
  std::string const _attributeName;
};

class AttributeAccessorMulti final : public AttributeAccessor {
 public:
  explicit AttributeAccessorMulti(Variable const* variable, arangodb::aql::AttributeNamePath&& path)
      : AttributeAccessor(variable), _path(std::move(path)) {}

  AqlValue get(CollectionNameResolver const& resolver, ExpressionContext const* context, bool& mustDestroy) override {
    AqlValue const& value = context->getVariableValue(_variable, false, mustDestroy);
    // use general version for multiple attributes (e.g. variable.attr.subattr)
    return value.get(resolver, _path.get(), mustDestroy, true);
  }

 private:
  arangodb::aql::AttributeNamePath const _path;
};

}  // namespace

/// @brief create the accessor
AttributeAccessor::AttributeAccessor(Variable const* variable)
    : _variable(variable) {}

/// @brief replace the variable in the accessor
void AttributeAccessor::replaceVariable(std::unordered_map<VariableId, Variable const*> const& replacements) {
  for (auto const& it : replacements) {
    if (it.first == _variable->id) {
      _variable = it.second;
      return;
    }
  }
}

AttributeAccessor* AttributeAccessor::create(arangodb::aql::AttributeNamePath&& path,
                                             Variable const* variable,
                                             bool dataIsFromCollection) {
  TRI_ASSERT(variable != nullptr);
  TRI_ASSERT(!path.empty());

  // determine accessor type
  // it is only safe to use the optimized accessor functions for system
  // attributes when the input data are collection documents. it is not safe to
  // use them for non-collection data, as the optimized functions may easily
  // create out-of-bounds accesses in that case
  AttributeNamePath::Type type = path.type();
  if (!dataIsFromCollection &&
      path.size() == 1) {
    type = AttributeNamePath::Type::SingleAttribute;
  }

  switch (type) {
    case AttributeNamePath::Type::IdAttribute:
      return new AttributeAccessorId(variable);
    case AttributeNamePath::Type::KeyAttribute:
      return new AttributeAccessorKey(variable);
    case AttributeNamePath::Type::FromAttribute:
      return new AttributeAccessorFrom(variable);
    case AttributeNamePath::Type::ToAttribute:
      return new AttributeAccessorTo(variable);
    case AttributeNamePath::Type::SingleAttribute:
      return new AttributeAccessorSingle(variable, path[0]);
    case AttributeNamePath::Type::MultiAttribute:
      return new AttributeAccessorMulti(variable, std::move(path));
  }

  ADB_UNREACHABLE;
}
