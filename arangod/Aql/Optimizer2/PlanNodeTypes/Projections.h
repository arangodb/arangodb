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

#include "velocypack/Builder.h"

#include "Aql/Optimizer2/Types/Types.h"
#include "Inspection/Types.h"

namespace arangodb::aql::optimizer2::types {

struct Projections {
  /*
   * NOTE:
   * TODO: Can we drop "string-only" support here?!
   * projection on a top-level attribute. will be returned as a string
   * for downwards-compatibility
   *
   * projection on a nested attribute (e.g. a.b.c). will be returned as an
   * array. this kind of projection did not exist before 3.7
   *
   * This statement is true for "projections" and "filterProjections".
   */
  ProjectionType projections{};
  ProjectionType filterProjections{};
  AttributeTypes::Numeric maxProjections;

  bool operator==(Projections const&) const = default;

  template<typename Inspector>
  friend auto inspect(Inspector& f, Projections& x) {
    return f.object(x).fields(f.field("projections", x.projections),
                              f.field("filterProjections", x.filterProjections),
                              f.field("maxProjections", x.maxProjections));
  }
};

}  // namespace arangodb::aql::optimizer2::types
