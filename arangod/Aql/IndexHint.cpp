////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "IndexHint.h"

#include "Aql/AstNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/QueryContext.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <ostream>

namespace {
std::string const TypeDisabled("disabled");
std::string const TypeIllegal("illegal");
std::string const TypeNone("none");
std::string const TypeSimple("simple");

std::string const FieldContainer("indexHint");
std::string const FieldForced("forced");
std::string const FieldHint("hint");
std::string const FieldLookahead("lookahead");
std::string const FieldType("type");

arangodb::aql::IndexHint::HintType fromTypeName(std::string const& typeName) {
  if (::TypeDisabled == typeName) {
    return arangodb::aql::IndexHint::HintType::Disabled;
  }
  if (::TypeSimple == typeName) {
    return arangodb::aql::IndexHint::HintType::Simple;
  }
  if (::TypeNone == typeName) {
    return arangodb::aql::IndexHint::HintType::None;
  }

  return arangodb::aql::IndexHint::HintType::Illegal;
}
}  // namespace

namespace arangodb {
namespace aql {

IndexHint::IndexHint() : _type{HintType::None}, _forced{false} {}

IndexHint::IndexHint(QueryContext& query, AstNode const* node) : IndexHint() {
  if (node->type == AstNodeType::NODE_TYPE_OBJECT) {
    for (size_t i = 0; i < node->numMembers(); i++) {
      AstNode const* child = node->getMember(i);

      if (child->type == AstNodeType::NODE_TYPE_OBJECT_ELEMENT) {
        TRI_ASSERT(child->numMembers() > 0);
        std::string_view name(child->getStringView());

        bool handled = false;

        if (name == StaticStrings::IndexHintOption) {
          // indexHint
          AstNode const* value = child->getMember(0);

          if (value->isStringValue()) {
            // indexHint: string
            if (_type == HintType::Disabled) {
              // disableIndex vs. indexHint is contradicting...
              ExecutionPlan::invalidOptionAttribute(
                  query, "contradicting", "FOR", name.data(), name.size());
            }
            _type = HintType::Simple;
            _hint.simple.emplace_back(value->getStringValue(),
                                      value->getStringLength());
          } else if (value->type == AstNodeType::NODE_TYPE_ARRAY) {
            // indexHint: string: array
            if (_type == HintType::Disabled) {
              // disableIndex vs. indexHint is contradicting...
              ExecutionPlan::invalidOptionAttribute(
                  query, "contradicting", "FOR", name.data(), name.size());
            }
            _type = HintType::Simple;
            for (size_t j = 0; j < value->numMembers(); j++) {
              AstNode const* member = value->getMember(j);
              if (member->isStringValue()) {
                _hint.simple.emplace_back(member->getStringValue(),
                                          member->getStringLength());
              }
            }
          }
          handled = !_hint.simple.empty();
        } else if (name == arangodb::StaticStrings::IndexHintOptionForce) {
          // forceIndexHint
          AstNode const* value = child->getMember(0);

          if (value->isBoolValue()) {
            // forceIndexHint: bool
            _forced = value->getBoolValue();
            handled = true;
          }
        } else if (name == arangodb::StaticStrings::IndexHintDisableIndex) {
          // disableIndex
          AstNode const* value = child->getMember(0);

          if (value->isBoolValue()) {
            // disableIndex: bool
            if (value->getBoolValue()) {
              // disableIndex: true. this will disable all index hints
              _type = HintType::Disabled;
              if (!_hint.simple.empty()) {
                // disableIndex vs. indexHint is contradicting...
                ExecutionPlan::invalidOptionAttribute(
                    query, "contradicting", "FOR", name.data(), name.size());
                _hint.simple.clear();
              }
              TRI_ASSERT(_hint.simple.empty());
            }
            handled = true;
          }
        } else if (name == arangodb::StaticStrings::IndexHintMaxProjections) {
          // maxProjections is a valid attribute, but handled elsewhere
          handled = true;
        } else if (name == ::FieldLookahead) {
          TRI_ASSERT(child->numMembers() > 0);
          AstNode const* value = child->getMember(0);

          if (value->isIntValue()) {
            _lookahead = value->getIntValue();
          } else {
            ExecutionPlan::invalidOptionAttribute(query, "invalid", "FOR",
                                                  name.data(), name.size());
          }
          handled = true;
        }

        if (!handled) {
          ExecutionPlan::invalidOptionAttribute(query, "unknown", "FOR",
                                                name.data(), name.size());
        }
      }
    }
  }
}

IndexHint::IndexHint(VPackSlice slice) : IndexHint() {
  // read index hint from slice
  // index hints were introduced in version 3.5. in previous versions they
  // are not available, so we need to be careful when reading them
  VPackSlice s = slice.get(::FieldContainer);
  if (s.isObject()) {
    _forced =
        basics::VelocyPackHelper::getBooleanValue(s, ::FieldForced, false);
    _lookahead = basics::VelocyPackHelper::getNumericValue(s, ::FieldLookahead,
                                                           _lookahead);
    _type = ::fromTypeName(
        basics::VelocyPackHelper::getStringValue(s, ::FieldType, ""));
  }

  if (_type != HintType::Illegal && _type != HintType::None) {
    TRI_ASSERT(s.isObject());

    if (_type == HintType::Simple) {
      VPackSlice hintSlice = s.get(::FieldHint);
      TRI_ASSERT(hintSlice.isArray());
      for (VPackSlice index : VPackArrayIterator(hintSlice)) {
        TRI_ASSERT(index.isString());
        _hint.simple.emplace_back(index.copyString());
      }
    }
  }
}

IndexHint::HintType IndexHint::type() const noexcept { return _type; }

bool IndexHint::isForced() const noexcept { return _forced; }

std::vector<std::string> const& IndexHint::hint() const noexcept {
  TRI_ASSERT(_type == HintType::Simple);
  return _hint.simple;
}

std::string IndexHint::typeName() const {
  switch (_type) {
    case HintType::Illegal:
      return ::TypeIllegal;
    case HintType::Disabled:
      return ::TypeDisabled;
    case HintType::None:
      return ::TypeNone;
    case HintType::Simple:
      return ::TypeSimple;
  }

  return ::TypeIllegal;
}

void IndexHint::toVelocyPack(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenObject());
  VPackObjectBuilder guard(&builder, ::FieldContainer);
  builder.add(::FieldForced, VPackValue(_forced));
  builder.add(::FieldLookahead, VPackValue(_lookahead));
  builder.add(::FieldType, VPackValue(typeName()));
  if (_type == HintType::Simple) {
    VPackArrayBuilder hintGuard(&builder, ::FieldHint);
    for (std::string const& index : _hint.simple) {
      builder.add(VPackValue(index));
    }
  }
}

std::string IndexHint::toString() const {
  VPackBuilder builder;
  {
    VPackObjectBuilder guard(&builder);
    toVelocyPack(builder);
  }
  return builder.slice().toJson();
}

std::ostream& operator<<(std::ostream& stream,
                         arangodb::aql::IndexHint const& hint) {
  stream << hint.toString();
  return stream;
}

}  // namespace aql
}  // namespace arangodb
