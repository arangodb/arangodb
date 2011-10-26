////////////////////////////////////////////////////////////////////////////////
/// @brief geo index
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
///
/// @author R. A. Parker
/// @author Copyright 2011-2010, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

/* GeoIndex.h - header file for GeoIndex algorithms  */
/* Version 1.0 24.9.2011 R. A. Parker                */

#include <Basics/Common.h>

/* first the things that a user might want to change */

/* a GeoString - a signed type of at least 64 bits   */
typedef long long GeoString;

/* percentage growth of slot or slotslot tables     */
#define GeoIndexGROW 50

/* Largest interval that is processed simply        */
#define GeoIndexSMALLINTERVAL 5

/* If this #define is there, then the INDEXDUMP and */
/* INDEXVALID functions are also available.  These  */
/* are not needed for normal production versions    */
/* the INDEXDUMP function also prints the data,     */
/* assumed to be a character string, if DEBUG is    */
/* set to 2.                                        */

/*  #define DEBUG 1 */

typedef struct
{
    double latitude;
    double longitude;
    void const * data;
}       GeoCoordinate;

typedef struct
{
    size_t length;
    GeoCoordinate * coordinates;
    double * distances;
}       GeoCoordinates;

typedef struct
{
    int slotct;
    int occslots;  /* number of occupied slots */
    int * sortslot;
      GeoString * geostrings;  /* These two indexed in  */
      GeoCoordinate * gc;      /* parallel by a "slot"  */
}       GeoIndex;

GeoIndex * GeoIndex_new(void);
void GeoIndex_free(GeoIndex * gi);
double GeoIndex_distance(GeoCoordinate * c1, GeoCoordinate * c2);
int GeoIndex_insert(GeoIndex * gi, GeoCoordinate * c);
int GeoIndex_remove(GeoIndex * gi, GeoCoordinate * c);
GeoCoordinates * GeoIndex_PointsWithinRadius(GeoIndex * gi,
                    GeoCoordinate * c, double d);
GeoCoordinates * GeoIndex_NearestCountPoints(GeoIndex * gi,
                    GeoCoordinate * c, int count);
void GeoIndex_CoordinatesFree(GeoCoordinates * clist);
#ifdef DEBUG
void GeoIndex_INDEXDUMP(GeoIndex * gi, FILE * f);
int  GeoIndex_INDEXVALID(GeoIndex * gi);
#endif

/* end of GeoIndex.h  */
