// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/heap-controller.h"

#include "src/execution/isolate-inl.h"
#include "src/heap/spaces.h"

namespace v8 {
namespace internal {

template <typename Trait>
double MemoryController<Trait>::GrowingFactor(Heap* heap, size_t max_heap_size,
                                              double gc_speed,
                                              double mutator_speed) {
  const double max_factor = MaxGrowingFactor(max_heap_size);
  const double factor =
      DynamicGrowingFactor(gc_speed, mutator_speed, max_factor);
  if (FLAG_trace_gc_verbose) {
    Isolate::FromHeap(heap)->PrintWithTimestamp(
        "[%s] factor %.1f based on mu=%.3f, speed_ratio=%.f "
        "(gc=%.f, mutator=%.f)\n",
        Trait::kName, factor, Trait::kTargetMutatorUtilization,
        gc_speed / mutator_speed, gc_speed, mutator_speed);
  }
  return factor;
}

template <typename Trait>
double MemoryController<Trait>::MaxGrowingFactor(size_t max_heap_size) {
  constexpr double kMinSmallFactor = 1.3;
  constexpr double kMaxSmallFactor = 2.0;
  constexpr double kHighFactor = 4.0;

  size_t max_size = max_heap_size;
  max_size = Max(max_size, Trait::kMinSize);

  // If we are on a device with lots of memory, we allow a high heap
  // growing factor.
  if (max_size >= Trait::kMaxSize) {
    return kHighFactor;
  }

  DCHECK_GE(max_size, Trait::kMinSize);
  DCHECK_LT(max_size, Trait::kMaxSize);

  // On smaller devices we linearly scale the factor: (X-A)/(B-A)*(D-C)+C
  double factor = (max_size - Trait::kMinSize) *
                      (kMaxSmallFactor - kMinSmallFactor) /
                      (Trait::kMaxSize - Trait::kMinSize) +
                  kMinSmallFactor;
  return factor;
}

// Given GC speed in bytes per ms, the allocation throughput in bytes per ms
// (mutator speed), this function returns the heap growing factor that will
// achieve the target_mutator_utilization_ if the GC speed and the mutator speed
// remain the same until the next GC.
//
// For a fixed time-frame T = TM + TG, the mutator utilization is the ratio
// TM / (TM + TG), where TM is the time spent in the mutator and TG is the
// time spent in the garbage collector.
//
// Let MU be target_mutator_utilization_, the desired mutator utilization for
// the time-frame from the end of the current GC to the end of the next GC.
// Based on the MU we can compute the heap growing factor F as
//
// F = R * (1 - MU) / (R * (1 - MU) - MU), where R = gc_speed / mutator_speed.
//
// This formula can be derived as follows.
//
// F = Limit / Live by definition, where the Limit is the allocation limit,
// and the Live is size of live objects.
// Let’s assume that we already know the Limit. Then:
//   TG = Limit / gc_speed
//   TM = (TM + TG) * MU, by definition of MU.
//   TM = TG * MU / (1 - MU)
//   TM = Limit *  MU / (gc_speed * (1 - MU))
// On the other hand, if the allocation throughput remains constant:
//   Limit = Live + TM * allocation_throughput = Live + TM * mutator_speed
// Solving it for TM, we get
//   TM = (Limit - Live) / mutator_speed
// Combining the two equation for TM:
//   (Limit - Live) / mutator_speed = Limit * MU / (gc_speed * (1 - MU))
//   (Limit - Live) = Limit * MU * mutator_speed / (gc_speed * (1 - MU))
// substitute R = gc_speed / mutator_speed
//   (Limit - Live) = Limit * MU  / (R * (1 - MU))
// substitute F = Limit / Live
//   F - 1 = F * MU  / (R * (1 - MU))
//   F - F * MU / (R * (1 - MU)) = 1
//   F * (1 - MU / (R * (1 - MU))) = 1
//   F * (R * (1 - MU) - MU) / (R * (1 - MU)) = 1
//   F = R * (1 - MU) / (R * (1 - MU) - MU)
template <typename Trait>
double MemoryController<Trait>::DynamicGrowingFactor(double gc_speed,
                                                     double mutator_speed,
                                                     double max_factor) {
  DCHECK_LE(Trait::kMinGrowingFactor, max_factor);
  DCHECK_GE(Trait::kMaxGrowingFactor, max_factor);
  if (gc_speed == 0 || mutator_speed == 0) return max_factor;

  const double speed_ratio = gc_speed / mutator_speed;

  const double a = speed_ratio * (1 - Trait::kTargetMutatorUtilization);
  const double b = speed_ratio * (1 - Trait::kTargetMutatorUtilization) -
                   Trait::kTargetMutatorUtilization;

  // The factor is a / b, but we need to check for small b first.
  double factor = (a < b * max_factor) ? a / b : max_factor;
  factor = Min(factor, max_factor);
  factor = Max(factor, Trait::kMinGrowingFactor);
  return factor;
}

template <typename Trait>
size_t MemoryController<Trait>::MinimumAllocationLimitGrowingStep(
    Heap::HeapGrowingMode growing_mode) {
  const size_t kRegularAllocationLimitGrowingStep = 8;
  const size_t kLowMemoryAllocationLimitGrowingStep = 2;
  size_t limit = (Page::kPageSize > MB ? Page::kPageSize : MB);
  return limit * (growing_mode == Heap::HeapGrowingMode::kConservative
                      ? kLowMemoryAllocationLimitGrowingStep
                      : kRegularAllocationLimitGrowingStep);
}

template <typename Trait>
size_t MemoryController<Trait>::CalculateAllocationLimit(
    Heap* heap, size_t current_size, size_t min_size, size_t max_size,
    size_t new_space_capacity, double factor,
    Heap::HeapGrowingMode growing_mode) {
  switch (growing_mode) {
    case Heap::HeapGrowingMode::kConservative:
    case Heap::HeapGrowingMode::kSlow:
      factor = Min(factor, Trait::kConservativeGrowingFactor);
      break;
    case Heap::HeapGrowingMode::kMinimal:
      factor = Trait::kMinGrowingFactor;
      break;
    case Heap::HeapGrowingMode::kDefault:
      break;
  }

  if (FLAG_heap_growing_percent > 0) {
    factor = 1.0 + FLAG_heap_growing_percent / 100.0;
  }

  if (FLAG_heap_growing_percent > 0) {
    factor = 1.0 + FLAG_heap_growing_percent / 100.0;
  }

  CHECK_LT(1.0, factor);
  CHECK_LT(0, current_size);
  const uint64_t limit =
      Max(static_cast<uint64_t>(current_size * factor),
          static_cast<uint64_t>(current_size) +
              MinimumAllocationLimitGrowingStep(growing_mode)) +
      new_space_capacity;
  const uint64_t limit_above_min_size = Max<uint64_t>(limit, min_size);
  const uint64_t halfway_to_the_max =
      (static_cast<uint64_t>(current_size) + max_size) / 2;
  const size_t result =
      static_cast<size_t>(Min(limit_above_min_size, halfway_to_the_max));
  if (FLAG_trace_gc_verbose) {
    Isolate::FromHeap(heap)->PrintWithTimestamp(
        "[%s] Limit: old size: %zu KB, new limit: %zu KB (%.1f)\n",
        Trait::kName, current_size / KB, result / KB, factor);
  }
  return result;
}

template class V8_EXPORT_PRIVATE MemoryController<V8HeapTrait>;
template class V8_EXPORT_PRIVATE MemoryController<GlobalMemoryTrait>;

const char* V8HeapTrait::kName = "HeapController";
const char* GlobalMemoryTrait::kName = "GlobalMemoryController";

}  // namespace internal
}  // namespace v8
