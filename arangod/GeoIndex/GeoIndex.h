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

/* GeoIndex.h - header file for GeoIndex algorithms  */
/* Version 2.2  25.11.2015  R. A. Parker             */

#ifndef ARANGOD_GEO_INDEX_GEO_INDEX_H
#define ARANGOD_GEO_INDEX_GEO_INDEX_H 1

#include "Basics/Common.h"

/* first the things that a user might want to change */

/* a GeoString - a signed type of at least 64 bits   */
typedef long long GeoString;

/* percentage growth of slot or slotslot tables     */
#define GeoIndexGROW 50

/* maximum number of points in a pot                */
/*  ***  note - must be even!                       */
/* smaller takes more space but is a little faster  */
#define GeoIndexPOTSIZE 6

/* choses the set of fixed points             */
#define GeoIndexFIXEDSET 6
/* 1 is just the N pole (doesn't really work) */
/* 2 is N and S pole - slow but OK            */
/* 3 is equilateral triangle on 0/180 long    */
/* 4 is four corners of a tetrahedron         */
/* 5 is trigonal bipyramid                    */
/* 6 is the  corners of octahedron (default)  */
/* 8 is eight corners of a cube               */

/* size of max-dist integer.                           */
/* 2 is 16-bit - smaller but slow when lots of points  */
/*     within a few hundred meters of target           */
/* 4 is 32-bit - larger and fast even when points are  */
/*     only centimeters apart.  Default                */
#define GEOFIXLEN 4
#if GEOFIXLEN == 2
typedef unsigned short GeoFix;
#endif
#if GEOFIXLEN == 4
typedef unsigned int GeoFix;
#endif

/* If this #define is there, then the INDEXDUMP and */
/* INDEXVALID functions are also available.  These  */
/* are not needed for normal production versions    */
/* the INDEXDUMP function also prints the data,     */
/* assumed to be a character string, if DEBUG is    */
/* set to 2.                                        */
#define TRI_GEO_DEBUG 1

typedef struct {
  double latitude;
  double longitude;
  void* data;
} GeoCoordinate;

typedef struct {
  size_t length;
  GeoCoordinate* coordinates;
  double* distances;
} GeoCoordinates;

typedef void GeoIndex;  /* to keep the structure private  */
typedef void GeoCursor; /* to keep the structure private  */

size_t GeoIndex_MemoryUsage(void*);

GeoIndex* GeoIndex_new(void);
void GeoIndex_free(GeoIndex* gi);
double GeoIndex_distance(GeoCoordinate* c1, GeoCoordinate* c2);
int GeoIndex_insert(GeoIndex* gi, GeoCoordinate* c);
int GeoIndex_remove(GeoIndex* gi, GeoCoordinate* c);
int GeoIndex_hint(GeoIndex* gi, int hint);
GeoCoordinates* GeoIndex_PointsWithinRadius(GeoIndex* gi, GeoCoordinate* c,
                                            double d);
GeoCoordinates* GeoIndex_NearestCountPoints(GeoIndex* gi, GeoCoordinate* c,
                                            int count);
GeoCursor* GeoIndex_NewCursor(GeoIndex* gi, GeoCoordinate* c);
GeoCoordinates* GeoIndex_ReadCursor(GeoCursor* gc, int count);
void GeoIndex_CursorFree(GeoCursor* gc);
void GeoIndex_CoordinatesFree(GeoCoordinates* clist);
#ifdef TRI_GEO_DEBUG
void GeoIndex_INDEXDUMP(GeoIndex* gi, FILE* f);
int GeoIndex_INDEXVALID(GeoIndex* gi);
#endif

#endif

/* end of GeoIndex.h  */
