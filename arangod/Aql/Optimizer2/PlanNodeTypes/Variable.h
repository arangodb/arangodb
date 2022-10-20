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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "velocypack/Builder.h"

namespace arangodb::aql::optimizer2::types {

using VariableId = size_t;
using Identifier = std::string;

struct Variable {
  VariableId id;
  Identifier name;

  bool isFullDocumentFromCollection;
  bool isDataFromCollection;
  std::optional<VPackBuilder>
      constantValue;  // Hint: Originally AqlValue used here

  bool operator==(Variable const& other) const {
    bool constantEqual = true;
    if (constantValue.has_value() && other.constantValue.has_value()) {
      constantEqual = this->constantValue.value().slice().binaryEquals(
          other.constantValue.value().slice());
    }
    return (this->id == other.id && this->name == other.name &&
            this->isFullDocumentFromCollection ==
                other.isFullDocumentFromCollection &&
            this->isDataFromCollection == other.isDataFromCollection &&
            constantEqual);
  }
};

template<typename Inspector>
auto inspect(Inspector& f, Variable& x) {
  return f.object(x).fields(
      f.field("id", x.id), f.field("name", x.name),
      f.field("isFullDocumentFromCollection", x.isFullDocumentFromCollection),
      f.field("isDataFromCollection", x.isDataFromCollection),
      f.field("constantValue", x.constantValue));
}

}  // namespace arangodb::aql::optimizer2::types
