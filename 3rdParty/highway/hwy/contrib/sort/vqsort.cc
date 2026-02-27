// Copyright 2021 Google LLC
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

#include "hwy/contrib/sort/vqsort.h"

#include <time.h>

#include <cstdint>

#include "hwy/base.h"
#include "hwy/contrib/sort/shared-inl.h"

// Check if we have sys/random.h. First skip some systems on which the check
// itself (features.h) might be problematic.
#if defined(ANDROID) || defined(__ANDROID__) || HWY_ARCH_RVV
#define VQSORT_GETRANDOM 0
#endif

#if !defined(VQSORT_GETRANDOM) && HWY_OS_LINUX
#include <features.h>

// ---- which libc
#if defined(__UCLIBC__)
#define VQSORT_GETRANDOM 1  // added Mar 2015, before uclibc-ng 1.0

#elif defined(__GLIBC__) && defined(__GLIBC_PREREQ)
#if __GLIBC_PREREQ(2, 25)
#define VQSORT_GETRANDOM 1
#else
#define VQSORT_GETRANDOM 0
#endif

#else
// Assume MUSL, which has getrandom since 2018. There is no macro to test, see
// https://www.openwall.com/lists/musl/2013/03/29/13.
#define VQSORT_GETRANDOM 1

#endif  // ---- which libc
#endif  // linux

#if !defined(VQSORT_GETRANDOM)
#define VQSORT_GETRANDOM 0
#endif

// Choose a seed source for SFC generator: 1=getrandom, 2=CryptGenRandom.
// Allow user override - not all Android support the getrandom wrapper.
#ifndef VQSORT_SECURE_SEED

#if VQSORT_GETRANDOM
#define VQSORT_SECURE_SEED 1
#elif defined(_WIN32) || defined(_WIN64)
#define VQSORT_SECURE_SEED 2
#else
#define VQSORT_SECURE_SEED 0
#endif

#endif  // VQSORT_SECURE_SEED

// Pull in dependencies of the chosen seed source.
#if VQSORT_SECURE_SEED == 1
#include <sys/random.h>
#elif VQSORT_SECURE_SEED == 2
#include <windows.h>
#pragma comment(lib, "advapi32.lib")
// Must come after windows.h.
#include <wincrypt.h>
#endif  // VQSORT_SECURE_SEED

namespace hwy {
namespace {

void Fill16Bytes(void* bytes) {
#if VQSORT_SECURE_SEED == 1
  // May block if urandom is not yet initialized.
  const ssize_t ret = getrandom(bytes, 16, /*flags=*/0);
  if (ret == 16) return;
#elif VQSORT_SECURE_SEED == 2
  HCRYPTPROV hProvider{};
  if (CryptAcquireContextA(&hProvider, nullptr, nullptr, PROV_RSA_FULL,
                           CRYPT_VERIFYCONTEXT)) {
    const BOOL ok =
        CryptGenRandom(hProvider, 16, reinterpret_cast<BYTE*>(bytes));
    CryptReleaseContext(hProvider, 0);
    if (ok) return;
  }
#endif

  // VQSORT_SECURE_SEED == 0, or one of the above failed. Get some entropy from
  // the address and the clock() timer.
  uint64_t* words = reinterpret_cast<uint64_t*>(bytes);
  uint64_t** seed_stack = &words;
  void (*seed_code)(void*) = &Fill16Bytes;
  const uintptr_t bits_stack = reinterpret_cast<uintptr_t>(seed_stack);
  const uintptr_t bits_code = reinterpret_cast<uintptr_t>(seed_code);
  const uint64_t bits_time = static_cast<uint64_t>(clock());
  words[0] = bits_stack ^ bits_time ^ 0xFEDCBA98;  // "Nothing up my sleeve"
  words[1] = bits_code ^ bits_time ^ 0x01234567;   // constants.
}

}  // namespace

uint64_t* GetGeneratorState() {
  thread_local uint64_t state[3] = {0};
  // This is a counter; zero indicates not yet initialized.
  if (HWY_UNLIKELY(state[2] == 0)) {
    Fill16Bytes(state);
    state[2] = 1;
  }
  return state;
}

}  // namespace hwy
