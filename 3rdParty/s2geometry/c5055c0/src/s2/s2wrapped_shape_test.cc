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

#include "s2/s2wrapped_shape.h"

#include <gtest/gtest.h>
#include "s2/s2lax_polygon_shape.h"
#include "s2/s2shapeutil_testing.h"
#include "s2/s2text_format.h"

TEST(S2WrappedShape, Coverage) {
  // Tests that all the S2Shape methods are implemented.

  auto shape = s2textformat::MakeLaxPolygonOrDie("0:0; 1:1, 1:2, 2:1");
  S2WrappedShape wrapped_shape(shape.get());
  s2testing::ExpectEqual(wrapped_shape, *shape);
}
