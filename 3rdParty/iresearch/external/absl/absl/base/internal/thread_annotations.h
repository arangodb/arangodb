// Copyright 2019 The Abseil Authors.
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
// File: thread_annotations.h
// -----------------------------------------------------------------------------
//
// WARNING: This is a backwards compatible header and it will be removed after
// the migration to prefixed thread annotations is finished; please include
// "absl/base/thread_annotations.h".
//
// This header file contains macro definitions for thread safety annotations
// that allow developers to document the locking policies of multi-threaded
// code. The annotations can also help program analysis tools to identify
// potential thread safety issues.
//
// These annotations are implemented using compiler attributes. Using the macros
// defined here instead of raw attributes allow for portability and future
// compatibility.
//
// When referring to mutexes in the arguments of the attributes, you should
// use variable names or more complex expressions (e.g. my_object->mutex_)
// that evaluate to a concrete mutex object whenever possible. If the mutex
// you want to refer to is not in scope, you may use a member pointer
// (e.g. &MyClass::mutex_) to refer to a mutex in some (unknown) object.

#ifndef IRESEARCH_ABSL_BASE_INTERNAL_THREAD_ANNOTATIONS_H_
#define IRESEARCH_ABSL_BASE_INTERNAL_THREAD_ANNOTATIONS_H_

#if defined(__clang__)
#define IRESEARCH_ABSL_INTERNAL_THREAD_ANNOTATION_ATTRIBUTE__(x)   __attribute__((x))
#else
#define IRESEARCH_ABSL_INTERNAL_THREAD_ANNOTATION_ATTRIBUTE__(x)   // no-op
#endif

// IRESEARCH_ABSL_INTERNAL_GUARDED_BY()
//
// Documents if a shared field or global variable needs to be protected by a
// mutex. IRESEARCH_ABSL_INTERNAL_GUARDED_BY() allows the user to specify a particular mutex that
// should be held when accessing the annotated variable.
//
// Although this annotation (and IRESEARCH_ABSL_INTERNAL_PT_GUARDED_BY, below) cannot be applied to
// local variables, a local variable and its associated mutex can often be
// combined into a small class or struct, thereby allowing the annotation.
//
// Example:
//
//   class Foo {
//     Mutex mu_;
//     int p1_ IRESEARCH_ABSL_INTERNAL_GUARDED_BY(mu_);
//     ...
//   };
#define IRESEARCH_ABSL_INTERNAL_GUARDED_BY(x) IRESEARCH_ABSL_INTERNAL_THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(x))

// IRESEARCH_ABSL_INTERNAL_PT_GUARDED_BY()
//
// Documents if the memory location pointed to by a pointer should be guarded
// by a mutex when dereferencing the pointer.
//
// Example:
//   class Foo {
//     Mutex mu_;
//     int *p1_ IRESEARCH_ABSL_INTERNAL_PT_GUARDED_BY(mu_);
//     ...
//   };
//
// Note that a pointer variable to a shared memory location could itself be a
// shared variable.
//
// Example:
//
//   // `q_`, guarded by `mu1_`, points to a shared memory location that is
//   // guarded by `mu2_`:
//   int *q_ IRESEARCH_ABSL_INTERNAL_GUARDED_BY(mu1_) IRESEARCH_ABSL_INTERNAL_PT_GUARDED_BY(mu2_);
#define IRESEARCH_ABSL_INTERNAL_PT_GUARDED_BY(x) IRESEARCH_ABSL_INTERNAL_THREAD_ANNOTATION_ATTRIBUTE__(pt_guarded_by(x))

// IRESEARCH_ABSL_INTERNAL_ACQUIRED_AFTER() / IRESEARCH_ABSL_INTERNAL_ACQUIRED_BEFORE()
//
// Documents the acquisition order between locks that can be held
// simultaneously by a thread. For any two locks that need to be annotated
// to establish an acquisition order, only one of them needs the annotation.
// (i.e. You don't have to annotate both locks with both IRESEARCH_ABSL_INTERNAL_ACQUIRED_AFTER
// and IRESEARCH_ABSL_INTERNAL_ACQUIRED_BEFORE.)
//
// As with IRESEARCH_ABSL_INTERNAL_GUARDED_BY, this is only applicable to mutexes that are shared
// fields or global variables.
//
// Example:
//
//   Mutex m1_;
//   Mutex m2_ IRESEARCH_ABSL_INTERNAL_ACQUIRED_AFTER(m1_);
#define IRESEARCH_ABSL_INTERNAL_ACQUIRED_AFTER(...) \
  IRESEARCH_ABSL_INTERNAL_THREAD_ANNOTATION_ATTRIBUTE__(acquired_after(__VA_ARGS__))

#define IRESEARCH_ABSL_INTERNAL_ACQUIRED_BEFORE(...) \
  IRESEARCH_ABSL_INTERNAL_THREAD_ANNOTATION_ATTRIBUTE__(acquired_before(__VA_ARGS__))

// IRESEARCH_ABSL_INTERNAL_EXCLUSIVE_LOCKS_REQUIRED() / IRESEARCH_ABSL_INTERNAL_SHARED_LOCKS_REQUIRED()
//
// Documents a function that expects a mutex to be held prior to entry.
// The mutex is expected to be held both on entry to, and exit from, the
// function.
//
// An exclusive lock allows read-write access to the guarded data member(s), and
// only one thread can acquire a lock exclusively at any one time. A shared lock
// allows read-only access, and any number of threads can acquire a shared lock
// concurrently.
//
// Generally, non-const methods should be annotated with
// IRESEARCH_ABSL_INTERNAL_EXCLUSIVE_LOCKS_REQUIRED, while const methods should be annotated with
// IRESEARCH_ABSL_INTERNAL_SHARED_LOCKS_REQUIRED.
//
// Example:
//
//   Mutex mu1, mu2;
//   int a IRESEARCH_ABSL_INTERNAL_GUARDED_BY(mu1);
//   int b IRESEARCH_ABSL_INTERNAL_GUARDED_BY(mu2);
//
//   void foo() IRESEARCH_ABSL_INTERNAL_EXCLUSIVE_LOCKS_REQUIRED(mu1, mu2) { ... }
//   void bar() const IRESEARCH_ABSL_INTERNAL_SHARED_LOCKS_REQUIRED(mu1, mu2) { ... }
#define IRESEARCH_ABSL_INTERNAL_EXCLUSIVE_LOCKS_REQUIRED(...) \
  IRESEARCH_ABSL_INTERNAL_THREAD_ANNOTATION_ATTRIBUTE__(exclusive_locks_required(__VA_ARGS__))

#define IRESEARCH_ABSL_INTERNAL_SHARED_LOCKS_REQUIRED(...) \
  IRESEARCH_ABSL_INTERNAL_THREAD_ANNOTATION_ATTRIBUTE__(shared_locks_required(__VA_ARGS__))

// IRESEARCH_ABSL_INTERNAL_LOCKS_EXCLUDED()
//
// Documents the locks acquired in the body of the function. These locks
// cannot be held when calling this function (as Abseil's `Mutex` locks are
// non-reentrant).
#define IRESEARCH_ABSL_INTERNAL_LOCKS_EXCLUDED(...) \
  IRESEARCH_ABSL_INTERNAL_THREAD_ANNOTATION_ATTRIBUTE__(locks_excluded(__VA_ARGS__))

// IRESEARCH_ABSL_INTERNAL_LOCK_RETURNED()
//
// Documents a function that returns a mutex without acquiring it.  For example,
// a public getter method that returns a pointer to a private mutex should
// be annotated with IRESEARCH_ABSL_INTERNAL_LOCK_RETURNED.
#define IRESEARCH_ABSL_INTERNAL_LOCK_RETURNED(x) \
  IRESEARCH_ABSL_INTERNAL_THREAD_ANNOTATION_ATTRIBUTE__(lock_returned(x))

// IRESEARCH_ABSL_INTERNAL_LOCKABLE
//
// Documents if a class/type is a lockable type (such as the `Mutex` class).
#define IRESEARCH_ABSL_INTERNAL_LOCKABLE \
  IRESEARCH_ABSL_INTERNAL_THREAD_ANNOTATION_ATTRIBUTE__(lockable)

// IRESEARCH_ABSL_INTERNAL_SCOPED_LOCKABLE
//
// Documents if a class does RAII locking (such as the `MutexLock` class).
// The constructor should use `LOCK_FUNCTION()` to specify the mutex that is
// acquired, and the destructor should use `IRESEARCH_ABSL_INTERNAL_UNLOCK_FUNCTION()` with no
// arguments; the analysis will assume that the destructor unlocks whatever the
// constructor locked.
#define IRESEARCH_ABSL_INTERNAL_SCOPED_LOCKABLE \
  IRESEARCH_ABSL_INTERNAL_THREAD_ANNOTATION_ATTRIBUTE__(scoped_lockable)

// IRESEARCH_ABSL_INTERNAL_EXCLUSIVE_LOCK_FUNCTION()
//
// Documents functions that acquire a lock in the body of a function, and do
// not release it.
#define IRESEARCH_ABSL_INTERNAL_EXCLUSIVE_LOCK_FUNCTION(...) \
  IRESEARCH_ABSL_INTERNAL_THREAD_ANNOTATION_ATTRIBUTE__(exclusive_lock_function(__VA_ARGS__))

// IRESEARCH_ABSL_INTERNAL_SHARED_LOCK_FUNCTION()
//
// Documents functions that acquire a shared (reader) lock in the body of a
// function, and do not release it.
#define IRESEARCH_ABSL_INTERNAL_SHARED_LOCK_FUNCTION(...) \
  IRESEARCH_ABSL_INTERNAL_THREAD_ANNOTATION_ATTRIBUTE__(shared_lock_function(__VA_ARGS__))

// IRESEARCH_ABSL_INTERNAL_UNLOCK_FUNCTION()
//
// Documents functions that expect a lock to be held on entry to the function,
// and release it in the body of the function.
#define IRESEARCH_ABSL_INTERNAL_UNLOCK_FUNCTION(...) \
  IRESEARCH_ABSL_INTERNAL_THREAD_ANNOTATION_ATTRIBUTE__(unlock_function(__VA_ARGS__))

// IRESEARCH_ABSL_INTERNAL_EXCLUSIVE_TRYLOCK_FUNCTION() / IRESEARCH_ABSL_INTERNAL_SHARED_TRYLOCK_FUNCTION()
//
// Documents functions that try to acquire a lock, and return success or failure
// (or a non-boolean value that can be interpreted as a boolean).
// The first argument should be `true` for functions that return `true` on
// success, or `false` for functions that return `false` on success. The second
// argument specifies the mutex that is locked on success. If unspecified, this
// mutex is assumed to be `this`.
#define IRESEARCH_ABSL_INTERNAL_EXCLUSIVE_TRYLOCK_FUNCTION(...) \
  IRESEARCH_ABSL_INTERNAL_THREAD_ANNOTATION_ATTRIBUTE__(exclusive_trylock_function(__VA_ARGS__))

#define IRESEARCH_ABSL_INTERNAL_SHARED_TRYLOCK_FUNCTION(...) \
  IRESEARCH_ABSL_INTERNAL_THREAD_ANNOTATION_ATTRIBUTE__(shared_trylock_function(__VA_ARGS__))

// IRESEARCH_ABSL_INTERNAL_ASSERT_EXCLUSIVE_LOCK() / IRESEARCH_ABSL_INTERNAL_ASSERT_SHARED_LOCK()
//
// Documents functions that dynamically check to see if a lock is held, and fail
// if it is not held.
#define IRESEARCH_ABSL_INTERNAL_ASSERT_EXCLUSIVE_LOCK(...) \
  IRESEARCH_ABSL_INTERNAL_THREAD_ANNOTATION_ATTRIBUTE__(assert_exclusive_lock(__VA_ARGS__))

#define IRESEARCH_ABSL_INTERNAL_ASSERT_SHARED_LOCK(...) \
  IRESEARCH_ABSL_INTERNAL_THREAD_ANNOTATION_ATTRIBUTE__(assert_shared_lock(__VA_ARGS__))

// IRESEARCH_ABSL_INTERNAL_NO_THREAD_SAFETY_ANALYSIS
//
// Turns off thread safety checking within the body of a particular function.
// This annotation is used to mark functions that are known to be correct, but
// the locking behavior is more complicated than the analyzer can handle.
#define IRESEARCH_ABSL_INTERNAL_NO_THREAD_SAFETY_ANALYSIS \
  IRESEARCH_ABSL_INTERNAL_THREAD_ANNOTATION_ATTRIBUTE__(no_thread_safety_analysis)

//------------------------------------------------------------------------------
// Tool-Supplied Annotations
//------------------------------------------------------------------------------

// IRESEARCH_ABSL_INTERNAL_TS_UNCHECKED should be placed around lock expressions that are not valid
// C++ syntax, but which are present for documentation purposes.  These
// annotations will be ignored by the analysis.
#define IRESEARCH_ABSL_INTERNAL_TS_UNCHECKED(x) ""

// IRESEARCH_ABSL_INTERNAL_TS_FIXME is used to mark lock expressions that are not valid C++ syntax.
// It is used by automated tools to mark and disable invalid expressions.
// The annotation should either be fixed, or changed to IRESEARCH_ABSL_INTERNAL_TS_UNCHECKED.
#define IRESEARCH_ABSL_INTERNAL_TS_FIXME(x) ""

// Like IRESEARCH_ABSL_INTERNAL_NO_THREAD_SAFETY_ANALYSIS, this turns off checking within the body of
// a particular function.  However, this attribute is used to mark functions
// that are incorrect and need to be fixed.  It is used by automated tools to
// avoid breaking the build when the analysis is updated.
// Code owners are expected to eventually fix the routine.
#define IRESEARCH_ABSL_INTERNAL_NO_THREAD_SAFETY_ANALYSIS_FIXME  IRESEARCH_ABSL_INTERNAL_NO_THREAD_SAFETY_ANALYSIS

// Similar to IRESEARCH_ABSL_INTERNAL_NO_THREAD_SAFETY_ANALYSIS_FIXME, this macro marks a IRESEARCH_ABSL_INTERNAL_GUARDED_BY
// annotation that needs to be fixed, because it is producing thread safety
// warning.  It disables the IRESEARCH_ABSL_INTERNAL_GUARDED_BY.
#define IRESEARCH_ABSL_INTERNAL_GUARDED_BY_FIXME(x)

// Disables warnings for a single read operation.  This can be used to avoid
// warnings when it is known that the read is not actually involved in a race,
// but the compiler cannot confirm that.
#define IRESEARCH_ABSL_INTERNAL_TS_UNCHECKED_READ(x) iresearch_absl_thread_safety_analysis::ts_unchecked_read(x)


namespace iresearch_absl_thread_safety_analysis {

// Takes a reference to a guarded data member, and returns an unguarded
// reference.
template <typename T>
inline const T& ts_unchecked_read(const T& v) IRESEARCH_ABSL_INTERNAL_NO_THREAD_SAFETY_ANALYSIS {
  return v;
}

template <typename T>
inline T& ts_unchecked_read(T& v) IRESEARCH_ABSL_INTERNAL_NO_THREAD_SAFETY_ANALYSIS {
  return v;
}

}  // namespace iresearch_absl_thread_safety_analysis

#endif  // IRESEARCH_ABSL_BASE_INTERNAL_THREAD_ANNOTATIONS_H_
