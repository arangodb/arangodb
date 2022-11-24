// Copyright 2021 Google Inc. All Rights Reserved.
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
//
// Helper functions for converting S2Shapes to typed shapes: S2Point,
// S2Polyline and S2Polygon.

#ifndef S2_S2SHAPEUTIL_CONVERSION_H_
#define S2_S2SHAPEUTIL_CONVERSION_H_

#include <memory>

#include "s2/s2point.h"
#include "s2/s2polygon.h"
#include "s2/s2polyline.h"
#include "s2/s2shape.h"

namespace s2shapeutil {

// Converts a 0-dimensional S2Shape into a list of S2Points.
// This method does allow an empty shape (i.e. a shape with no vertices).
std::vector<S2Point> ShapeToS2Points(const S2Shape& multipoint);

// Converts a 1-dimensional S2Shape into an S2Polyline. Converts the first
// chain in the shape to a vector of S2Point vertices and uses that to construct
// the S2Polyline. Note that the input 1-dimensional S2Shape must contain at
// most 1 chain, and that this method does not accept an empty shape (i.e. a
// shape with no vertices).
std::unique_ptr<S2Polyline> ShapeToS2Polyline(const S2Shape& line);

// Converts a 2-dimensional S2Shape into an S2Polygon. Each chain is converted
// to an S2Loop and the vector of loops is used to construct the S2Polygon.
std::unique_ptr<S2Polygon> ShapeToS2Polygon(const S2Shape& poly);

}  // namespace s2shapeutil

#endif  // S2_S2SHAPEUTIL_CONVERSION_H_
