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

#ifndef S2_THREAD_TESTING_H_
#define S2_THREAD_TESTING_H_

#include "absl/synchronization/mutex.h"

namespace s2testing {

// A test harness for verifying the thread safety properties of a class.  It
// creates one writer thread and several reader threads, and repeatedly
// alternates between executing some operation in the writer thread and then
// executing some other operation in parallel in the reader threads.
//
// It is intended for testing thread-compatible classes, i.e. those where
// const methods are thread safe and non-const methods are not thread safe.
class ReaderWriterTest {
 public:
  ReaderWriterTest() {}
  virtual ~ReaderWriterTest() {}

  // Create the given number of reader threads and execute the given number of
  // (write, read) iterations.
  void Run(int num_readers, int iters);

  // The writer thread calls the following function once between reads.
  virtual void WriteOp() = 0;

  // Each reader thread calls the following function once between writes.
  virtual void ReadOp() = 0;

 protected:
  void ReaderLoop();  // The main loop of each reader thread.

  // The following fields are guarded by lock_.
  absl::Mutex lock_;
  int num_writes_ = 0;
  int num_readers_left_ = 0;

  // TODO(ericv): Consider removing these condition variables now that the

  // Signalled when a new write is ready to be processed.
  absl::CondVar write_ready_;
  // Signalled when all readers have processed the latest write.
  absl::CondVar all_readers_done_;
};

}  // namespace s2testing

#endif  // S2_THREAD_TESTING_H_
