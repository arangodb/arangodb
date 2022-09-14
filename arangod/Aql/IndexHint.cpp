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

#include <ostream>

namespace arangodb {
namespace aql {
namespace {
std::string_view constexpr kTypeDisabled("disabled");
std::string_view constexpr kTypeIllegal("illegal");
std::string_view constexpr kTypeNone("none");
std::string_view constexpr kTypeSimple("simple");

std::string_view constexpr kFieldContainer("indexHint");
std::string_view constexpr kFieldForced("forced");
std::string_view constexpr kFieldHint("hint");
std::string_view constexpr kFieldType("type");

IndexHint::HintType fromTypeName(std::string_view typeName) noexcept {
  if (kTypeDisabled == typeName) {
    return IndexHint::Disabled;
  }
  if (kTypeSimple == typeName) {
    return IndexHint::Simple;
  }
  if (kTypeNone == typeName) {
    return IndexHint::None;
  }

  return IndexHint::Illegal;
}
}  // namespace

IndexHint::IndexHint(QueryContext& query, AstNode const* node) {
  auto getBooleanValue = [](AstNode const* child, bool& result) {
    AstNode const* value = child->getMember(0);

    if (value->isBoolValue()) {
      result = value->getBoolValue();
      return true;
    }
    return false;
  };

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
        } else if (name == StaticStrings::IndexHintOptionForce) {
          // forceIndexHint
          if (getBooleanValue(child, _forced)) {
            handled = true;
          }
        } else if (name == StaticStrings::WaitForSyncString) {
          // waitForSync
          if (getBooleanValue(child, _waitForSync)) {
            handled = true;
          }
        } else if (name == StaticStrings::IndexHintDisableIndex) {
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
        } else if (name == StaticStrings::MaxProjections ||
                   name == StaticStrings::UseCache) {
          // "maxProjections" and "useCache" are valid attributes,
          // but handled elsewhere
          handled = true;
        } else if (name == StaticStrings::IndexLookahead) {
          TRI_ASSERT(child->numMembers() > 0);
          AstNode const* value = child->getMember(0);

          if (value->isIntValue()) {
            _lookahead = value->getIntValue();
          } else {
            ExecutionPlan::invalidOptionAttribute(query, "invalid", "FOR",
                                                  name.data(), name.size());
          }
          handled = true;
        } else {
          ExecutionPlan::invalidOptionAttribute(query, "unknown", "FOR",
                                                name.data(), name.size());
          handled = true;
        }

        if (!handled) {
          VPackBuilder builder;
          child->getMember(0)->toVelocyPackValue(builder);
          std::string msg = "invalid value " + builder.toJson() + " in ";
          ExecutionPlan::invalidOptionAttribute(query, msg.data(), "FOR",
                                                name.data(), name.size());
        }
      }
    }
  }
}

IndexHint::IndexHint(VPackSlice slice) {
  // read index hint from slice
  // index hints were introduced in version 3.5. in previous versions they
  // are not available, so we need to be careful when reading them
  VPackSlice s = slice.get(kFieldContainer);
  if (s.isObject()) {
    _lookahead = basics::VelocyPackHelper::getNumericValue(
        s, StaticStrings::IndexLookahead, _lookahead);
    _type = fromTypeName(
        basics::VelocyPackHelper::getStringValue(s, kFieldType, ""));
    _forced = basics::VelocyPackHelper::getBooleanValue(s, kFieldForced, false);
    _waitForSync = basics::VelocyPackHelper::getBooleanValue(
        s, StaticStrings::WaitForSyncString, false);
  }

  if (_type != HintType::Illegal && _type != HintType::None) {
    TRI_ASSERT(s.isObject());

    if (_type == HintType::Simple) {
      VPackSlice hintSlice = s.get(kFieldHint);
      TRI_ASSERT(hintSlice.isArray());
      for (VPackSlice index : VPackArrayIterator(hintSlice)) {
        TRI_ASSERT(index.isString());
        _hint.simple.emplace_back(index.copyString());
      }
    }
  }
}

std::vector<std::string> const& IndexHint::hint() const noexcept {
  TRI_ASSERT(_type == HintType::Simple);
  return _hint.simple;
}

std::string_view IndexHint::typeName() const {
  switch (_type) {
    case HintType::Illegal:
      return kTypeIllegal;
    case HintType::Disabled:
      return kTypeDisabled;
    case HintType::None:
      return kTypeNone;
    case HintType::Simple:
      return kTypeSimple;
  }

  return kTypeIllegal;
}

void IndexHint::toVelocyPack(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenObject());
  VPackObjectBuilder guard(&builder, kFieldContainer);
  builder.add(kFieldForced, VPackValue(_forced));
  builder.add(StaticStrings::IndexLookahead, VPackValue(_lookahead));
  builder.add(kFieldType, VPackValue(typeName()));
  if (_type == HintType::Simple) {
    VPackArrayBuilder hintGuard(&builder, kFieldHint);
    for (auto const& index : _hint.simple) {
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

std::ostream& operator<<(std::ostream& stream, IndexHint const& hint) {
  stream << hint.toString();
  return stream;
}

}  // namespace aql
}  // namespace arangodb
