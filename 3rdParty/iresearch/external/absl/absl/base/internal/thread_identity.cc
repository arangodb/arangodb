// Copyright 2017 The Abseil Authors.
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

#include "absl/base/internal/thread_identity.h"

#ifndef _WIN32
#include <pthread.h>
#include <signal.h>
#endif

#include <atomic>
#include <cassert>
#include <memory>

#include "absl/base/call_once.h"
#include "absl/base/internal/raw_logging.h"
#include "absl/base/internal/spinlock.h"

namespace iresearch_absl {
IRESEARCH_ABSL_NAMESPACE_BEGIN
namespace base_internal {

#if IRESEARCH_ABSL_THREAD_IDENTITY_MODE != IRESEARCH_ABSL_THREAD_IDENTITY_MODE_USE_CPP11
namespace {
// Used to co-ordinate one-time creation of our pthread_key
absl::once_flag init_thread_identity_key_once;
pthread_key_t thread_identity_pthread_key;
std::atomic<bool> pthread_key_initialized(false);

void AllocateThreadIdentityKey(ThreadIdentityReclaimerFunction reclaimer) {
  pthread_key_create(&thread_identity_pthread_key, reclaimer);
  pthread_key_initialized.store(true, std::memory_order_release);
}
}  // namespace
#endif

#if IRESEARCH_ABSL_THREAD_IDENTITY_MODE == IRESEARCH_ABSL_THREAD_IDENTITY_MODE_USE_TLS || \
    IRESEARCH_ABSL_THREAD_IDENTITY_MODE == IRESEARCH_ABSL_THREAD_IDENTITY_MODE_USE_CPP11
// The actual TLS storage for a thread's currently associated ThreadIdentity.
// This is referenced by inline accessors in the header.
// "protected" visibility ensures that if multiple instances of Abseil code
// exist within a process (via dlopen() or similar), references to
// thread_identity_ptr from each instance of the code will refer to
// *different* instances of this ptr.
#ifdef __GNUC__
__attribute__((visibility("protected")))
#endif  // __GNUC__
#if IRESEARCH_ABSL_PER_THREAD_TLS
// Prefer __thread to thread_local as benchmarks indicate it is a bit faster.
ABSL_PER_THREAD_TLS_KEYWORD ThreadIdentity* thread_identity_ptr = nullptr;
#elif defined(IRESEARCH_ABSL_HAVE_THREAD_LOCAL)
thread_local ThreadIdentity* thread_identity_ptr = nullptr;
#endif  // IRESEARCH_ABSL_PER_THREAD_TLS
#endif  // TLS or CPP11

void SetCurrentThreadIdentity(
    ThreadIdentity* identity, ThreadIdentityReclaimerFunction reclaimer) {
  assert(CurrentThreadIdentityIfPresent() == nullptr);
  // Associate our destructor.
  // NOTE: This call to pthread_setspecific is currently the only immovable
  // barrier to CurrentThreadIdentity() always being async signal safe.
#if IRESEARCH_ABSL_THREAD_IDENTITY_MODE == IRESEARCH_ABSL_THREAD_IDENTITY_MODE_USE_POSIX_SETSPECIFIC
  // NOTE: Not async-safe.  But can be open-coded.
  iresearch_absl::call_once(init_thread_identity_key_once, AllocateThreadIdentityKey,
                  reclaimer);

#if defined(__EMSCRIPTEN__) || defined(__MINGW32__)
  // Emscripten and MinGW pthread implementations does not support signals.
  // See https://kripken.github.io/emscripten-site/docs/porting/pthreads.html
  // for more information.
  pthread_setspecific(thread_identity_pthread_key,
                      reinterpret_cast<void*>(identity));
#else
  // We must mask signals around the call to setspecific as with current glibc,
  // a concurrent getspecific (needed for GetCurrentThreadIdentityIfPresent())
  // may zero our value.
  //
  // While not officially async-signal safe, getspecific within a signal handler
  // is otherwise OK.
  sigset_t all_signals;
  sigset_t curr_signals;
  sigfillset(&all_signals);
  pthread_sigmask(SIG_SETMASK, &all_signals, &curr_signals);
  pthread_setspecific(thread_identity_pthread_key,
                      reinterpret_cast<void*>(identity));
  pthread_sigmask(SIG_SETMASK, &curr_signals, nullptr);
#endif  // !__EMSCRIPTEN__ && !__MINGW32__

#elif IRESEARCH_ABSL_THREAD_IDENTITY_MODE == IRESEARCH_ABSL_THREAD_IDENTITY_MODE_USE_TLS
  // NOTE: Not async-safe.  But can be open-coded.
  iresearch_absl::call_once(init_thread_identity_key_once, AllocateThreadIdentityKey,
                  reclaimer);
  pthread_setspecific(thread_identity_pthread_key,
                      reinterpret_cast<void*>(identity));
  thread_identity_ptr = identity;
#elif IRESEARCH_ABSL_THREAD_IDENTITY_MODE == IRESEARCH_ABSL_THREAD_IDENTITY_MODE_USE_CPP11
  thread_local std::unique_ptr<ThreadIdentity, ThreadIdentityReclaimerFunction>
      holder(identity, reclaimer);
  thread_identity_ptr = identity;
#else
#error Unimplemented IRESEARCH_ABSL_THREAD_IDENTITY_MODE
#endif
}

#if IRESEARCH_ABSL_THREAD_IDENTITY_MODE == IRESEARCH_ABSL_THREAD_IDENTITY_MODE_USE_TLS || \
    IRESEARCH_ABSL_THREAD_IDENTITY_MODE == IRESEARCH_ABSL_THREAD_IDENTITY_MODE_USE_CPP11

// Please see the comment on `CurrentThreadIdentityIfPresent` in
// thread_identity.h. Because DLLs cannot expose thread_local variables in
// headers, we opt for the correct-but-slower option of placing the definition
// of this function only in a translation unit inside DLL.
#if defined(IRESEARCH_ABSL_BUILD_DLL) || defined(IRESEARCH_ABSL_CONSUME_DLL)
ThreadIdentity* CurrentThreadIdentityIfPresent() { return thread_identity_ptr; }
#endif
#endif

void ClearCurrentThreadIdentity() {
#if IRESEARCH_ABSL_THREAD_IDENTITY_MODE == IRESEARCH_ABSL_THREAD_IDENTITY_MODE_USE_TLS || \
    IRESEARCH_ABSL_THREAD_IDENTITY_MODE == IRESEARCH_ABSL_THREAD_IDENTITY_MODE_USE_CPP11
  thread_identity_ptr = nullptr;
#elif IRESEARCH_ABSL_THREAD_IDENTITY_MODE == \
      IRESEARCH_ABSL_THREAD_IDENTITY_MODE_USE_POSIX_SETSPECIFIC
  // pthread_setspecific expected to clear value on destruction
  assert(CurrentThreadIdentityIfPresent() == nullptr);
#endif
}

#if IRESEARCH_ABSL_THREAD_IDENTITY_MODE == IRESEARCH_ABSL_THREAD_IDENTITY_MODE_USE_POSIX_SETSPECIFIC
ThreadIdentity* CurrentThreadIdentityIfPresent() {
  bool initialized = pthread_key_initialized.load(std::memory_order_acquire);
  if (!initialized) {
    return nullptr;
  }
  return reinterpret_cast<ThreadIdentity*>(
      pthread_getspecific(thread_identity_pthread_key));
}
#endif

}  // namespace base_internal
IRESEARCH_ABSL_NAMESPACE_END
}  // namespace absl
