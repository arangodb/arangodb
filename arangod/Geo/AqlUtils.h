////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GEO_AQL_UTILS_H
#define ARANGOD_GEO_AQL_UTILS_H 1

#include "Basics/Result.h"
#include "Geo/Shapes.h"

namespace arangodb {
namespace aql {
  struct AstNode;
  struct Variable;
}
namespace geo {
struct QueryParams;

/// Static helper methods to translate an AQL condition
/// into the corresponding geo::QueryParams
struct AqlUtils {
  static void parseCondition(aql::AstNode const* node,
                             aql::Variable const* reference,
                             geo::QueryParams& params);
  
private:
  static geo::Coordinate parseGeoDistance(aql::AstNode const* node, aql::Variable const* ref);
  static geo::Coordinate parseDistFCall(aql::AstNode const* node, aql::Variable const* ref);
  static void handleNode(aql::AstNode const* node, aql::Variable const* ref, geo::QueryParams& params);
};

}  // namespace geo
}  // namespace arangodb

#endif
