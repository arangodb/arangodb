// Copyright 2022 Google LLC
// SPDX-License-Identifier: Apache-2.0
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

#ifndef HIGHWAY_HWY_PER_TARGET_H_
#define HIGHWAY_HWY_PER_TARGET_H_

#include <stddef.h>

// Per-target functions.

namespace hwy {

// Returns size in bytes of a vector, i.e. `Lanes(ScalableTag<uint8_t>())`.
//
// Do not cache the result, which may change after calling DisableTargets, or
// if software requests a different vector size (e.g. when entering/exiting SME
// streaming mode). Instead call this right before the code that depends on the
// result, without any DisableTargets or SME transition in-between. Note that
// this involves an indirect call, so prefer not to call this frequently nor
// unnecessarily.
size_t VectorBytes();

}  // namespace hwy

#endif  // HIGHWAY_HWY_PER_TARGET_H_
