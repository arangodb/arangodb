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

#ifndef HIGHWAY_HWY_EXAMPLES_SKELETON_SHARED_H_
#define HIGHWAY_HWY_EXAMPLES_SKELETON_SHARED_H_

// Definitions shared between SIMD implementation and normal code. Such a
// header is optional and can be omitted if the SIMD implementation is
// self-contained (does not share any constants with normal code).

namespace skeleton {

constexpr float kMultiplier = 1.5f;

}  // namespace skeleton

#endif  // HIGHWAY_HWY_EXAMPLES_SKELETON_SHARED_H_
