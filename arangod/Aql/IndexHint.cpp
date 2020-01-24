////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2019 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

#include <ostream>

namespace {
std::string const TypeIllegal("illegal");
std::string const TypeNone("none");
std::string const TypeSimple("simple");

std::string const FieldContainer("indexHint");
std::string const FieldForced("forced");
std::string const FieldHint("hint");
std::string const FieldType("type");

bool extractForced(arangodb::aql::AstNode const* node) {
  using arangodb::aql::AstNode;
  using arangodb::aql::AstNodeType;
  using arangodb::aql::AstNodeValueType;

  bool forced = false;

  if (node->type == AstNodeType::NODE_TYPE_OBJECT) {
    for (size_t i = 0; i < node->numMembers(); i++) {
      AstNode const* child = node->getMember(i);

      if (child->type == AstNodeType::NODE_TYPE_OBJECT_ELEMENT) {
        VPackStringRef name(child->getStringValue(), child->getStringLength());

        if (name == arangodb::StaticStrings::IndexHintOptionForce) {
          TRI_ASSERT(child->numMembers() > 0);
          AstNode const* value = child->getMember(0);

          if (value->type == AstNodeType::NODE_TYPE_VALUE &&
              value->value.type == AstNodeValueType::VALUE_TYPE_BOOL) {
            forced = value->value.value._bool;
          }
        }
      }
    }
  }

  return forced;
}

arangodb::aql::IndexHint::HintType fromTypeName(std::string const& typeName) {
  if (::TypeSimple == typeName) {
    return arangodb::aql::IndexHint::HintType::Simple;
  } else if (::TypeNone == typeName) {
    return arangodb::aql::IndexHint::HintType::None;
  }

  return arangodb::aql::IndexHint::HintType::Illegal;
}
}  // namespace

namespace arangodb {
namespace aql {

IndexHint::IndexHint() : _type{HintType::None}, _forced{false} {}

IndexHint::IndexHint(AstNode const* node)
    : _type{HintType::None}, _forced{::extractForced(node)} {
  if (node->type == AstNodeType::NODE_TYPE_OBJECT) {
    for (size_t i = 0; i < node->numMembers(); i++) {
      AstNode const* child = node->getMember(i);

      if (child->type == AstNodeType::NODE_TYPE_OBJECT_ELEMENT) {
        VPackStringRef name(child->getStringValue(), child->getStringLength());

        if (name == StaticStrings::IndexHintOption) {
          TRI_ASSERT(child->numMembers() > 0);
          AstNode const* value = child->getMember(0);

          if (value->type == AstNodeType::NODE_TYPE_VALUE &&
              value->value.type == AstNodeValueType::VALUE_TYPE_STRING) {
            _type = HintType::Simple;
            _hint.simple.emplace_back(value->getStringValue(), value->getStringLength());
          }

          if (value->type == AstNodeType::NODE_TYPE_ARRAY) {
            _type = HintType::Simple;
            for (size_t j = 0; j < value->numMembers(); j++) {
              AstNode const* member = value->getMember(j);
              if (member->type == AstNodeType::NODE_TYPE_VALUE &&
                  member->value.type == AstNodeValueType::VALUE_TYPE_STRING) {
                _hint.simple.emplace_back(member->getStringValue(),
                                          member->getStringLength());
              }
            }
          }
        }
      }
    }
  }
}

IndexHint::IndexHint(VPackSlice const& slice)
    : IndexHint() {
      
  // read index hint from slice
  // index hints were introduced in version 3.5. in previous versions they
  // are not available, so we need to be careful when reading them
  VPackSlice s = slice.get(::FieldContainer);
  if (s.isObject()) {
    _type = ::fromTypeName(
          basics::VelocyPackHelper::getStringValue(s, ::FieldType, ""));
    _forced = basics::VelocyPackHelper::getBooleanValue(s, ::FieldForced, false);
  }

  if (_type != HintType::Illegal && _type != HintType::None) {
    VPackSlice container = slice.get(::FieldContainer);
    TRI_ASSERT(container.isObject());

    if (_type == HintType::Simple) {
      VPackSlice hintSlice = container.get(::FieldHint);
      TRI_ASSERT(hintSlice.isArray());
      for (VPackSlice index : VPackArrayIterator(hintSlice)) {
        TRI_ASSERT(index.isString());
        _hint.simple.emplace_back(index.copyString());
      }
    }
  }
}

IndexHint::HintType IndexHint::type() const { return _type; }

bool IndexHint::isForced() const { return _forced; }

std::vector<std::string> const& IndexHint::hint() const {
  TRI_ASSERT(_type == HintType::Simple);
  return _hint.simple;
}

std::string IndexHint::typeName() const {
  switch (_type) {
    case HintType::Illegal:
      return ::TypeIllegal;
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

std::ostream& operator<<(std::ostream& stream, arangodb::aql::IndexHint const& hint) {
  stream << hint.toString();
  return stream;
}

}  // namespace aql
}  // namespace arangodb
