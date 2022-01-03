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

#pragma once

////////////////////////////////////////////////////////////////////////////////
/// @brief common place for all kludges and temporary workarounds required for
///        integration if the IResearch library with ArangoDB
///        NOTE1: all functionality in this file is not nesesarily optimal
///        NOTE2: all functionality in this file is to be considered deprecated
////////////////////////////////////////////////////////////////////////////////

#include "IResearchLinkMeta.h"

namespace arangodb {
namespace iresearch {
namespace kludge {

void mangleType(std::string& name);
void mangleAnalyzer(std::string& name);

void mangleNull(std::string& name);
void mangleBool(std::string& name);
void mangleNumeric(std::string& name);

void mangleField(std::string& name,
                 iresearch::FieldMeta::Analyzer const& analyzer);

//////////////////////////////////////////////////////////////////////////////
/// @brief a read-write mutex implementation
///        supports recursive read-lock acquisition
///        supports recursive write-lock acquisition
///        supports downgrading write-lock to a read lock
///        does not support upgrading a read-lock to a write-lock
///        write-locks are given acquisition preference over read-locks
/// @note the following will cause a deadlock with the current implementation:
///       read-lock(threadA) -> write-lock(threadB) -> read-lock(threadA)
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API read_write_mutex final {
 public:
  // for use with std::lock_guard/std::unique_lock for read operations
  class read_mutex {
   public:
    explicit read_mutex(read_write_mutex& mutex) noexcept : mutex_(mutex) {}
    read_mutex& operator=(read_mutex&) = delete;  // because of reference
    void lock() { mutex_.lock_read(); }
    bool try_lock() { return mutex_.try_lock_read(); }
    void unlock() { mutex_.unlock(); }

   private:
    read_write_mutex& mutex_;
  };  // read_mutex

  // for use with std::lock_guard/std::unique_lock for write operations
  class write_mutex {
   public:
    explicit write_mutex(read_write_mutex& mutex) noexcept : mutex_(mutex) {}
    write_mutex& operator=(write_mutex&) = delete;  // because of reference
    void lock() { mutex_.lock_write(); }
    bool owns_write() { return mutex_.owns_write(); }
    bool try_lock() { return mutex_.try_lock_write(); }
    void unlock(bool exclusive_only = false) { mutex_.unlock(exclusive_only); }

   private:
    read_write_mutex& mutex_;
  };  // write_mutex

  read_write_mutex() noexcept;
  ~read_write_mutex() noexcept;

  void lock_read();
  void lock_write();
  bool owns_write() noexcept;
  bool try_lock_read();
  bool try_lock_write();

  // The mutex must be locked by the current thread of execution, otherwise, the
  // behavior is undefined.
  // @param exclusive_only if true then only downgrade a lock to a read-lock
  void unlock(bool exclusive_only = false);

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  std::atomic<size_t> concurrent_count_;
  size_t exclusive_count_;
  std::atomic<std::thread::id> exclusive_owner_;
  size_t exclusive_owner_recursion_count_;
  std::mutex mutex_;
  std::condition_variable reader_cond_;
  std::condition_variable writer_cond_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
};  // read_write_mutex

}  // namespace kludge
}  // namespace iresearch
}  // namespace arangodb
