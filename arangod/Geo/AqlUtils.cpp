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

#include "AqlUtils.h"
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Function.h"
#include "Aql/Variable.h"
#include "Geo/GeoJsonParser.h"
#include "Geo/GeoParams.h"
#include "Geo/Shapes.h"

#include <string>
#include <vector>

using namespace arangodb;
using namespace arangodb::geo;

// Handle GEO_DISTANCE(<something>, doc.field)
geo::Coordinate AqlUtils::parseGeoDistance(aql::AstNode const* args,
                                           aql::Variable const* ref) {
  // aql::AstNode* dist = node->getMemberUnchecked(0);
  TRI_ASSERT(args->numMembers() == 2);
  if (args->numMembers() != 2) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH);
  }
  // either doc.geo or [doc.lng, doc.lat]
  aql::AstNode const* var = args->getMember(1);
  TRI_ASSERT(var->isAttributeAccessForVariable(ref, true) ||
             var->isArray() &&
                 var->getMember(0)->isAttributeAccessForVariable(ref, true) &&
                 var->getMember(1)->isAttributeAccessForVariable(ref, true));
  aql::AstNode* cc = args->getMemberUnchecked(0);
  TRI_ASSERT(cc->type != aql::NODE_TYPE_ATTRIBUTE_ACCESS);
  if (cc->type == aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  }

  Result res;
  if (cc->type == aql::NODE_TYPE_ARRAY) {  // [lng, lat] is valid input
    TRI_ASSERT(cc->numMembers() == 2);
    return geo::Coordinate(/*lat*/ cc->getMember(1)->getDoubleValue(),
                           /*lon*/ cc->getMember(0)->getDoubleValue());
  } else {
    Result res;
    VPackBuilder jsonB;
    cc->toVelocyPackValue(jsonB);
    VPackSlice json = jsonB.slice();
    geo::ShapeContainer shape;
    if (json.isArray() && json.length() >= 2) {
      res = shape.parseCoordinates(json, /*GeoJson*/ true);
    } else {
      res = geo::GeoJsonParser::parseGeoJson(json, shape);
    }
    if (res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
    }
    return shape.centroid();
  }
}

// either GEO_DISTANCE or DISTANCE
geo::Coordinate AqlUtils::parseDistFCall(aql::AstNode const* node,
                                         aql::Variable const* ref) {
  TRI_ASSERT(node->type == aql::NODE_TYPE_FCALL);
  aql::AstNode* args = node->getMemberUnchecked(0);
  aql::Function* func = static_cast<aql::Function*>(node->getData());
  TRI_ASSERT(func != nullptr);
  if (func->name == "GEO_DISTANCE") {
    return AqlUtils::parseGeoDistance(args, ref);
  }
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH);
}

void AqlUtils::handleNode(aql::AstNode const* node, aql::Variable const* ref,
                          geo::QueryParams& params) {
  switch (node->type) {
    // Handle GEO_CONTAINS(<geoJson-object>, doc.field)
    // or GEO_INTERSECTS(<geoJson-object>, doc.field)
    case aql::NODE_TYPE_FCALL: {
      // TODO handle GEO_CONTAINS / INTERSECT
      aql::AstNode* args = node->getMemberUnchecked(0);
      TRI_ASSERT(args->numMembers() == 2);
      if (args->numMembers() != 2) {
        THROW_ARANGO_EXCEPTION(
            TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH);
      }
      aql::AstNode* cc = args->getMemberUnchecked(0);
      TRI_ASSERT(
          args->getMemberUnchecked(1)->isAttributeAccessForVariable(ref, true));
      TRI_ASSERT(cc->type != aql::NODE_TYPE_ATTRIBUTE_ACCESS);

      // arrays can't occur only handle real GeoJSON
      VPackBuilder geoJsonBuilder;
      cc->toVelocyPackValue(geoJsonBuilder);
      VPackSlice json = geoJsonBuilder.slice();
      Result res = geo::GeoJsonParser::parseGeoJson(json, params.filterShape);
      if (res.fail()) {
        THROW_ARANGO_EXCEPTION(res);
      }

      aql::Function* func = static_cast<aql::Function*>(node->getData());
      TRI_ASSERT(func != nullptr);
      if (func->name == "GEO_CONTAINS") {
        params.filterType = geo::FilterType::CONTAINS;
      } else if (func->name == "GEO_INTERSECTS") {
        params.filterType = geo::FilterType::INTERSECTS;
      } else {
        TRI_ASSERT(false);
      }
      break;
    }
    // Handle GEO_DISTANCE(<something>, doc.field) [<|<=|=>|>] <constant>
    case aql::NODE_TYPE_OPERATOR_BINARY_LE:
      params.maxInclusive = true;
    case aql::NODE_TYPE_OPERATOR_BINARY_LT: {
      TRI_ASSERT(node->numMembers() == 2);
      geo::Coordinate c =
          AqlUtils::parseDistFCall(node->getMemberUnchecked(0), ref);
      if (params.origin != geo::Coordinate::Invalid() &&
          params.origin != c) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
      }
      // LOG_TOPIC(ERR, Logger::FIXME) << "Found center: " << c.toString();

      params.origin = std::move(c);
      aql::AstNode const* max = node->getMemberUnchecked(1);
      TRI_ASSERT(max->type == aql::NODE_TYPE_VALUE);
      if (!max->isValueType(aql::VALUE_TYPE_STRING)) {
        params.maxDistance = max->getDoubleValue();
      }  // else assert(max->getStringValue() == "unlimited")
      break;
    }
    case aql::NODE_TYPE_OPERATOR_BINARY_GE:
      params.minInclusive = true;
    case aql::NODE_TYPE_OPERATOR_BINARY_GT: {
      TRI_ASSERT(node->numMembers() == 2);
      geo::Coordinate c =
          AqlUtils::parseDistFCall(node->getMemberUnchecked(0), ref);
      if (params.origin != geo::Coordinate::Invalid() &&
          params.origin != c) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
      }
      // LOG_TOPIC(ERR, Logger::FIXME) << "Found center: " << c.toString();

      aql::AstNode const* min = node->getMemberUnchecked(1);
      TRI_ASSERT(min->type == aql::NODE_TYPE_VALUE);
      params.origin = c;
      params.minDistance = min->getDoubleValue();
      break;
    }
    default:
      break;
  }
}

void AqlUtils::parseCondition(aql::AstNode const* node,
                              aql::Variable const* reference,
                              geo::QueryParams& params) {
  if (aql::Ast::IsAndOperatorType(node->type)) {
    for (size_t i = 0; i < node->numMembers(); i++) {
      handleNode(node->getMemberUnchecked(i), reference, params);
    }
  } else {
    handleNode(node, reference, params);
  }
}
