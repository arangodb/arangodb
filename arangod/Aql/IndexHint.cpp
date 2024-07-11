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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "IndexHint.h"

#include "Aql/AstNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/QueryContext.h"
#include "Basics/NumberUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"

#include <absl/strings/str_cat.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <ostream>

namespace arangodb::aql {
namespace {
std::string_view constexpr kTypeDisabled("disabled");
std::string_view constexpr kTypeNone("none");
std::string_view constexpr kTypeSimple("simple");
std::string_view constexpr kTypeNested("nested");

std::string_view constexpr kFieldContainer("indexHint");
std::string_view constexpr kFieldForced("forced");
std::string_view constexpr kFieldHint("hint");
std::string_view constexpr kFieldType("type");

std::pair<IndexHint::DepthType, bool> getDepth(std::string_view level) {
  if (level == "base") {
    return std::make_pair(IndexHint::BaseDepth, true);
  }
  bool valid = false;
  IndexHint::DepthType levelId =
      NumberUtils::atoi_positive<IndexHint::DepthType>(
          level.data(), level.data() + level.size(), valid);
  return std::make_pair(levelId, valid);
}

bool getBooleanValue(AstNode const* child, bool& result) {
  AstNode const* value = child->getMember(0);

  if (value->isBoolValue()) {
    result = value->isTrue();
    return true;
  }
  return false;
}

bool handleStringOrArray(AstNode const* value,
                         std::function<void(AstNode const*)> const& cb) {
  if (value->isStringValue()) {
    cb(value);
    return true;
  }
  if (value->type == NODE_TYPE_ARRAY) {
    for (size_t i = 0; i < value->numMembers(); ++i) {
      AstNode const* member = value->getMember(i);
      if (!member->isStringValue()) {
        return false;
      }
      cb(member);
    }
    return true;
  }
  return false;
}

}  // namespace

IndexHint::IndexHint(QueryContext& query, AstNode const* node,
                     IndexHint::FromCollectionOperation) {
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

          bool ok = true;
          if (!handleStringOrArray(value, [&](AstNode const* value) {
                TRI_ASSERT(value->isStringValue());
                if (isDisabled()) {
                  // disableIndex vs. indexHint is contradicting...
                  ExecutionPlan::invalidOptionAttribute(query, "contradicting",
                                                        "FOR", name);
                }
                if (!std::holds_alternative<SimpleContents>(_hint)) {
                  // reset to simple type
                  _hint = SimpleContents{};
                }
                std::get<SimpleContents>(_hint).emplace_back(
                    value->getStringValue(), value->getStringLength());
              })) {
            ok = false;
          }

          handled = ok && !empty();
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
              if (!empty()) {
                // disableIndex vs. indexHint is contradicting...
                ExecutionPlan::invalidOptionAttribute(query, "contradicting",
                                                      "FOR", name);
              }
              _hint = Disabled{};
              TRI_ASSERT(empty());
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
                                                  name);
          }
          handled = true;
        } else {
          ExecutionPlan::invalidOptionAttribute(query, "unknown", "FOR", name);
          handled = true;
        }

        if (!handled) {
          VPackBuilder builder;
          child->getMember(0)->toVelocyPackValue(builder);
          ExecutionPlan::invalidOptionAttribute(
              query, absl::StrCat("invalid value ", builder.toJson(), " in"),
              "FOR", name);
        }
      }
    }
  }
}

IndexHint::IndexHint(QueryContext& query, AstNode const* node,
                     IndexHint::FromTraversal op)
    : IndexHint(query, node, /*hasLevels*/ true) {}

IndexHint::IndexHint(QueryContext& query, AstNode const* node,
                     IndexHint::FromPathsQuery op)
    : IndexHint(query, node, /*hasLevels*/ false) {}

// internal constructor for nested index hints for graph operations
IndexHint::IndexHint(QueryContext& query, AstNode const* node, bool hasLevels) {
  if (node->type == AstNodeType::NODE_TYPE_OBJECT) {
    for (size_t i = 0; i < node->numMembers(); i++) {
      AstNode const* child = node->getMember(i);

      if (child->type == AstNodeType::NODE_TYPE_OBJECT_ELEMENT) {
        TRI_ASSERT(child->numMembers() > 0);
        std::string_view name(child->getStringView());

        if (name == StaticStrings::IndexHintOption) {
          // indexHint
          AstNode const* value = child->getMember(0);

          if (value->type == AstNodeType::NODE_TYPE_OBJECT) {
            _hint = NestedContents{};
            if (!parseNestedHint(value, std::get<NestedContents>(_hint),
                                 hasLevels)) {
              _hint = NoContents{};
            }
          }
          if (empty()) {
            VPackBuilder builder;
            child->getMember(0)->toVelocyPackValue(builder);
            ExecutionPlan::invalidOptionAttribute(
                query, absl::StrCat("invalid value ", builder.toJson(), " in"),
                "TRAVERSAL", name);
          }
        } else if (name == StaticStrings::IndexHintOptionForce) {
          // forceIndexHint
          getBooleanValue(child, _forced);
        }
      }
    }
  }
}

IndexHint::IndexHint(velocypack::Slice slice) {
  std::string_view type{kTypeNone};

  VPackSlice s = slice.get(kFieldContainer);
  if (s.isObject()) {
    _lookahead = basics::VelocyPackHelper::getNumericValue(
        s, StaticStrings::IndexLookahead, _lookahead);
    _forced = basics::VelocyPackHelper::getBooleanValue(s, kFieldForced, false);
    _waitForSync = basics::VelocyPackHelper::getBooleanValue(
        s, StaticStrings::WaitForSyncString, false);

    type = basics::VelocyPackHelper::getStringView(s, kFieldType, type);
  }

  if (type == kTypeNone) {
    // no index hint specified
    _hint = NoContents{};
  } else if (type == kTypeSimple) {
    // deserialize simple index hint
    _hint = SimpleContents{};
    VPackSlice hintSlice = s.get(kFieldHint);
    TRI_ASSERT(hintSlice.isArray());
    for (auto index : VPackArrayIterator(hintSlice)) {
      TRI_ASSERT(index.isString());
      std::get<SimpleContents>(_hint).emplace_back(index.copyString());
    }
  } else if (type == kTypeNested) {
    // deserialize bested index hint
    _hint = NestedContents{};
    VPackSlice hintSlice = s.get(kFieldHint);
    TRI_ASSERT(hintSlice.isObject());
    for (auto collection : VPackObjectIterator(hintSlice)) {
      TRI_ASSERT(collection.value.isObject());
      for (auto direction : VPackObjectIterator(collection.value)) {
        TRI_ASSERT(direction.value.isObject());
        for (auto level : VPackObjectIterator(direction.value)) {
          std::string_view key = level.key.stringView();

          auto [levelId, valid] = getDepth(key);
          TRI_ASSERT(valid);

          auto& ref = std::get<NestedContents>(
              _hint)[collection.key.stringView()][direction.key.stringView()];
          TRI_ASSERT(level.value.isArray());
          for (auto index : VPackArrayIterator(level.value)) {
            TRI_ASSERT(index.isString());
            ref[levelId].emplace_back(index.copyString());
          }
        }
      }
    }
  } else if (type == kTypeDisabled) {
    _hint = Disabled{};
  }
}

// returns true for hint types simple and nested
bool IndexHint::isSet() const noexcept { return isSimple() || isNested(); }

bool IndexHint::isSimple() const noexcept {
  return std::holds_alternative<SimpleContents>(_hint);
}

bool IndexHint::isNested() const noexcept {
  return std::holds_alternative<NestedContents>(_hint);
}

bool IndexHint::isDisabled() const noexcept {
  return std::holds_alternative<Disabled>(_hint);
}

IndexHint::PossibleIndexes const& IndexHint::candidateIndexes() const noexcept {
  TRI_ASSERT(isSimple());
  return std::get<SimpleContents>(_hint);
}

bool IndexHint::empty() const noexcept {
  if (isSimple()) {
    return std::get<SimpleContents>(_hint).empty();
  }
  if (isNested()) {
    for (auto const& collection : std::get<NestedContents>(_hint)) {
      for (auto const& direction : collection.second) {
        for (auto const& level : direction.second) {
          if (!level.second.empty()) {
            return false;
          }
        }
      }
    }
    // fallthrough intentional
  }

  return true;
}

std::string_view IndexHint::typeName() const noexcept {
  if (std::holds_alternative<SimpleContents>(_hint)) {
    return kTypeSimple;
  }
  if (std::holds_alternative<NestedContents>(_hint)) {
    return kTypeNested;
  }
  if (std::holds_alternative<Disabled>(_hint)) {
    return kTypeDisabled;
  }
  return kTypeNone;
}

void IndexHint::toVelocyPack(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenObject());
  VPackObjectBuilder guard(&builder, kFieldContainer);
  builder.add(kFieldForced, VPackValue(_forced));
  builder.add(StaticStrings::IndexLookahead, VPackValue(_lookahead));
  builder.add(kFieldType, VPackValue(typeName()));
  if (isSimple()) {
    VPackArrayBuilder hintGuard(&builder, kFieldHint);
    indexesToVelocyPack(builder, std::get<SimpleContents>(_hint));
  } else if (isNested()) {
    VPackObjectBuilder hintGuard(&builder, kFieldHint);
    for (auto const& collection : std::get<NestedContents>(_hint)) {
      builder.add(collection.first, VPackValue(VPackValueType::Object));
      for (auto const& direction : collection.second) {
        builder.add(direction.first, VPackValue(VPackValueType::Object));
        for (auto const& level : direction.second) {
          if (level.first == BaseDepth) {
            builder.add(VPackValue("base"));
          } else {
            builder.add(VPackValue(std::to_string(level.first)));
          }
          VPackArrayBuilder hintGuard(&builder);
          indexesToVelocyPack(builder, level.second);
        }
        builder.close();
      }
      builder.close();
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

IndexHint IndexHint::getFromNested(std::string_view direction,
                                   std::string_view collection,
                                   IndexHint::DepthType depth) const {
  IndexHint specific;

  auto appendIndexes = [this, &specific](auto const& indexes) {
    if (!specific.isSimple()) {
      specific._hint = SimpleContents{};
      specific._forced = _forced;
    }
    // append only
    for (auto const& index : indexes) {
      std::get<SimpleContents>(specific._hint).emplace_back(index);
    }
  };

  // in case the index hint is not nested, then we simply return an empty
  // index hint.
  if (isNested()) {
    // our index hint is nested, now extract the values for the requested
    // direction, collection, and lookup level
    auto& hints = std::get<NestedContents>(_hint);
    // find collection
    if (auto it = hints.find(collection); it != hints.end()) {
      // find direction
      if (auto it2 = it->second.find(direction); it2 != it->second.end()) {
        // find specific depth
        auto it3 = it2->second.find(depth);
        if (it3 != it2->second.end()) {
          appendIndexes(it3->second);
        }
        // find base depth, if it was not already the original depth queried
        if (depth != BaseDepth) {
          // append all indexes for base-depth
          it3 = it2->second.find(BaseDepth);
          if (it3 != it2->second.end()) {
            appendIndexes(it3->second);
          }
        }
      }
    }
  }

  return specific;
}

void IndexHint::indexesToVelocyPack(
    velocypack::Builder& builder,
    IndexHint::PossibleIndexes const& indexes) const {
  for (auto const& index : indexes) {
    builder.add(VPackValue(index));
  }
}

bool IndexHint::parseNestedHint(AstNode const* node,
                                IndexHint::NestedContents& hint,
                                bool hasLevels) const {
  TRI_ASSERT(node->type == AstNodeType::NODE_TYPE_OBJECT);

  // iterate over all collections
  for (size_t i = 0; i < node->numMembers(); i++) {
    AstNode const* child = node->getMember(i);

    if (child->type != AstNodeType::NODE_TYPE_OBJECT_ELEMENT) {
      return false;
    }

    std::string_view collectionName(child->getStringView());

    AstNode const* sub = child->getMember(0);

    if (sub->type != NODE_TYPE_OBJECT) {
      return false;
    }

    // iterate over all directions
    for (size_t j = 0; j < sub->numMembers(); j++) {
      AstNode const* child = sub->getMember(j);

      if (child->type != AstNodeType::NODE_TYPE_OBJECT_ELEMENT) {
        return false;
      }

      std::string_view directionName(child->getStringView());

      if (directionName != StaticStrings::GraphDirectionInbound &&
          directionName != StaticStrings::GraphDirectionOutbound) {
        // wrong attribute name
        return false;
      }

      auto& ref = hint[collectionName][directionName];

      AstNode const* sub = child->getMember(0);

      if (hasLevels) {
        if (sub->type != NODE_TYPE_OBJECT) {
          return false;
        }

        // iterate over all levels
        for (size_t k = 0; k < sub->numMembers(); k++) {
          AstNode const* level = sub->getMember(k);

          if (level->type != AstNodeType::NODE_TYPE_OBJECT_ELEMENT) {
            return false;
          }

          std::string_view levelName(level->getStringView());

          auto [levelId, valid] = getDepth(levelName);
          if (!valid) {
            return false;
          }

          AstNode const* value = level->getMember(0);

          if (!handleStringOrArray(value, [&](AstNode const* value) {
                TRI_ASSERT(value->isStringValue());
                ref[levelId].emplace_back(value->getStringValue(),
                                          value->getStringLength());
              })) {
            return false;
          }
        }
      } else {
        // no levels
        if (!handleStringOrArray(sub, [&](AstNode const* value) {
              TRI_ASSERT(value->isStringValue());
              ref[IndexHint::BaseDepth].emplace_back(value->getStringValue(),
                                                     value->getStringLength());
            })) {
          return false;
        }
      }
    }
  }

  return true;
}

std::ostream& operator<<(std::ostream& stream, IndexHint const& hint) {
  stream << hint.toString();
  return stream;
}

}  // namespace arangodb::aql
