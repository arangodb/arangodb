// Copyright 2005 Google Inc. All Rights Reserved.
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
// All of the values below were obtained by a combination of hand analysis and
// Mathematica.  In general, S2_TAN_PROJECTION produces the most uniform
// shapes and sizes of cells, S2_LINEAR_PROJECTION is considerably worse, and
// S2_QUADRATIC_PROJECTION is somewhere in between (but generally closer to
// the tangent projection than the linear one).
//
// Note that S2_LINEAR_PROJECTION can be useful for analysis even when another
// projection is being used, since it allows many cell metrics to be bounded
// in terms of (u,v) coordinates rather than (s,t) coordinates.  (With the
// linear projection, u = 2 * s - 1 and similarly for v.)  Similarly,
// S2_TAN_PROJECTION allows cell metrics to be bounded in terms of (u,v)
// coordinate changes when they are measured as distances on the unit sphere.

#include "s2/s2metrics.h"

namespace S2 {

const LengthMetric kMinAngleSpan(
    S2_PROJECTION == S2_LINEAR_PROJECTION ? 1.0 :                      // 1.000
    S2_PROJECTION == S2_TAN_PROJECTION ? M_PI / 2 :                    // 1.571
    S2_PROJECTION == S2_QUADRATIC_PROJECTION ? 4. / 3 :                // 1.333
    0);

const LengthMetric kMaxAngleSpan(
    S2_PROJECTION == S2_LINEAR_PROJECTION ? 2 :                        // 2.000
    S2_PROJECTION == S2_TAN_PROJECTION ? M_PI / 2 :                    // 1.571
    S2_PROJECTION == S2_QUADRATIC_PROJECTION ? 1.704897179199218452 :  // 1.705
    0);

const LengthMetric kAvgAngleSpan(M_PI / 2);                    // 1.571
// This is true for all projections.

const LengthMetric kMinWidth(
    S2_PROJECTION == S2_LINEAR_PROJECTION ? sqrt(2. / 3) :             // 0.816
    S2_PROJECTION == S2_TAN_PROJECTION ? M_PI / (2 * sqrt(2)) :        // 1.111
    S2_PROJECTION == S2_QUADRATIC_PROJECTION ? 2 * sqrt(2) / 3 :       // 0.943
    0);

const LengthMetric kMaxWidth(kMaxAngleSpan.deriv());
// This is true for all projections.

const LengthMetric kAvgWidth(
    S2_PROJECTION == S2_LINEAR_PROJECTION ? 1.411459345844456965 :     // 1.411
    S2_PROJECTION == S2_TAN_PROJECTION ? 1.437318638925160885 :        // 1.437
    S2_PROJECTION == S2_QUADRATIC_PROJECTION ? 1.434523672886099389 :  // 1.435
    0);

const LengthMetric kMinEdge(
    S2_PROJECTION == S2_LINEAR_PROJECTION ? 2 * sqrt(2) / 3 :          // 0.943
    S2_PROJECTION == S2_TAN_PROJECTION ? M_PI / (2 * sqrt(2)) :        // 1.111
    S2_PROJECTION == S2_QUADRATIC_PROJECTION ? 2 * sqrt(2) / 3 :       // 0.943
    0);

const LengthMetric kMaxEdge(kMaxAngleSpan.deriv());
// This is true for all projections.

const LengthMetric kAvgEdge(
    S2_PROJECTION == S2_LINEAR_PROJECTION ? 1.440034192955603643 :     // 1.440
    S2_PROJECTION == S2_TAN_PROJECTION ? 1.461667032546739266 :        // 1.462
    S2_PROJECTION == S2_QUADRATIC_PROJECTION ? 1.459213746386106062 :  // 1.459
    0);

const LengthMetric kMinDiag(
    S2_PROJECTION == S2_LINEAR_PROJECTION ? 2 * sqrt(2) / 3 :          // 0.943
    S2_PROJECTION == S2_TAN_PROJECTION ? M_PI * sqrt(2) / 3 :          // 1.481
    S2_PROJECTION == S2_QUADRATIC_PROJECTION ? 8 * sqrt(2) / 9 :       // 1.257
    0);

const LengthMetric kMaxDiag(
    S2_PROJECTION == S2_LINEAR_PROJECTION ? 2 * sqrt(2) :              // 2.828
    S2_PROJECTION == S2_TAN_PROJECTION ? M_PI * sqrt(2. / 3) :         // 2.565
    S2_PROJECTION == S2_QUADRATIC_PROJECTION ? 2.438654594434021032 :  // 2.439
    0);

const LengthMetric kAvgDiag(
    S2_PROJECTION == S2_LINEAR_PROJECTION ? 2.031817866418812674 :     // 2.032
    S2_PROJECTION == S2_TAN_PROJECTION ? 2.063623197195635753 :        // 2.064
    S2_PROJECTION == S2_QUADRATIC_PROJECTION ? 2.060422738998471683 :  // 2.060
    0);

const AreaMetric kMinArea(
    S2_PROJECTION == S2_LINEAR_PROJECTION ? 4 / (3 * sqrt(3)) :        // 0.770
    S2_PROJECTION == S2_TAN_PROJECTION ? (M_PI*M_PI) / (4*sqrt(2)) :   // 1.745
    S2_PROJECTION == S2_QUADRATIC_PROJECTION ? 8 * sqrt(2) / 9 :       // 1.257
    0);

const AreaMetric kMaxArea(
    S2_PROJECTION == S2_LINEAR_PROJECTION ? 4 :                        // 4.000
    S2_PROJECTION == S2_TAN_PROJECTION ? M_PI * M_PI / 4 :             // 2.467
    S2_PROJECTION == S2_QUADRATIC_PROJECTION ? 2.635799256963161491 :  // 2.636
    0);

const AreaMetric kAvgArea(4 * M_PI / 6);                       // 2.094
// This is true for all projections.

const double kMaxEdgeAspect = (
    S2_PROJECTION == S2_LINEAR_PROJECTION ? sqrt(2) :                  // 1.414
    S2_PROJECTION == S2_TAN_PROJECTION ?  sqrt(2) :                    // 1.414
    S2_PROJECTION == S2_QUADRATIC_PROJECTION ? 1.442615274452682920 :  // 1.443
    0);

const double kMaxDiagAspect = sqrt(3);                             // 1.732
// This is true for all projections.

}  // namespace S2
