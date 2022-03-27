// Copyright 2020 Google Inc. All Rights Reserved.
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

#ifndef S2_S2MEMORY_TRACKER_H_
#define S2_S2MEMORY_TRACKER_H_

#include "s2/s2error.h"
#include "s2/util/gtl/compact_array.h"

// S2MemoryTracker is a helper class for tracking and limiting the memory
// usage of S2 operations.  It provides the following functionality:
//
//  - Tracks the current and maximum memory usage of certain S2 classes
//    (including S2Builder, S2BooleanOperation, and S2BufferOperation).
//
//  - Supports cancelling the current operation if a given memory limit would
//    otherwise be exceeded.
//
//  - Invokes an optional callback after every N bytes of memory allocation,
//    and periodically within certain calculations that might take a long
//    time.  This gives the client an opportunity to cancel the current
//    operation for any reason, e.g. because the memory usage of the entire
//    thread or process is too high, a deadline was exceeded, an external
//    cancellation request was received, etc.
//
// To use it, clients simply create an S2MemoryTracker object and pass it to
// the desired S2 operations.  For example:
//
//   S2MemoryTracker tracker;
//   tracker.set_limit_bytes(500 << 20);     // 500 MB limit
//   S2Builder::Options options;
//   options.set_memory_tracker(&tracker);
//   S2Builder builder{options};
//   ...
//   S2Error error;
//   if (!builder.Build(&error)) {
//     if (error.code() == S2Error::RESOURCE_EXHAUSTED) {
//       S2_LOG(ERROR) << error;  // Memory limit exceeded
//     }
//   }
//
// Here is an example showing how to invoke a callback after every 10 MB of
// memory allocation:
//
//   tracker.set_periodic_callback(10 << 20 /*10 MB*/, [&]() {
//       if (MyCancellationCheck()) {
//         tracker.SetError(S2Error::CANCELLED, "Operation cancelled");
//       }
//     });
//
// Note that the callback is invoked based on cumulative allocation rather
// than current usage, e.g. a loop that repeatedly allocates and frees 1 MB
// would invoke the callback every 10 iterations.  Also note that the callback
// has control over what type of error is generated.
//
// This class is not thread-safe and therefore all objects associated with a
// single S2MemoryTracker should be accessed using a single thread.
//
// Implementation Notes
// --------------------
//
// In order to write a new class that tracks memory using S2MemoryTracker,
// users must analyze their data structures and make appropriate method calls.
// The major drawback to this approach is that it is fragile, since users
// might change their code but not their memory tracking.  The only way to
// avoid this problem is through rigorous testing.  See s2builder_test.cc
// and s2memory_tracker_testing.h for useful techniques.
//
// Note that malloc hooks are not a good solution for memory tracking within
// the S2 library.  The reasons for this include: (1) malloc hooks are
// program-wide and affect all threads, (2) the S2 library is used on many
// platforms (and by open source projects) and cannot depend on the features
// of specific memory allocators, and (3) certain S2 code paths can allocate a
// lot of memory at once, so it is better to predict and avoid such
// allocations rather than detecting them after the fact (as would happen with
// malloc hooks).
class S2MemoryTracker {
 public:
  S2MemoryTracker() = default;

  // The current tracked memory usage.
  //
  // CAVEAT: When an operation is cancelled (e.g. due to a memory limit being
  // exceeded) the value returned may be wildly inaccurate.  This is because
  // this method reports attempted rather than actual memory allocation, and
  // S2 operations often attempt to allocate memory even on their failure /
  // early exit code paths.
  int64 usage_bytes() const { return usage_bytes_; }

  // The maximum tracked memory usage.
  //
  // CAVEAT: When an operation is cancelled the return value may be wildly
  // inaccurate (see usage() for details).
  int64 max_usage_bytes() const { return max_usage_bytes_; }

  // Specifies a memory limit in bytes.  Whenever the tracked memory usage
  // would exceed this value, an error of type S2Error::RESOURCE_EXHAUSTED is
  // generated and the current operation will be cancelled.  If the value is
  // kNoLimit then memory usage is tracked but not limited.
  //
  // DEFAULT: kNoLimit
  int64 limit_bytes() const { return limit_bytes_; }
  void set_limit_bytes(int64 limit_bytes) { limit_bytes_ = limit_bytes; }

  // Indicates that memory usage is unlimited.
  static constexpr int64 kNoLimit = std::numeric_limits<int64>::max();

  // Returns the tracker's current error status.  Whenever an error exists
  // the current S2 operation will be cancelled.
  const S2Error& error() const { return error_; }

  // Returns true if no memory tracking errors have occurred.  If this method
  // returns false then the current S2 operation will be cancelled.
  bool ok() const { return error_.ok(); }

  // Sets the error status of the memory tracker.  Typically this method is
  // called from the periodic callback (see below).  Setting the error code to
  // anything other than S2Error::OK requests cancellation of the current S2
  // operation.
  //
  // CAVEAT: Do not use this method to clear an existing error unless you know
  // what you're doing.  Clients are not required to track memory accurately
  // once an operation has been cancelled, and therefore the only safe way to
  // reset the error status is to delete the S2MemoryTracker::Client object
  // where the error occurred (which will free all its memory and restore the
  // S2MemoryTracker to an accurate state).
  template <typename... Args>
  void SetError(S2Error::Code code, const absl::FormatSpec<Args...>& format,
                const Args&... args);

  // This version of SetError copies an existing error.
  void SetError(S2Error error);

  // A function that is called periodically to check whether the current
  // S2 operation should be cancelled.
  using PeriodicCallback = std::function<void ()>;

  // Sets a function that is called after every "callback_alloc_delta_bytes"
  // of tracked memory allocation to check whether the current operation
  // should be cancelled.  The callback may also be called periodically during
  // calculations that take a long time.  Once an error has occurred, further
  // callbacks are suppressed.
  void set_periodic_callback(int64 callback_alloc_delta_bytes,
                             PeriodicCallback periodic_callback) {
    callback_alloc_delta_bytes_ = callback_alloc_delta_bytes;
    callback_ = periodic_callback;
    callback_alloc_limit_bytes_ = alloc_bytes_ + callback_alloc_delta_bytes_;
  }
  int64 callback_alloc_delta_bytes() const {
    return callback_alloc_delta_bytes_;
  }
  const PeriodicCallback& periodic_callback() const { return callback_; }

  // Resets usage() and max_usage() to zero and clears any error.  Leaves all
  // other parameters unchanged.
  void Reset() {
    error_.Clear();
    usage_bytes_ = max_usage_bytes_ = alloc_bytes_ = 0;
    callback_alloc_limit_bytes_ = callback_alloc_delta_bytes_;
  }

  //////////////////////////////////////////////////////////////////////
  //
  // Everything below this point is only needed by classes that use
  // S2MemoryTracker to track their memory usage.

  // S2MemoryTracker::Client is used to track the memory used by a given S2
  // operation.  It points to an S2MemoryTracker tracker object and updates
  // the tracker's state as memory is allocated and freed.  Several client
  // objects can point to the same memory tracker; this makes it easier for
  // one S2 operation to use another S2 operation in its implementation.  (For
  // example, S2BooleanOperation is implemented using S2Builder.)
  //
  // The client object keeps track of its own memory usage in addition to
  // updating the shared S2MemoryTracker state.  This allows the memory used
  // by a particular client to be automatically subtracted from the total when
  // that client is destroyed.
  class Client {
   private:
    // Forward declaration to avoid code duplication.
    template <class T, bool exact> bool AddSpaceInternal(T* v, int64 n);

   public:
    // Specifies the S2MemoryTracker that will be used to track the memory
    // usage of this client.  Several S2 operations can use the same memory
    // tracker by creating different Client objects, e.g. S2BooleanOperation
    // has a client to track its memory usage, and it also uses S2Builder
    // which creates its own client.  This allows the total memory usage of
    // both classes to be tracked and controlled.

    // Default constructor.  Note that this class can be used without calling
    // Init(), but in that case memory usage is not tracked.
    Client() = default;

    // Convenience constructor that calls Init().
    explicit Client(S2MemoryTracker* tracker) {
      Init(tracker);
    }

    // Initializes this client to use the given memory tracker.  This function
    // may be called more than once (which is equivalent to destroying this
    // client and transferring the current memory usage to a new client).
    void Init(S2MemoryTracker* tracker) {
      int64 usage_bytes = client_usage_bytes_;
      Tally(-usage_bytes);
      tracker_ = tracker;
      Tally(usage_bytes);
    }

    // Returns the memory tracker associated with this client object.
    S2MemoryTracker* tracker() const { return tracker_; }

    // Returns true if this client has been initialized.
    bool is_active() const { return tracker_ != nullptr; }

    // When a Client object is destroyed, any remaining memory is subtracted
    // from the associated S2MemoryTracker (under the assumption that the
    // associated S2 operation has been destroyed as well).
    ~Client() { Tally(-client_usage_bytes_); }

    // Returns the current tracked memory usage.
    // XXX(ericv): Return 0 when not active.
    int64 usage_bytes() const {
      return tracker_ ? tracker_->usage_bytes() : 0;
    }

    // Returns the current tracked memory usage of this client only.
    // Returns zero if tracker() has not been initialized.
    int64 client_usage_bytes() const { return client_usage_bytes_; }

    // Returns the tracker's current error status.
    const S2Error& error() const;

    // Returns true if no memory tracking errors have occurred.  If this method
    // returns false then the current S2 operation will be cancelled.
    bool ok() const { return tracker_ ? tracker_->ok() : true; }

    // Records a "delta" bytes of memory use (positive or negative), returning
    // false if the current operation should be cancelled.
    bool Tally(int64 delta_bytes) {
      if (!is_active()) return true;
      client_usage_bytes_ += delta_bytes;
      return tracker_->Tally(delta_bytes);
    }

    // Specifies that "delta" bytes of memory will be allocated and then later
    // freed.  Returns false if the current operation should be cancelled.
    bool TallyTemp(int64 delta_bytes);

    // Adds the memory used by the given vector to the current tally.  Returns
    // false if the current operation should be cancelled.
    template <class T>
    inline bool Tally(const std::vector<T>& v) {
      return Tally(v.capacity() * sizeof(v[0]));
    }

    // Subtracts the memory used by the given vector from the current tally.
    // Returns false if the current operation should be cancelled.
    template <class T>
    inline bool Untally(const std::vector<T>& v) {
      return Tally(-v.capacity() * sizeof(v[0]));
    }

    // Ensures that the given vector has space for "n" additional elements and
    // tracks any additional memory that was allocated for this purpose.  This
    // method should be called before actually adding any elements to the
    // vector, otherwise memory will not be tracked correctly.  Returns false
    // if the current operation should be cancelled.
    template <class T>
    inline bool AddSpace(T* v, int64 n) {
      return AddSpaceInternal<T, false>(v, n);
    }

    // Like AddSpace() except that if more memory is needed the vector is
    // sized to hold exactly "n" additional elements.  Returns false if the
    // current operation should be cancelled.
    template <class T>
    inline bool AddSpaceExact(T* v, int64 n) {
      return AddSpaceInternal<T, true>(v, n);
    }

    // Deallocates storage for the given vector and updates the memory
    // tracking accordingly.  Returns false if the current operation should be
    // cancelled.
    template <class T>
    inline bool Clear(T* v) {
      int64 old_capacity = v->capacity();
      T().swap(*v);
      return Tally(-old_capacity * sizeof((*v)[0]));
    }

    // Returns the number of allocated bytes used by gtl::compact_array<T>.
    // (This class is similar to std::vector but stores a small number of
    // elements inline.)
    template <class T>
    static int64 GetCompactArrayAllocBytes(const gtl::compact_array<T>& array);

    // Returns the estimated minimum number of allocated bytes for each
    // additional entry in an absl::btree_* container (e.g. absl::btree_map)
    // including overhead.  This estimate is accurate only if the container
    // nodes are nearly full, as when elements are added in sorted order.
    // Otherwise nodes are expected to be 75% full on average.
    //
    // This function can be used to approximately track memory usage while
    // such a container is being built.  Once construction is complete, the
    // exact memory usage can be determined using "container.bytes_used()".
    template <class T>
    static int64 GetBtreeMinBytesPerEntry();

   private:
    S2MemoryTracker* tracker_ = nullptr;
    int64 client_usage_bytes_ = 0;
  };

 private:
  friend class Client;

  bool Tally(int64 delta_bytes);
  void SetLimitExceededError();

  int64 usage_bytes_ = 0;
  int64 max_usage_bytes_ = 0;
  int64 limit_bytes_ = kNoLimit;
  int64 alloc_bytes_ = 0;
  S2Error error_;
  PeriodicCallback callback_;
  int64 callback_alloc_delta_bytes_ = 0;
  int64 callback_alloc_limit_bytes_ = kNoLimit;
};


//////////////////   Implementation details follow   ////////////////////

template <typename... Args>
void S2MemoryTracker::SetError(S2Error::Code code,
                               const absl::FormatSpec<Args...>& format,
                               const Args&... args) {
  error_.Init(code, format, args...);
}

inline bool S2MemoryTracker::Tally(int64 delta_bytes) {
  usage_bytes_ += delta_bytes;
  alloc_bytes_ += std::max(int64{0}, delta_bytes);
  max_usage_bytes_ = std::max(max_usage_bytes_, usage_bytes_);
  if (usage_bytes_ > limit_bytes_ && ok()) SetLimitExceededError();
  if (callback_ && alloc_bytes_ >= callback_alloc_limit_bytes_) {
    callback_alloc_limit_bytes_ = alloc_bytes_ + callback_alloc_delta_bytes_;
    if (ok()) callback_();
  }
  return ok();
}

template <class T, bool exact>
inline bool S2MemoryTracker::Client::AddSpaceInternal(T* v, int64 n) {
  int64 new_size = v->size() + n;
  int64 old_capacity = v->capacity();
  if (new_size <= old_capacity) return true;
  int64 new_capacity = exact ? new_size : std::max(new_size, 2 * old_capacity);
  // Note that reserve() allocates new storage before freeing the old storage.
  if (!Tally(new_capacity * sizeof((*v)[0]))) return false;
  v->reserve(new_capacity);
  S2_DCHECK_EQ(v->capacity(), new_capacity);
  return Tally(-old_capacity * sizeof((*v)[0]));
}

// Returns the number of bytes used by compact_array<T>.
template <class T>
int64 S2MemoryTracker::Client::GetCompactArrayAllocBytes(
    const gtl::compact_array<T>& array) {
  // Unfortunately this information isn't part of the public API.
  enum {
    kMaxInlinedBytes = 11,
    kInlined = kMaxInlinedBytes / sizeof(T)
  };
  int n = array.capacity();
  return (n <= kInlined) ? 0 : n * sizeof(T);
}

template <class T>
int64 S2MemoryTracker::Client::GetBtreeMinBytesPerEntry() {
  return 1.12 * sizeof(typename T::value_type);
}

#endif  // S2_S2MEMORY_TRACKER_H_
