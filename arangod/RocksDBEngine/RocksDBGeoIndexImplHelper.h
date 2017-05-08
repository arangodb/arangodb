////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

// MUST BE ONLY INCLUDED IN RocksDBGeoIndexImpl.cpp after struct definitions!
// IT CAN NOT BE USED IN OTHER
// This file has only been added to keep Richards code clean. So it is easier
// for him to spot relevant changes.

#ifndef ARANGOD_ROCKSDB_GEO_INDEX_IMPL_HELPER_H
#define ARANGOD_ROCKSDB_GEO_INDEX_IMPL_HELPER_H 1

#include <RocksDBEngine/RocksDBGeoIndexImpl.h>

#include <RocksDBEngine/RocksDBCommon.h>
#include <RocksDBEngine/RocksDBEngine.h>
#include <RocksDBEngine/RocksDBKey.h>

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace rocksdbengine {

VPackBuilder CoordToVpack(GeoCoordinate* coord) {
  VPackBuilder rv{};
  rv.openArray();
  rv.add(VPackValue(coord->latitude));   // double
  rv.add(VPackValue(coord->longitude));  // double
  rv.add(VPackValue(coord->data));       // uint64_t
  rv.close();
  return rv;
}

void VpackToCoord(VPackSlice const& slice, GeoCoordinate* gc) {
  TRI_ASSERT(slice.isArray() && slice.length() == 3);
  gc->latitude = slice.at(0).getDouble();
  gc->longitude = slice.at(1).getDouble();
  gc->data = slice.at(2).getUInt();
}

VPackBuilder PotToVpack(GeoPot* pot) {
  VPackBuilder rv{};
  rv.openArray();                      // open
  rv.add(VPackValue(pot->LorLeaf));    // int
  rv.add(VPackValue(pot->RorPoints));  // int
  rv.add(VPackValue(pot->middle));     // GeoString
  {
    rv.openArray();  // array GeoFix //uint 16/32
    for (std::size_t i = 0; i < GeoIndexFIXEDPOINTS; i++) {
      rv.add(VPackValue(pot->maxdist[i]));  // unit 16/32
    }
    rv.close();  // close array
  }
  rv.add(VPackValue(pot->start));  // GeoString
  rv.add(VPackValue(pot->end));    // GeoString
  rv.add(VPackValue(pot->level));  // int
  {
    rv.openArray();  // arrray of int
    for (std::size_t i = 0; i < GeoIndexPOTSIZE; i++) {
      rv.add(VPackValue(pot->points[i]));  // int
    }
    rv.close();  // close array
  }
  rv.close();  // close
  return rv;
}

void VpackToPot(VPackSlice const& slice, GeoPot* rv) {
  TRI_ASSERT(slice.isArray());
  rv->LorLeaf = (int)slice.at(0).getInt();    // int
  rv->RorPoints = (int)slice.at(1).getInt();  // int
  rv->middle = slice.at(2).getUInt();         // GeoString
  {
    auto maxdistSlice = slice.at(3);
    TRI_ASSERT(maxdistSlice.isArray());
    TRI_ASSERT(maxdistSlice.length() == GeoIndexFIXEDPOINTS);
    for (std::size_t i = 0; i < GeoIndexFIXEDPOINTS; i++) {
      rv->maxdist[i] = (int)maxdistSlice.at(i).getUInt();  // unit 16/33
    }
  }
  rv->start = (int)slice.at(4).getUInt();  // GeoString
  rv->end = slice.at(5).getUInt();         // GeoString
  rv->level = (int)slice.at(6).getInt();   // int
  {
    auto pointsSlice = slice.at(7);
    TRI_ASSERT(pointsSlice.isArray());
    TRI_ASSERT(pointsSlice.length() == GeoIndexFIXEDPOINTS);
    for (std::size_t i = 0; i < GeoIndexPOTSIZE; i++) {
      rv->points[i] = (int)pointsSlice.at(i).getInt();  // int
    }
  }
}

}  // namespace rocksdbengine
}  // namespace arangodb
#endif
