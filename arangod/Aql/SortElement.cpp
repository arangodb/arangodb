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

#include <absl/strings/str_cat.h>

using namespace arangodb;
using namespace arangodb::aql;

SortElement::SortElement(Variable const* v, bool asc)
    : var(v), ascending(asc) {}

SortElement::SortElement(Variable const* v, bool asc,
                         std::vector<std::string> path)
    : var(v), ascending(asc), attributePath(std::move(path)) {}

std::string SortElement::toString() const {
  std::string result = absl::StrCat("$", var->id);
  for (auto const& it : attributePath) {
    absl::StrAppend(&result, ".", it);
  }
  absl::StrAppend(&result, ascending ? " ASC" : " DESC");
  return result;
}
