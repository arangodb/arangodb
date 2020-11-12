////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "directory.hpp"

#include <chrono>
#include <thread>

#include "shared.hpp"
#include "data_input.hpp"
#include "data_output.hpp"
#include "utils/log.hpp"
#include "utils/thread_utils.hpp"

namespace iresearch {

// ----------------------------------------------------------------------------
// --SECTION--                                        index_lock implementation
// ----------------------------------------------------------------------------

bool index_lock::try_lock(size_t wait_timeout /* = 1000 */) noexcept {
  using namespace std::chrono_literals;

  constexpr size_t LOCK_POLL_INTERVAL = 1000;

  try {
    bool locked = lock();
    const size_t max_sleep_count = wait_timeout / LOCK_POLL_INTERVAL;

    for (size_t sleep_count = 0;
         !locked && (wait_timeout == LOCK_WAIT_FOREVER || sleep_count < max_sleep_count);
         ++sleep_count) {
      std::this_thread::sleep_for(std::chrono::milliseconds(LOCK_POLL_INTERVAL));
      locked = lock();
    }

    return locked;
  } catch (...) {
  }

  return false;
}

}
