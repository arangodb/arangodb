////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "Geo.h"
#include "GeoParser.h"

#include "Basics/VelocyPackHelper.h"
#include "Logger/Logger.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "Utils/CollectionNameResolver.h"

#include "StorageEngine/EngineSelectorFeature.h"
#include "MMFiles/MMFilesAqlFunctions.h"
#include "MMFiles/MMFilesGeoIndex.h"
#include "RocksDBEngine/RocksDBAqlFunctions.h"
#include "RocksDBEngine/RocksDBFulltextIndex.h"
#include "RocksDBEngine/RocksDBGeoIndex.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <geometry/s2.h>
#include <geometry/s2loop.h>
#include <geometry/s2polygon.h>
#include <geometry/strings/split.h>
#include <geometry/strings/strutil.h>

#include <vector>
using std::vector;

#include <string>
using std::string;

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;

class S2Polygon;

bool Geo::contains(const AqlValue geoJSONA, const AqlValue geoJSONB) {
  GeoParser gp;
  // verify if object is in geojson format
  if (!gp.parseGeoJSONType(geoJSONA) || !gp.parseGeoJSONType(geoJSONB)) {
    // TODO: add invalid geo json error
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_GRAPH_INVALID_GRAPH, "Invalid GeoJSON format.");
  }

  if (gp.parseGeoJSONTypePolygon(geoJSONA) && gp.parseGeoJSONTypePolygon(geoJSONB)) {
    return polygonContainsPolygon(geoJSONA, geoJSONB);
  } else {
    // TODO: add invalid geo json error
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_GRAPH_INVALID_GRAPH, "Invalid GeoJSON format.");
  }
};

/// @brief function polygon contained in polygon
bool Geo::polygonContainsPolygon(const AqlValue geoJSONA, const AqlValue geoJSONB) {
  GeoParser gp;
  bool result;
  
  //  polygon
  S2Polygon* polyA = gp.parseGeoJSONPolygon(geoJSONA);
  S2Polygon* polyB = gp.parseGeoJSONPolygon(geoJSONB);

  result = polyA->Contains(polyB);
  return result;
};

/// @brief function point contained in polygon
bool Geo::polygonContainsPoint(const AqlValue geoJSONA, const AqlValue geoJSONB) {
  GeoParser gp;
  bool result;
  
  S2Polygon* poly = gp.parseGeoJSONPolygon(geoJSONA);
  S2Point point = gp.parseGeoJSONPoint(geoJSONB);

  result = poly->Contains(point);
  return result;
};

/// @brief function main equals function which detects types and moves forward with specific function
bool Geo::equals(const AqlValue geoJSONA, const AqlValue geoJSONB, transaction::Methods* trx) {
  GeoParser gp;
  // verify if object is in geojson format
  if (!gp.parseGeoJSONType(geoJSONA) || !gp.parseGeoJSONType(geoJSONB)) {
    // TODO: add invalid geo json error
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_GRAPH_INVALID_GRAPH, "Invalid GeoJSON type.");
  }

  if (gp.parseGeoJSONTypePolygon(geoJSONA) && gp.parseGeoJSONTypePolygon(geoJSONB)) {
    return polygonEqualsPolygon(geoJSONA, geoJSONB);
  } else if (gp.parseGeoJSONTypePoint(geoJSONA) && gp.parseGeoJSONTypePoint(geoJSONB)) {
    return pointEqualsPoint(geoJSONA, geoJSONB, trx);
  } else if (gp.parseGeoJSONTypePolyline(geoJSONA) && gp.parseGeoJSONTypePolyline(geoJSONB)) {
    return polylineEqualsPolyline(geoJSONA, geoJSONB);
  } else {
    // TODO: add invalid geo json error
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_GRAPH_INVALID_GRAPH, "GeoJSON types differ.");
  }
};

// return all points available which are inside of the given polygon
AqlValue Geo::helperPointsInPolygon(const AqlValue collectionName, const AqlValue geoJSONA, transaction::Methods* trx) {
  GeoParser gp;
  // verify if object is in geojson format
  if (!gp.parseGeoJSONType(geoJSONA)) {
    // TODO: add invalid geo json error
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_GRAPH_INVALID_GRAPH, "Invalid GeoJSON type.");
  }

  if (gp.parseGeoJSONTypePolygon(geoJSONA)) {
    return pointsInPolygon(collectionName, geoJSONA, trx);
  } else {
    // TODO: add invalid geo json error
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_GRAPH_INVALID_GRAPH, "Invalid GeoJSON polygon.");
  }
};

bool Geo::pointEqualsPoint(const AqlValue geoJSONA, const AqlValue geoJSONB, transaction::Methods* trx) {
  //GeoParser gp;
  //S2Point pointA = gp.parseGeoJSONPoint(geoJSONA);
  //S2Point pointB = gp.parseGeoJSONPoint(geoJSONB);

  return !AqlValue::Compare(trx, geoJSONA, geoJSONB, true);
};

bool Geo::polygonEqualsPolygon(const AqlValue geoJSONA, const AqlValue geoJSONB) {
  GeoParser gp;
  S2Polygon* polyA = gp.parseGeoJSONPolygon(geoJSONA);
  S2Polygon* polyB = gp.parseGeoJSONPolygon(geoJSONB);

  return polyA->BoundaryEquals(polyB);
};

bool Geo::polylineEqualsPolyline(const AqlValue geoJSONA, const AqlValue geoJSONB) {
  GeoParser gp;
  S2Polyline* polylineA = gp.parseGeoJSONPolyline(geoJSONA);
  S2Polyline* polylineB = gp.parseGeoJSONPolyline(geoJSONB);

  return polylineA->ApproxEquals(polylineB);
};

// main distance function which detects types and moves forward with specific function
AqlValue Geo::distance(const AqlValue geoJSONA, const AqlValue geoJSONB, transaction::Methods* trx) {
  GeoParser gp;
  if (gp.parseGeoJSONTypePoint(geoJSONA) && gp.parseGeoJSONTypePolygon(geoJSONB)) {
    return distancePointToPolygon(geoJSONA, geoJSONB);
  } else if (gp.parseGeoJSONTypePolygon(geoJSONA) && gp.parseGeoJSONTypePoint(geoJSONB)) {
    return distancePointToPolygon(geoJSONB, geoJSONA);
  } else if (gp.parseGeoJSONTypePoint(geoJSONA) && gp.parseGeoJSONTypePoint(geoJSONB)) {
    return distancePointToPoint(geoJSONA, geoJSONB, trx);
  } else {
    // TODO: add invalid geo json error
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_GRAPH_INVALID_GRAPH, "Invalid GeoJSON polygon.");
  }
};

// returns the distance of a point to a polygon
AqlValue Geo::distancePointToPolygon(const AqlValue geoJSONA, const AqlValue geoJSONB) {
  GeoParser gp;
  S2Polygon* poly = gp.parseGeoJSONPolygon(geoJSONB);
  S2Point point = gp.parseGeoJSONPoint(geoJSONA);

  S1Angle d = S1Angle(poly->Project(point), point);
  LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "result in (I): " << d.degrees();
  return AqlValue(AqlValueHintDouble(d.degrees() * 10000));
};

// returns the distance of a point to a point
AqlValue Geo::distancePointToPoint(const AqlValue geoJSONA, const AqlValue geoJSONB, transaction::Methods* trx) {
  GeoParser gp;
  Geo g;

  if (g.pointEqualsPoint(geoJSONA, geoJSONB, trx)) {
    return AqlValue(AqlValueHintDouble(0.0));
  } else {
    S2Point pointA = gp.parseGeoJSONPoint(geoJSONA);
    S2Point pointB = gp.parseGeoJSONPoint(geoJSONB);

    S2LatLng x = S2LatLng(pointA);
    S2LatLng y = S2LatLng(pointB);

    return AqlValue(AqlValueHintDouble(x.GetDistance(y).Normalized().degrees() * 10000));
  }
};

arangodb::RocksDBGeoIndex* getRocksDBGeoIndex(
    transaction::Methods* trx, TRI_voc_cid_t const& cid,
    std::string const& collectionName) {
  // NOTE:
  // Due to trx lock the shared_index stays valid
  // as long as trx stays valid.
  // It is save to return the Raw pointer.
  // It can only be used until trx is finished.
  trx->addCollectionAtRuntime(cid, collectionName);
  Result res = trx->state()->ensureCollections();
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(res.errorNumber(), res.errorMessage());
  }

  auto document = trx->documentCollection(cid);
  if (document == nullptr) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, "'%s'",
                                  collectionName.c_str());
  }

  arangodb::RocksDBGeoIndex* index = nullptr;
  for (auto const& idx : document->getIndexes()) {
    if (idx->type() == arangodb::Index::TRI_IDX_TYPE_GEO1_INDEX ||
        idx->type() == arangodb::Index::TRI_IDX_TYPE_GEO2_INDEX) {
      index = static_cast<arangodb::RocksDBGeoIndex*>(idx.get());
      break;
    }
  }

  if (index == nullptr) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_GEO_INDEX_MISSING,
                                  collectionName.c_str());
  }

  trx->pinData(cid);
  return index;
}

/// @brief Load geoindex for collection name
arangodb::MMFilesGeoIndex* getMMFilesGeoIndex(
    transaction::Methods* trx, TRI_voc_cid_t const& cid,
    std::string const& collectionName) {
  // NOTE:
  // Due to trx lock the shared_index stays valid
  // as long as trx stays valid.
  // It is save to return the Raw pointer.
  // It can only be used until trx is finished.
  trx->addCollectionAtRuntime(cid, collectionName);

  // fetch document collection in transactional context
  auto document = trx->documentCollection(cid);

  // if not found, throw exception
  if (document == nullptr) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                  "'%s'", collectionName.c_str());
  }

  // create empty index object
  arangodb::MMFilesGeoIndex* index = nullptr;

  // look for available geo indices within document collection context
  for (auto const& idx : document->getIndexes()) {
    if (idx->type() == arangodb::Index::TRI_IDX_TYPE_GEO1_INDEX ||
        idx->type() == arangodb::Index::TRI_IDX_TYPE_GEO2_INDEX) {
      index = static_cast<arangodb::MMFilesGeoIndex*>(idx.get());
      break;
    }
  }

  // throw exception if no index found, currently this method only works
  // if a index is available. this will be extended by a full collection
  // scan in the future. This will be slower, but will return a result.
  if (index == nullptr) {
    THROW_ARANGO_EXCEPTION_PARAMS(TRI_ERROR_QUERY_GEO_INDEX_MISSING,
                                  collectionName.c_str());
  }

  trx->pinData(cid);

  return index;
}

AqlValue buildRocksDBGeoResult(transaction::Methods* trx,
                               LogicalCollection* collection,
                               rocksdbengine::GeoCoordinates* cors,
                               TRI_voc_cid_t const& cid,
                               S2Polygon* poly
                               ) {

  // if nullptr, return empty array
  if (cors == nullptr) {
    return AqlValue(arangodb::basics::VelocyPackHelper::EmptyArrayValue());
  }

  // if no results, return empty array
  size_t const nCoords = cors->length;
  if (nCoords == 0) {
    GeoIndex_CoordinatesFree(cors);
    return AqlValue(arangodb::basics::VelocyPackHelper::EmptyArrayValue());
  }

  // defines a geo struct with distance value
  struct geo_coordinate_distance_t {
    geo_coordinate_distance_t(double distance, LocalDocumentId token)
        : _distance(distance), _token(token) {}

    double _distance;
    LocalDocumentId _token;
  };

  std::vector<geo_coordinate_distance_t> distances;

  try {
    distances.reserve(nCoords);

    for (size_t i = 0; i < nCoords; ++i) {
      distances.emplace_back(geo_coordinate_distance_t(
          cors->distances[i],
          arangodb::MMFilesGeoIndex::toLocalDocumentId(
              cors->coordinates[i].data)));
    }
  } catch (...) {
    // in case of exception -> free geo index
    GeoIndex_CoordinatesFree(cors);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  // free geo index
  GeoIndex_CoordinatesFree(cors);

  // sort result by distance
  std::sort(distances.begin(), distances.end(),
      [](geo_coordinate_distance_t const& left,
        geo_coordinate_distance_t const& right) {
      return left._distance < right._distance;
      });

  try {
    ManagedDocumentResult mmdr;
    // create builder object within transaction context
    transaction::BuilderLeaser builder(trx);
    // open result array
    builder->openArray();

    AqlValueMaterializer materializer(trx);
    VPackSlice s;
    // Debug only. Prints the returned length of results
    // int LENGTH = 0;
    for (auto& it : distances) {
      if (collection->readDocument(trx, it._token, mmdr)) {
        VPackSlice doc(mmdr.vpack());
        if (doc.hasKey("coordinates")) {
          VPackSlice coordinates(doc.get("coordinates"));
          if (coordinates.at(0).isDouble() && coordinates.at(1).isDouble()) {
            S2Point point = S2LatLng::FromDegrees(coordinates.at(1).getDouble(), coordinates.at(0).getDouble()).Normalized().ToPoint();
            // if point is within poly, add object to result set
            if (poly->Contains(point)) {
              // Debug only. Prints the returned length of results
              // LENGTH = LENGTH + 1;
              mmdr.addToBuilder(*builder.get(), true);
            }
          }
        }
      }
    }
    // close result array
    builder->close();

    //Debug only. Prints the returned length of results
    //LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "LENGTH OF RESULT SET : " << LENGTH;

    // return calculated aql value
    return AqlValue(builder.get());
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

AqlValue buildMMFilesGeoResult(transaction::Methods* trx,
                               LogicalCollection* collection,
                               GeoCoordinates* cors,
                               TRI_voc_cid_t const& cid,
                               S2Polygon* poly
                               ) {

  // if nullptr, return empty array
  if (cors == nullptr) {
    return AqlValue(arangodb::basics::VelocyPackHelper::EmptyArrayValue());
  }

  // if no results, return empty array
  size_t const nCoords = cors->length;
  if (nCoords == 0) {
    GeoIndex_CoordinatesFree(cors);
    return AqlValue(arangodb::basics::VelocyPackHelper::EmptyArrayValue());
  }

  // defines a geo struct with distance value
  struct geo_coordinate_distance_t {
    geo_coordinate_distance_t(double distance, LocalDocumentId token)
        : _distance(distance), _token(token) {}

    double _distance;
    LocalDocumentId _token;
  };

  std::vector<geo_coordinate_distance_t> distances;

  try {
    distances.reserve(nCoords);

    for (size_t i = 0; i < nCoords; ++i) {
      distances.emplace_back(geo_coordinate_distance_t(
          cors->distances[i],
          arangodb::MMFilesGeoIndex::toLocalDocumentId(
              cors->coordinates[i].data)));
    }
  } catch (...) {
    // in case of exception -> free geo index
    GeoIndex_CoordinatesFree(cors);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  // free geo index
  GeoIndex_CoordinatesFree(cors);

  // sort result by distance
  std::sort(distances.begin(), distances.end(),
      [](geo_coordinate_distance_t const& left,
        geo_coordinate_distance_t const& right) {
      return left._distance < right._distance;
      });

  try {
    ManagedDocumentResult mmdr;
    // create builder object within transaction context
    transaction::BuilderLeaser builder(trx);
    // open result array
    builder->openArray();

    AqlValueMaterializer materializer(trx);
    VPackSlice s;
    // Debug only. Prints the returned length of results
    // int LENGTH = 0;
    for (auto& it : distances) {
      if (collection->readDocument(trx, it._token, mmdr)) {
        VPackSlice doc(mmdr.vpack());
        if (doc.hasKey("coordinates")) {
          VPackSlice coordinates(doc.get("coordinates"));
          if (coordinates.at(0).isDouble() && coordinates.at(1).isDouble()) {
            S2Point point = S2LatLng::FromDegrees(coordinates.at(1).getDouble(), coordinates.at(0).getDouble()).Normalized().ToPoint();
            // if point is within poly, add object to result set
            if (poly->Contains(point)) {
              // Debug only. Prints the returned length of results
              // LENGTH = LENGTH + 1;
              mmdr.addToBuilder(*builder.get(), true);
            }
          }
        }
      }
    }
    // close result array
    builder->close();

    //Debug only. Prints the returned length of results
    //LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "LENGTH OF RESULT SET : " << LENGTH;

    // return calculated aql value
    return AqlValue(builder.get());
  } catch (...) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

// Returns all available points within a Polygon
AqlValue Geo::pointsInPolygon(const AqlValue collectionName, const AqlValue geoJSONA, transaction::Methods* trx) {
  GeoParser gp;
  // collectionName: Collection name
  // geoJSONA: Must be type Polygon

  // Parse Polygon
  S2Polygon* poly = gp.parseGeoJSONPolygon(geoJSONA);

  // 1. Calculate the center of the polygon
  S2Point center = poly->GetCentroid();

  // 2. Calculate the highest radius available
  double radius = 0.0;
  double calculatedDistance = 0.0;

  vector<S2Point> pointsOfPolygon = gp.parseGeoJSONMultiPoint(geoJSONA);
  for ( auto &i : pointsOfPolygon ) {
    calculatedDistance = S1Angle(center, i).degrees();
    if (calculatedDistance > radius) {
      radius = calculatedDistance;
    }
  }

  // convert radius to to m
  radius = radius * 100 * 1000 + 1000;

  // read collection name
  std::string const collectionNameString(collectionName.slice().copyString());

  // check if geo index is available -- if no index available -> full scan
  TRI_voc_cid_t cid = trx->resolver()->getCollectionIdLocal(collectionNameString);


  AqlValue indexedPoints;

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  bool haveRocks = engine->typeName() == RocksDBEngine::EngineName;
  if (haveRocks) {
    arangodb::RocksDBGeoIndex* index = getRocksDBGeoIndex(trx, cid, collectionNameString);
    // if maintainer mode enabled a core will be returned by this if index == nullptr
    TRI_ASSERT(index != nullptr);
    // stops whether or not a ditch has been created for the collection
    TRI_ASSERT(trx->isPinned(cid));

    rocksdbengine::GeoCoordinates* cors = index->withinQuery(
        trx, S2LatLng(center).lng().degrees(), S2LatLng(center).lat().degrees(), radius);
    
    // 4. Check each point if it exists in the range of the given polygon
    indexedPoints = buildRocksDBGeoResult(trx, index->collection(), cors, cid, poly);
  } else {
    arangodb::MMFilesGeoIndex* index = getMMFilesGeoIndex(trx, cid, collectionNameString);
    // if maintainer mode enabled a core will be returned by this if index == nullptr
    TRI_ASSERT(index != nullptr);
    // stops whether or not a ditch has been created for the collection
    TRI_ASSERT(trx->isPinned(cid));

    GeoCoordinates* cors = index->withinQuery(
        trx, S2LatLng(center).lng().degrees(), S2LatLng(center).lat().degrees(), radius);

    // 4. Check each point if it exists in the range of the given polygon
    indexedPoints = buildMMFilesGeoResult(trx, index->collection(), cors, cid, poly);
  }

  // 5. Put all fitting points into a new AqlValue MultiPoint Value and return
  return indexedPoints;
};

// main intersection function which detects types and moves forward with specific function
bool Geo::intersect(const AqlValue geoJSONA, const AqlValue geoJSONB) {
  GeoParser gp;
  if (!gp.parseGeoJSONType(geoJSONA) || !gp.parseGeoJSONType(geoJSONB)) {
    // TODO: add invalid geo json error
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_GRAPH_INVALID_GRAPH, "Invalid GeoJSON type.");
  }

  if (gp.parseGeoJSONTypePolygon(geoJSONA) && gp.parseGeoJSONTypePolygon(geoJSONB)) {
    return intersectPolygonWithPolygon(geoJSONA, geoJSONB);
  } else {
    // TODO: add invalid geo json error
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_GRAPH_INVALID_GRAPH, "Invalid GeoJSON types to match.");
  }
};

// returns bool if poly<->poly intersection is true or false
bool Geo::intersectPolygonWithPolygon(const AqlValue geoJSONA, const AqlValue geoJSONB) {
  GeoParser gp;
  bool result;

  S2Polygon* polyA = gp.parseGeoJSONPolygon(geoJSONA);
  S2Polygon* polyB = gp.parseGeoJSONPolygon(geoJSONB);

  result = polyA->Intersects(polyB);
  return result;
};
