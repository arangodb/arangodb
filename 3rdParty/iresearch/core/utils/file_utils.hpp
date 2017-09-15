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

#ifndef IRESEARCH_FILE_UTILS_H
#define IRESEARCH_FILE_UTILS_H

#include <memory>
#include <cstdio>
#include <functional>

#ifdef _WIN32
  #include <tchar.h>
  #define file_path_t wchar_t*
  #define file_stat _wstat
  #define file_fstat _fstat
  #define file_stat_t struct _stat
  #define file_no _fileno
  #define mode_t unsigned short
  #define file_open(name, mode) iresearch::file_utils::open(name, _T(mode))
#else
  #define file_path_t char*
  #define file_stat stat
  #define file_fstat fstat
  #define file_stat_t struct stat    
  #define file_no fileno
  #define file_open(name, mode) iresearch::file_utils::open(name, mode)
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

NS_END
NS_END

#endif
