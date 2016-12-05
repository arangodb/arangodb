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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "Basics/StringRef.h"
#include "Basics/VelocyPackHelper.h"
#include "GeoIndex.h"
#include "Indexes/GeoIndex.h"
#include "Logger/Logger.h"
#include "VocBase/transaction.h"

using namespace arangodb;
GeoIndexIterator::GeoIndexIterator(LogicalCollection* collection,
                                     arangodb::Transaction* trx,
                                     ManagedDocumentResult* mmdr,
                                     GeoIndex const* index,
                                     arangodb::aql::AstNode const* cond,
                                     arangodb::aql::Variable const* var)
    : IndexIterator(collection, trx, mmdr, index),
      _index(index),
      _cursor(nullptr),
      _coor(),
      _condition(cond),
      _variable(var),
      _lat(0),
      _lon(0),
      _near(true),
      _withinRange(0),
      _withinLessEq(false)
      // lookup will hold the inforamtion if this is a cursor for
      // near/within and the reference point
      //_lookups(trx, node, reference, index->fields()),
    {
      evaluateCondition();
    }

void GeoIndexIterator::evaluateCondition() {
  //LOG_TOPIC(DEBUG, Logger::DEVEL) << "ENTER evaluate Condition";

  if (_condition) {
    auto numMembers = _condition->numMembers();

    if(numMembers >= 2){
      _lat = _condition->getMember(0)->getMember(1)->getDoubleValue();
      LOG_TOPIC(DEBUG, Logger::DEVEL) << "lat: " << _lat;
      _lon = _condition->getMember(1)->getMember(1)->getDoubleValue();
      LOG_TOPIC(DEBUG, Logger::DEVEL) << "lon: " << _lon;
    }

    if (numMembers == 2){ //near
      _near = true;
      LOG_TOPIC(DEBUG, Logger::DEVEL) << "INDEX CONFIGURED FOR NEAR";
    } else { //within
      _near = false;
      _withinRange = _condition->getMember(2)->getMember(1)->getDoubleValue();
      _withinLessEq = _condition->getMember(3)->getMember(1)->getDoubleValue();

      LOG_TOPIC(DEBUG, Logger::DEVEL) << "INDEX CONFIGURED FOR WITHIN  with range " << _withinRange;
    }

  } else {
    LOG(ERR) << "No Condition passed to GeoIndexIterator constructor";
  }

  //LOG_TOPIC(DEBUG, Logger::DEVEL) << "EXIT evaluate Condition";
}

IndexLookupResult GeoIndexIterator::next() {
  //LOG_TOPIC(DEBUG, Logger::DEVEL) << "ENTER next";
  if (!_cursor){
    createCursor(_lat,_lon);
  }

  auto coords = std::unique_ptr<GeoCoordinates>(::GeoIndex_ReadCursor(_cursor,1));
  if(coords && coords->length){
    if(_near || GeoIndex_distance(&_coor, &coords->coordinates[0]) <= _withinRange ){
      auto revision = ::GeoIndex::toRevision(coords->coordinates[0].data);
      return IndexLookupResult{revision};
    }
  }
  // if there are no more results we return the default constructed IndexLookupResult
  return IndexLookupResult{};
}

void GeoIndexIterator::nextBabies(std::vector<IndexLookupResult>& result, size_t batchSize) {
  //LOG_TOPIC(DEBUG, Logger::DEVEL) << "ENTER nextBabies " << batchSize;
  if (!_cursor){
    createCursor(_lat,_lon);
  }

  result.clear();
  if (batchSize > 0) {
    auto coords = std::unique_ptr<GeoCoordinates>(::GeoIndex_ReadCursor(_cursor,batchSize));
    size_t length = coords ? coords->length : 0;
    //LOG_TOPIC(DEBUG, Logger::DEVEL) << "length " << length;
    if (!length){
      return;
    }


    for(std::size_t index = 0; index < length; ++index){
      //LOG_TOPIC(DEBUG, Logger::DEVEL) << "near " << _near << " max allowed range: " << _withinRange
      //                                                    << " actual range: "  << GeoIndex_distance(&_coor, &coords->coordinates[index]) ;
      if (_near || GeoIndex_distance(&_coor, &coords->coordinates[index]) <= _withinRange ){
        //LOG_TOPIC(DEBUG, Logger::DEVEL) << "add above to result" ;
        result.emplace_back(IndexLookupResult(::GeoIndex::toRevision(coords->coordinates[index].data)));
      } else {
        break;
      }
    }
  }
  //LOG_TOPIC(DEBUG, Logger::DEVEL) << "EXIT nextBabies " << result.size();
}

::GeoCursor* GeoIndexIterator::replaceCursor(::GeoCursor* c){
  if(_cursor){
    ::GeoIndex_CursorFree(_cursor);
  }
 _cursor = c;
 return _cursor;
}

::GeoCursor* GeoIndexIterator::createCursor(double lat, double lon){
  _coor = GeoCoordinate{lat, lon, 0};
  return replaceCursor(::GeoIndex_NewCursor(_index->_geoIndex, &_coor));
}

/// @brief creates an IndexIterator for the given Condition
IndexIterator* GeoIndex::iteratorForCondition(
    arangodb::Transaction* trx,
    ManagedDocumentResult* mmdr,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, bool) const {
  TRI_IF_FAILURE("HashIndex::noIterator")  {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  return new GeoIndexIterator(_collection, trx, mmdr, this, node, reference);
}


void GeoIndexIterator::reset() {
  replaceCursor(nullptr);
}

GeoIndex::GeoIndex(TRI_idx_iid_t iid, arangodb::LogicalCollection* collection,
                     VPackSlice const& info)
    : Index(iid, collection, info),
      _variant(INDEX_GEO_INDIVIDUAL_LAT_LON),
      _geoJson(false),
      _geoIndex(nullptr) {
  TRI_ASSERT(iid != 0);
  _unique = false;
  _sparse = true;

  if (_fields.size() == 1) {
    _geoJson = arangodb::basics::VelocyPackHelper::getBooleanValue(
        info, "geoJson", false);
    auto& loc = _fields[0];
    _location.reserve(loc.size());
    for (auto const& it : loc) {
      _location.emplace_back(it.name);
    }
    _variant =
        _geoJson ? INDEX_GEO_COMBINED_LAT_LON : INDEX_GEO_COMBINED_LON_LAT;
  } else if (_fields.size() == 2) {
    _variant = INDEX_GEO_INDIVIDUAL_LAT_LON;
    auto& lat = _fields[0];
    _latitude.reserve(lat.size());
    for (auto const& it : lat) {
      _latitude.emplace_back(it.name);
    }
    auto& lon = _fields[1];
    _longitude.reserve(lon.size());
    for (auto const& it : lon) {
      _longitude.emplace_back(it.name);
    }
  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "GeoIndex can only be created with one or two fields.");
  }

  _geoIndex = GeoIndex_new();

  if (_geoIndex == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

GeoIndex::~GeoIndex() {
  if (_geoIndex != nullptr) {
    GeoIndex_free(_geoIndex);
  }
}

size_t GeoIndex::memory() const { return GeoIndex_MemoryUsage(_geoIndex); }

/// @brief return a JSON representation of the index
void GeoIndex::toVelocyPack(VPackBuilder& builder, bool withFigures) const {
  // Basic index
  Index::toVelocyPack(builder, withFigures);

  if (_variant == INDEX_GEO_COMBINED_LAT_LON ||
      _variant == INDEX_GEO_COMBINED_LON_LAT) {
    builder.add("geoJson", VPackValue(_geoJson));
  }

  // geo indexes are always non-unique
  // geo indexes are always sparse.
  // "ignoreNull" has the same meaning as "sparse" and is only returned for
  // backwards compatibility
  // the "constraint" attribute has no meaning since ArangoDB 2.5 and is only
  // returned for backwards compatibility
  builder.add("constraint", VPackValue(false));
  builder.add("unique", VPackValue(false));
  builder.add("ignoreNull", VPackValue(true));
  builder.add("sparse", VPackValue(true));
}

/// @brief Test if this index matches the definition
bool GeoIndex::matchesDefinition(VPackSlice const& info) const {
  TRI_ASSERT(info.isObject());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  VPackSlice typeSlice = info.get("type");
  TRI_ASSERT(typeSlice.isString());
  StringRef typeStr(typeSlice);
  TRI_ASSERT(typeStr == typeName());
#endif
  auto value = info.get("id");
  if (!value.isNone()) {
    // We already have an id.
    if(!value.isString()) {
      // Invalid ID
      return false;
    }
    // Short circuit. If id is correct the index is identical.
    StringRef idRef(value);
    return idRef == std::to_string(_iid);
  }
  value = info.get("fields");
  if (!value.isArray()) {
    return false;
  }

  size_t const n = static_cast<size_t>(value.length());
  if (n != _fields.size()) {
    return false;
  }
  if (_unique != arangodb::basics::VelocyPackHelper::getBooleanValue(
                     info, "unique", false)) {
    return false;
  }
  if (_sparse != arangodb::basics::VelocyPackHelper::getBooleanValue(
                     info, "sparse", true)) {
    return false;
  }

  if (n == 1) {
    if (_geoJson != arangodb::basics::VelocyPackHelper::getBooleanValue(
        info, "geoJson", false)) {
      return false;
    }
  }

  // This check takes ordering of attributes into account.
  std::vector<arangodb::basics::AttributeName> translate;
  for (size_t i = 0; i < n; ++i) {
    translate.clear();
    VPackSlice f = value.at(i);
    if (!f.isString()) {
      // Invalid field definition!
      return false;
    }
    arangodb::StringRef in(f);
    TRI_ParseAttributeString(in, translate, true);
    if (!arangodb::basics::AttributeName::isIdentical(_fields[i], translate,
                                                      false)) {
      return false;
    }
  }
  return true;
}

int GeoIndex::insert(arangodb::Transaction*, TRI_voc_rid_t revisionId,
                     VPackSlice const& doc, bool isRollback) {
  double latitude;
  double longitude;

  if (_variant == INDEX_GEO_INDIVIDUAL_LAT_LON) {
    VPackSlice lat = doc.get(_latitude);
    if (!lat.isNumber()) {
      // Invalid, no insert. Index is sparse
      return TRI_ERROR_NO_ERROR;
    }

    VPackSlice lon = doc.get(_longitude);
    if (!lon.isNumber()) {
      // Invalid, no insert. Index is sparse
      return TRI_ERROR_NO_ERROR;
    }
    latitude = lat.getNumericValue<double>();
    longitude = lon.getNumericValue<double>();
  } else {
    VPackSlice loc = doc.get(_location);
    if (!loc.isArray() || loc.length() < 2) {
      // Invalid, no insert. Index is sparse
      return TRI_ERROR_NO_ERROR;
    }
    VPackSlice first = loc.at(0);
    if (!first.isNumber()) {
      // Invalid, no insert. Index is sparse
      return TRI_ERROR_NO_ERROR;
    }
    VPackSlice second = loc.at(1);
    if (!second.isNumber()) {
      // Invalid, no insert. Index is sparse
      return TRI_ERROR_NO_ERROR;
    }
    if (_geoJson) {
      longitude = first.getNumericValue<double>();
      latitude = second.getNumericValue<double>();
    } else {
      latitude = first.getNumericValue<double>();
      longitude = second.getNumericValue<double>();
    }
  }

  // and insert into index
  GeoCoordinate gc;
  gc.latitude = latitude;
  gc.longitude = longitude;
  gc.data = fromRevision(revisionId);

  int res = GeoIndex_insert(_geoIndex, &gc);

  if (res == -1) {
    LOG(WARN) << "found duplicate entry in geo-index, should not happen";
    return TRI_set_errno(TRI_ERROR_INTERNAL);
  } else if (res == -2) {
    return TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
  } else if (res == -3) {
    LOG(DEBUG) << "illegal geo-coordinates, ignoring entry";
    return TRI_ERROR_NO_ERROR;
  } else if (res < 0) {
    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }

  return TRI_ERROR_NO_ERROR;
}

int GeoIndex::remove(arangodb::Transaction*, TRI_voc_rid_t revisionId, 
                     VPackSlice const& doc, bool isRollback) {
  double latitude = 0.0;
  double longitude = 0.0;
  bool ok = true;

  if (_variant == INDEX_GEO_INDIVIDUAL_LAT_LON) {
    VPackSlice lat = doc.get(_latitude);
    VPackSlice lon = doc.get(_longitude);
    if (!lat.isNumber()) {
      ok = false;
    } else {
      latitude = lat.getNumericValue<double>();
    }
    if (!lon.isNumber()) {
      ok = false;
    } else {
      longitude = lon.getNumericValue<double>();
    }
  } else {
    VPackSlice loc = doc.get(_location);
    if (!loc.isArray() || loc.length() < 2) {
      ok = false;
    } else {
      VPackSlice first = loc.at(0);
      if (!first.isNumber()) {
        ok = false;
      }
      VPackSlice second = loc.at(1);
      if (!second.isNumber()) {
        ok = false;
      }
      if (ok) {
        if (_geoJson) {
          longitude = first.getNumericValue<double>();
          latitude = second.getNumericValue<double>();
        } else {
          latitude = first.getNumericValue<double>();
          longitude = second.getNumericValue<double>();
        }
      }
    }
  }

  if (!ok) {
    return TRI_ERROR_NO_ERROR;
  }

  GeoCoordinate gc;
  gc.latitude = latitude;
  gc.longitude = longitude;
  gc.data = fromRevision(revisionId);

  // ignore non-existing elements in geo-index
  GeoIndex_remove(_geoIndex, &gc);

  return TRI_ERROR_NO_ERROR;
}

int GeoIndex::unload() {
  // create a new, empty index
  auto empty = GeoIndex_new();

  if (empty == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  
  // free the old one
  if (_geoIndex != nullptr) {
    GeoIndex_free(_geoIndex);
  }

  // and assign it
  _geoIndex = empty;

  return TRI_ERROR_NO_ERROR;
}

/// @brief looks up all points within a given radius
GeoCoordinates* GeoIndex::withinQuery(arangodb::Transaction* trx, double lat,
                                       double lon, double radius) const {
  GeoCoordinate gc;
  gc.latitude = lat;
  gc.longitude = lon;

  return GeoIndex_PointsWithinRadius(_geoIndex, &gc, radius);
}

/// @brief looks up the nearest points
GeoCoordinates* GeoIndex::nearQuery(arangodb::Transaction* trx, double lat,
                                     double lon, size_t count) const {
  GeoCoordinate gc;
  gc.latitude = lat;
  gc.longitude = lon;

  return GeoIndex_NearestCountPoints(_geoIndex, &gc, static_cast<int>(count));
}
