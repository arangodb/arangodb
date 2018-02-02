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

#include "s2/s2error.h"

#include <gtest/gtest.h>

TEST(S2Error, Basic) {
  S2Error error;
  error.Init(S2Error::DUPLICATE_VERTICES,
             "Vertex %d is the same as vertex %d", 23, 47);
  // Prepend additional context to the message.
  error.Init(error.code(),
             "Loop %d: %s", 5, error.text().c_str());
  EXPECT_EQ(S2Error::DUPLICATE_VERTICES, error.code());
  EXPECT_EQ("Loop 5: Vertex 23 is the same as vertex 47", error.text());
}
