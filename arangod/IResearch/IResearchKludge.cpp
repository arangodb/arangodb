////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchKludge.h"

#include "utils/thread_utils.hpp"

#include "Basics/Common.h"

using namespace std::chrono_literals;

namespace {

constexpr auto RW_MUTEX_WAIT_TIMEOUT = 50ms;

}

namespace arangodb {
namespace iresearch {
namespace kludge {

const char TYPE_DELIMITER = '\0';
const char ANALYZER_DELIMITER = '\1';

irs::string_ref const NULL_SUFFIX("\0_n", 3);
irs::string_ref const BOOL_SUFFIX("\0_b", 3);
irs::string_ref const NUMERIC_SUFFIX("\0_d", 3);
irs::string_ref const STRING_SUFFIX("\0_s", 3);

void mangleType(std::string& name) { name += TYPE_DELIMITER; }

void mangleAnalyzer(std::string& name) { name += ANALYZER_DELIMITER; }

void mangleNull(std::string& name) {
  name.append(NULL_SUFFIX.c_str(), NULL_SUFFIX.size());
}

void mangleBool(std::string& name) {
  name.append(BOOL_SUFFIX.c_str(), BOOL_SUFFIX.size());
}

void mangleNumeric(std::string& name) {
  name.append(NUMERIC_SUFFIX.c_str(), NUMERIC_SUFFIX.size());
}

void mangleString(std::string& name) {
  name.append(STRING_SUFFIX.c_str(), STRING_SUFFIX.size());
}

void mangleField(
    std::string& name,
    bool isSearchFilter,
    iresearch::FieldMeta::Analyzer const& analyzer) {
  if (isSearchFilter || analyzer._pool->requireMangled()) {
    name += ANALYZER_DELIMITER;
    name += analyzer._shortName;
  } else {
    mangleString(name);
  }
}

read_write_mutex::read_write_mutex() noexcept
  : concurrent_count_(0),
    exclusive_count_(0),
    exclusive_owner_(std::thread::id()),
    exclusive_owner_recursion_count_(0) {
}

read_write_mutex::~read_write_mutex() noexcept {
#ifdef IRESEARCH_DEBUG
  // ensure mutex is not locked before destroying it
  auto lock = irs::make_unique_lock(mutex_, std::try_to_lock);
  assert(lock && !concurrent_count_.load() && !exclusive_count_);
#endif
}

void read_write_mutex::lock_read() {
  // if have write lock
  if (owns_write()) {
    ++exclusive_owner_recursion_count_; // write recursive lock

    return;
  }

  auto lock = irs::make_unique_lock(mutex_);

  // yield if there is already a writer waiting
  // wait for notification (possibly with writers waiting) or no more writers waiting
  while (exclusive_count_ && std::cv_status::timeout == reader_cond_.wait_for(lock, RW_MUTEX_WAIT_TIMEOUT)) {}

  ++concurrent_count_;
}

void read_write_mutex::lock_write() {
  // if have write lock
  if (owns_write()) {
    ++exclusive_owner_recursion_count_; // write recursive lock

    return;
  }

  auto lock = irs::make_unique_lock(mutex_);

  ++exclusive_count_; // mark mutex with writer-waiting state

  // wait until lock is held exclusively by the current thread
  while (concurrent_count_) {
    try {
      writer_cond_.wait_for(lock, RW_MUTEX_WAIT_TIMEOUT);
    } catch (...) {
      // 'wait_for' may throw according to specification
    }
  }

  --exclusive_count_;
  exclusive_owner_.store(std::this_thread::get_id());
  lock.release(); // disassociate the associated mutex without unlocking it
}

bool read_write_mutex::owns_write() noexcept {
  return exclusive_owner_.load() == std::this_thread::get_id();
}

bool read_write_mutex::try_lock_read() {
  // if have write lock
  if (owns_write()) {
    ++exclusive_owner_recursion_count_; // write recursive lock

    return true;
  }

  auto lock = irs::make_unique_lock(mutex_, std::try_to_lock);

  if (!lock || exclusive_count_) {
    return false;
  }

  ++concurrent_count_;

  return true;
}

bool read_write_mutex::try_lock_write() {
  // if have write lock
  if (owns_write()) {
    ++exclusive_owner_recursion_count_; // write recursive lock

    return true;
  }

  auto lock = irs::make_unique_lock(mutex_, std::try_to_lock);

  if (!lock || concurrent_count_) {
    return false;
  }

  exclusive_owner_.store(std::this_thread::get_id());
  lock.release(); // disassociate the associated mutex without unlocking it

  return true;
}

void read_write_mutex::unlock(bool exclusive_only /*= false*/) {
  // if have write lock
  if (owns_write()) {
    if (exclusive_owner_recursion_count_) {
      if (!exclusive_only) { // a recursively locked mutex is always top-level write locked
        --exclusive_owner_recursion_count_; // write recursion unlock one level
      }

      return;
    }

    auto lock = irs::make_unique_lock(mutex_, std::adopt_lock);

    if (exclusive_only) {
      ++concurrent_count_; // acquire the read-lock
    }

    exclusive_owner_.store(std::thread::id());
    reader_cond_.notify_all(); // wake all reader and writers
    writer_cond_.notify_all(); // wake all reader and writers

    return;
  }

  if (exclusive_only) {
    return; // NOOP for readers
  }

  // ...........................................................................
  // after here assume have read lock
  // ...........................................................................

  #ifdef IRESEARCH_DEBUG
    auto count = --concurrent_count_;
    assert(count != size_t(-1)); // ensure decrement was for a positive number (i.e. not --0)
    UNUSED(count);
  #else
    --concurrent_count_;
  #endif // IRESEARCH_DEBUG

  // FIXME: this should be changed to SCOPED_LOCK_NAMED, as right now it is not
  // guaranteed that we can succesfully acquire the mutex here. and if we don't,
  // there is no guarantee that the notify_all will wake up queued waiter.

  // try to acquire mutex for use with cond
  auto lock = irs::make_unique_lock(mutex_, std::try_to_lock);

  // wake only writers since this is a reader
  // wake even without lock since writer may be waiting in lock_write() on cond
  // the latter might also indicate a bug if deadlock occurs with SCOPED_LOCK()
  writer_cond_.notify_all();
}

}  // namespace kludge
}  // namespace iresearch
}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
