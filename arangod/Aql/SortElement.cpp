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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "SortElement.h"

#include "Aql/Variable.h"
#include "Basics/debugging.h"

#include <absl/strings/str_cat.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/Iterator.h>

using namespace arangodb;
using namespace arangodb::aql;

SortElement::SortElement(Variable const* v, bool asc) : var(v), ascending(asc) {
  TRI_ASSERT(var != nullptr);
}

SortElement::SortElement(Variable const* v, bool asc,
                         std::vector<std::string> path)
    : SortElement(v, asc) {
  attributePath = std::move(path);
}

SortElement SortElement::create(Variable const* v, bool asc) {
  return SortElement(v, asc);
}

SortElement SortElement::createWithPath(Variable const* v, bool asc,
                                        std::vector<std::string> path) {
  return SortElement(v, asc, std::move(path));
}

/// @brief resets variable to var, and clears the attributePath
void SortElement::resetTo(Variable const* v) {
  var = v;
  attributePath.clear();
}

void SortElement::replaceVariables(
    std::unordered_map<VariableId, Variable const*> const& replacements) {
  var = Variable::replace(var, replacements);
}

void SortElement::replaceAttributeAccess(Variable const* searchVariable,
                                         std::span<std::string_view> attribute,
                                         Variable const* replaceVariable) {
  if (var != searchVariable) {
    return;
  }

  // if the attribute path is  $var.a.b and we replace $var.a by $other,
  // the attribute path should become just `b`, i.e. $other.b.

  auto it1 = attributePath.begin();
  auto it2 = attribute.begin();

  bool isPrefix = true;
  while (it2 != attribute.end()) {
    if (it1 == attributePath.end() || *it1 != *it2) {
      // this path does not match the prefix
      isPrefix = false;
      break;
    }
    ++it1;
    ++it2;
  }

  if (!isPrefix) {
    return;
  }

  // it1 now points to the remainder. Remove the prefix.
  attributePath.erase(attributePath.cbegin(), it1);

  var = replaceVariable;
}

std::string SortElement::toString() const {
  std::string result = absl::StrCat("$", var->id);
  for (auto const& it : attributePath) {
    absl::StrAppend(&result, ".", it);
  }
  absl::StrAppend(&result, ascending ? " ASC" : " DESC");
  return result;
}

void SortElement::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder obj(&builder);
  builder.add(VPackValue("inVariable"));
  var->toVelocyPack(builder);
  builder.add("ascending", VPackValue(ascending));
  if (!attributePath.empty()) {
    builder.add(VPackValue("path"));
    VPackArrayBuilder arr(&builder);
    for (auto const& a : attributePath) {
      builder.add(VPackValue(a));
    }
  }
}

SortElement SortElement::fromVelocyPack(Ast* ast, velocypack::Slice info) {
  bool ascending = info.get("ascending").getBoolean();
  Variable* v = Variable::varFromVPack(ast, info, "inVariable");

  SortElement elem(v, ascending);
  // Is there an attribute path?
  if (auto path = info.get("path"); path.isArray()) {
    // Get a list of strings out and add to the path:
    for (auto it : VPackArrayIterator(path)) {
      if (it.isString()) {
        elem.attributePath.emplace_back(it.copyString());
      }
    }
  }
  return elem;
}
