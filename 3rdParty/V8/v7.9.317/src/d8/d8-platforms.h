// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_D8_D8_PLATFORMS_H_
#define V8_D8_D8_PLATFORMS_H_

#include <cstdint>
#include <memory>

namespace v8 {

class Platform;

// Returns a predictable v8::Platform implementation.
// orker threads are disabled, idle tasks are disallowed, and the time reported
// by {MonotonicallyIncreasingTime} is deterministic.
std::unique_ptr<Platform> MakePredictablePlatform(
    std::unique_ptr<Platform> platform);

// Returns a v8::Platform implementation which randomly delays tasks (both
// foreground and background) for stress-testing different interleavings.
// If {random_seed} is 0, a random seed is chosen.
std::unique_ptr<Platform> MakeDelayedTasksPlatform(
    std::unique_ptr<Platform> platform, int64_t random_seed);

}  // namespace v8

#endif  // V8_D8_D8_PLATFORMS_H_
