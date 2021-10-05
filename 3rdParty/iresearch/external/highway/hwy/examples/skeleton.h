// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Demo interface to target-specific code in skeleton.cc

// Normal header with include guard and namespace.
#ifndef HIGHWAY_HWY_EXAMPLES_SKELETON_H_
#define HIGHWAY_HWY_EXAMPLES_SKELETON_H_

#include <stddef.h>

// Platform-specific definitions used for declaring an interface, independent of
// the SIMD instruction set.
#include "hwy/base.h"  // HWY_RESTRICT

namespace skeleton {

// Computes base-2 logarithm by converting to float. Supports dynamic dispatch.
void CallFloorLog2(const uint8_t* HWY_RESTRICT in, const size_t count,
                   uint8_t* HWY_RESTRICT out);

}  // namespace skeleton

#endif  // HIGHWAY_HWY_EXAMPLES_SKELETON_H_
