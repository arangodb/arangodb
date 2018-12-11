// Copyright 2013 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// Author: ericv@google.com (Eric Veach)

#ifndef S2_S2SHAPEUTIL_BUILD_POLYGON_BOUNDARIES_H_
#define S2_S2SHAPEUTIL_BUILD_POLYGON_BOUNDARIES_H_

#include <vector>

#include "s2/s2shape_index.h"

namespace s2shapeutil {

// The purpose of this function is to construct polygons consisting of
// multiple loops.  It takes as input a collection of loops whose boundaries
// do not cross, and groups them into polygons whose interiors do not
// intersect (where the boundary of each polygon may consist of multiple
// loops).
//
// some of those islands have lakes, then the input to this function would
// islands, and their lakes.  Each loop would actually be present twice, once
// in each direction (see below).  The output would consist of one polygon
// representing each lake, one polygon representing each island not including
// islands or their lakes, and one polygon representing the rest of the world
//
// This method is intended for internal use; external clients should use
// S2Builder, which has more convenient interface.
//
// The input consists of a set of connected components, where each component
// consists of one or more loops.  The components must satisfy the following
// properties:
//
//  - The loops in each component must form a subdivision of the sphere (i.e.,
//    they must cover the entire sphere without overlap), except that a
//    component may consist of a single loop if and only if that loop is
//    degenerate (i.e., its interior is empty).
//
//  - The boundaries of different components must be disjoint (i.e. no
//    crossing edges or shared vertices).
//
//  - No component should be empty, and no loop should have zero edges.
//
// The output consists of a set of polygons, where each polygon is defined by
// the collection of loops that form its boundary.  This function does not
// actually construct any S2Shapes; it simply identifies the loops that belong
// to each polygon.
void BuildPolygonBoundaries(
    const std::vector<std::vector<S2Shape*>>& components,
    std::vector<std::vector<S2Shape*>>* polygons);

}  // namespace s2shapeutil

#endif  // S2_S2SHAPEUTIL_BUILD_POLYGON_BOUNDARIES_H_
