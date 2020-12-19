////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017-2018 ArangoDB GmbH, Cologne, Germany
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

#include "Index.h"

#include <string>
#include <vector>

#include <s2/s2latlng.h>
#include <s2/s2region_coverer.h>

#include <velocypack/Slice.h>

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Function.h"
#include "Aql/Variable.h"
#include "Basics/VelocyPackHelper.h"
#include "Geo/GeoJson.h"
#include "Geo/GeoParams.h"
#include "Geo/Utils.h"
#include "Geo/ShapeContainer.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

namespace arangodb {
namespace geo_index {

Index::Index(VPackSlice const& info,
             std::vector<std::vector<basics::AttributeName>> const& fields)
    : _variant(Variant::NONE) {
  _coverParams.fromVelocyPack(info);

  if (fields.size() == 1) {
    bool geoJson = basics::VelocyPackHelper::getBooleanValue(info, "geoJson", false);
    // geojson means [<longitude>, <latitude>] or
    // json object {type:"<name>, coordinates:[]}.
    _variant = geoJson ? Variant::GEOJSON : Variant::COMBINED_LAT_LON;

    auto& loc = fields[0];
    _location.reserve(loc.size());
    std::transform(loc.begin(), loc.end(), std::back_inserter(_location),
                   [](auto const& a) { return a.name; } );
  } else if (fields.size() == 2) {
    _variant = Variant::INDIVIDUAL_LAT_LON;
    auto& lat = fields[0];
    _latitude.reserve(lat.size());
    std::transform(lat.begin(), lat.end(), std::back_inserter(_latitude),
                   [](auto const& a) { return a.name; } );

    auto& lon = fields[1];
    _longitude.reserve(lon.size());
    std::transform(lon.begin(), lon.end(), std::back_inserter(_longitude),
                   [](auto const& a) { return a.name; } );
  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "geo index can only be created with one or two fields.");
  }
}

/// @brief Parse document and return cells for indexing
Result Index::indexCells(VPackSlice const& doc, std::vector<S2CellId>& cells,
                         S2Point& centroid) const {

  if (_variant == Variant::GEOJSON) {
    VPackSlice loc = doc.get(_location);
    if (loc.isArray()) {
      return geo::utils::indexCellsLatLng(loc, /*geojson*/ true, cells, centroid);
    }
    geo::ShapeContainer shape;
    Result r = geo::geojson::parseRegion(loc, shape);
    if (r.ok()) {
      S2RegionCoverer coverer(_coverParams.regionCovererOpts());
      cells = shape.covering(&coverer);
      centroid = shape.centroid();
      if (!S2LatLng(centroid).is_valid()) {
        return TRI_ERROR_BAD_PARAMETER;
      }
    } else if (r.is(TRI_ERROR_NOT_IMPLEMENTED)) {
      // ignore not-implemented error on inserts, because index is sparse
      return TRI_ERROR_BAD_PARAMETER;
    }
    return r;
  } else if (_variant == Variant::COMBINED_LAT_LON) {
    VPackSlice loc = doc.get(_location);
    return geo::utils::indexCellsLatLng(loc, /*geojson*/ false, cells, centroid);
  } else if (_variant == Variant::INDIVIDUAL_LAT_LON) {
    VPackSlice lat = doc.get(_latitude);
    VPackSlice lon = doc.get(_longitude);
    if (!lat.isNumber() || !lon.isNumber()) {
      return TRI_ERROR_BAD_PARAMETER;
    }
    S2LatLng ll = S2LatLng::FromDegrees(lat.getNumericValue<double>(),
                                        lon.getNumericValue<double>());
    if (!ll.is_valid()) {
      LOG_TOPIC("8173c", DEBUG, arangodb::Logger::FIXME)
          << "illegal geo-coordinates, ignoring entry";
      return TRI_ERROR_NO_ERROR;
    }
    centroid = ll.ToPoint();
    cells.emplace_back(centroid);
    return TRI_ERROR_NO_ERROR;
  }
  return TRI_ERROR_INTERNAL;
}

Result Index::shape(velocypack::Slice const& doc, geo::ShapeContainer& shape) const {
  if (_variant == Variant::GEOJSON) {
    VPackSlice loc = doc.get(_location);
    if (loc.isArray() && loc.length() >= 2) {
      return shape.parseCoordinates(loc, /*geoJson*/ true);
    } else if (loc.isObject()) {
      return geo::geojson::parseRegion(loc, shape);
    }
    return TRI_ERROR_BAD_PARAMETER;
  } else if (_variant == Variant::COMBINED_LAT_LON) {
    VPackSlice loc = doc.get(_location);
    return shape.parseCoordinates(loc, /*geoJson*/ false);
  } else if (_variant == Variant::INDIVIDUAL_LAT_LON) {
    VPackSlice lon = doc.get(_longitude);
    VPackSlice lat = doc.get(_latitude);
    if (!lon.isNumber() || !lat.isNumber()) {
      return TRI_ERROR_BAD_PARAMETER;
    }
    shape.resetCoordinates(lat.getNumericValue<double>(), lon.getNumericValue<double>());
    return TRI_ERROR_NO_ERROR;
  }
  return TRI_ERROR_INTERNAL;
}

// Handle GEO_DISTANCE(<something>, doc.field)
S2LatLng Index::parseGeoDistance(aql::AstNode const* args, aql::Variable const* ref) {
  // aql::AstNode* dist = node->getMemberUnchecked(0);
  TRI_ASSERT(args->numMembers() == 2);
  if (args->numMembers() != 2) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH);
  }
  // either doc.geo or [doc.lng, doc.lat]
  aql::AstNode const* var = args->getMember(1);
  TRI_ASSERT(var->isAttributeAccessForVariable(ref, true) ||
             (var->isArray() && var->getMember(0)->isAttributeAccessForVariable(ref, true) &&
              var->getMember(1)->isAttributeAccessForVariable(ref, true)));
  aql::AstNode* cc = args->getMemberUnchecked(0);
  TRI_ASSERT(cc->type != aql::NODE_TYPE_ATTRIBUTE_ACCESS);
  if (cc->type == aql::NODE_TYPE_ATTRIBUTE_ACCESS) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  }

  if (cc->type == aql::NODE_TYPE_ARRAY) {  // [lng, lat] is valid input
    TRI_ASSERT(cc->numMembers() == 2);
    return S2LatLng::FromDegrees(/*lat*/ cc->getMember(1)->getDoubleValue(),
                                 /*lon*/ cc->getMember(0)->getDoubleValue());
  } else {
    VPackBuilder jsonB;
    cc->toVelocyPackValue(jsonB);
    VPackSlice json = jsonB.slice();
    geo::ShapeContainer shape;
    Result res;
    if (json.isArray() && json.length() >= 2) {
      res = shape.parseCoordinates(json, /*GeoJson*/ true);
    } else {
      res = geo::geojson::parseRegion(json, shape);
    }
    if (res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
    }
    return S2LatLng(shape.centroid());
  }
}

// either parses GEO_DISTANCE call argument values
S2LatLng Index::parseDistFCall(aql::AstNode const* node, aql::Variable const* ref) {
  TRI_ASSERT(node->type == aql::NODE_TYPE_FCALL);
  aql::AstNode* args = node->getMemberUnchecked(0);
  aql::Function const* func = static_cast<aql::Function const*>(node->getData());
  TRI_ASSERT(func != nullptr);
  if (func->name == "GEO_DISTANCE") {
    return Index::parseGeoDistance(args, ref);
  }
  // we should not get here for any other functions, not even DISTANCE
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_INTERNAL,
      std::string("parseDistFCall called for unexpected function '") +
          func->name + "'");
}

void Index::handleNode(aql::AstNode const* node, aql::Variable const* ref,
                       geo::QueryParams& qp) {
  switch (node->type) {
    // Handle GEO_CONTAINS(<geoJson-object>, doc.field)
    // or GEO_INTERSECTS(<geoJson-object>, doc.field)
    case aql::NODE_TYPE_FCALL: {
      // TODO handle GEO_CONTAINS / INTERSECT
      aql::AstNode const* args = node->getMemberUnchecked(0);
      TRI_ASSERT(args->numMembers() == 2);
      if (args->numMembers() != 2) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH);
      }

      aql::AstNode const* geoJson = args->getMemberUnchecked(0);
      aql::AstNode const* symbol = args->getMemberUnchecked(1);
      // geojson or array with variables
      TRI_ASSERT(symbol->isAttributeAccessForVariable(ref, true) ||
                 (symbol->isArray() && symbol->numMembers() == 2 &&
                  symbol->getMember(0)->getAttributeAccessForVariable(true) != nullptr &&
                  symbol->getMember(1)->getAttributeAccessForVariable(true) != nullptr));
      TRI_ASSERT(geoJson->type != aql::NODE_TYPE_REFERENCE);

      // arrays can't occur only handle real GeoJSON
      VPackBuilder bb;
      geoJson->toVelocyPackValue(bb);
      Result res = geo::geojson::parseRegion(bb.slice(), qp.filterShape);
      if (res.fail()) {
        THROW_ARANGO_EXCEPTION(res);
      }

      aql::Function* func = static_cast<aql::Function*>(node->getData());
      TRI_ASSERT(func != nullptr);
      if (func->name == "GEO_CONTAINS") {
        qp.filterType = geo::FilterType::CONTAINS;
      } else if (func->name == "GEO_INTERSECTS") {
        qp.filterType = geo::FilterType::INTERSECTS;
      } else {
        TRI_ASSERT(false);
      }
      break;
    }
    // Handle GEO_DISTANCE(<something>, doc.field) [<|<=|=>|>] <constant>
    case aql::NODE_TYPE_OPERATOR_BINARY_LE:
      qp.maxInclusive = true;
      // intentional fallthrough
      [[fallthrough]];
    case aql::NODE_TYPE_OPERATOR_BINARY_LT: {
      TRI_ASSERT(node->numMembers() == 2);
      qp.origin = Index::parseDistFCall(node->getMemberUnchecked(0), ref);
      if (!qp.origin.is_valid()) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_INVALID_GEO_VALUE);
      }

      aql::AstNode const* max = node->getMemberUnchecked(1);
      TRI_ASSERT(max->type == aql::NODE_TYPE_VALUE);
      if (max->type != aql::NODE_TYPE_VALUE) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_INVALID_GEO_VALUE);
      }
      if (!max->isValueType(aql::VALUE_TYPE_STRING)) {
        qp.maxDistance = max->getDoubleValue();
      }  // else assert(max->getStringValue() == "unlimited")
      break;
    }
    case aql::NODE_TYPE_OPERATOR_BINARY_GE:
      qp.minInclusive = true;
      // intentional fallthrough
      [[fallthrough]];
    case aql::NODE_TYPE_OPERATOR_BINARY_GT: {
      TRI_ASSERT(node->numMembers() == 2);
      qp.origin = Index::parseDistFCall(node->getMemberUnchecked(0), ref);
      if (!qp.origin.is_valid()) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_INVALID_GEO_VALUE);
      }

      aql::AstNode const* min = node->getMemberUnchecked(1);
      TRI_ASSERT(min->type == aql::NODE_TYPE_VALUE);
      if (min->type != aql::NODE_TYPE_VALUE) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_INVALID_GEO_VALUE);
      }
      qp.minDistance = min->getDoubleValue();
      break;
    }
    default:
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_INVALID_GEO_VALUE);
  }
}

void Index::parseCondition(aql::AstNode const* node, aql::Variable const* reference,
                           geo::QueryParams& params) {
  if (aql::Ast::IsAndOperatorType(node->type)) {
    for (size_t i = 0; i < node->numMembers(); i++) {
      handleNode(node->getMemberUnchecked(i), reference, params);
    }
  } else {
    handleNode(node, reference, params);
  }
  
  // allow for GEO_DISTANCE(g, d.geometry) <= 0
  if (params.filterType == geo::FilterType::NONE &&
      params.minDistance == 0 &&
      params.maxDistance == 0 &&
      params.maxInclusive) {
    params.maxDistance = geo::kRadEps * geo::kEarthRadiusInMeters;
  }
}

}  // namespace geo_index
}  // namespace arangodb
