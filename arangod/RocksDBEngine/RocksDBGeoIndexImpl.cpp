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
/// @author R. A. Parker
////////////////////////////////////////////////////////////////////////////////

/* GeoIndex.c -   GeoIndex algorithms                */
/* Version 2.1   8.1.2012  R. A. Parker              */
#define _USE_MATH_DEFINES
#include <cmath>
#include <cstddef>
#include <limits>

#include "RocksDBEngine/RocksDBGeoIndexImpl.h"
#include "RocksDBEngine/RocksDBMethods.h"

/* Radius of the earth used for distances  */
#define EARTHRADIAN 6371000.0

#define GEOSLOTSTART 50
#define GEOPOTSTART 100

#if GeoIndexFIXEDSET == 2
#define GeoIndexFIXEDPOINTS 2
#endif
#if GeoIndexFIXEDSET == 3
#define GeoIndexFIXEDPOINTS 3
#endif
#if GeoIndexFIXEDSET == 4
#define GeoIndexFIXEDPOINTS 4
#endif
#if GeoIndexFIXEDSET == 5
#define GeoIndexFIXEDPOINTS 5
#endif
#if GeoIndexFIXEDSET == 6
#define GeoIndexFIXEDPOINTS 6
#endif
#if GeoIndexFIXEDSET == 8
#define GeoIndexFIXEDPOINTS 8
#endif
#ifndef GeoIndexFIXEDPOINTS
#define GeoIndexFIXEDPOINTS 1
#endif

namespace arangodb { namespace rocksdbengine {

/* =================================================== */
/*                GeoIndexFixed structure.             */
/* Only occurs once, and that is in the GeoIx struct   */
/* holds the x,y and z coordinates (between -1 and +1) */
/* of the fixed points used for pot rejection purposes */
/* They are computed at GeoIndex_new time and not      */
/* changed after that                                  */
/* =================================================== */
typedef struct {
  double x[GeoIndexFIXEDPOINTS];
  double y[GeoIndexFIXEDPOINTS];
  double z[GeoIndexFIXEDPOINTS];
} GeoIndexFixed;
/* =================================================== */
/*                  GeoPot structure                   */
/* These only occur in the main index itself, and the  */
/* GeoIx structure has an array of them.  The data     */
/* items are arranged so that the access during a      */
/* search is approximately sequential, which should be */
/* a little faster on most machines.                   */
/* The first two data items are used for several       */
/* different purposes.  LorLeaf is zero for a leaf pot */
/* and the left child for a non-leaf pot.  RorPoints   */
/* is the right child for a non-leaf pot, and the      */
/* number of points in the pot for a leaf pot          */
/* The three GeoString values give the bounds (weak)   */
/* for the Hilbert values in this pot.  middle is not  */
/* used for a leaf pot.                                */
/* maxdist is the maximum, over all points descendent  */
/* from this pot, of the distances to the fixed points */
/* level is the AVL-level.  It is 1 for a leaf pot,    */
/* and always at least 1 more and at most 2 more than  */
/* each of its children, and exactly 1 more than at    */
/* least one of its children, - the AVL spec.          */
/* "points" lists the slotid of the points.  This is   */
/* only used for a leaf pot.                           */
/* =================================================== */
typedef struct {
  int32_t LorLeaf;
  int32_t RorPoints;
  GeoString middle;
  GeoFix maxdist[GeoIndexFIXEDPOINTS];
  GeoString start;
  GeoString end;
  int32_t level;
  int32_t points[GeoIndexPOTSIZE];
} GeoPot;
/* =================================================== */
/*                 GeoIx structure                     */
/* This is the REAL GeoIndex structure - the one in    */
/* the GeoIndex.h file is just a sham (it says it is   */
/* a char!) to keep the structure private so that the  */
/* GeoIndex.h is short and contains only data of       */
/* interest to the user.                               */
/* The GeoIx structure basically consists of two       */
/* arrays - the slots (the points) and the pots (the   */
/* Balanced (AVL) search tree for finding near points) */
/* The Fixed-point data is held here also, giving the  */
/* x, y and z coordinates of the fixed points, this    */
/* data being the fastest to use                       */
/* potct and slotct are used when the index needs to   */
/* grow (because it has run out of slots or pots)      */
/* There is no provision at present for the index to   */
/* get smaller when the majority of points are deleted */
/* =================================================== */
typedef struct {
  uint64_t objectId;   /* Rocksdb Index objectId    */
  GeoIndexFixed fixed; /* fixed point data          */
  int nextFreePot;           /* pots allocated      */
  int nextFreeSlot;          /* slots allocated     */
  RocksDBMethods *rocksMethods;
  //GeoPot* ypots;       /* the pots themselves     */// gone
  //GeoCoordinate* gxc;  /* the slots themselves    */// gone
  //size_t _memoryUsed;  /* the amount of memory currently used */// gone
}  GeoIx;
/* =================================================== */
/*              GeoDetailedPoint  structure            */
/* The routine GeoMkDetail is given a point - really   */
/* just a latitude and longitude, and computes all the */
/* values in this GeoDetailedPoint structure.          */
/* This is intended to include everything that will be */
/* needed about the point, and is called both for the  */
/* searches (count and distance) and the updates       */
/* (insert and remove).  It is only ever useful        */
/* locally - it is created, populated, used and        */
/* forgotten all within a single user's call           */
/* the GeoIx is noted there to simplify some calls     */
/* The GeoCoordinate (a pointer to the user's one)     */
/* is included.  The x, y and z coordinates (between   */
/* 1 and -1) are computed, as is the GeoString - the   */
/* Hilbert curve value used to decide where in the     */
/* index a point belongs.  The fixdist array is the    */
/* distance to the fixed points.                       */
/* The other two entries (snmd and distrej) are not    */
/* computed by GeoMkDetail, but are put put in place   */
/* later, for the searches only, by GeoSetDistance.    */
/* They basically hold the radius of the circle around */
/* the target point outside which indexed points will  */
/* be too far to be of interest.  This is set once and */
/* for all in the case of a search-by-distance, but    */
/* for a search-by-count the interesting distance      */
/* decreases as further points are found.              */
/* Anyway, snmd hold the radius in SNMD form (squared  */
/* normalized mole distance) being the distance in     */
/* three-dimensional space between two points passing  */
/* through the earth (as a mole digs!) - this being    */
/* the fastest to compute on the fly, and is used for  */
/* looking at individual points to decide whether to   */
/* include them.  The distrej array, on the other hand */
/* is the array of distances to the fixed points, and  */
/* is used to reject pots (leaf or non-leaf).          */
/* The routine GeoPotJunk is used to test this,        */
/* by comparing the distances in the pot the this array*/
/* =================================================== */
typedef struct {// stays
  GeoIx* gix;
  GeoCoordinate* gc;
  double x;
  double y;
  double z;
  GeoString gs;
  GeoFix fixdist[GeoIndexFIXEDPOINTS];
  double snmd;
  GeoFix distrej[GeoIndexFIXEDPOINTS];
} GeoDetailedPoint;
/* =================================================== */
/*                   GeoResults   structure            */
/* During the searches, this structure is used to      */
/* accumulate the points that will be returned         */
/* In the case of a search-by-distance, the results are*/
/* simply a list, which is grown by about 50% if the   */
/* initial allocation (100) is inadequte.  In the case */
/* of a search-by-count, the exact number needed is    */
/* known from the start, but the structure is not just */
/* a simple list in this case.  Instead it is organized*/
/* as a "priority queue" to enable large values of the */
/* <count> parameter to be rapidly processed.  In the  */
/* case of count, each value is kept to be larger that */
/* both of its "children" - at 2n+1 and 2n+2.  Hence   */
/* the largest distance is always at position 0 and can*/
/* be readily found, but if it is to be replaced, there*/
/* is some procession (no more than log(count) work)   */
/* to do to find the correct place to insert the new   */
/* one in the priority queue.  This work is done in the*/
/* GeoResultsInsertPoint routine (not used by distance)*/
/* =================================================== */
typedef struct {
  int pointsct;
  int allocpoints;
  int* slot;
  double* snmd;
} GeoResults;
/* =================================================== */
/*                 GeoStack    structure               */
/* During searches of both kinds, at any time there is */
/* this "stack" (first-in-last-out) of pots still to be*/
/* processed.  At the start of a search of either type,*/
/* this structure is populated (by GeoStackSet) by     */
/* starting at the root pot, and selecting a child that*/
/* could contain the target point.  The other pot is   */
/* put on the stack and processing continues.  The     */
/* stack is then processed by taking a pot off,        */
/* discarding it if the maximum distance to a fixed    */
/* point is too low, and otherwise putting both the    */
/* children onto the stack (since it is faster to do   */
/* this than suffer the cache miss to determine whether*/
/* either or both of the children can be rejected)     */
/* =================================================== */
typedef struct {
  GeoResults* gr;
  GeoDetailedPoint* gd;
  int stacksize;
  int potid[50];
} GeoStack;
/* =================================================== */
/*                  GeoPath structure                  */
/* Similar in many ways to the GeoStack, above, this   */
/* structure is used during insertion and deletion.    */
/* Notice that the pots of the index to not contain    */
/* pointers to their parent, since this is not needed  */
/* during a search.  During insertion and removal,     */
/* however, it is necessary to move upwards to         */
/* propogate the maximum distances and to balance the  */
/* tree.  Hence the GeoFind procedure, called at the   */
/* beginning of insertion and deletion, populates this */
/* structure so that the full path from the root node  */
/* to the current pot being considered is known, and   */
/* its parent found when needed.                       */
/* =================================================== */
typedef struct {
  GeoIx* gix;
  int pathlength;
  int path[50];
} GeoPath;
}}

// must be included here after struct definition
#include <RocksDBEngine/RocksDBCommon.h>
#include <RocksDBEngine/RocksDBKey.h>
#include <RocksDBEngine/RocksDBCounterManager.h>
#include <RocksDBEngine/RocksDBEngine.h>
#include <RocksDBEngine/RocksDBGeoIndexImpl.h>
#include <StorageEngine/EngineSelectorFeature.h>

namespace arangodb { namespace rocksdbengine {

static GeoCoordinate& fromPersistent(char const* in, GeoCoordinate& out){
  const char* start = in;

  //convert latituide and longitute to uint64 for network transfer / storage
  uint64_t fromStorage = rocksutils::fromPersistent<uint64_t>(start);
  out.latitude = rocksutils::intToDouble(fromStorage);

  fromStorage = rocksutils::fromPersistent<uint64_t>(start);
  out.longitude = rocksutils::intToDouble(fromStorage);

  out.data = rocksutils::fromPersistent<uint64_t>(start);

  return out;
}

static void toPersistent(GeoCoordinate& in, char* out){
  char* start = out;

  uint64_t toStorage = rocksutils::doubleToInt(in.latitude);
  rocksutils::toPersistent(toStorage, start);

  toStorage = rocksutils::doubleToInt(in.longitude);
  rocksutils::toPersistent(toStorage, start);

  rocksutils::toPersistent(in.data, start);
}

static GeoPot& fromPersistent(char const* in, GeoPot& out){
  const char* start = in;

  out.LorLeaf = rocksutils::fromPersistent<int32_t>(start);
  out.RorPoints = rocksutils::fromPersistent<int32_t>(start);
  out.middle = rocksutils::fromPersistent<GeoString>(start);

  for(std::size_t i = 0; i < GeoIndexFIXEDPOINTS; i++){
    out.maxdist[i] = rocksutils::fromPersistent<GeoFix>(start);
  }

  out.start = rocksutils::fromPersistent<GeoString>(start);
  out.end = rocksutils::fromPersistent<GeoString>(start);
  out.level = rocksutils::fromPersistent<int32_t>(start);

  for(std::size_t i = 0; i < GeoIndexFIXEDPOINTS; i++){
    out.points[i] = rocksutils::fromPersistent<int32_t>(start);
  }

  return out;
}

static void toPersistent(GeoPot const& in, char* out){
  char* start = out;

  rocksutils::toPersistent(in.LorLeaf, start);
  rocksutils::toPersistent(in.RorPoints, start);
  rocksutils::toPersistent(in.middle, start);

  for(std::size_t i = 0; i< GeoIndexFIXEDPOINTS; i++){
    rocksutils::toPersistent(in.maxdist[i], start);
  }

  rocksutils::toPersistent(in.start, start);
  rocksutils::toPersistent(in.end, start);
  rocksutils::toPersistent(in.level, start);

  for(std::size_t i = 0; i< GeoIndexFIXEDPOINTS; i++){
    rocksutils::toPersistent(in.points[i], start);
  }
}

/* CRUD interface */

void GeoIndex_setRocksMethods(GeoIdx* gi, RocksDBMethods* trx) {
  GeoIx* gix = (GeoIx*)gi;
  gix->rocksMethods = trx;
}

void GeoIndex_clearRocks(GeoIdx* gi) {
  GeoIx* gix = (GeoIx*)gi;
  gix->rocksMethods = nullptr;
}

inline void RocksRead(GeoIx * gix, RocksDBKey const& key, std::string *val) {
  arangodb::Result r = gix->rocksMethods->Get(RocksDBColumnFamily::geo(), key, val);
  if (!r.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(r.errorNumber(), r.errorMessage());
  }
}

inline void RocksWrite(GeoIx * gix,
                       RocksDBKey const& key,
                       rocksdb::Slice const& slice) {
  // only true when called from GeoIndex_new
  if (gix->rocksMethods == nullptr) {
    rocksdb::TransactionDB *db = rocksutils::globalRocksDB();
    rocksdb::WriteOptions wo;
    wo.sync = true;
    rocksdb::Status s = db->Put(wo, RocksDBColumnFamily::geo(), key.string(), slice);
    if (!s.ok()) {
      arangodb::Result r = rocksutils::convertStatus(s, rocksutils::index);
      THROW_ARANGO_EXCEPTION_MESSAGE(r.errorNumber(), r.errorMessage());
    }
  } else {
    arangodb::Result r = gix->rocksMethods->Put(RocksDBColumnFamily::geo(),
                                                key, slice, rocksutils::index);
    if (!r.ok()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(r.errorNumber(), r.errorMessage());
    }
  }
}
  
inline void RocksDelete(GeoIx* gix, RocksDBKey const& key) {
  arangodb::Result r = gix->rocksMethods->Delete(RocksDBColumnFamily::geo(),
                                                 key);
  if (!r.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(r.errorNumber(), r.errorMessage());
  }
}

void SlotRead(GeoIx * gix, int slot, GeoCoordinate * gc /*out param*/)
{
  RocksDBKey key = RocksDBKey::GeoIndexValue(gix->objectId, slot, true);
  std::string slotValue;
  RocksRead(gix, key, &slotValue);
  fromPersistent(slotValue.data(),*gc);
}
void SlotWrite(GeoIx * gix,int slot, GeoCoordinate * gc)
{
  RocksDBKey key = RocksDBKey::GeoIndexValue(gix->objectId, slot, true);
  char data[sizeof (GeoCoordinate)];
  toPersistent(*gc, &data[0]);
  RocksWrite(gix, key, rocksdb::Slice(&data[0], sizeof(GeoCoordinate)));
}

void PotRead(GeoIx * gix, int pot, GeoPot * gp)
{
  RocksDBKey key = RocksDBKey::GeoIndexValue(gix->objectId, pot, false);
  std::string potValue;
  RocksRead(gix, key, &potValue);
  TRI_ASSERT(potValue.size() == sizeof(GeoPot));
  fromPersistent(potValue.data(), *gp);
}

void PotWrite(GeoIx * gix, int pot, GeoPot * gp) {
  RocksDBKey key = RocksDBKey::GeoIndexValue(gix->objectId, pot, false);
  char data[sizeof (GeoPot)];
  toPersistent(*gp, &data[0]);
  RocksWrite(gix, key, rocksdb::Slice(&data[0], sizeof(GeoPot)));
}

/* =================================================== */
/*                GeoIndex_Distance routine            */
/* This is the user-facing routine to compute the      */
/* distance in meters between any two points, given    */
/* by latitude and longitude in a pair of GeoCoordinate*/
/* structures.  It operates by first converting the    */
/* two points into x, y and z coordinates in 3-space,  */
/* then computing the distance between them (again in  */
/* three space) using Pythagoras, computing the angle  */
/* subtended at the earth's centre, between the two    */
/* points, and finally muliply this angle (in radians) */
/* by the earth's radius to convert it into meters.    */
/* =================================================== */
double GeoIndex_distance(GeoCoordinate* c1, GeoCoordinate* c2) {
  /* math.h under MacOS defines y1 and j1 as global variable */
  double xx1, yy1, z1, x2, y2, z2, mole;
  z1 = sin(c1->latitude * M_PI / 180.0);
  xx1 = cos(c1->latitude * M_PI / 180.0) * cos(c1->longitude * M_PI / 180.0);
  yy1 = cos(c1->latitude * M_PI / 180.0) * sin(c1->longitude * M_PI / 180.0);
  z2 = sin(c2->latitude * M_PI / 180.0);
  x2 = cos(c2->latitude * M_PI / 180.0) * cos(c2->longitude * M_PI / 180.0);
  y2 = cos(c2->latitude * M_PI / 180.0) * sin(c2->longitude * M_PI / 180.0);
  mole = sqrt((xx1 - x2) * (xx1 - x2) + (yy1 - y2) * (yy1 - y2) +
              (z1 - z2) * (z1 - z2));
  if (mole > 2.0) mole = 2.0; /* make sure arcsin succeeds! */
  return 2.0 * EARTHRADIAN * asin(mole / 2.0);
}
/* =================================================== */
/*          GeoIndexFreePot                            */
/* takes the supplied pot, and puts it back onto the   */
/* free list.                                          */
/* =================================================== */
void GeoIndexFreePot(GeoIx* gix, int pot) {// rewrite delete in rocksdb
  RocksDBKey key = RocksDBKey::GeoIndexValue(gix->objectId, pot, false);
  RocksDelete(gix, key);
}
/* =================================================== */
/*            GeoIndexNewPot                           */
/* During insertion, it may happen that a leaf pot     */
/* becomes full.  In this case this routine is called  */
/* (always twice, as it happens) to allocate a new     */
/* leaf pot, and a new pot to become the parent of both*/
/* the old and the new leaf pots.  Usually this will   */
/* be a simple matter of taking a pot off the free     */
/* list, but occasionally the free list will be empty, */
/* in which case the pot array must be realloced.      */
/* NOTICE that in this case, the pots may have moved,  */
/* so it is critically important ot ensure that any    */
/* pointers to pots are re-computed after this routine */
/* has been called!  The GeoIndex_insert routine is    */
/* therefore careful to get the new pots (if any are   */
/* needed) before it gets too far into things.         */
/* =================================================== */
int GeoIndexNewPot(GeoIx* gix) {// rocksdb initial put
  return gix->nextFreePot++;
}
/* =================================================== */
/*         GeoIndex_new routine                        */
/* User-facing routine to create a whole new GeoIndex. */
/* Much of the bulk of the code in this routine is     */
/* populating the fixed points, depending on which     */
/* set of fixed points are in used.                    */
/* The first job is to allocate the initial arrays for */
/* holding the points, and the pots that index them.   */
/* If this fails, no harm is done and the nullptr      */
/* is returned.  Otherwise all the point and pots are  */
/* put onto their respective free lists.               */
/* The fixed point structure is then set up.           */
/* Finally the root pot (pot 1) is set up to be a leaf */
/* pot containing no points, but with the start and end*/
/* GeoString values (points on the Hilbert Curve) set  */
/* to be "low values" and "high values" respectively,  */
/* being slightly outside the range of possible        */
/* GeoString values of real (latitude, longitude)      */
/* points                                              */
/* =================================================== */
GeoIdx* GeoIndex_new(uint64_t objectId,
                     int numPots,
                     int numSlots) {
  TRI_ASSERT(objectId != 0);
  GeoIx* gix;
  int i;

  gix = static_cast<GeoIx*>(
      TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(GeoIx), false));

  if (gix == nullptr) {
    return (GeoIdx*)gix;
  }
  // need to set these to null
  gix->rocksMethods = nullptr;

  /* set up the fixed points structure  */

  for (i = 0; i < GeoIndexFIXEDPOINTS; i++) {
    double lat = 90.0;
    double lon = 0.0;
#if GeoIndexFIXEDSET == 2
    if (i == 1) {
      lat = -90.0;
      lon = 0.0;
    }
#endif
#if GeoIndexFIXEDSET == 3
    if (i == 1) {
      lat = -30.0;
      lon = 0.0;
    }
    if (i == 2) {
      lat = -30;
      lon = 180.0;
    }
#endif
#if GeoIndexFIXEDSET == 4
    if (i == 1) {
      lat = -19.471220634490691369246;
      lon = 180.0;
    }
    if (i == 2) {
      lat = -19.471220634490691369246;
      lon = -60.0;
    }
    if (i == 3) {
      lat = -19.471220634490691369246;
      lon = 60.0;
    }
#endif
#if GeoIndexFIXEDSET == 5
    if (i == 1) {
      lat = -90.0;
      lon = 0.0;
    }
    if (i == 2) {
      lat = 0.0;
      lon = 0.0;
    }
    if (i == 3) {
      lat = 0.0;
      lon = 120.0;
    }
    if (i == 4) {
      lat = 0.0;
      lon = -120.0;
    }
#endif
#if GeoIndexFIXEDSET == 6
    if (i == 1) {
      lat = -90.0;
      lon = 0.0;
    }
    if (i == 2) {
      lat = 0.0;
      lon = 0.0;
    }
    if (i == 3) {
      lat = 0.0;
      lon = 180.0;
    }
    if (i == 4) {
      lat = 0.0;
      lon = 90.0;
    }
    if (i == 5) {
      lat = 0.0;
      lon = -90.0;
    }
#endif
#if GeoIndexFIXEDSET == 8
    if (i == 1) {
      lat = -90.0;
      lon = 0.0;
    }
    if (i == 2) {
      lat = 19.471220634490691369246;
      lon = 0.0;
    }
    if (i == 3) {
      lat = -19.471220634490691369246;
      lon = 180.0;
    }
    if (i == 4) {
      lat = 19.471220634490691369246;
      lon = 120.0;
    }
    if (i == 5) {
      lat = -19.471220634490691369246;
      lon = -60.0;
    }
    if (i == 6) {
      lat = 19.471220634490691369246;
      lon = -120.0;
    }
    if (i == 7) {
      lat = -19.471220634490691369246;
      lon = 60.0;
    }
#endif

    double z = sin(lat * M_PI / 180.0);
    double x = cos(lat * M_PI / 180.0) * cos(lon * M_PI / 180.0);
    double y = cos(lat * M_PI / 180.0) * sin(lon * M_PI / 180.0);
    (gix->fixed.x)[i] = x;
    (gix->fixed.y)[i] = y;
    (gix->fixed.z)[i] = z;
  }
  /* set up the root pot  */
  
  gix->objectId = objectId;
  if (numPots == 0 || numSlots == 0) { // first run
    gix->nextFreePot = 2;
    gix->nextFreeSlot = 1;
    
    GeoPot gp;
    gp.LorLeaf = 0;   //leaf pot
    gp.RorPoints = 0; // with no points in it!
    gp.middle = 0ll;
    gp.start = 0ll;
    gp.end = 0x1FFFFFFFFFFFFFll;
    gp.level = 1;
    for (i = 0; i < GeoIndexFIXEDPOINTS; i++) gp.maxdist[i] = 0;
    PotWrite(gix, 1, &gp);// pot 1 is root
  } else {
    gix->nextFreePot = numPots + 1;
    gix->nextFreeSlot = numSlots + 1;
  }
  return (GeoIdx*)gix;
}
/* =================================================== */
/*               GeoIndex_free routine                 */
/* Destroys the GeoIndex, and frees all the memory that*/
/* this GeoIndex system allocated.  Note that any      */
/* objects that may have been pointed to by the user's */
/* data pointers are (of course) not freed by this call*/
/* =================================================== */
void GeoIndex_free(GeoIdx* gi) {// like drop
  GeoIx* gix;

  if (gi == nullptr) {
    return;
  }

  gix = (GeoIx*)gi;
  //TRI_Free(TRI_UNKNOWN_MEM_ZONE, gix->gxc);
  //TRI_Free(TRI_UNKNOWN_MEM_ZONE, gix->ypots);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, gix);
}
/* =================================================== */
/*        GeoMkHilbert   routine                       */
/* Points in this system are indexed by the "GeoString */
/* value, which is the distance to the point along the */
/* Hilbert Curve.  This space-filling curve is best    */
/* understood in a square, where the curve joins the   */
/* bottom left to the bottom right.  It consists of    */
/* four copies of the Hilbert curve, one in each of the*/
/* four squares, going via the points half-way up the  */
/* left side, the middle of the (large) square and half*/
/* way up the right side.  Notice that the first and   */
/* last of these are flipped on a diagonal, whereas the*/
/* middle two, going along the top half, are in the    */
/* original orientation, but at half the size.  This   */
/* description matches the code below, except that the */
/* two hemispheres are imagined to be squares where the*/
/* poles are the top line and the bottom line of the   */
/* square.                                             */
/* =================================================== */

/* 2^25 / 90 rounded down.  Used to convert */
/* degrees of longitude and latitude into   */
/* integers for use making a GeoString      */
#define STRINGPERDEGREE 372827.01
/* 2^26 - 1 = 0x3ffffff                     */
#define HILBERTMAX 67108863
GeoString GeoMkHilbert(GeoCoordinate* c) {
  /* math.h under MacOS defines y1 and j1 as global variable */
  double xx1, yy1;
  GeoString z;
  int x, y;
  int i, temp;
  yy1 = c->latitude + 90.0;
  z = 0;
  xx1 = c->longitude;
  if (c->longitude < 0.0) {
    xx1 = c->longitude + 180.0;
    z = 1;
  }
  x = (int)(xx1 * STRINGPERDEGREE);
  y = (int)(yy1 * STRINGPERDEGREE);
  for (i = 0; i < 26; i++) {
    z <<= 2;
    int nz = ((y >> 24) & 2) + (x >> 25);
    x = (x << 1) & (HILBERTMAX);
    y = (y << 1) & (HILBERTMAX);
    if (nz == 0) {
      temp = x;
      x = y;
      y = temp;
    }
    if (nz == 1) {
      temp = HILBERTMAX - x;
      x = HILBERTMAX - y;
      y = temp;
      z += 3;
    }
    if (nz == 2) {
      z += 1;
    }
    if (nz == 3) {
      z += 2;
    }
  }
  return z + 1ll;
}
/* =================================================== */
/*          GeoMkDetail  routine                       */
/* At the beginning of both searches, and also at the  */
/* start of an insert or remove, this routine is called*/
/* to compute all the detail that can usefully be found*/
/* once and for all.  The timings below were on done on*/
/* a 2011 ordinary desktop pentium                     */
/* 0.94 microseconds is - very approximately - 20% of  */
/* the execution time of searches and/or updates, so   */
/* is an obvious target for future speedups should they*/
/* be required (possibly by using less-accurate trig.  */
/* it consists of three essentially separate tasks     */
/*     1.  Find the GeoString (Hilbert) value.         */
/*     2.  compute the x, y and z coordinates          */
/*     3.  find the distances to the fixed points      */
/* all of these are needed for all of the operations   */
/* =================================================== */
#if GEOFIXLEN == 2
#define ARCSINFIX 41720.0
/* resolution about 300 meters  */
#endif
#if GEOFIXLEN == 4
#define ARCSINFIX 1520000000.0
/*  resolution about 3 cm  */
#endif
void GeoMkDetail(GeoIx* gix, GeoDetailedPoint* gd, GeoCoordinate* c) {
  /* entire routine takes about 0.94 microseconds  */
  /* math.h under MacOS defines y1 and j1 as global variable */
  gd->gix = gix;
  gd->gc = c;      //  XQXQ copies the pointer!
  /* The GeoString computation takes about 0.17 microseconds  */
  gd->gs = GeoMkHilbert(c);
  double const lat = c->latitude * M_PI / 180.0;
  double const lon = c->longitude * M_PI / 180.0;
  double latCos = cos(lat);
  /* This part takes about 0.32 microseconds  */
  gd->z = sin(lat);
  gd->x = latCos * cos(lon);
  gd->y = latCos * sin(lon);
  /* And this bit takes about 0.45 microseconds  */
  for (int i = 0; i < GeoIndexFIXEDPOINTS; i++) {
    double xx1 = (gix->fixed.x)[i];
    double yy1 = (gix->fixed.y)[i];
    double z1 = (gix->fixed.z)[i];
    double snmd = (xx1 - gd->x) * (xx1 - gd->x) + (yy1 - gd->y) * (yy1 - gd->y) +
           (z1 - gd->z) * (z1 - gd->z);
    (gd->fixdist)[i] = (GeoFix)(asin(sqrt(snmd) / 2.0) * ARCSINFIX);
  }
}
/* =================================================== */
/*                GeoMetersToSNMD                      */
/* When searching for a point "by distance" rather than*/
/* by count, this routine is used to reverse-engineer  */
/* the distance in meters into a Squared Normalized    */
/* Mole Distance (SNMD), since this is faster to       */
/* compute for each individual point.  Hence, rather   */
/* than convert all the distances to meters and compare*/
/* the system works backwards a bit so that, for each  */
/* point considered, only half of the distance         */
/* calculation needs to be done.  This is, of course   */
/* considerably faster.                                */
/* =================================================== */
double GeoMetersToSNMD(double meters) {
  double angle, hnmd;
  angle = 0.5 * meters / EARTHRADIAN;
  hnmd = sin(angle); /* half normalized mole distance  */
  if (angle >= M_PI / 2.0)
    return 4.0;
  else
    return hnmd * hnmd * 4.0;
}

double GeoFixtoSNMD(GeoFix gf) {
  double x;
  x = gf;
  x = x / ARCSINFIX;
  x = sin(x);
  x = x * x;
  x = x * 4.0;
  return x;
}
/* =================================================== */
/*                     GeoSetDistance                  */
/* During a search (of either type), the target point  */
/* is first "detailed".  When the distance of interest */
/* to the target point is known (either at the start   */
/* of a search-by-distance or each time a new good     */
/* point is found during a search-by-count) this       */
/* routine is called to set the snmd and distrej valeus*/
/* so that as much as possible is known to speed up    */
/* consideration of any new points                     */
/* =================================================== */
void GeoSetDistance(GeoDetailedPoint* gd, double snmd) {
  GeoFix gf;
  int i;
  gd->snmd = snmd;
  gf = (GeoFix)(asin(sqrt(snmd) / 2.0) * ARCSINFIX);
  gf++;
  for (i = 0; i < GeoIndexFIXEDPOINTS; i++) {
    if ((gd->fixdist)[i] <= gf)
      (gd->distrej)[i] = 0;
    else
      (gd->distrej)[i] = (gd->fixdist)[i] - gf;
  }
}

/* =================================================== */
/*                GeoStackSet  routine                 */
/* The searches (by count and by distance) both start  */
/* by detailing the point and then calling GeoStackSet */
/* Starting from the root pot (pot 1) the tree is      */
/* descended towards the (actually the earliest) pot   */
/* that could contain the target point.  As the        */
/* descent proceeds, the other child of each parent pot*/
/* is put onto the stack, so that after the routine    */
/* completes, the pots on the stack are a division of  */
/* the index into a set of (disjoint) intervals with   */
/* a strong tendency for the ones containing near      */
/* points (on the Hilbert curve, anyway) to be on the  */
/* to of the stack and to contain few points           */
/* =================================================== */
void GeoStackSet(GeoStack* gk, GeoDetailedPoint* gd, GeoResults* gr) {
  int pot;
  GeoIx* gix;
  GeoPot gp;
  gix = gd->gix;
  gk->gr = gr;
  gk->gd = gd;
  gk->stacksize = 0;
  pot = 1;
  while (1) {
    PotRead(gix,pot,&gp);
    if (gp.LorLeaf == 0) break;
    if (gp.middle > gd->gs) {
      gk->potid[gk->stacksize] = gp.RorPoints;
      pot = gp.LorLeaf;
    } else {
      gk->potid[gk->stacksize] = gp.LorLeaf;
      pot = gp.RorPoints;
    }
    gk->stacksize++;
  }
  gk->potid[gk->stacksize] = pot;
}
/* =================================================== */
/*            GeoResultsCons  routine                  */
/* Constructs (allocates) a new structure suitable for */
/* holding the results of a search.  The GeoResults    */
/* structure just holds the slotid of each point chosen*/
/* and the (SNMD) distance to the target point         */
/* =================================================== */
GeoResults* GeoResultsCons(int alloc) {
  GeoResults* gres;
  int* sa;
  double* dd;

  if (alloc <= 0) {
    return nullptr;
  }

  gres = static_cast<GeoResults*>(
      TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(GeoResults), false));
  sa = static_cast<int*>(
      TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, alloc * sizeof(int), false));
  dd = static_cast<double*>(
      TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, alloc * sizeof(double), false));
  if ((gres == nullptr) || (sa == nullptr) || (dd == nullptr)) {
    if (gres != nullptr) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, gres);
    }

    if (sa != nullptr) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, sa);
    }

    if (dd != nullptr) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, dd);
    }

    return nullptr;
  }
  gres->pointsct = 0;
  gres->allocpoints = alloc;
  gres->slot = sa;
  gres->snmd = dd;
  /* no need to initialize maxsnmd */
  return gres;
}
/* =================================================== */
/*                 GeoResultsStartCount                */
/* The GeoResultsCons routine allocates the memory     */
/* but if the search is by count, it is also necessary */
/* to initialize the results list with "fake" points   */
/* at the impossible SNMD distance of 10, so that any  */
/* real point will be closer than that and be taken    */
/* The GeoResultsStartCount routine does just that     */
/* =================================================== */
void GeoResultsStartCount(GeoResults* gr) {
  int i;
  for (i = 0; i < gr->allocpoints; i++) {
    gr->slot[i] = 0;
    gr->snmd[i] = 10.0;
  }
}
/* =================================================== */
/*              GeoResultsInsertPoint                  */
/* when a point is to be considered as a candidate for */
/* being returned in a search-by-count process, the    */
/* slot and snmd are presented to this routine.  If the*/
/* point is too distant, it is ignored.  Otherwise the */
/* most distant "old" point (which is always at zero   */
/* as the results are maintained as a priority queue   */
/* in this case) is discarded, and the new point must  */
/* be put into its proper place to re-establish the    */
/* priority queue - that every entry n is greater than */
/* or equal, in SNMD distance, than both its children  */
/* which are at 2n+1 and 2n+2                          */
/* =================================================== */
void GeoResultsInsertPoint(GeoResults* gr, int slot, double snmd) {
  /* math.h under MacOS defines y1 and j1 as global variable */
  int i, temp;
  if (snmd >= gr->snmd[0]) return;
  if (gr->slot[0] == 0) gr->pointsct++;
  i = 0; /* i is now considered empty  */
  while (1) {
    int jj1 = 2 * i + 1;
    int jj2 = 2 * i + 2;
    if (jj1 < gr->allocpoints) {
      if (jj2 < gr->allocpoints) {
        if (gr->snmd[jj1] > gr->snmd[jj2]) {
          temp = jj1;
          // jj1=jj2;
          jj2 = temp;
        }
        /* so now jj2 is >= jj1 */
        if (gr->snmd[jj2] <= snmd) {
          gr->snmd[i] = snmd;
          gr->slot[i] = slot;
          return;
        }
        gr->snmd[i] = gr->snmd[jj2];
        gr->slot[i] = gr->slot[jj2];
        i = jj2;
        continue;
      }
      if (gr->snmd[jj1] <= snmd) {
        gr->snmd[i] = snmd;
        gr->slot[i] = slot;
        return;
      }
      gr->snmd[i] = gr->snmd[jj1];
      gr->slot[i] = gr->slot[jj1];
      i = jj1;
      continue;
    }
    gr->snmd[i] = snmd;
    gr->slot[i] = slot;
    return;
  }
}
/* =================================================== */
/*                GeoResultsGrow                       */
/* During a search-by distance (the search-by-count    */
/* allocates the correct size at the outset) it may be */
/* necessary to return an unbounded amount of data.    */
/* initially 100 entries are allocted, but this routine*/
/* ensures that another one is available.  If the      */
/* allocation fails, -1 is returned.                   */
/* =================================================== */
int GeoResultsGrow(GeoResults* gr) {
  int newsiz;
  int* sa;
  double* dd;
  if (gr->pointsct < gr->allocpoints) return 0;
  /* otherwise grow by about 50%  */
  newsiz = gr->pointsct + (gr->pointsct / 2) + 1;
  if (newsiz > 1000000000) return -1;
  sa = static_cast<int*>(
      TRI_Reallocate(TRI_UNKNOWN_MEM_ZONE, gr->slot, newsiz * sizeof(int)));
  dd = static_cast<double*>(
      TRI_Reallocate(TRI_UNKNOWN_MEM_ZONE, gr->snmd, newsiz * sizeof(double)));
  if ((sa == nullptr) || (dd == nullptr)) {
    if (sa != nullptr) gr->slot = sa;
    if (dd != nullptr) gr->snmd = dd;
    return -1;
  }

  gr->slot = sa;
  gr->snmd = dd;
  gr->allocpoints = newsiz;
  return 0;
}

/* =================================================== */
/*                   GeoAnswers                        */
/* At the end of any search (of either type) the       */
/* GeoResults structure holds the slotid and snmd      */
/* distance of the points to be returned.  This routine*/
/* constructs and populates the GeoCoordinates         */
/* structure with the require data by fetching the     */
/* coodinates from the index, and by convertin the     */
/* snmd distance into meters.  It should be noticed    */
/* that the latitude and longitude are copied into the */
/* new data, so that the GeoCoordinates structure      */
/* remains valid even if the index is subsequently     */
/* updated or even freed.  NOTICE also that the        */
/* distances returned may not agree precisely with the */
/* distances that could be calculated by a separate    */
/* call to GeoIndex_distance because of rounding errors*/
/* =================================================== */
GeoCoordinates* GeoAnswers(GeoIx* gix, GeoResults* gr, bool returnDistances) {
  GeoCoordinates* ans;
  GeoCoordinate* gc;
  int i, j;
  double mole;

  if (gr->pointsct == 0) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, gr->slot);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, gr->snmd);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, gr);
    return nullptr;
  }

  ans = static_cast<GeoCoordinates*>(
      TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(GeoCoordinates), false));
  gc = static_cast<GeoCoordinate*>(TRI_Allocate(
      TRI_UNKNOWN_MEM_ZONE, gr->pointsct * sizeof(GeoCoordinate), false));

  if ((ans == nullptr) || (gc == nullptr)) {
    if (ans != nullptr) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, ans);
    }
    if (gc != nullptr) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, gc);
    }
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, gr->slot);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, gr->snmd);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, gr);
    return nullptr;
  }
  ans->length = gr->pointsct;
  ans->coordinates = gc;
  j = 0;
  for (i = 0; i < gr->allocpoints; i++) {
    if (j >= gr->pointsct) break;
    int slot = gr->slot[i];
    //GeoCoordinate * Rslot;
    if (slot == 0) continue;
    SlotRead(gix,slot,ans->coordinates+j);
    if (returnDistances) {
      mole = sqrt(gr->snmd[i]);
      if (mole > 2.0) mole = 2.0; /* make sure arcsin succeeds! */
      gr->snmd[j] = 2.0 * EARTHRADIAN * asin(mole / 2.0);
    }
    j++;
  }
  // note that these are uncalculated if returnDistances is false!
  ans->distances = gr->snmd;

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, gr->slot);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, gr);

  return ans;
}
/* =================================================== */
/*                 GeoPotJunk                          */
/* A detailed point containing the target point set    */
/* with the current distance is compared to a pot      */
/* If any of the fixed points are too close to all the */
/* descendents of a pot, 1 is returned to indicate that*/
/* the pot is "junk" = it may be ignored in its        */
/* entirety because it contains no points close enough */
/* to the target.  Otherwise 0 is returned.            */
/* =================================================== */
int GeoPotJunk(GeoDetailedPoint* gd, int pot) {
  int i;
  GeoPot gp;
  PotRead(gd->gix,pot,&gp);
  for (i = 0; i < GeoIndexFIXEDPOINTS; i++)
    if (gp.maxdist[i] < gd->distrej[i]) return 1;
  return 0;
}
/* =================================================== */
/*                   GeoSNMD                           */
/* Finds the SNMD (Squared NormalizedMole Distance)    */
/* from the point (which must be "detailed" gd, and the*/
/* ordinary point (just given by lat/longitude)        */
/* The cartesian coordinates of the ordinary point are */
/* found, and then the differences squared returned.   */
/* =================================================== */
double GeoSNMD(GeoDetailedPoint* gd, GeoCoordinate* c) {
  double const lat = c->latitude * M_PI / 180.0;
  double const lon = c->longitude * M_PI / 180.0;
  double const latCos = cos(lat);
  double x, y, z;
  z = sin(lat);
  x = latCos * cos(lon);
  y = latCos * sin(lon);
  return (x - gd->x) * (x - gd->x) + (y - gd->y) * (y - gd->y) +
         (z - gd->z) * (z - gd->z);
}
/* =================================================== */
/*           GeoIndex_PointsWithinRadius               */
/* This is the basic user-visible call to find all the */
/* the points in the index that are within the         */
/* specified distance of the target point              */
/* First the GeoIndex must be cast to the correct      */
/* (GeoIx) structure so that it can be used!           */
/* the result structure is then set up initially to    */
/* hold up to 100 results points, and the point is then*/
/* detailed (GeoString, x,y,z and distances to fixed   */
/* points).  The stack is then populated with the      */
/* initial descending set of pots ending with the one  */
/* nearest the target point, and the distance set on   */
/* the detailed point by converting the meters into an */
/* SNMD.  The pots on the stack are then considered.   */
/* If the call to GeoPotJunk indicates that there are  */
/* no points in that pot within the required circle,   */
/* the pot is discarded.  Otherwise, if the pot is a   */
/* leaf pot, the points are considered individually,   */
/* and notice the recovery to free everything if there */
/* is a need to grow the results structure and there   */
/* is not enough memory.  If the pot is not a leaf pot */
/* it is replaced on the stack by both its children    */
/* Processing continues until the stack is empty       */
/* At the end, the GeoAnswers routine is used to       */
/* convert the pot/snmd collection of the GeoResults   */
/* structure, into the distance (in meters) and the    */
/* GeoCoordinate data (lat/longitude and data pointer) */
/* needed for the return to the caller.                */
/* =================================================== */
GeoCoordinates* GeoIndex_PointsWithinRadius(GeoIdx* gi, GeoCoordinate* c,
                                            double d) {
  GeoResults* gres;
  GeoCoordinates* answer;
  GeoDetailedPoint gd;
  GeoStack gk;
  GeoPot gp;
  GeoCoordinate Xslot;
  int r, slot, i;
  double snmd, maxsnmd;
  GeoIx* gix;
  if (c->longitude < -180.0) return nullptr;
  if (c->longitude > 180.0) return nullptr;
  if (c->latitude < -90.0) return nullptr;
  if (c->latitude > 90.0) return nullptr;
  gix = (GeoIx*)gi;
  gres = GeoResultsCons(100);
  if (gres == nullptr) return nullptr;
  GeoMkDetail(gix, &gd, c);
  GeoStackSet(&gk, &gd, gres);
  maxsnmd = GeoMetersToSNMD(d);
  GeoSetDistance(&gd, maxsnmd);
  gk.stacksize++;
  while (gk.stacksize >= 1) {
    gk.stacksize--;
    int pot = gk.potid[gk.stacksize];
    if (GeoPotJunk(&gd, pot)) continue;
    PotRead(gix,pot,&gp);
    if (gp.LorLeaf == 0) {
      for (i = 0; i < gp.RorPoints; i++) {
        slot = gp.points[i];
        SlotRead(gix,slot,&Xslot);
        snmd = GeoSNMD(&gd, &Xslot);
        if (snmd > (maxsnmd * 1.00000000000001)) continue;
        r = GeoResultsGrow(gres);
        if (r == -1) {
          TRI_Free(TRI_UNKNOWN_MEM_ZONE, gres->snmd);
          TRI_Free(TRI_UNKNOWN_MEM_ZONE, gres->slot);
          TRI_Free(TRI_UNKNOWN_MEM_ZONE, gres);
          return nullptr;
        }
        gres->slot[gres->pointsct] = slot;
        gres->snmd[gres->pointsct] = snmd;
        gres->pointsct++;
      }
    } else {
      gk.potid[gk.stacksize++] = gp.LorLeaf;
      gk.potid[gk.stacksize++] = gp.RorPoints;
    }
  }
  answer = GeoAnswers(gix, gres, true);
  return answer; /* note - this may be nullptr  */
}
/* =================================================== */
/*            GeoIndex_NearestCountPoints              */
/* The other user-visible search call, which finds the */
/* nearest <count> points for a user-specified <count> */
/* processing is not dissimilar to the previous routine*/
/* but here the results structure is allocated at the  */
/* correct size and used as a priority queue.  Since   */
/* it always helps if more points are found (the       */
/* distance of interest drops, so that pots are more   */
/* readily rejected) some care is taken when a pot is  */
/* not rejected to put the one most likely to contain  */
/* useful points onto the top of the stack for early   */
/* processing.                                         */
/* =================================================== */
GeoCoordinates* GeoIndex_NearestCountPoints(GeoIdx* gi, GeoCoordinate* c,
                                            int count) {
  GeoResults* gr;
  GeoDetailedPoint gd;
  GeoCoordinates* answer;
  GeoStack gk;
  GeoPot gp;
  int slot, i, left;
  GeoCoordinate Xslot;
  double snmd;
  GeoIx* gix;
  if (c->longitude < -180.0) return nullptr;
  if (c->longitude > 180.0) return nullptr;
  if (c->latitude < -90.0) return nullptr;
  if (c->latitude > 90.0) return nullptr;

  gix = (GeoIx*)gi;
  gr = GeoResultsCons(count);
  if (gr == nullptr) return nullptr;
  GeoMkDetail(gix, &gd, c);
  GeoStackSet(&gk, &gd, gr);
  GeoResultsStartCount(gr);
  left = count;

  while (gk.stacksize >= 0) {
    int pot = gk.potid[gk.stacksize--];
    PotRead(gix,pot,&gp);
    if (left <= 0) {
      GeoSetDistance(&gd, gr->snmd[0]);
      if (GeoPotJunk(&gd, pot)) continue;
    }
    if (gp.LorLeaf == 0) {
      for (i = 0; i < gp.RorPoints; i++) {
        slot = gp.points[i];
        SlotRead(gix,slot,&Xslot);
        snmd = GeoSNMD(&gd, &Xslot);
        GeoResultsInsertPoint(gr, slot, snmd);
        left--;
        if (left < -1) left = -1;
      }
    } else {
      if (gd.gs > gp.middle) {
        gk.potid[++gk.stacksize] = gp.LorLeaf;
        gk.potid[++gk.stacksize] = gp.RorPoints;
      } else {
        gk.potid[++gk.stacksize] = gp.RorPoints;
        gk.potid[++gk.stacksize] = gp.LorLeaf;
      }
    }
  }
  answer = GeoAnswers(gix, gr, true);
  return answer; /* note - this may be nullptr  */
}
/* =================================================== */
/*             GeoIndexFreeSlot                        */
/* return the specified slot to the free list          */
/* =================================================== */
void GeoIndexFreeSlot(GeoIx* gix, int slot) {
  RocksDBKey key = RocksDBKey::GeoIndexValue(gix->objectId, slot, true);
  RocksDelete(gix, key);
}
/* =================================================== */
/*           GeoIndexNewSlot                           */
/* If there is a fre slot already on the free list,    */
/* just return its slot number.  Otherwise the entire  */
/* slot list is realloc'd.  Although this might change */
/* the physical memory location of all the indexed     */
/* points, this is not a problem since the slotid      */
/* values are not changed.                             */
/* The GeoIndexGROW, which specifies the percentage    */
/* of growth to be used, is in GeoIndex.h.  Notice also*/
/* that some care is take to ensure that, in the case  */
/* of memory allocation failure, the index is still    */
/* kept unchanged even though the new point cannot be  */
/* added to the index.                                 */
/* =================================================== */
int GeoIndexNewSlot(GeoIx* gix) {
  /*int j;
  GeoCoordinate* gc;
  if (gix->gxc[0].latitude == 0.0) {
    // do the growth calculation in long long to make sure it doesn't
    // overflow when the size gets to be near 2^31
    long long x = gix->slotct;
    long long y = 100 + GeoIndexGROW;
    x = x * y + 99;
    y = 100;
    x = x / y;
    if (x > 2000000000L) return -2;
    int newslotct = (int)x;
    gc = static_cast<GeoCoordinate*>(TRI_Reallocate(
        TRI_UNKNOWN_MEM_ZONE, gix->gxc, newslotct * sizeof(GeoCoordinate)));

    if (gc == nullptr) {
      return -2;
    }
    gix->gxc = gc;

    // update memory usage
    gix->_memoryUsed -= gix->slotct * sizeof(GeoCoordinate);
    gix->_memoryUsed += newslotct * sizeof(GeoCoordinate);

    for (j = gix->slotct; j < newslotct; j++) GeoIndexFreeSlot(gix, j);
    gix->slotct = newslotct;
  }
  j = (int)(gix->gxc[0].latitude);
  gix->gxc[0].latitude = gix->gxc[j].latitude;*/
  return gix->nextFreeSlot++;
}
/* =================================================== */
/*                 GeoFind                             */
/* This routine is used during insertion and removal,  */
/* but is not used during the searches.                */
/* Find the given point if it is in the index, and set */
/* the GeoPath data structure to give the path from the*/
/* root pot (pot 1) to the leaf pot, if any, containing*/
/* the sepecified (detailed) point, or - if the point  */
/* is not present, to the first leaf pot into which the*/
/* specified point may be inserted.                    */
/* To start with, the index tree is descended, starting*/
/* with the root (which, rather bizzarly, is at the    */
/* top of this tree!) always taking the right branch if*/
/* both would do, to reach the rightmost leaf pot that */
/* could contain the specified point.                  */
/* We then proceed leftwards through the points until  */
/* either the specified point is found in the index, or*/
/* the first leaf pot is found that could contain the  */
/* specified point.  It is worth noting that the first */
/* pot of all has "low-values" as its "start" GeoString*/
/* so that this process cannot go off the front of the */
/* index.  Notice also that it is not expected to be   */
/* very common that a large number of points with the  */
/* same GeoString (so within 30 centimeters!) will be  */
/* inserted into the index, and that even if there are */
/* the inefficiency of this code is only moderate, and */
/* manifests itself only during maintenance            */
/* the return value is 1 if the point is found and 2   */
/* if it is not found                                  */
/* =================================================== */
int GeoFind(GeoPath* gt, GeoDetailedPoint* gd) {
  int pot, pot1;
  int i;
  int slot;
  GeoIx* gix;
  GeoCoordinate gc;
  GeoPot gp;
  gix = gd->gix;
  gt->gix = gix;
  pot = 1;
  gt->pathlength = 0;
  while (1) {
    PotRead(gix,pot,&gp);
    gt->path[gt->pathlength] = pot;
    gt->pathlength++;
    if (gp.LorLeaf == 0) break;
    if (gp.middle > gd->gs)
      pot = gp.LorLeaf;
    else
      pot = gp.RorPoints;
  }
  /* so we have a pot such that top is bigger but bottom isn't  */
  while (1) /* so look for an exact match  */
  {
    for (i = 0; i < gp.RorPoints; i++) {
      slot = gp.points[i];
      SlotRead(gix,slot,&gc);
      if (((gd->gc)->latitude == gc.latitude) &&
          ((gd->gc)->longitude == gc.longitude) &&
          ((gd->gc)->data == gc.data)) {
        gt->path[gt->pathlength] = i;
        return 1;
      }
    }
    if (gp.start < gd->gs) break;
    /* need to find the predecessor of this pot  */
    /* this is expected to be a rare event, so   */
    /* no time is wasted  to simplify this!      */
    while (1) {
      gt->pathlength--;
      pot1 = gt->path[gt->pathlength - 1];
      PotRead(gix,pot1,&gp);
      if (pot == gp.RorPoints) break; /* cannot go off the front  */
      pot = pot1;
    }
    PotRead(gix,pot1,&gp);
    pot = gp.LorLeaf;
    /* now we have a pot whose iterated right child we want  */
    while (1) {
      PotRead(gix,pot,&gp);
      gt->path[gt->pathlength] = pot;
      gt->pathlength++;
      if (gp.LorLeaf == 0) break;
      pot = gp.RorPoints;
    }
  }
  return 2;
}
/* =================================================== */
/*          GeoPopulateMaxdist                         */
/* During maintencance, when the points in a leaf pot  */
/* have been changed, this routine merely looks at all */
/* the points in the pot, details them, and rebuilds   */
/* the list of maximum distances.                      */
/* =================================================== */
void GeoPopulateMaxdist(GeoIx* gix, GeoPot* gp, GeoString* gsa) {
  int i, j;
  GeoDetailedPoint gd;
  GeoCoordinate Xslot;
  gsa[0] = 0x1FFFFFFFFFFFFFll;
  gsa[1] = 0ll;
  for (j = 0; j < GeoIndexFIXEDPOINTS; j++) gp->maxdist[j] = 0;
  for (i = 0; i < gp->RorPoints; i++) {
    SlotRead(gix,gp->points[i],&Xslot);
    GeoMkDetail(gix, &gd, &Xslot);
    for (j = 0; j < GeoIndexFIXEDPOINTS; j++)
      if (gd.fixdist[j] > gp->maxdist[j]) gp->maxdist[j] = gd.fixdist[j];
    if (gd.gs < gsa[0]) gsa[0] = gd.gs;
    if (gd.gs > gsa[1]) gsa[1] = gd.gs;
  }
  gp->level = 1;
}
/* =================================================== */
/*              GeoGetPot                              */
/* This routine simply converts a path and a height    */
/* into a pot id.                                      */
/* =================================================== */
int GeoGetPot(GeoPath* gt, int height) {
  return gt->path[gt->pathlength - height];
}
/* =================================================== */
/*             GeoAdjust                               */
/* During insertion and deletion, this routine is used */
/* to populate the data correctly for the parent pot   */
/* specified (which may not be a leaf pot) by taking   */
/* the data from the child pots.  It populates the     */
/* start, middle and end GeoStrings, the level, and    */
/* the maximum distances to the fixed points.          */
/* =================================================== */
void GeoAdjust(GeoIx* gix, int potx) /* the kids are alright */
{
  int poty, potz; /* x = (yz)  */
  int i;
  GeoPot gpx;
  GeoPot gpy;
  GeoPot gpz;
  PotRead(gix,potx,&gpx);
  poty = gpx.LorLeaf;
  PotRead(gix,poty,&gpy);
  potz = gpx.RorPoints;
  PotRead(gix,potz,&gpz);
  gpx.start = gpy.start;
  gpx.end = gpz.end;
  gpx.middle = gpz.start;
  gpx.level = gpy.level;
  if ((gpz.level) > gpx.level) gpx.level = gpz.level;
  gpx.level++;
  for (i = 0; i < GeoIndexFIXEDPOINTS; i++) {
    gpx.maxdist[i] = gpy.maxdist[i];
    if (gpx.maxdist[i] < gpz.maxdist[i]) gpx.maxdist[i] = gpz.maxdist[i];
  }
  PotWrite(gix,potx,&gpx);
}
  
/* =================================================== */
/*             New GeoAdjust = GeoDistLev              */
/* During insertion and deletion, this routine is used */
/* to populate the data correctly for the parent pot   */
/* gpx from the child pots gpy, gpz.  It populates the */
/* start, middle and end GeoStrings, the level, and    */
/* the maximum distances to the fixed points.          */
/* =================================================== */

void GeoDistLev(GeoPot * gpx, GeoPot * gpy, GeoPot * gpz)
{
  int i;
  gpx->start = gpy->start;
  gpx->end = gpz->end;
  gpx->middle = gpz->start;
  gpx->level = gpy->level;
  if ((gpz->level) > gpx->level) gpx->level = gpz->level;
  gpx->level++;
  for (i = 0; i < GeoIndexFIXEDPOINTS; i++) {
    gpx->maxdist[i] = gpy->maxdist[i];
    if (gpx->maxdist[i] < gpz->maxdist[i]) gpx->maxdist[i] = gpz->maxdist[i];
  }
}
  
/* =================================================== */
/*             RotateLeft                              */
/* The operation used during tree balancing to convert */
/* A(BC) into (AB)C.  To start with, E is A(BC) and    */
/* D is BC.  D is then change to be (AB) and           */
/* GeoAdjust is used to re-populate its data.  E is    */
/* then set to be DC = (AB)C, and again GeoAdjust is   */
/* used to set the GeoStrings, level and distances to  */
/* the fixed points, taking the data from the children */
/* in both cases                                       */
/* =================================================== */
void RotateLeft(GeoIx* gix, int pote) {
  int pota, potb, potc, potd;
  GeoPot gpa,gpb,gpc,gpd,gpe;
  PotRead(gix,pote,&gpe);
  potd = gpe.RorPoints;
  PotRead(gix,potd,&gpd);
  pota = gpe.LorLeaf;
  potb = gpd.LorLeaf;
  potc = gpd.RorPoints;
  PotRead(gix,pota,&gpa);
  PotRead(gix,potb,&gpb);
  PotRead(gix,potc,&gpc);
  GeoDistLev(&gpd,&gpa,&gpb);
  gpd.LorLeaf = pota;
  gpd.RorPoints = potb;
  PotWrite(gix,potd,&gpd);
  GeoDistLev(&gpe,&gpd,&gpc);
  gpe.LorLeaf = potd;
  gpe.RorPoints = potc;
  PotWrite(gix,pote,&gpe);
}
/* =================================================== */
/*                 RotateRight                         */
/* The mirror-image or inverse of RotateLeft.          */
/* Changes (AB)C into A(BC).  The given parent pot is  */
/* E = (AB)C and D is AB.  D is then reused to be BC   */
/* and GeoAdjusted, and then E set to be AD = A(BC) and*/
/* also GeoAdjusted                                    */
/* =================================================== */
void RotateRight(GeoIx* gix, int pote) {
  int pota, potb, potc, potd;
  GeoPot gpa,gpb,gpc,gpd,gpe;
  PotRead(gix,pote,&gpe);
  potd = gpe.LorLeaf;
  PotRead(gix,potd,&gpd);
  pota = gpd.LorLeaf;
  potb = gpd.RorPoints;
  potc = gpe.RorPoints;
  PotRead(gix,pota,&gpa);
  PotRead(gix,potb,&gpb);
  PotRead(gix,potc,&gpc);
  gpd.LorLeaf = potb;
  gpd.RorPoints = potc;
  GeoDistLev(&gpd,&gpb,&gpc);
  PotWrite(gix,potd,&gpd);  // same inefficiency as RotateLeft;
  gpe.LorLeaf = pota;
  gpe.RorPoints = potd;
  GeoDistLev(&gpe,&gpa,&gpd);
  PotWrite(gix,pote,&gpe);
}
/* =================================================== */
/*        GeoIndex_insert                              */
/* The user-facing routine to insert a new point into  */
/* the index.  First the index is cast into a GeoIx    */
/* so that it can be used, and then the point is       */
/* sanity checked.  The point is then detailed and the */
/* GeoFind routine called.  If the point is found, this*/
/* is an error.  Otherwise a new slot is populated with*/
/* the data from the point, and then the point is put  */
/* into the first leaf pot into which it may go based  */
/* on its GeoString value.  If there is no room in that*/
/* pot, the pot is split into two (necessitating a tree*/
/* balancing operation) which starts by obtaining the  */
/* two new pots. . . continued below                   */
/* =================================================== */
int GeoIndex_insert(GeoIdx* gi, GeoCoordinate* c) {
  int i, j, slot, pot, pot1;
  int pota, poty, potz;
  int lva, lvy, lvz;
  int height, rebalance;
  GeoDetailedPoint gd;
  GeoCoordinate Xslot;
  GeoPath gt;
  GeoPot gp;
  GeoPot gp1;
  GeoPot gp2;
  GeoPot gpx;
  GeoPot gpy;
  GeoPot gpz;
  GeoPot gpa;
  GeoString gsa[2];
  GeoIx* gix;
  gix = (GeoIx*)gi;
  rebalance = 0;
  if (c->longitude < -180.0) return -3;
  if (c->longitude > 180.0) return -3;
  if (c->latitude < -90.0) return -3;
  if (c->latitude > 90.0) return -3;
  GeoMkDetail(gix, &gd, c);
  i = GeoFind(&gt, &gd);
  if (i == 1) return -1;
  pot = gt.path[gt.pathlength - 1];
  PotRead(gix,pot,&gp);
  /* new point, so we try to put it in  */
  slot = GeoIndexNewSlot(gix);
  if (slot == -2) return -2; /* no room  :(  */
  //SlotRead(gix,slot,&Xslot);
  Xslot.latitude = c->latitude;
  Xslot.longitude = c->longitude;
  Xslot.data = c->data;
  SlotWrite(gix,slot,&Xslot);
// XQXQ need to insert this
  /* check first if we are going to need two new pots, and  */
  /* if we are, go get them now before we get  too tangled  */
  if (gp.RorPoints == GeoIndexPOTSIZE) {
    rebalance = 1;
    pot1 = GeoIndexNewPot(gix);
    int pot2 = GeoIndexNewPot(gix);
    //PotRead(gix,pot,&gp); /* XQXQ won't have to do this on Rocks */
    if ((pot1 == -2) || (pot2 == -2)) {
      GeoIndexFreeSlot(gix, slot);
      if (pot1 != -2) GeoIndexFreePot(gix, pot1);
      if (pot2 != -2) GeoIndexFreePot(gix, pot2);
      return -2;
    }
    /* =================================================== */
    /*         GeoIndex_insert continued                   */
    /* New pots are pot1 and pot2 which will be the new    */
    /* leaf pots with half the points each, and the old    */
    /* pot will become the parent of both of them          */
    /* After moving all the points to pot2, the half with  */
    /* the lowest GeoString are moved into pot1.  The two  */
    /* pots are then inspected with GeoPopulateMaxdist     */
    /* to ascertain what the actual distances and GeoString*/
    /* values are.  The GeoString boundary between the two */
    /* pots is set at the midpoint between the current     */
    /* actual boundaries and finally the current pot is    */
    /* set to be either pot1 or pot2 depending on where the*/
    /* new point (which has still not been inserted) shoud */
    /* go.   Continued below . . . .                       */
    /* =================================================== */
    //PotRead(gix,pot1,&gp1);  //XQXQ don't think this is even needed now!
    //PotRead(gix,pot2,&gp2);  //XQXQ on Rocks it is reading a pot that is not there.
    /* pot is old one, pot1 and pot2 are the new ones  */
    gp1.LorLeaf = 0;   /* leaf pot  */
    gp1.RorPoints = 0; /* no points in it yet  */
                        /* first move the points from pot to pot2          */
    gp2.LorLeaf = 0;   /* leaf pot  */
    gp2.RorPoints = gp.RorPoints;
    for (i = 0; i < gp.RorPoints; i++) gp2.points[i] = gp.points[i];
    /* move the first half of the points from pot2 to pot1 */
    GeoString gsl[GeoIndexPOTSIZE],mid,mings;
    for(j=0;j<GeoIndexPOTSIZE;j++) {
      SlotRead(gix,gp2.points[j],&Xslot);
      gsl[j] = GeoMkHilbert(&Xslot);
    }
    for (i = 0; i < (GeoIndexPOTSIZE / 2); i++) {
      int jj1=std::numeric_limits<int>::max();
      mid=0x1FFFFFFFFFFFFFll;
      for(j=0;j<GeoIndexPOTSIZE;j++)
      {
        // some value has to be less than mid or we will end up in the assert
        if(gsl[j]==0xfffffffffffffffful) continue;
        if(gsl[j]<mid)
        {
          jj1=j;
          mid=gsl[j];
        }
      }
      TRI_ASSERT(jj1 != std::numeric_limits<int>::max()); // jj1 -- must have been set
      gsl[jj1]=0xfffffffffffffffful;
    }
    for (i = 0; i < GeoIndexPOTSIZE; i++) {
      if(gsl[i]!=0xfffffffffffffffful) continue;
      gp1.points[gp1.RorPoints] = gp2.points[i];
      gp1.RorPoints++;
    }
    j=0;
    for (i = 0; i < GeoIndexPOTSIZE; i++) {
      gp2.points[j] = gp2.points[i];
      if(gsl[i]==0xfffffffffffffffful)
        gp2.RorPoints--;
      else
        j++;
    }
    GeoPopulateMaxdist(gix, &gp2, gsa);
    mings = gsa[0];
    GeoPopulateMaxdist(gix, &gp1, gsa);
    mings = (mings + gsa[1]) / 2ll;
    gp1.start = gp.start;
    gp1.end = mings;
    gp2.start = mings;
    gp2.end = gp.end;
    gp.LorLeaf = pot1;
    gp.RorPoints = pot2;
    GeoDistLev(&gp,&gp1,&gp2);
    PotWrite(gix,pot,&gp);
    PotWrite(gix,pot1,&gp1);
    PotWrite(gix,pot2,&gp2);
    gt.pathlength++;
    if (gd.gs < mings) {
      gp = gp1;
      pot=pot1;
      gt.path[gt.pathlength - 1] = pot1;
    } else {
      gp = gp2;
      pot=pot2;
      gt.path[gt.pathlength - 1] = pot2;
    }
  }
  /* =================================================== */
  /*          GeoIndex_insert continued                  */
  /* finally the new point is inserted into the pot, and */
  /* the maximum distances to the fixed points propogated*/
  /* up as far as necessary.  The rebalancing of the tree*/
  /* is then done, but only if the pot splitting happend */
  /* to rebalance, the sequence of pots going back up is */
  /* traversed using the path structure, and the standard*/
  /* AVL balancing is used by doing the necessary        */
  /* rotations and level changes necessary to ensure that*/
  /* every parent has at least one child one level lower */
  /* and the other child is either also one level lower, */
  /* or two levels lower.  The details are also given in */
  /* the accompanying documentation                      */
  /* =================================================== */
  /* so we have a pot and a path we can use   */
  /* gp is the pot, gt set correctly          */
  gp.points[gp.RorPoints] = slot;
  gp.RorPoints++;
  PotWrite(gix,pot,&gp);
  /* now propagate the maxdistances */
  j=gt.pathlength-1;
  while(j>=0) {
    int changed=0;
    PotRead(gix,gt.path[j],&gpa);
    for (i = 0; i < GeoIndexFIXEDPOINTS; i++) {
      if (gd.fixdist[i] > gpa.maxdist[i]) {
        gpa.maxdist[i] = gd.fixdist[i];
        changed=1;
      }
    }
    if(changed==0)  break;
    PotWrite(gix,gt.path[j],&gpa);
    j--;
  }
  /* just need to balance the tree  */
  if (rebalance == 0) return 0;
  height = 2;
  while (true) {
    int potx = GeoGetPot(&gt, height);
    PotRead(gix,potx,&gpx);
    int lvx = gpx.level;
    if (potx == 1) break;
    /* root pot ?      */
    pot1 = GeoGetPot(&gt, height + 1); /* pot1=parent(x)  */
    PotRead(gix,pot1,&gp1);
    int lv1 = gp1.level;
    if (lv1 > lvx) break;
    if (gp1.LorLeaf == potx) /* gpx is the left child? */
    {
      pota = gp1.RorPoints; /* 1 = (xa)  */
      PotRead(gix,pota,&gpa);
      lva = gpa.level;
      if ((lva + 1) == lv1) /* so it is legal to up lev(1) */
      {
        gp1.level++;
        PotWrite(gix,pot1,&gp1);
        height++;
        continue;
      }
      poty = gpx.RorPoints;
      PotRead(gix,poty,&gpy);
      lvy = gpy.level;
      potz = gpx.LorLeaf;
      PotRead(gix,potz,&gpz);
      lvz = gpz.level;
      if (lvy <= lvz) {
        RotateRight(gix, pot1);
        height++;
        continue;
      }
      RotateLeft(gix, potx);
      RotateRight(gix, pot1);
    } else /* gpx is the right child */
    {
      pota = gp1.LorLeaf; /* 1 = (ax)  */
      PotRead(gix,pota,&gpa);
      lva = gpa.level;
      if ((lva + 1) == lv1) /* so it is legal to up lev(1) */
      {
        gp1.level++;
        PotWrite(gix,pot1,&gp1);
        height++;
        continue;
      }
      poty = gpx.LorLeaf;
      PotRead(gix,poty,&gpy);
      lvy = gpy.level;
      potz = gpx.RorPoints;
      PotRead(gix,potz,&gpz);
      lvz = gpz.level;
      if (lvy <= lvz) {
        RotateLeft(gix, pot1);
        height++;
        continue;
      }
      RotateRight(gix, potx);
      RotateLeft(gix, pot1);
    }
  }
  return 0;
}
/* =================================================== */
/*            GeoIndex_remove                          */
/* As a user-facing routine, this starts by casting the*/
/* GeoIndex structure to a GeoIx, so that its members  */
/* can be accessed.  The point is then detailed, and   */
/* GeoFind is used to check whether it is there.  If   */
/* not, this is an error.  Otherwise the point is      */
/* removed from the pot and the distances recalculated */
/* using the GeoPopulateMaxdist routine.  It is then   */
/* checked whether there are now too few points in the */
/* pot that used to contain the point, and if so there */
/* are eight cases as to what is to be done.  In four  */
/* of them, a point is moved from the adjacent leaf pot*/
/* which may be at the same level or one lower, and may*/
/* be either side of the current one.  This is done if */
/* there are too many points in the two leaf pots to   */
/* amalgamate them.  In the other four cases the two   */
/* leaf pots are amalgamated, which results in the     */
/* releasing of two pots (which are put back into the  */
/* free chain using GeoIndexFreePot) Continued . . . . */
/* =================================================== */
int GeoIndex_remove(GeoIdx* gi, GeoCoordinate* c) {
  GeoDetailedPoint gd;
  GeoCoordinate Xslot;
  int rebalance;
  int levn, levc;
  GeoPot gp;
  int potp;
  GeoPot gpp;
  int potb;
  GeoPot gpb;
  int potn;
  GeoPot gpn;
  int potc;
  GeoPot gpc;
  GeoPath gt;
  GeoString gsa[2];
  int i, pot, potix, slot, pathix;
  GeoIx* gix;
  if (c->longitude < -180.0) return -3;
  if (c->longitude > 180.0) return -3;
  if (c->latitude < -90.0) return -3;
  if (c->latitude > 90.0) return -3;
  gix = (GeoIx*)gi;
  GeoMkDetail(gix, &gd, c);
  i = GeoFind(&gt, &gd);
  if (i != 1) return -1;
  pot = gt.path[gt.pathlength - 1];
  PotRead(gix,pot,&gp);
  potix = gt.path[gt.pathlength];
  slot = gp.points[potix];
  GeoIndexFreeSlot(gix, slot);              // XQXQ Need to delete slot
  gp.points[potix] = gp.points[gp.RorPoints - 1];
  gp.RorPoints--;
  GeoPopulateMaxdist(gix, &gp, gsa);
  PotWrite(gix,pot,&gp);
  if (pot == 1) return 0; /* just allow root pot to have fewer points */
  rebalance = 0;
  if ((2 * gp.RorPoints) < GeoIndexPOTSIZE) {
    int j, js;
    GeoString mings, gs;
    potp = gt.path[gt.pathlength - 2];
    PotRead(gix,potp,&gpp);
    if (gpp.LorLeaf == pot) {
      /*  Left   */
      potb = gpp.RorPoints;
      PotRead(gix,potb,&gpb);
      if (gpb.LorLeaf == 0) {
        /*  Left Brother  */
        if ((gpb.RorPoints + gp.RorPoints) > GeoIndexPOTSIZE) {
          /*  Left Brother Lots  */
          mings = 0x1FFFFFFFFFFFFFll;
          js = 0;
          for (j = 0; j < gpb.RorPoints; j++) {
            SlotRead(gix,gpb.points[j],&Xslot);
            gs = GeoMkHilbert(&Xslot);
            if (gs < mings) {
              mings = gs;
              js = j;
            }
          }
          gp.points[gp.RorPoints] = gpb.points[js];
          gpb.points[js] = gpb.points[gpb.RorPoints - 1];
          gpb.RorPoints--;
          gp.RorPoints++;
          GeoPopulateMaxdist(gix, &gp, gsa);
          mings = gsa[1];
          GeoPopulateMaxdist(gix, &gpb, gsa);
          mings = (mings + gsa[0]) / 2ll;
          gp.end = mings;
          gpb.start = mings;
          gpp.middle = mings;
          PotWrite(gix,pot,&gp);
          PotWrite(gix,potb,&gpb);
          GeoAdjust(gix, potp);
        } else {
          /*  Left Brother Few  */
          gpp.LorLeaf = 0;
          i = 0;
          for (j = 0; j < gpb.RorPoints; j++)
            gpp.points[i++] = gpb.points[j];
          for (j = 0; j < gp.RorPoints; j++) gpp.points[i++] = gp.points[j];
          gpp.RorPoints = i;
          GeoIndexFreePot(gix, pot);
          GeoIndexFreePot(gix, potb);
          GeoPopulateMaxdist(gix, &gpp, gsa);
          gt.pathlength--;
          rebalance = 1;
          PotWrite(gix,potp,&gpp);
        }
      } else {
        /* Left Nephew */
        potn = gpb.LorLeaf;
        PotRead(gix,potn,&gpn);
        if ((gpn.RorPoints + gp.RorPoints) > GeoIndexPOTSIZE) {
          /*  Left Nephew Lots  */
          mings = 0x1FFFFFFFFFFFFFll;
          js = 0;
          for (j = 0; j < gpn.RorPoints; j++) {
            SlotRead(gix,gpn.points[j],&Xslot);
            gs = GeoMkHilbert(&Xslot);
            if (gs < mings) {
              mings = gs;
              js = j;
            }
          }
          gp.points[gp.RorPoints] = gpn.points[js];
          gpn.points[js] = gpn.points[gpn.RorPoints - 1];
          gpn.RorPoints--;
          gp.RorPoints++;
          GeoPopulateMaxdist(gix, &gp, gsa);
          mings = gsa[1];
          GeoPopulateMaxdist(gix, &gpn, gsa);
          mings = (mings + gsa[0]) / 2ll;
          gp.end = mings;
          gpn.start = mings;
          gpb.start = mings;
          gpp.middle = mings;
          PotWrite(gix,pot,&gp);
          PotWrite(gix,potn,&gpn);
          GeoAdjust(gix, potb);
          GeoAdjust(gix, potp);
        } else {
          /*  Left Nephew Few  */
          potc = gpb.RorPoints;
          i = gp.RorPoints;
          for (j = 0; j < gpn.RorPoints; j++) gp.points[i++] = gpn.points[j];
          gp.RorPoints = i;
          gpp.RorPoints = potc;
          gpp.middle = gpb.middle;
          gp.end = gpp.middle;
          GeoIndexFreePot(gix, potn);
          GeoIndexFreePot(gix, potb);
          GeoPopulateMaxdist(gix, &gp, gsa);
          PotWrite(gix,pot,&gp);
          PotWrite(gix,potp,&gpp);
          GeoAdjust(gix, potp);
          gt.pathlength--;
          rebalance = 1;
        }
      }
    } else {
      /*  Right  */
      potb = gpp.LorLeaf;
      PotRead(gix,potb,&gpb);
      if (gpb.LorLeaf == 0) {
        /*  Right Brother  */
        if ((gpb.RorPoints + gp.RorPoints) > GeoIndexPOTSIZE) {
          /*  Right Brother Lots  */
          mings = 0ll;
          js = 0;
          for (j = 0; j < gpb.RorPoints; j++) {
            SlotRead(gix,gpb.points[j],&Xslot);
            gs = GeoMkHilbert(&Xslot);
            if (gs > mings) {
              mings = gs;
              js = j;
            }
          }
          gp.points[gp.RorPoints] = gpb.points[js];
          gpb.points[js] = gpb.points[gpb.RorPoints - 1];
          gpb.RorPoints--;
          gp.RorPoints++;
          GeoPopulateMaxdist(gix, &gp, gsa);
          mings = gsa[0];
          GeoPopulateMaxdist(gix, &gpb, gsa);
          mings = (mings + gsa[1]) / 2ll;
          gp.start = mings;
          gpb.end = mings;
          gpp.middle = mings;
          PotWrite(gix,pot,&gp);
          PotWrite(gix,potb,&gpb);
          GeoAdjust(gix, potp);
        } else {
          /*  Right Brother Few  */
          /* observe this is identical to Left Brother Few  */
          gpp.LorLeaf = 0;
          i = 0;
          for (j = 0; j < gpb.RorPoints; j++)
            gpp.points[i++] = gpb.points[j];
          for (j = 0; j < gp.RorPoints; j++) gpp.points[i++] = gp.points[j];
          gpp.RorPoints = i;
          GeoIndexFreePot(gix, pot);
          GeoIndexFreePot(gix, potb);
          GeoPopulateMaxdist(gix, &gpp, gsa);
          gt.pathlength--;
          rebalance = 1;
          //PotWrite(gix,pot,&gp);
          PotWrite(gix,potp,&gpp);
        }
      } else {
        /* Right Nephew */
        potn = gpb.RorPoints;
        PotRead(gix,potn,&gpn);
        if ((gpn.RorPoints + gp.RorPoints) > GeoIndexPOTSIZE) {
          /*  Right Nephew Lots  */
          mings = 0ll;
          js = 0;
          for (j = 0; j < gpn.RorPoints; j++) {
            SlotRead(gix,gpn.points[j],&Xslot);
            gs = GeoMkHilbert(&Xslot);
            if (gs > mings) {
              mings = gs;
              js = j;
            }
          }
          gp.points[gp.RorPoints] = gpn.points[js];
          gpn.points[js] = gpn.points[gpn.RorPoints - 1];
          gpn.RorPoints--;
          gp.RorPoints++;
          GeoPopulateMaxdist(gix, &gp, gsa);
          mings = gsa[0];
          GeoPopulateMaxdist(gix, &gpn, gsa);
          mings = (mings + gsa[1]) / 2ll;
          gp.start = mings;
          gpn.end = mings;
          gpb.end = mings;
          gpp.middle = mings;
          PotWrite(gix,pot,&gp);
          PotWrite(gix,potn,&gpn);
          GeoAdjust(gix, potb);
          GeoAdjust(gix, potp);
        } else {
          /*  Right Nephew Few  */
          potc = gpb.LorLeaf;
          i = gp.RorPoints;
          for (j = 0; j < gpn.RorPoints; j++) gp.points[i++] = gpn.points[j];
          gp.RorPoints = i;
          gpp.LorLeaf = potc;
          gpp.middle = gpb.middle;
          gp.start = gpb.middle;
          GeoIndexFreePot(gix, potn);
          GeoIndexFreePot(gix, potb);
          GeoPopulateMaxdist(gix, &gp, gsa);
          PotWrite(gix,pot,&gp);
          PotWrite(gix,potp,&gpp);
          GeoAdjust(gix, potp);
          gt.pathlength--;
          rebalance = 1;
        }
      }
    }
  }
  /* =================================================== */
  /*          GeoIndex_remove    continued               */
  /* Again the balancing of the tree is fairly standard  */
  /* and documented in the associated documentation to   */
  /* this routine.  At every stage in this process the   */
  /* parent potp of the current pot may not be balanced  */
  /* as pot has just had its level reduced.  To tell what*/
  /* to do, the product i of the level differences is    */
  /* calculated.  This should be 1 or 2, but may be 3 or */
  /* 4, and in each case some further investigation soon */
  /* shows what rotations and further upward balancing   */
  /* may be needed.           continued . . .            */
  /* =================================================== */
  pathix = gt.pathlength - 1;
  while ((pathix > 0) && (rebalance == 1)) {
    /* Deletion rebalancing  */
    rebalance = 0;
    pathix--;
    potp = gt.path[pathix];
    PotRead(gix,potp,&gpp);
    int levp = gpp.level;
    pot = gpp.LorLeaf;
    potb = gpp.RorPoints;
    PotRead(gix,pot,&gp);
    PotRead(gix,potb,&gpb);
    int lev = gp.level;
    int levb = gpb.level;
    i = (levp - lev) * (levp - levb);
    if (i == 4) {
      gpp.level--;
      PotWrite(gix,potp,&gpp);
      rebalance = 1;
    }
    if (i == 3) {
      if ((levp - lev) == 3) {
        potn = gpb.LorLeaf;
        PotRead(gix,potn,&gpn);
        potc = gpb.RorPoints;
        PotRead(gix,potc,&gpc);
        levn = gpn.level;
        levc = gpc.level;
        if (levn <= levc) {
          RotateLeft(gix, potp);
          if (levn < levc) rebalance = 1;
        } else {
          RotateRight(gix, potb);
          RotateLeft(gix, potp);
          rebalance = 1;
        }
      } else {
        potn = gp.LorLeaf;
        PotRead(gix,potn,&gpn);
        potc = gp.RorPoints;
        PotRead(gix,potc,&gpc);
        levn = gpn.level;
        levc = gpc.level;
        if (levn >= levc) {
          RotateRight(gix, potp);
          if (levn > levc) rebalance = 1;
        } else {
          RotateLeft(gix, pot);
          RotateRight(gix, potp);
          rebalance = 1;
        }
      }
    }
    GeoAdjust(gix, potp);
  }
  /* =================================================== */
  /*          GeoIndex_remove    continued               */
  /* In the case of deletion, it is not so easy to see   */
  /* what the new maximum distances are given the point  */
  /* deleted, so the GeoAdjust routine is used all the   */
  /* way up.                                             */
  /* =================================================== */
  while (pathix > 0) {
    pathix--;
    pot = gt.path[pathix];
    GeoAdjust(gix, pot);
  }
  return 0;
}
/* =================================================== */
/*                GeoIndex_CoordinatesFree             */
/* The user-facing routine that must be called by the  */
/* user when the results of a search are finished with */
/* =================================================== */
void GeoIndex_CoordinatesFree(GeoCoordinates* clist) {
  if (clist == nullptr) {
    return;
  }
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, clist->coordinates);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, clist->distances);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, clist);
}
/* =================================================== */
/*            GeoIndex_hint does nothing!              */
/* it is here for possible future compatibilty         */
/* =================================================== */
int GeoIndex_hint(GeoIdx* gi, int hint) { return 0; }

/* =================================================== */
/*                 GeoCr structure                     */
/* This is the REAL GeoCursor structure - the one in   */
/* the GeoIndex.h file is just a sham (it says it is   */
/* a char!) to keep the structure private so that the  */
/* GeoIndex.h is short and contains only data of       */
/* interest to the user.                               */
/* =================================================== */

typedef struct {
  int pot;
  GeoFix dist;
} hpot;  // pot for putting on the heap

static bool hpotcompare(hpot a, hpot b) { return (a.dist > b.dist); }

typedef struct {
  int slot;
  double snmd;
} hslot;  // pot for putting on the heap

bool hslotcompare(hslot a, hslot b) {
  if (a.snmd > b.snmd)
    return true;
  else
    return false;
}

typedef struct {
  GeoIx* Ix; /* GeoIndex          */
  GeoDetailedPoint gd;
  double potsnmd;
  double slotsnmd;
  std::vector<hpot> potheap;
  std::vector<hslot> slotheap;
} GeoCr;

GeoFix makedist(GeoPot* pot, GeoDetailedPoint* gd) {
  GeoFix dist, d1;
  int i;
  dist = 0;
  for (i = 0; i < GeoIndexFIXEDPOINTS; i++) {
    if (gd->fixdist[i] > pot->maxdist[i])
      d1 = gd->fixdist[i] - pot->maxdist[i];
    else
      d1 = 0;
    if (d1 > dist) dist = d1;
  }
  return dist;
}

GeoCursor* GeoIndex_NewCursor(GeoIdx* gi, GeoCoordinate* c) {
  GeoPot root;
  if (c->longitude < -180.0) return nullptr;
  if (c->longitude > 180.0) return nullptr;
  if (c->latitude < -90.0) return nullptr;
  if (c->latitude > 90.0) return nullptr;
  GeoIx* gix = (GeoIx*)gi;
  GeoCr* gcr = nullptr;

  try {
    gcr = new GeoCr;
  } catch (...) {
  }

  if (gcr == nullptr) {
    return (GeoCursor*)gcr;
  }
  gcr->Ix = gix;

#if 0
  std::vector<hpot>* p = new (&gcr->potheap) std::vector<hpot>();
  std::vector<hslot>* q = new (&gcr->slotheap) std::vector<hslot>();
  (void)p;  // avoid compiler warnings - I just want to call
  (void)q;  // the constructors and have no use for p,q.
#endif
  GeoMkDetail(gix, &(gcr->gd), c);
  hpot hp;
  hp.pot = 1;
  PotRead(gix,1,&root);
  hp.dist = makedist(&root, &(gcr->gd));
  gcr->potsnmd = GeoFixtoSNMD(hp.dist);
  gcr->slotsnmd = 20.0;
  gcr->potheap.push_back(hp);
  // not necessary here because potheap only contains one element
  //std::push_heap(gcr->potheap.begin(), gcr->potheap.end(), hpotcompare);
  TRI_ASSERT(gcr->potheap.size() == 1);
  // cppcheck-suppress *
  return (GeoCursor*)gcr;
}

GeoCoordinates* GeoIndex_ReadCursor(GeoCursor* gc, int count, bool returnDistances, double maxDistance) {
  int i, j, r;
  GeoCoordinate ct;
  GeoCoordinates* gcts;
  GeoCr* gcr;
  GeoIx* gix;
  GeoPot pot;
  GeoPot pot1;
  double tsnmd;
  hslot hs;
  hpot hp;
  gcr = (GeoCr*)gc;
  gix=gcr->Ix;
  GeoResults* gr = GeoResultsCons(count);
  if (gr == nullptr) return nullptr;
  while (gr->pointsct < count) {
    if (gcr->potsnmd < gcr->slotsnmd * 1.000001) {
      // smash top pot - if there is one
      if (gcr->potheap.size() == 0) break;  // that's all there is
      PotRead(gix,gcr->potheap.front().pot,&pot);
      // anyway remove top from heap
      std::pop_heap(gcr->potheap.begin(), gcr->potheap.end(), hpotcompare);
      gcr->potheap.pop_back();
      if (pot.LorLeaf == 0) {
        // leaf pot - put all the points into the points heap
        for (i = 0; i < pot.RorPoints; i++) {
          j = pot.points[i];
          SlotRead(gix,j,&ct);
          hs.snmd = GeoSNMD(&(gcr->gd), &ct);
          hs.slot = j;
          gcr->slotheap.push_back(hs);
          std::push_heap(gcr->slotheap.begin(), gcr->slotheap.end(),
                         hslotcompare);
        }
        if (!gcr->slotheap.empty()) {
          gcr->slotsnmd = gcr->slotheap.front().snmd;
        }
      } else {
        hp.pot = pot.LorLeaf;
        PotRead(gix,hp.pot,&pot1);
        hp.dist = makedist(&pot1, &(gcr->gd));
        gcr->potheap.push_back(hp);
        std::push_heap(gcr->potheap.begin(), gcr->potheap.end(), hpotcompare);
        hp.pot = pot.RorPoints;
        PotRead(gix,hp.pot,&pot1);
        hp.dist = makedist(&pot1, &(gcr->gd));
        gcr->potheap.push_back(hp);
        std::push_heap(gcr->potheap.begin(), gcr->potheap.end(), hpotcompare);
      }
      gcr->potsnmd = 10.0;
      if (!gcr->potheap.empty()) {
        PotRead(gix,gcr->potheap.front().pot,&pot);
        gcr->potsnmd = GeoFixtoSNMD(makedist(&pot, &(gcr->gd)));
      }
    } else {
      if (gcr->slotheap.empty()) break;  // that's all there is
      int slox = gcr->slotheap.front().slot;
      tsnmd = gcr->slotheap.front().snmd;
      r = GeoResultsGrow(gr);
      if (r == -1) {
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, gr->snmd);
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, gr->slot);
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, gr);
        return nullptr;
      }
      gr->slot[gr->pointsct] = slox;
      gr->snmd[gr->pointsct] = tsnmd;
      gr->pointsct++;
      gcr->slotsnmd = 5.0;
      std::pop_heap(gcr->slotheap.begin(), gcr->slotheap.end(), hslotcompare);
      gcr->slotheap.pop_back();
      if (!gcr->slotheap.empty()) {
        gcr->slotsnmd = gcr->slotheap.front().snmd;
      }
    }
  }
  gcts = GeoAnswers(gix, gr, returnDistances);
  return gcts;
}

void GeoIndex_CursorFree(GeoCursor* gc) {
  delete reinterpret_cast<GeoCr*>(gc);
}

/* =================================================== */
/*        The remaining routines are usually           */
/* only compiled in for debugging purposes.  They allow*/
/* the dumping of the index (to a specified file) and  */
/* a self-check to see whether the index itself seems  */
/* to be correct.                                      */
/* =================================================== */
#ifdef TRI_GEO_DEBUG

void RecursivePotDump(GeoIx* gix, FILE* f, int pot) {
  int i;
  GeoPot gp;
  GeoCoordinate gc;
  PotRead(gix,pot,&gp);
  fprintf(f, "GP. pot %d level %d  Kids %d %d\n", pot, gp.level, gp.LorLeaf,
          gp.RorPoints);
  fprintf(f, "strings %llx %llx %llx\n", gp.start, gp.middle, gp.end); // fixme - usage of printf is broken
                                                                       // fast_uint64_t is not necessarily a long long
  fprintf(f, "maxdists ");
  for (i = 0; i < GeoIndexFIXEDPOINTS; i++) fprintf(f, " %x", gp.maxdist[i]);
  fprintf(f, "\n");
  if (gp.LorLeaf == 0) {
    fprintf(f, "Leaf pot containing %d points . . .\n", gp.RorPoints);
    for (i = 0; i < gp.RorPoints; i++) {
      fprintf(f, "Child %d Point %d  ", i, gp.points[i]);
      SlotRead(gix,gp.points[i],&gc);
      fprintf(f, "Lat.  %9.4f,  Long. %9.4f", gc.latitude, gc.longitude);
#if TRI_GEO_DEBUG == 2
      fprintf(f, " %llu", (unsigned long long)gc.data);
#endif
      fprintf(f, "\n");
    }
  } else {
    fprintf(f, "\nPot %d - Left  Child of pot %d\n", gp.LorLeaf, pot);
    RecursivePotDump(gix, f, gp.LorLeaf);
    fprintf(f, "\nPot %d - Right Child of pot %d\n", gp.RorPoints, pot);
    RecursivePotDump(gix, f, gp.RorPoints);
  }
}

void GeoIndex_INDEXDUMP(GeoIdx* gi, FILE* f) {
  GeoIx* gix;
  gix = (GeoIx*)gi;
  fprintf(f, "Dump of entire index.  %d pots and %d slots allocated\n",
          gix->potct, gix->slotct);
  RecursivePotDump(gix, f, 1);
}

int RecursivePotValidate(GeoIx* gix, int pot, int* usage) {
  int i, j;
  GeoPot gp;
  GeoDetailedPoint gd;
  GeoFix maxdist[GeoIndexFIXEDPOINTS];
  GeoPot gpa, gpb;
  GeoCoordinate gc;
  PotRead(gix,pot,&gp);
  usage[0]++;
  if (gp.LorLeaf == 0) {
    if ((pot != 1) && (2 * gp.RorPoints < GeoIndexPOTSIZE)) return 1;
    for (j = 0; j < GeoIndexFIXEDPOINTS; j++) maxdist[j] = 0;
    if (gp.level != 1) return 10;
    for (i = 0; i < gp.RorPoints; i++) {
      SlotRead(gix,gp.points[i],&gc);
      GeoMkDetail(gix, &gd, &gc);
      for (j = 0; j < GeoIndexFIXEDPOINTS; j++)
        if (maxdist[j] < gd.fixdist[j]) maxdist[j] = gd.fixdist[j];
      if (gd.gs < gp.start) return 8;
      if (gd.gs > gp.end) return 9;
    }
    for (j = 0; j < GeoIndexFIXEDPOINTS; j++)
      if (maxdist[j] != gp.maxdist[j]) return 7;
    usage[1] += gp.RorPoints;
    return 0;
  } else {
    int pota = gp.LorLeaf;
    int potb = gp.RorPoints;
    PotRead(gix,pota,&gpa);
    PotRead(gix,potb,&gpb);
    int lev = gp.level;
    int leva = gpa.level;
    int levb = gpb.level;
    if (leva >= lev) return 2;
    if (levb >= lev) return 3;
    i = (lev - leva) * (lev - levb);
    if (i > 2) return 4;
    if (gp.middle != gpa.end) return 5;
    if (gp.middle != gpb.start) return 6;
    if (gp.start != gpa.start) return 11;
    if (gp.end != gpb.end) return 12;
    for (j = 0; j < GeoIndexFIXEDPOINTS; j++) maxdist[j] = gpa.maxdist[j];
    for (j = 0; j < GeoIndexFIXEDPOINTS; j++)
      if (maxdist[j] < gpb.maxdist[j]) maxdist[j] = gpb.maxdist[j];
    for (j = 0; j < GeoIndexFIXEDPOINTS; j++)
      if (maxdist[j] != gp.maxdist[j]) return 13;
    i = RecursivePotValidate(gix, gp.LorLeaf, usage);
    if (i != 0) return i;
    i = RecursivePotValidate(gix, gp.RorPoints, usage);
    if (i != 0) return i;
    return 0;
  }
}

  // rethink this function
int GeoIndex_INDEXVALID(GeoIdx* gi) {
  int usage[2];  // pots and slots
  int j, pot, slot;
  GeoIx* gix;
  GeoPot gp;
  gix = (GeoIx*)gi;
  usage[0] = 0;
  usage[1] = 0;
  j = RecursivePotValidate(gix, 1, usage);
  if (j != 0) return j;
  pot = 0;
  PotRead(gix,pot,&gp);
  pot = gp.LorLeaf;
  usage[0]++;
  while (pot != 0) {
    PotRead(gix,pot,&gp);
    pot = gp.LorLeaf;
    usage[0]++;
  }
  if (usage[0] != gix->potct) return 14;
  PotRead(gix,1,&gp);
  if (gp.start != 0) return 15;
  if (gp.end != 0x1FFFFFFFFFFFFFll) return 16;
  slot = 0;
  usage[1]++;
  slot = (int)((gix->gxc[slot]).latitude);    //XQXQ needs changing when testable
  while (slot != 0) {
    usage[1]++;
    slot = (int)((gix->gxc[slot]).latitude);  //XQXQ so does this
  }
  if (usage[1] != gix->slotct) return 17;
  return 0;
}

#endif
}}
/* end of GeoIndex.c  */
