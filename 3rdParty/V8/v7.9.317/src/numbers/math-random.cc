// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/numbers/math-random.h"

#include "src/base/utils/random-number-generator.h"
#include "src/common/assert-scope.h"
#include "src/execution/isolate.h"
#include "src/objects/contexts-inl.h"
#include "src/objects/fixed-array.h"
#include "src/objects/smi.h"

namespace v8 {
namespace internal {

void MathRandom::InitializeContext(Isolate* isolate,
                                   Handle<Context> native_context) {
  Handle<FixedDoubleArray> cache = Handle<FixedDoubleArray>::cast(
      isolate->factory()->NewFixedDoubleArray(kCacheSize));
  for (int i = 0; i < kCacheSize; i++) cache->set(i, 0);
  native_context->set_math_random_cache(*cache);
  Handle<PodArray<State>> pod =
      PodArray<State>::New(isolate, 1, AllocationType::kOld);
  native_context->set_math_random_state(*pod);
  ResetContext(*native_context);
}

void MathRandom::ResetContext(Context native_context) {
  native_context.set_math_random_index(Smi::zero());
  State state = {0, 0};
  PodArray<State>::cast(native_context.math_random_state()).set(0, state);
}

Address MathRandom::RefillCache(Isolate* isolate, Address raw_native_context) {
  Context native_context = Context::cast(Object(raw_native_context));
  DisallowHeapAllocation no_gc;
  PodArray<State> pod =
      PodArray<State>::cast(native_context.math_random_state());
  State state = pod.get(0);
  // Initialize state if not yet initialized. If a fixed random seed was
  // requested, use it to reset our state the first time a script asks for
  // random numbers in this context. This ensures the script sees a consistent
  // sequence.
  if (state.s0 == 0 && state.s1 == 0) {
    uint64_t seed;
    if (FLAG_random_seed != 0) {
      seed = FLAG_random_seed;
    } else {
      isolate->random_number_generator()->NextBytes(&seed, sizeof(seed));
    }
    state.s0 = base::RandomNumberGenerator::MurmurHash3(seed);
    state.s1 = base::RandomNumberGenerator::MurmurHash3(~seed);
    CHECK(state.s0 != 0 || state.s1 != 0);
  }

  FixedDoubleArray cache =
      FixedDoubleArray::cast(native_context.math_random_cache());
  // Create random numbers.
  for (int i = 0; i < kCacheSize; i++) {
    // Generate random numbers using xorshift128+.
    base::RandomNumberGenerator::XorShift128(&state.s0, &state.s1);
    cache.set(i, base::RandomNumberGenerator::ToDouble(state.s0));
  }
  pod.set(0, state);

  Smi new_index = Smi::FromInt(kCacheSize);
  native_context.set_math_random_index(new_index);
  return new_index.ptr();
}

}  // namespace internal
}  // namespace v8
