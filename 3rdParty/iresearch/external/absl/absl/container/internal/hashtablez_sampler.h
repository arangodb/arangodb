// Copyright 2018 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// -----------------------------------------------------------------------------
// File: hashtablez_sampler.h
// -----------------------------------------------------------------------------
//
// This header file defines the API for a low level library to sample hashtables
// and collect runtime statistics about them.
//
// `HashtablezSampler` controls the lifecycle of `HashtablezInfo` objects which
// store information about a single sample.
//
// `Record*` methods store information into samples.
// `Sample()` and `Unsample()` make use of a single global sampler with
// properties controlled by the flags hashtablez_enabled,
// hashtablez_sample_rate, and hashtablez_max_samples.
//
// WARNING
//
// Using this sampling API may cause sampled Swiss tables to use the global
// allocator (operator `new`) in addition to any custom allocator.  If you
// are using a table in an unusual circumstance where allocation or calling a
// linux syscall is unacceptable, this could interfere.
//
// This utility is internal-only. Use at your own risk.

#ifndef IRESEARCH_ABSL_CONTAINER_INTERNAL_HASHTABLEZ_SAMPLER_H_
#define IRESEARCH_ABSL_CONTAINER_INTERNAL_HASHTABLEZ_SAMPLER_H_

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include "absl/base/internal/per_thread_tls.h"
#include "absl/base/optimization.h"
#include "absl/container/internal/have_sse.h"
#include "absl/synchronization/mutex.h"
#include "absl/utility/utility.h"

namespace iresearch_absl {
IRESEARCH_ABSL_NAMESPACE_BEGIN
namespace container_internal {

// Stores information about a sampled hashtable.  All mutations to this *must*
// be made through `Record*` functions below.  All reads from this *must* only
// occur in the callback to `HashtablezSampler::Iterate`.
struct HashtablezInfo {
  // Constructs the object but does not fill in any fields.
  HashtablezInfo();
  ~HashtablezInfo();
  HashtablezInfo(const HashtablezInfo&) = delete;
  HashtablezInfo& operator=(const HashtablezInfo&) = delete;

  // Puts the object into a clean state, fills in the logically `const` members,
  // blocking for any readers that are currently sampling the object.
  void PrepareForSampling() IRESEARCH_ABSL_EXCLUSIVE_LOCKS_REQUIRED(init_mu);

  // These fields are mutated by the various Record* APIs and need to be
  // thread-safe.
  std::atomic<size_t> capacity;
  std::atomic<size_t> size;
  std::atomic<size_t> num_erases;
  std::atomic<size_t> num_rehashes;
  std::atomic<size_t> max_probe_length;
  std::atomic<size_t> total_probe_length;
  std::atomic<size_t> hashes_bitwise_or;
  std::atomic<size_t> hashes_bitwise_and;

  // `HashtablezSampler` maintains intrusive linked lists for all samples.  See
  // comments on `HashtablezSampler::all_` for details on these.  `init_mu`
  // guards the ability to restore the sample to a pristine state.  This
  // prevents races with sampling and resurrecting an object.
  iresearch_absl::Mutex init_mu;
  HashtablezInfo* next;
  HashtablezInfo* dead IRESEARCH_ABSL_GUARDED_BY(init_mu);

  // All of the fields below are set by `PrepareForSampling`, they must not be
  // mutated in `Record*` functions.  They are logically `const` in that sense.
  // These are guarded by init_mu, but that is not externalized to clients, who
  // can only read them during `HashtablezSampler::Iterate` which will hold the
  // lock.
  static constexpr int kMaxStackDepth = 64;
  iresearch_absl::Time create_time;
  int32_t depth;
  void* stack[kMaxStackDepth];
};

inline void RecordRehashSlow(HashtablezInfo* info, size_t total_probe_length) {
#if IRESEARCH_ABSL_INTERNAL_RAW_HASH_SET_HAVE_SSE2
  total_probe_length /= 16;
#else
  total_probe_length /= 8;
#endif
  info->total_probe_length.store(total_probe_length, std::memory_order_relaxed);
  info->num_erases.store(0, std::memory_order_relaxed);
  // There is only one concurrent writer, so `load` then `store` is sufficient
  // instead of using `fetch_add`.
  info->num_rehashes.store(
      1 + info->num_rehashes.load(std::memory_order_relaxed),
      std::memory_order_relaxed);
}

inline void RecordStorageChangedSlow(HashtablezInfo* info, size_t size,
                                     size_t capacity) {
  info->size.store(size, std::memory_order_relaxed);
  info->capacity.store(capacity, std::memory_order_relaxed);
  if (size == 0) {
    // This is a clear, reset the total/num_erases too.
    info->total_probe_length.store(0, std::memory_order_relaxed);
    info->num_erases.store(0, std::memory_order_relaxed);
  }
}

void RecordInsertSlow(HashtablezInfo* info, size_t hash,
                      size_t distance_from_desired);

inline void RecordEraseSlow(HashtablezInfo* info) {
  info->size.fetch_sub(1, std::memory_order_relaxed);
  // There is only one concurrent writer, so `load` then `store` is sufficient
  // instead of using `fetch_add`.
  info->num_erases.store(
      1 + info->num_erases.load(std::memory_order_relaxed),
      std::memory_order_relaxed);
}

HashtablezInfo* SampleSlow(int64_t* next_sample);
void UnsampleSlow(HashtablezInfo* info);

#if defined(IRESEARCH_ABSL_INTERNAL_HASHTABLEZ_SAMPLE)
#error IRESEARCH_ABSL_INTERNAL_HASHTABLEZ_SAMPLE cannot be directly set
#endif  // defined(IRESEARCH_ABSL_INTERNAL_HASHTABLEZ_SAMPLE)

#if defined(IRESEARCH_ABSL_INTERNAL_HASHTABLEZ_SAMPLE)
class HashtablezInfoHandle {
 public:
  explicit HashtablezInfoHandle() : info_(nullptr) {}
  explicit HashtablezInfoHandle(HashtablezInfo* info) : info_(info) {}
  ~HashtablezInfoHandle() {
    if (IRESEARCH_ABSL_PREDICT_TRUE(info_ == nullptr)) return;
    UnsampleSlow(info_);
  }

  HashtablezInfoHandle(const HashtablezInfoHandle&) = delete;
  HashtablezInfoHandle& operator=(const HashtablezInfoHandle&) = delete;

  HashtablezInfoHandle(HashtablezInfoHandle&& o) noexcept
      : info_(iresearch_absl::exchange(o.info_, nullptr)) {}
  HashtablezInfoHandle& operator=(HashtablezInfoHandle&& o) noexcept {
    if (IRESEARCH_ABSL_PREDICT_FALSE(info_ != nullptr)) {
      UnsampleSlow(info_);
    }
    info_ = iresearch_absl::exchange(o.info_, nullptr);
    return *this;
  }

  inline void RecordStorageChanged(size_t size, size_t capacity) {
    if (IRESEARCH_ABSL_PREDICT_TRUE(info_ == nullptr)) return;
    RecordStorageChangedSlow(info_, size, capacity);
  }

  inline void RecordRehash(size_t total_probe_length) {
    if (IRESEARCH_ABSL_PREDICT_TRUE(info_ == nullptr)) return;
    RecordRehashSlow(info_, total_probe_length);
  }

  inline void RecordInsert(size_t hash, size_t distance_from_desired) {
    if (IRESEARCH_ABSL_PREDICT_TRUE(info_ == nullptr)) return;
    RecordInsertSlow(info_, hash, distance_from_desired);
  }

  inline void RecordErase() {
    if (IRESEARCH_ABSL_PREDICT_TRUE(info_ == nullptr)) return;
    RecordEraseSlow(info_);
  }

  friend inline void swap(HashtablezInfoHandle& lhs,
                          HashtablezInfoHandle& rhs) {
    std::swap(lhs.info_, rhs.info_);
  }

 private:
  friend class HashtablezInfoHandlePeer;
  HashtablezInfo* info_;
};
#else
// Ensure that when Hashtablez is turned off at compile time, HashtablezInfo can
// be removed by the linker, in order to reduce the binary size.
class HashtablezInfoHandle {
 public:
  explicit HashtablezInfoHandle() = default;
  explicit HashtablezInfoHandle(std::nullptr_t) {}

  inline void RecordStorageChanged(size_t /*size*/, size_t /*capacity*/) {}
  inline void RecordRehash(size_t /*total_probe_length*/) {}
  inline void RecordInsert(size_t /*hash*/, size_t /*distance_from_desired*/) {}
  inline void RecordErase() {}

  friend inline void swap(HashtablezInfoHandle& /*lhs*/,
                          HashtablezInfoHandle& /*rhs*/) {}
};
#endif  // defined(IRESEARCH_ABSL_INTERNAL_HASHTABLEZ_SAMPLE)

#if defined(IRESEARCH_ABSL_INTERNAL_HASHTABLEZ_SAMPLE)
extern IRESEARCH_ABSL_PER_THREAD_TLS_KEYWORD int64_t global_next_sample;
#endif  // defined(IRESEARCH_ABSL_INTERNAL_HASHTABLEZ_SAMPLE)

// Returns an RAII sampling handle that manages registration and unregistation
// with the global sampler.
inline HashtablezInfoHandle Sample() {
#if defined(IRESEARCH_ABSL_INTERNAL_HASHTABLEZ_SAMPLE)
  if (IRESEARCH_ABSL_PREDICT_TRUE(--global_next_sample > 0)) {
    return HashtablezInfoHandle(nullptr);
  }
  return HashtablezInfoHandle(SampleSlow(&global_next_sample));
#else
  return HashtablezInfoHandle(nullptr);
#endif  // !ABSL_PER_THREAD_TLS
}

// Holds samples and their associated stack traces with a soft limit of
// `SetHashtablezMaxSamples()`.
//
// Thread safe.
class HashtablezSampler {
 public:
  // Returns a global Sampler.
  static HashtablezSampler& Global();

  HashtablezSampler();
  ~HashtablezSampler();

  // Registers for sampling.  Returns an opaque registration info.
  HashtablezInfo* Register();

  // Unregisters the sample.
  void Unregister(HashtablezInfo* sample);

  // The dispose callback will be called on all samples the moment they are
  // being unregistered. Only affects samples that are unregistered after the
  // callback has been set.
  // Returns the previous callback.
  using DisposeCallback = void (*)(const HashtablezInfo&);
  DisposeCallback SetDisposeCallback(DisposeCallback f);

  // Iterates over all the registered `StackInfo`s.  Returning the number of
  // samples that have been dropped.
  int64_t Iterate(const std::function<void(const HashtablezInfo& stack)>& f);

 private:
  void PushNew(HashtablezInfo* sample);
  void PushDead(HashtablezInfo* sample);
  HashtablezInfo* PopDead();

  std::atomic<size_t> dropped_samples_;
  std::atomic<size_t> size_estimate_;

  // Intrusive lock free linked lists for tracking samples.
  //
  // `all_` records all samples (they are never removed from this list) and is
  // terminated with a `nullptr`.
  //
  // `graveyard_.dead` is a circular linked list.  When it is empty,
  // `graveyard_.dead == &graveyard`.  The list is circular so that
  // every item on it (even the last) has a non-null dead pointer.  This allows
  // `Iterate` to determine if a given sample is live or dead using only
  // information on the sample itself.
  //
  // For example, nodes [A, B, C, D, E] with [A, C, E] alive and [B, D] dead
  // looks like this (G is the Graveyard):
  //
  //           +---+    +---+    +---+    +---+    +---+
  //    all -->| A |--->| B |--->| C |--->| D |--->| E |
  //           |   |    |   |    |   |    |   |    |   |
  //   +---+   |   | +->|   |-+  |   | +->|   |-+  |   |
  //   | G |   +---+ |  +---+ |  +---+ |  +---+ |  +---+
  //   |   |         |        |        |        |
  //   |   | --------+        +--------+        |
  //   +---+                                    |
  //     ^                                      |
  //     +--------------------------------------+
  //
  std::atomic<HashtablezInfo*> all_;
  HashtablezInfo graveyard_;

  std::atomic<DisposeCallback> dispose_;
};

// Enables or disables sampling for Swiss tables.
void SetHashtablezEnabled(bool enabled);

// Sets the rate at which Swiss tables will be sampled.
void SetHashtablezSampleParameter(int32_t rate);

// Sets a soft max for the number of samples that will be kept.
void SetHashtablezMaxSamples(int32_t max);

// Configuration override.
// This allows process-wide sampling without depending on order of
// initialization of static storage duration objects.
// The definition of this constant is weak, which allows us to inject a
// different value for it at link time.
extern "C" bool AbslContainerInternalSampleEverything();

}  // namespace container_internal
IRESEARCH_ABSL_NAMESPACE_END
}  // namespace absl

#endif  // IRESEARCH_ABSL_CONTAINER_INTERNAL_HASHTABLEZ_SAMPLER_H_
