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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_FILE_UTILS_H
#define IRESEARCH_FILE_UTILS_H

#include <memory>
#include <cstdio>
#include <functional>
#include <fcntl.h> // open/_wopen

#ifdef _WIN32  
  #include <tchar.h>
  #include <io.h> // _close
  #define file_path_t wchar_t*
  #define file_stat _wstat
  #define file_fstat _fstat
  #define file_stat_t struct _stat
  #define file_no _fileno
  #define mode_t unsigned short
  #define file_open(name, mode) iresearch::file_utils::open(name, _T(mode))
  #define posix_create _wcreat
  #define posix_open _wopen
  #define posix_close _close

  #define IR_FADVICE_NORMAL 0
  #define IR_FADVICE_SEQUENTIAL 1
  #define IR_FADVICE_RANDOM 2
  #define IR_FADVICE_DONTNEED 4
  #define IR_FADVICE_NOREUSE 5
#else
  #include <unistd.h> // close
  #define file_path_t char*
  #define file_stat stat
  #define file_fstat fstat
  #define file_stat_t struct stat    
  #define file_no fileno
  #define file_open(name, mode) iresearch::file_utils::open(name, mode)
  #define posix_create creat
  #define posix_open open
  #define posix_close close

  #define IR_FADVICE_NORMAL POSIX_FADV_NORMAL
  #define IR_FADVICE_SEQUENTIAL POSIX_FADV_SEQUENTIAL
  #define IR_FADVICE_RANDOM POSIX_FADV_RANDOM
  #define IR_FADVICE_DONTNEED POSIX_FADV_DONTNEED
  #define IR_FADVICE_NOREUSE POSIX_FADV_NOREUSE
#endif

#include "shared.hpp"

NS_ROOT
NS_BEGIN(file_utils)

// -----------------------------------------------------------------------------
// --SECTION--                                                         lock file
// -----------------------------------------------------------------------------

struct lock_file_deleter {
  void operator()(void* handle) const;
}; // lock_file_deleter

typedef std::unique_ptr<void, lock_file_deleter> lock_handle_t;

lock_handle_t create_lock_file(const file_path_t file);
bool verify_lock_file(const file_path_t file);

// -----------------------------------------------------------------------------
// --SECTION--                                                             stats
// -----------------------------------------------------------------------------

ptrdiff_t file_size(const file_path_t file) NOEXCEPT;
ptrdiff_t file_size(int fd) NOEXCEPT;
ptrdiff_t block_size(int fd) NOEXCEPT;

// -----------------------------------------------------------------------------
// --SECTION--                                                         open file
// -----------------------------------------------------------------------------

struct file_deleter {
  void operator()(FILE* f) const NOEXCEPT {
    if (f) ::fclose(f);
  }
}; // file_deleter

typedef std::unique_ptr<FILE, file_deleter> handle_t;

handle_t open(const file_path_t path, const file_path_t mode) NOEXCEPT;
handle_t open(FILE* file, const file_path_t mode) NOEXCEPT;

// -----------------------------------------------------------------------------
// --SECTION--                                                   directory utils
// -----------------------------------------------------------------------------

bool is_directory(const file_path_t name) NOEXCEPT;

bool visit_directory(
  const file_path_t name,
  const std::function<bool(const file_path_t name)>& visitor,
  bool include_dot_dir = true
);

// -----------------------------------------------------------------------------
// --SECTION--                                                              misc
// -----------------------------------------------------------------------------

bool file_sync(const file_path_t name) NOEXCEPT;
bool file_sync(int fd) NOEXCEPT;

NS_END
NS_END

#endif