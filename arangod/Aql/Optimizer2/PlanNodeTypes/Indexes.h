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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Aql/Optimizer2/Types/Types.h"

// TODO: This type may be not required. Please check later.
namespace arangodb::aql::optimizer2::types {

struct IndexFigure {
  AttributeTypes::Numeric memory;

  bool operator==(IndexFigure const&) const = default;
};

template<typename Inspector>
auto inspect(Inspector& f, IndexFigure& x) {
  return f.object(x).fields(f.field("memory", x.memory));
};

// Note: e.g. used in IndexNode
// Note2: TraversalNode e.g. has its own implementation.
struct IndexHandle {
  AttributeTypes::String id;
  AttributeTypes::String type;
  AttributeTypes::String name;
  std::vector<AttributeTypes::String> fields;
  bool unique;
  bool sparse;
  std::optional<AttributeTypes::Double> selectivityEstimate;
  std::optional<IndexFigure> figures;

  bool operator==(IndexHandle const&) const = default;
};

template<typename Inspector>
auto inspect(Inspector& f, IndexHandle& x) {
  return f.object(x).fields(
      f.field("id", x.id), f.field("type", x.type), f.field("name", x.name),
      f.field("fields", x.fields), f.field("unique", x.unique),
      f.field("sparse", x.sparse),
      f.field("selectivityEstimate", x.selectivityEstimate),
      f.field("figures", x.figures));
};

}  // namespace arangodb::aql::optimizer2::types
