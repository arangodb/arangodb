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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Aql/AqlValue.h"
#include "Aql/AqlValueMaterializer.h"
#include "Aql/AstNode.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Function.h"
#include "Aql/Functions.h"
#include "Basics/Result.h"
#include "Geo/Ellipsoid.h"
#include "Geo/GeoJson.h"
#include "Geo/ShapeContainer.h"
#include "Geo/Utils.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <s2/s2loop.h>
#include <s2/s2polygon.h>

using namespace arangodb;

namespace arangodb::aql {
namespace {
AqlValue geoContainsIntersect(
    ExpressionContext* expressionContext, AstNode const&,
    aql::functions::VPackFunctionParametersView parameters, char const* func,
    bool contains) {
  auto* vopts = &expressionContext->trx().vpackOptions();
  AqlValue const& p1 =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  AqlValue const& p2 =
      aql::functions::extractFunctionParameterValue(parameters, 1);

  if (!p1.isObject()) {
    aql::functions::registerWarning(
        expressionContext, func,
        Result(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
               "Expecting GeoJSON object"));
    return AqlValue(AqlValueHintNull());
  }

  AqlValueMaterializer mat1(vopts);
  geo::ShapeContainer outer, inner;
  auto res = geo::json::parseRegion(mat1.slice(p1), outer,
                                    /*legacy=*/false);
  if (res.fail()) {
    aql::functions::registerWarning(expressionContext, func, res);
    return AqlValue(AqlValueHintNull());
  }
  if (contains && !outer.isAreaType()) {
    aql::functions::registerWarning(
        expressionContext, func,
        Result(
            TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
            "Only Polygon and MultiPolygon types are valid as first argument"));
    return AqlValue(AqlValueHintNull());
  }

  AqlValueMaterializer mat2(vopts);
  if (p2.isArray()) {
    res = geo::json::parseCoordinates<true>(mat2.slice(p2), inner,
                                            /*geoJson=*/true);
  } else {
    res = geo::json::parseRegion(mat2.slice(p2), inner,
                                 /*legacy=*/false);
  }
  if (res.fail()) {
    aql::functions::registerWarning(expressionContext, func, res);
    return AqlValue(AqlValueHintNull());
  }

  bool result;
  try {
    result = contains ? outer.contains(inner) : outer.intersects(inner);
    return AqlValue(AqlValueHintBool(result));
  } catch (basics::Exception const& ex) {
    res.reset(ex.code(), ex.what());
    aql::functions::registerWarning(expressionContext, func, res);
    return AqlValue(AqlValueHintBool(false));
  }
}

static Result parseGeoPolygon(VPackSlice polygon, VPackBuilder& b) {
  // check if nested or not
  bool unnested = false;
  for (VPackSlice v : VPackArrayIterator(polygon)) {
    if (v.isArray() && v.length() == 2) {
      unnested = true;
    }
  }

  if (unnested) {
    b.openArray();
  }

  if (!polygon.isArray()) {
    return Result(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                  "Polygon needs to be an array of positions.");
  }

  for (VPackSlice v : VPackArrayIterator(polygon)) {
    if (v.isArray() && v.length() > 2) {
      b.openArray();
      for (VPackSlice const coord : VPackArrayIterator(v)) {
        if (coord.isNumber()) {
          b.add(VPackValue(coord.getNumber<double>()));
        } else if (coord.isArray()) {
          if (coord.length() < 2) {
            return Result(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                          "a Position needs at least two numeric values");
          } else {
            b.openArray();
            for (VPackSlice const innercord : VPackArrayIterator(coord)) {
              if (innercord.isNumber()) {
                b.add(VPackValue(innercord.getNumber<double>()));  // TODO
              } else if (innercord.isArray() && innercord.length() == 2) {
                if (innercord.at(0).isNumber() && innercord.at(1).isNumber()) {
                  b.openArray();
                  b.add(VPackValue(innercord.at(0).getNumber<double>()));
                  b.add(VPackValue(innercord.at(1).getNumber<double>()));
                  b.close();
                } else {
                  return Result(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                                "coordinate is not a number");
                }
              } else {
                return Result(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                              "not an array describing a position");
              }
            }
            b.close();
          }
        } else {
          return Result(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                        "not an array containing positions");
        }
      }
      b.close();
    } else if (v.isArray() && v.length() == 2) {
      if (polygon.length() > 2) {
        b.openArray();
        for (VPackSlice const innercord : VPackArrayIterator(v)) {
          if (innercord.isNumber()) {
            b.add(VPackValue(innercord.getNumber<double>()));
          } else if (innercord.isArray() && innercord.length() == 2) {
            if (innercord.at(0).isNumber() && innercord.at(1).isNumber()) {
              b.openArray();
              b.add(VPackValue(innercord.at(0).getNumber<double>()));
              b.add(VPackValue(innercord.at(1).getNumber<double>()));
              b.close();
            } else {
              return Result(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                            "coordinate is not a number");
            }
          } else {
            return Result(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                          "not a numeric value");
          }
        }
        b.close();
      } else {
        return Result(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                      "a Polygon needs at least three positions");
      }
    } else {
      return Result(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                    "not an array containing positions");
    }
  }

  if (unnested) {
    b.close();
  }

  return {TRI_ERROR_NO_ERROR};
}

Result parseShape(ExpressionContext* exprCtx, AqlValue const& value,
                  geo::ShapeContainer& shape) {
  auto* vopts = &exprCtx->trx().vpackOptions();
  AqlValueMaterializer mat(vopts);

  if (value.isArray()) {
    return geo::json::parseCoordinates<true>(mat.slice(value), shape,
                                             /*geoJson=*/true);
  }
  return geo::json::parseRegion(mat.slice(value), shape,
                                /*legacy=*/false);
}

geo::Ellipsoid const* detEllipsoid(ExpressionContext* expressionContext,
                                   AqlValue const& p,
                                   std::string_view functionName) {
  if (auto slice = p.slice(); slice.isString()) {
    if (auto ell = geo::utils::ellipsoidFromString(slice.stringView()); ell) {
      return ell;
    }
    aql::functions::registerWarning(
        expressionContext, functionName,
        Result(TRI_ERROR_BAD_PARAMETER,
               fmt::format(
                   "invalid ellipsoid {} specified, falling back to \"sphere\"",
                   slice.stringView())));
  } else {
    aql::functions::registerWarning(
        expressionContext, functionName,
        Result(TRI_ERROR_BAD_PARAMETER,
               "invalid value for ellipsoid specified, falling "
               "back to \"sphere\""));
  }
  return &geo::SPHERE;
}

}  // namespace

/// @brief function DISTANCE
AqlValue functions::Distance(ExpressionContext* expressionContext,
                             AstNode const&,
                             VPackFunctionParametersView parameters) {
  static char const* AFN = "DISTANCE";

  AqlValue const& lat1 =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  AqlValue const& lon1 =
      aql::functions::extractFunctionParameterValue(parameters, 1);
  AqlValue const& lat2 =
      aql::functions::extractFunctionParameterValue(parameters, 2);
  AqlValue const& lon2 =
      aql::functions::extractFunctionParameterValue(parameters, 3);

  // non-numeric input...
  if (!lat1.isNumber() || !lon1.isNumber() || !lat2.isNumber() ||
      !lon2.isNumber()) {
    registerWarning(expressionContext, AFN,
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  bool failed;
  bool error = false;
  double lat1Value = lat1.toDouble(failed);
  error |= failed;
  double lon1Value = lon1.toDouble(failed);
  error |= failed;
  double lat2Value = lat2.toDouble(failed);
  error |= failed;
  double lon2Value = lon2.toDouble(failed);
  error |= failed;

  if (error) {
    registerWarning(expressionContext, AFN,
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  auto toRadians = [](double degrees) -> double {
    return degrees * (std::acos(-1.0) / 180.0);
  };

  double p1 = toRadians(lat1Value);
  double p2 = toRadians(lat2Value);
  double d1 = toRadians(lat2Value - lat1Value);
  double d2 = toRadians(lon2Value - lon1Value);

  double a =
      std::sin(d1 / 2.0) * std::sin(d1 / 2.0) +
      std::cos(p1) * std::cos(p2) * std::sin(d2 / 2.0) * std::sin(d2 / 2.0);

  double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
  double const EARTHRADIAN = 6371000.0;  // metres

  return numberValue(EARTHRADIAN * c, true);
}

/// @brief function GEO_DISTANCE
AqlValue functions::GeoDistance(ExpressionContext* exprCtx, AstNode const&,
                                VPackFunctionParametersView parameters) {
  constexpr char const AFN[] = "GEO_DISTANCE";
  geo::ShapeContainer shape1, shape2;

  auto res = parseShape(
      exprCtx, aql::functions::extractFunctionParameterValue(parameters, 0),
      shape1);

  if (res.fail()) {
    registerWarning(exprCtx, AFN, res);
    return AqlValue(AqlValueHintNull());
  }

  res = parseShape(exprCtx,
                   aql::functions::extractFunctionParameterValue(parameters, 1),
                   shape2);

  if (res.fail()) {
    registerWarning(exprCtx, AFN, res);
    return AqlValue(AqlValueHintNull());
  }

  if (parameters.size() > 2) {
    auto e = detEllipsoid(exprCtx, parameters[2], AFN);
    return numberValue(shape1.distanceFromCentroid(shape2.centroid(), *e),
                       true);
  }
  return numberValue(shape1.distanceFromCentroid(shape2.centroid()), true);
}

/// @brief function GEO_IN_RANGE
AqlValue functions::GeoInRange(ExpressionContext* ctx, AstNode const& node,
                               VPackFunctionParametersView args) {
  TRI_ASSERT(ctx);
  TRI_ASSERT(aql::NODE_TYPE_FCALL == node.type);

  auto const* impl = static_cast<aql::Function*>(node.getData());
  TRI_ASSERT(impl);

  auto const argc = args.size();

  if (argc < 4 || argc > 7) {
    registerWarning(ctx, impl->name.c_str(),
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  geo::ShapeContainer shape1, shape2;
  auto res = parseShape(
      ctx, aql::functions::extractFunctionParameterValue(args, 0), shape1);

  if (res.fail()) {
    registerWarning(ctx, impl->name.c_str(), res);
    return AqlValue(AqlValueHintNull());
  }

  res = parseShape(ctx, aql::functions::extractFunctionParameterValue(args, 1),
                   shape2);

  if (res.fail()) {
    registerWarning(ctx, impl->name.c_str(), res);
    return AqlValue(AqlValueHintNull());
  }

  auto const& lowerBound =
      aql::functions::extractFunctionParameterValue(args, 2);

  if (!lowerBound.isNumber()) {
    registerWarning(
        ctx, impl->name.c_str(),
        {TRI_ERROR_BAD_PARAMETER, "3rd argument requires a number"});
    return AqlValue(AqlValueHintNull());
  }

  auto const& upperBound =
      aql::functions::extractFunctionParameterValue(args, 3);

  if (!upperBound.isNumber()) {
    registerWarning(
        ctx, impl->name.c_str(),
        {TRI_ERROR_BAD_PARAMETER, "4th argument requires a number"});
    return AqlValue(AqlValueHintNull());
  }

  bool includeLower = true;
  bool includeUpper = true;
  geo::Ellipsoid const* ellipsoid = &geo::SPHERE;

  if (argc > 4) {
    auto const& includeLowerValue =
        aql::functions::extractFunctionParameterValue(args, 4);

    if (!includeLowerValue.isBoolean()) {
      registerWarning(
          ctx, impl->name.c_str(),
          {TRI_ERROR_BAD_PARAMETER, "5th argument requires a bool"});
      return AqlValue(AqlValueHintNull());
    }

    includeLower = includeLowerValue.toBoolean();

    if (argc > 5) {
      auto const& includeUpperValue =
          aql::functions::extractFunctionParameterValue(args, 5);

      if (!includeUpperValue.isBoolean()) {
        registerWarning(
            ctx, impl->name.c_str(),
            {TRI_ERROR_BAD_PARAMETER, "6th argument requires a bool"});
        return AqlValue(AqlValueHintNull());
      }

      includeUpper = includeUpperValue.toBoolean();
    }

    if (argc > 6) {
      auto const& value =
          aql::functions::extractFunctionParameterValue(args, 6);
      ellipsoid = detEllipsoid(ctx, value, impl->name);
    }
  }

  auto const minDistance = lowerBound.toDouble();
  auto const maxDistance = upperBound.toDouble();
  auto const distance =
      (ellipsoid == &geo::SPHERE
           ? shape1.distanceFromCentroid(shape2.centroid())
           : shape1.distanceFromCentroid(shape2.centroid(), *ellipsoid));

  return AqlValue{AqlValueHintBool{
      (includeLower ? distance >= minDistance : distance > minDistance) &&
      (includeUpper ? distance <= maxDistance : distance < maxDistance)}};
}

/// @brief function GEO_CONTAINS
AqlValue functions::GeoContains(ExpressionContext* expressionContext,
                                AstNode const& node,
                                VPackFunctionParametersView parameters) {
  return geoContainsIntersect(expressionContext, node, parameters,
                              "GEO_CONTAINS", true);
}

/// @brief function GEO_INTERSECTS
AqlValue functions::GeoIntersects(ExpressionContext* expressionContext,
                                  AstNode const& node,
                                  VPackFunctionParametersView parameters) {
  return geoContainsIntersect(expressionContext, node, parameters,
                              "GEO_INTERSECTS", false);
}

/// @brief function GEO_EQUALS
AqlValue functions::GeoEquals(ExpressionContext* expressionContext,
                              AstNode const&,
                              VPackFunctionParametersView parameters) {
  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  AqlValue p1 = aql::functions::extractFunctionParameterValue(parameters, 0);
  AqlValue p2 = aql::functions::extractFunctionParameterValue(parameters, 1);

  if (!p1.isObject() || !p2.isObject()) {
    registerWarning(expressionContext, "GEO_EQUALS",
                    Result(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                           "Expecting GeoJSON object"));
    return AqlValue(AqlValueHintNull());
  }

  AqlValueMaterializer mat1(vopts);
  AqlValueMaterializer mat2(vopts);

  geo::ShapeContainer first, second;
  auto res1 = geo::json::parseRegion(mat1.slice(p1), first,
                                     /*legacy=*/false);
  auto res2 = geo::json::parseRegion(mat2.slice(p2), second,
                                     /*legacy=*/false);

  if (res1.fail()) {
    registerWarning(expressionContext, "GEO_EQUALS", res1);
    return AqlValue(AqlValueHintNull());
  }
  if (res2.fail()) {
    registerWarning(expressionContext, "GEO_EQUALS", res2);
    return AqlValue(AqlValueHintNull());
  }

  bool result = first.equals(second);
  return AqlValue(AqlValueHintBool(result));
}

/// @brief function GEO_AREA
AqlValue functions::GeoArea(ExpressionContext* expressionContext,
                            AstNode const&,
                            VPackFunctionParametersView parameters) {
  std::string_view const functionName = "GEO_AREA";
  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  AqlValue p1 = aql::functions::extractFunctionParameterValue(parameters, 0);
  AqlValue p2 = aql::functions::extractFunctionParameterValue(parameters, 1);

  AqlValueMaterializer mat(vopts);

  geo::ShapeContainer shape;
  auto res = geo::json::parseRegion(mat.slice(p1), shape,
                                    /*legacy=*/false);

  if (res.fail()) {
    registerWarning(expressionContext, functionName, res);
    return AqlValue(AqlValueHintNull());
  }

  return AqlValue(AqlValueHintDouble(
      shape.area(*detEllipsoid(expressionContext, p2, functionName))));
}

/// @brief function IS_IN_POLYGON
AqlValue functions::IsInPolygon(ExpressionContext* expressionContext,
                                AstNode const&,
                                VPackFunctionParametersView parameters) {
  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  AqlValue const& coords =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  AqlValue p2 = aql::functions::extractFunctionParameterValue(parameters, 1);
  AqlValue p3 = aql::functions::extractFunctionParameterValue(parameters, 2);

  if (!coords.isArray()) {
    registerWarning(expressionContext, "IS_IN_POLYGON",
                    TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  double latitude, longitude;
  bool geoJson = false;
  if (p2.isArray()) {
    if (p2.length() < 2) {
      registerInvalidArgumentWarning(expressionContext, "IS_IN_POLYGON");
      return AqlValue(AqlValueHintNull());
    }
    AqlValueMaterializer materializer(vopts);
    VPackSlice arr = materializer.slice(p2);
    geoJson = p3.isBoolean() && p3.toBoolean();
    // if geoJson, map [lon, lat] -> lat, lon
    VPackSlice lat = geoJson ? arr[1] : arr[0];
    VPackSlice lon = geoJson ? arr[0] : arr[1];
    if (!lat.isNumber() || !lon.isNumber()) {
      registerInvalidArgumentWarning(expressionContext, "IS_IN_POLYGON");
      return AqlValue(AqlValueHintNull());
    }
    latitude = lat.getNumber<double>();
    longitude = lon.getNumber<double>();
  } else if (p2.isNumber() && p3.isNumber()) {
    bool failed1 = false, failed2 = false;
    latitude = p2.toDouble(failed1);
    longitude = p3.toDouble(failed2);
    if (failed1 || failed2) {
      registerInvalidArgumentWarning(expressionContext, "IS_IN_POLYGON");
      return AqlValue(AqlValueHintNull());
    }
  } else {
    registerInvalidArgumentWarning(expressionContext, "IS_IN_POLYGON");
    return AqlValue(AqlValueHintNull());
  }

  S2Loop loop;
  loop.set_s2debug_override(S2Debug::DISABLE);
  auto res = geo::json::parseLoop(coords.slice(), loop, geoJson);
  if (res.fail() || !loop.IsValid()) {
    registerWarning(expressionContext, "IS_IN_POLYGON", res);
    return AqlValue(AqlValueHintNull());
  }

  S2LatLng latLng = S2LatLng::FromDegrees(latitude, longitude).Normalized();
  return AqlValue(AqlValueHintBool(loop.Contains(latLng.ToPoint())));
}

/// @brief geo constructors

/// @brief function GEO_POINT
AqlValue functions::GeoPoint(ExpressionContext* expressionContext,
                             AstNode const&,
                             VPackFunctionParametersView parameters) {
  transaction::Methods* trx = &expressionContext->trx();
  size_t const n = parameters.size();

  if (n < 2) {
    // no parameters
    return AqlValue(AqlValueHintNull());
  }

  AqlValue lon1 = aql::functions::extractFunctionParameterValue(parameters, 0);
  AqlValue lat1 = aql::functions::extractFunctionParameterValue(parameters, 1);

  // non-numeric input
  if (!lat1.isNumber() || !lon1.isNumber()) {
    registerWarning(expressionContext, "GEO_POINT",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  bool failed;
  bool error = false;
  double lon1Value = lon1.toDouble(failed);
  error |= failed;
  double lat1Value = lat1.toDouble(failed);
  error |= failed;

  if (error) {
    registerWarning(expressionContext, "GEO_POINT",
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  transaction::BuilderLeaser builder(trx);
  builder->openObject();
  builder->add("type", VPackValue("Point"));
  builder->add("coordinates", VPackValue(VPackValueType::Array));
  builder->add(VPackValue(lon1Value));
  builder->add(VPackValue(lat1Value));
  builder->close();
  builder->close();

  return AqlValue(builder->slice(), builder->size());
}

/// @brief function GEO_MULTIPOINT
AqlValue functions::GeoMultiPoint(ExpressionContext* expressionContext,
                                  AstNode const&,
                                  VPackFunctionParametersView parameters) {
  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  size_t const n = parameters.size();

  if (n < 1) {
    // no parameters
    return AqlValue(AqlValueHintNull());
  }

  AqlValue const& geoArray =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!geoArray.isArray()) {
    registerWarning(expressionContext, "GEO_MULTIPOINT",
                    TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }
  if (geoArray.length() < 2) {
    registerWarning(expressionContext, "GEO_MULTIPOINT",
                    Result(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                           "a MultiPoint needs at least two positions"));
    return AqlValue(AqlValueHintNull());
  }

  transaction::BuilderLeaser builder(trx);

  builder->openObject();
  builder->add("type", VPackValue("MultiPoint"));
  builder->add("coordinates", VPackValue(VPackValueType::Array));

  AqlValueMaterializer materializer(vopts);
  VPackSlice s = materializer.slice(geoArray);
  for (VPackSlice v : VPackArrayIterator(s)) {
    if (v.isArray()) {
      builder->openArray();
      for (auto const& coord : VPackArrayIterator(v)) {
        if (coord.isNumber()) {
          builder->add(VPackValue(coord.getNumber<double>()));
        } else {
          registerWarning(
              expressionContext, "GEO_MULTIPOINT",
              Result(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                     "not a numeric value"));
          return AqlValue(AqlValueHintNull());
        }
      }
      builder->close();
    } else {
      registerWarning(expressionContext, "GEO_MULTIPOINT",
                      Result(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                             "not an array containing positions"));
      return AqlValue(AqlValueHintNull());
    }
  }

  builder->close();
  builder->close();

  return AqlValue(builder->slice(), builder->size());
}

/// @brief function GEO_POLYGON
AqlValue functions::GeoPolygon(ExpressionContext* expressionContext,
                               AstNode const&,
                               VPackFunctionParametersView parameters) {
  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  size_t const n = parameters.size();

  if (n < 1) {
    // no parameters
    return AqlValue(AqlValueHintNull());
  }

  AqlValue const& geoArray =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!geoArray.isArray()) {
    registerWarning(expressionContext, "GEO_POLYGON",
                    TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  transaction::BuilderLeaser builder(trx);
  builder->openObject();
  builder->add("type", VPackValue("Polygon"));
  builder->add("coordinates", VPackValue(VPackValueType::Array));

  AqlValueMaterializer materializer(vopts);
  VPackSlice s = materializer.slice(geoArray);

  Result res = parseGeoPolygon(s, *builder.get());
  if (res.fail()) {
    registerWarning(expressionContext, "GEO_POLYGON", res);
    return AqlValue(AqlValueHintNull());
  }

  builder->close();  // coordinates
  builder->close();  // object

  // Now actually parse the result with S2:
  S2Polygon polygon;
  res = geo::json::parsePolygon(builder->slice(), polygon);
  if (res.fail()) {
    registerWarning(expressionContext, "GEO_POLYGON", res);
    return AqlValue(AqlValueHintNull());
  }

  return AqlValue(builder->slice(), builder->size());
}

/// @brief function GEO_MULTIPOLYGON
AqlValue functions::GeoMultiPolygon(ExpressionContext* expressionContext,
                                    AstNode const&,
                                    VPackFunctionParametersView parameters) {
  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  size_t const n = parameters.size();

  if (n < 1) {
    // no parameters
    return AqlValue(AqlValueHintNull());
  }

  AqlValue const& geoArray =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!geoArray.isArray()) {
    registerWarning(expressionContext, "GEO_MULTIPOLYGON",
                    TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }

  AqlValueMaterializer materializer(vopts);
  VPackSlice s = materializer.slice(geoArray);

  /*
  return GEO_MULTIPOLYGON([
    [
      [[40, 40], [20, 45], [45, 30], [40, 40]]
    ],
    [
      [[20, 35], [10, 30], [10, 10], [30, 5], [45, 20], [20, 35]],
      [[30, 20], [20, 15], [20, 25], [30, 20]]
    ]
  ])
  */

  TRI_ASSERT(s.isArray());
  if (s.length() < 2) {
    registerWarning(
        expressionContext, "GEO_MULTIPOLYGON",
        Result(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
               "a MultiPolygon needs at least two Polygons inside."));
    return AqlValue(AqlValueHintNull());
  }

  transaction::BuilderLeaser builder(trx);
  builder->openObject();
  builder->add("type", VPackValue("MultiPolygon"));
  builder->add("coordinates", VPackValue(VPackValueType::Array));

  for (auto const& arrayOfPolygons : VPackArrayIterator(s)) {
    if (!arrayOfPolygons.isArray()) {
      registerWarning(
          expressionContext, "GEO_MULTIPOLYGON",
          Result(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                 "a MultiPolygon needs at least two Polygons inside."));
      return AqlValue(AqlValueHintNull());
    }
    builder->openArray();  // arrayOfPolygons
    for (VPackSlice v : VPackArrayIterator(arrayOfPolygons)) {
      Result res = parseGeoPolygon(v, *builder.get());
      if (res.fail()) {
        registerWarning(expressionContext, "GEO_MULTIPOLYGON", res);
        return AqlValue(AqlValueHintNull());
      }
    }
    builder->close();  // arrayOfPolygons close
  }

  builder->close();
  builder->close();

  // Now actually parse the result with S2:
  S2Polygon polygon;
  auto res = geo::json::parseMultiPolygon(builder->slice(), polygon);
  if (res.fail()) {
    registerWarning(expressionContext, "GEO_MULTIPOLYGON", res);
    return AqlValue(AqlValueHintNull());
  }

  return AqlValue(builder->slice(), builder->size());
}

/// @brief function GEO_LINESTRING
AqlValue functions::GeoLinestring(ExpressionContext* expressionContext,
                                  AstNode const&,
                                  VPackFunctionParametersView parameters) {
  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  size_t const n = parameters.size();

  if (n < 1) {
    // no parameters
    return AqlValue(AqlValueHintNull());
  }

  AqlValue const& geoArray =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!geoArray.isArray()) {
    registerWarning(expressionContext, "GEO_LINESTRING",
                    TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }
  if (geoArray.length() < 2) {
    registerWarning(expressionContext, "GEO_LINESTRING",
                    Result(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                           "a LineString needs at least two positions"));
    return AqlValue(AqlValueHintNull());
  }

  transaction::BuilderLeaser builder(trx);

  builder->add(VPackValue(VPackValueType::Object));
  builder->add("type", VPackValue("LineString"));
  builder->add("coordinates", VPackValue(VPackValueType::Array));

  AqlValueMaterializer materializer(vopts);
  VPackSlice s = materializer.slice(geoArray);
  for (VPackSlice v : VPackArrayIterator(s)) {
    if (v.isArray()) {
      builder->openArray();
      for (auto const& coord : VPackArrayIterator(v)) {
        if (coord.isNumber()) {
          builder->add(VPackValue(coord.getNumber<double>()));
        } else {
          registerWarning(
              expressionContext, "GEO_LINESTRING",
              Result(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                     "not a numeric value"));
          return AqlValue(AqlValueHintNull());
        }
      }
      builder->close();
    } else {
      registerWarning(expressionContext, "GEO_LINESTRING",
                      Result(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                             "not an array containing positions"));
      return AqlValue(AqlValueHintNull());
    }
  }

  builder->close();
  builder->close();

  return AqlValue(builder->slice(), builder->size());
}

/// @brief function GEO_MULTILINESTRING
AqlValue functions::GeoMultiLinestring(ExpressionContext* expressionContext,
                                       AstNode const&,
                                       VPackFunctionParametersView parameters) {
  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  size_t const n = parameters.size();

  if (n < 1) {
    // no parameters
    return AqlValue(AqlValueHintNull());
  }

  AqlValue const& geoArray =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!geoArray.isArray()) {
    registerWarning(expressionContext, "GEO_MULTILINESTRING",
                    TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(AqlValueHintNull());
  }
  if (geoArray.length() < 1) {
    registerWarning(
        expressionContext, "GEO_MULTILINESTRING",
        Result(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
               "a MultiLineString needs at least one array of linestrings"));
    return AqlValue(AqlValueHintNull());
  }

  transaction::BuilderLeaser builder(trx);

  builder->add(VPackValue(VPackValueType::Object));
  builder->add("type", VPackValue("MultiLineString"));
  builder->add("coordinates", VPackValue(VPackValueType::Array));

  AqlValueMaterializer materializer(vopts);
  VPackSlice s = materializer.slice(geoArray);
  for (VPackSlice v : VPackArrayIterator(s)) {
    if (v.isArray()) {
      if (v.length() > 1) {
        builder->openArray();
        for (VPackSlice const inner : VPackArrayIterator(v)) {
          if (inner.isArray()) {
            builder->openArray();
            for (VPackSlice const coord : VPackArrayIterator(inner)) {
              if (coord.isNumber()) {
                builder->add(VPackValue(coord.getNumber<double>()));
              } else {
                registerWarning(
                    expressionContext, "GEO_MULTILINESTRING",
                    Result(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                           "not a numeric value"));
                return AqlValue(AqlValueHintNull());
              }
            }
            builder->close();
          } else {
            registerWarning(
                expressionContext, "GEO_MULTILINESTRING",
                Result(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                       "not an array containing positions"));
            return AqlValue(AqlValueHintNull());
          }
        }
        builder->close();
      } else {
        registerWarning(expressionContext, "GEO_MULTILINESTRING",
                        Result(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                               "not an array containing linestrings"));
        return AqlValue(AqlValueHintNull());
      }
    } else {
      registerWarning(expressionContext, "GEO_MULTILINESTRING",
                      Result(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                             "not an array containing positions"));
      return AqlValue(AqlValueHintNull());
    }
  }

  builder->close();
  builder->close();

  return AqlValue(builder->slice(), builder->size());
}

}  // namespace arangodb::aql
