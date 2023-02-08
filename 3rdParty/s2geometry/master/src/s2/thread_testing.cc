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

#include "s2/thread_testing.h"

#include <functional>
#include <memory>
#include <thread>

#include "s2/base/logging.h"
#include "absl/memory/memory.h"

using absl::make_unique;
using std::unique_ptr;

namespace s2testing {

class ReaderThreadPool {
 public:
  ReaderThreadPool(const std::function<void ()>& callback, int num_threads)
      : threads_(make_unique<std::thread[]>(num_threads)),
        num_threads_(num_threads) {
    for (int i = 0; i < num_threads_; ++i) {
      threads_[i] = std::thread(callback);
    }
  }
  ~ReaderThreadPool() {
    for (int i = 0; i < num_threads_; ++i) threads_[i].join();
  }

 private:
  unique_ptr<std::thread[]> threads_;
  int num_threads_;
};

void ReaderWriterTest::ReaderLoop() {
  lock_.Lock();
  for (int last_write = 0; ; last_write = num_writes_) {
    while (num_writes_ == last_write) {
      write_ready_.Wait(&lock_);
    }
    if (num_writes_ < 0) break;

    // Release the lock first so that all reader threads can run in parallel.
    lock_.Unlock();
    ReadOp();
    lock_.Lock();
    if (--num_readers_left_ == 0) {
      all_readers_done_.Signal();
    }
  }
  lock_.Unlock();
}

void ReaderWriterTest::Run(int num_readers, int iters) {
  ReaderThreadPool pool(std::bind(&ReaderWriterTest::ReaderLoop, this),
                        num_readers);
  lock_.Lock();
  for (int iter = 0; iter < iters; ++iter) {
    // Loop invariant: lock_ is held and num_readers_left_ == 0.
    S2_DCHECK_EQ(0, num_readers_left_);
    WriteOp();

    // Now set the readers loose.
    num_readers_left_ = num_readers;
    ++num_writes_;
    write_ready_.SignalAll();
    while (num_readers_left_ > 0) {
      all_readers_done_.Wait(&lock_);
    }
  }
  // Signal the readers to exit.
  num_writes_ = -1;
  write_ready_.SignalAll();
  lock_.Unlock();
  // ReaderThreadPool destructor waits for all threads to complete.
}

}  // namespace s2testing
