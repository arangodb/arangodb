// Copyright 2018 Google Inc. All Rights Reserved.
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

#ifndef S2_S2SHAPEUTIL_TESTING_H_
#define S2_S2SHAPEUTIL_TESTING_H_

#include "s2/s2shape.h"
#include "s2/s2shape_index.h"

namespace s2testing {

// Verifies that all methods of the two S2Shapes return identical results,
// except for id() and type_tag().
void ExpectEqual(const S2Shape& a, const S2Shape& b);

// Verifies that two S2ShapeIndexes have identical contents (including all the
// S2Shapes in both indexes).
void ExpectEqual(const S2ShapeIndex& a, const S2ShapeIndex& b);

}  // namespace s2testing

#endif  // S2_S2SHAPEUTIL_TESTING_H_
