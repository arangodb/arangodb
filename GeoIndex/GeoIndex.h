////////////////////////////////////////////////////////////////////////////////
/// @brief geo index
///
/// @file
///
/// DISCLAIMER
///
/// Copyright by triAGENS GmbH - All rights reserved.
///
/// The Programs (which include both the software and documentation)
/// contain proprietary information of triAGENS GmbH; they are
/// provided under a license agreement containing restrictions on use and
/// disclosure and are also protected by copyright, patent and other
/// intellectual and industrial property laws. Reverse engineering,
/// disassembly or decompilation of the Programs, except to the extent
/// required to obtain interoperability with other independently created
/// software or as specified by law, is prohibited.
///
/// The Programs are not intended for use in any nuclear, aviation, mass
/// transit, medical, or other inherently dangerous applications. It shall
/// be the licensee's responsibility to take all appropriate fail-safe,
/// backup, redundancy, and other measures to ensure the safe use of such
/// applications if the Programs are used for such purposes, and triAGENS
/// GmbH disclaims liability for any damages caused by such use of
/// the Programs.
///
/// This software is the confidential and proprietary information of
/// triAGENS GmbH. You shall not disclose such confidential and
/// proprietary information and shall use it only in accordance with the
/// terms of the license agreement you entered into with triAGENS GmbH.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author R. A. Parker
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

/* GeoIndex.h - header file for GeoIndex algorithms  */
/* Version 2.1   8.1.2012   R. A. Parker             */

#ifdef GEO_TRIAGENS
#include <BasicsC/common.h>
#else
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#endif

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
#define DEBUG 1

typedef struct
{
    double latitude;
    double longitude;
    void * data;
}       GeoCoordinate;

typedef struct
{
    size_t length;
    GeoCoordinate * coordinates;
    double * distances;
}       GeoCoordinates;

typedef char GeoIndex;   /* to keep the structure private  */

GeoIndex * GeoIndex_new(void);
void GeoIndex_free(GeoIndex * gi);
double GeoIndex_distance(GeoCoordinate * c1, GeoCoordinate * c2);
int GeoIndex_insert(GeoIndex * gi, GeoCoordinate * c);
int GeoIndex_remove(GeoIndex * gi, GeoCoordinate * c);
int GeoIndex_hint(GeoIndex * gi, int hint);
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
