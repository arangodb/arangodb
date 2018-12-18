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
// The S2 library defines extra validity checks throughout the code that can
// optionally be enabled or disabled.  By default, these validity checks are
// enabled in debug-mode builds (including fastbuild) and disabled in
// optimized builds.
//
// There are two ways to change the default behavior:
//
//  - The command line --s2debug flag, which changes the global default.
//
//  - The S2Debug enum, which allows validity checks to be enabled or disabled
//    for specific objects (e.g., an S2Polygon).
//
// If you want to intentionally create invalid geometry (e.g., in a test), the
// S2Debug enum is preferable.  For example, to create an invalid S2Polygon,
// you can do this:
//
//   S2Polygon invalid;
//   invalid.set_s2debug_override(S2Debug::DISABLE);
//
// There is also a convenience constructor:
//
//   vector<unique_ptr<S2Loop>> loops = ...;
//   S2Polygon invalid(loops, S2Debug::DISABLE);
//
// There are a few checks that cannot be disabled this way (e.g., internal
// functions that require S2Points to be unit length).  If you absolutely need
// to disable these checks, you can set FLAGS_s2debug for the duration of a
// specific test like this:
//
// TEST(MyClass, InvalidGeometry) {
//   FLAGS_s2debug = false;  // Automatically restored between tests
//   ...
// }

#ifndef S2_S2DEBUG_H_
#define S2_S2DEBUG_H_

#include "s2/base/commandlineflags.h"
#include "s2/base/integral_types.h"

// Command line flag that enables extra validity checking throughout the S2
// code.  It is turned on by default in debug-mode builds.
DECLARE_bool(s2debug);

// Class that allows the --s2debug validity checks to be enabled or disabled
// for specific objects (e.g., see S2Polygon).
enum class S2Debug : uint8 {
  ALLOW,    // Validity checks are controlled by --s2debug
  DISABLE   // No validity checks even when --s2debug is true
};

#endif  // S2_S2DEBUG_H_
