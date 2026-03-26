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

#pragma once

#include <ctime>
#include <functional>
#include <span>

#include "store/data_input.hpp"
#include "store/data_output.hpp"
#include "utils/memory.hpp"
#include "utils/noncopyable.hpp"

namespace irs {

class directory_attributes;

// An interface for abstract resource locking
struct index_lock : private util::noncopyable {
  DECLARE_IO_PTR(index_lock, unlock);
  DEFINE_FACTORY_INLINE(index_lock);

  static constexpr size_t kLockWaitForever = std::numeric_limits<size_t>::max();

  virtual ~index_lock() = default;

  // Checks whether the guarded resource is locked
  // result - true if resource is already locked
  // Returns call success
  virtual bool is_locked(bool& result) const noexcept = 0;

  // Locks the guarded resource
  // Returns true on success
  virtual bool lock() = 0;

  // Tries to lock the guarded resource within the specified amount of time
  // wait_timeout - timeout between different locking attempts
  // Returns true on success
  bool try_lock(size_t wait_timeout = 1000) noexcept;

  // Nnlocks the guarded resource
  // Returns call success
  virtual bool unlock() noexcept = 0;
};

// Defines access patterns for data in a directory
enum class IOAdvice : uint32_t {
  // Indicates that caller has no advice to give about its access
  // pattern for the data
  NORMAL = 0,

  // Indicates that caller expects to access data sequentially
  SEQUENTIAL = 1,

  // Indicates that caller expects to access data in random order
  RANDOM = 2,

  // Indicates that caller expects that data will not be accessed
  // in the near future
  READONCE = 4,

  // Convenience value for READONCE | SEQUENTIAL
  // explicitly required for MSVC2013
  READONCE_SEQUENTIAL = 5,

  // Convenience value for READONCE | RANDOM
  // explicitly required for MSVC2013
  READONCE_RANDOM = 6,

  // File is opened for non-buffered reads
  DIRECT_READ = 8
};

ENABLE_BITMASK_ENUM(IOAdvice);  // enable bitmap operations on the enum

// Represents a flat directory of write once/read many files
struct directory : private util::noncopyable {
 public:
  using visitor_f = std::function<bool(std::string_view)>;
  using ptr = std::unique_ptr<directory>;

  virtual ~directory() = default;

  // Opens output stream associated with the file
  // name - name of the file to open
  // Returns output stream associated with the file with the specified name
  virtual index_output::ptr create(std::string_view name) noexcept = 0;

  // Check whether the file specified by the given name exists
  // result - true if file already exists
  // name - name of the file
  // Returns call success
  virtual bool exists(bool& result, std::string_view name) const noexcept = 0;

  // Returns the length of the file specified by the given name
  // result - length of the file specified by the name
  // name -  name of the file
  // Returns call success
  virtual bool length(uint64_t& result,
                      std::string_view name) const noexcept = 0;

  // Creates an index level lock with the specified name
  // name - name of the lock
  // Returns lock hande
  virtual index_lock::ptr make_lock(std::string_view name) noexcept = 0;

  // Returns modification time of the file specified by the given name
  // result - file last modified time
  // name - name of the file
  // Returns call success
  virtual bool mtime(std::time_t& result,
                     std::string_view name) const noexcept = 0;

  // Opens input stream associated with the existing file
  // name - name of the file to open
  // Returns input stream associated with the file with the specified name
  virtual index_input::ptr open(std::string_view name,
                                IOAdvice advice) const noexcept = 0;

  // Removes the file specified by the given name from directory
  // name - name of the file
  // Rreturns true if file has been removed
  virtual bool remove(std::string_view name) noexcept = 0;

  // Renames the 'src' file to 'dst'
  // src - initial name of the file
  // dst - final name of the file
  // Returns true if file has been renamed
  virtual bool rename(std::string_view src, std::string_view dst) noexcept = 0;

  // Ensures that all modification have been sucessfully persisted
  // files - files to sync
  // Returns call success
  virtual bool sync(std::span<const std::string_view> files) noexcept = 0;

  // Applies the specified 'visitor' to every filename in a directory
  // visitor - visitor to be applied
  // Returns 'false' if visitor has returned 'false', 'true' otherwise
  virtual bool visit(const visitor_f& visitor) const = 0;

  // Returns directory attributes
  virtual directory_attributes& attributes() noexcept = 0;

  // Returns directory attributes
  const directory_attributes& attributes() const noexcept {
    return const_cast<directory*>(this)->attributes();
  }
};

}  // namespace irs
