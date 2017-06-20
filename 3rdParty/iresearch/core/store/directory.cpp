//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#include "shared.hpp"
#include "directory.hpp"
#include "data_input.hpp"
#include "data_output.hpp"
#include "utils/log.hpp"
#include "utils/thread_utils.hpp"

NS_ROOT

// ----------------------------------------------------------------------------
// --SECTION--                                         directory implementation
// ----------------------------------------------------------------------------

directory::~directory() {}

// ----------------------------------------------------------------------------
// --SECTION--                                        index_lock implementation
// ----------------------------------------------------------------------------

index_lock::~index_lock() {}

bool index_lock::try_lock(size_t wait_timeout /* = 1000 */) NOEXCEPT {
  const size_t LOCK_POLL_INTERVAL = 1000;

  try {
    bool locked = lock();
    const size_t max_sleep_count = wait_timeout / LOCK_POLL_INTERVAL;

    for (size_t sleep_count = 0;
         !locked && (wait_timeout == LOCK_WAIT_FOREVER || sleep_count < max_sleep_count);
         ++sleep_count) {
      sleep_ms(LOCK_POLL_INTERVAL);
      locked = lock();
    }

    return locked;
  } catch (...) {
    IR_EXCEPTION();
  }

  return false;
}

NS_END
