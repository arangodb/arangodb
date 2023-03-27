////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Common.h"
#include "Basics/operating-system.h"

#include <thread>

namespace arangodb::basics {

////////////////////////////////////////////////////////////////////////////////
/// @brief Function to let CPU relax inside spinlock.
////////////////////////////////////////////////////////////////////////////////
inline void cpu_relax() noexcept {
  // <boost/fiber/detail/cpu_relax.hpp> used as reference
  // We cannot use it directly because it's detail and
  // doesn't compile on windows if I just include it
#if defined(_WIN32)
  YieldProcessor();
#elif (defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__) ||   \
       defined(__ARM_ARCH_6ZK__) || defined(__ARM_ARCH_6T2__) || \
       defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) ||    \
       defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) ||   \
       defined(__ARM_ARCH_7S__) || defined(__ARM_ARCH_8A__) ||   \
       defined(__aarch64__))
  asm volatile("yield" ::: "memory");
#elif (defined(__i386) || defined(_M_IX86) || defined(__x86_64__) || \
       defined(_M_X64))
  asm volatile("pause" ::: "memory");
#else
  static constexpr std::chrono::microseconds us0{0};
  std::this_thread::sleep_for(us0);
#endif
}

}  // namespace arangodb::basics
