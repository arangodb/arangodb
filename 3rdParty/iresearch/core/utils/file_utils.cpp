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

#include "shared.hpp"
#include "file_utils.hpp"
#include "process_utils.hpp"
#include "network_utils.hpp"

#include "string.hpp"
#include "error/error.hpp"
#include "utils/log.hpp"
#include "utils/memory.hpp"

#if defined(__APPLE__)
  #include <sys/param.h> // for MAXPATHLEN
#endif

#include <sys/stat.h>
#include <sys/types.h>

#include <boost/locale/encoding.hpp>

#ifdef _WIN32

#include <Windows.h>
#include <Shlwapi.h>
#include <io.h> // for _get_osfhandle
#pragma comment(lib, "Shlwapi.lib")

#else

#include <sys/file.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#endif // _WIN32

NS_LOCAL

#ifdef _WIN32

  // workaround for path MAX_PATH
  const std::basic_string<wchar_t> path_prefix(L"\\\\?\\");

#endif

#ifdef _WIN32
  const std::basic_string<wchar_t> path_separator(L"\\");
#else
  const std::basic_string<char> path_separator("/");
#endif

inline int path_stats(file_stat_t& info, const file_path_t path) {
  // MSVC2013 _wstat64(...) reports ENOENT for '\' terminated paths
  // MSVC2015/MSVC2017 treat '\' terminated paths properly
  #if defined(_MSC_VER) && _MSC_VER == 1800
    auto parts = irs::file_utils::path_parts(path);

    return file_stat(
      parts.basename.empty() ? std::wstring(parts.dirname).c_str() : path,
      &info
    );
  #else
    return file_stat(path, &info);
  #endif
}

NS_END

NS_ROOT
NS_BEGIN(file_utils)

#ifdef _WIN32

void lock_file_deleter::operator()(void* handle) const {
  if (handle) {
    ::CloseHandle(reinterpret_cast<HANDLE>(handle));
  }
}

bool write(HANDLE fd, const char* buf, size_t size) {
  DWORD written;
  auto res = ::WriteFile(fd, buf, DWORD(size), &written, NULL);
  return res && size == written;
}

bool exists(const file_path_t file) {
  return TRUE == ::PathFileExists(file);
}

bool verify_lock_file(const file_path_t file) {
  if (!exists(file)) {
    return false; // not locked
  }

  HANDLE handle = ::CreateFileW(
    file,
    GENERIC_WRITE | GENERIC_READ, // write access
    0, // prevent access from other processes
    NULL, // default security attributes
    OPEN_EXISTING, // open existing file
    FILE_ATTRIBUTE_NORMAL, // use normal file attributes 
    NULL);

  if (INVALID_HANDLE_VALUE == handle) {
    if (ERROR_SHARING_VIOLATION == GetLastError()) {
      return true; // locked
    }
    return false; // not locked
  }

  char buf[256]{};
  // read file content
  {
    DWORD size;
    const BOOL res = ::ReadFile(handle, buf, sizeof buf, &size, NULL);
    ::CloseHandle(handle);

    if (FALSE == res || 0 == size || sizeof buf == size) {
      // invalid file format or string too long
      return false; // not locked
    }
  }

  // check hostname
  const size_t len = strlen(buf); // hostname length 
  if (!is_same_hostname(buf, len)) {
    auto path = boost::locale::conv::utf_to_utf<char>(file);
    IR_FRMT_INFO("Index locked by another host, hostname: '%s', file: '%s'", buf, path.c_str());
    return true; // locked
  }

  // check pid
  const char* pid = buf + len + 1;
  if (is_valid_pid(pid)) {
    auto path = boost::locale::conv::utf_to_utf<char>(file);
    IR_FRMT_INFO("Index locked by another process, PID: '%s', file: '%s'", pid, path.c_str());
    return true; // locked
  }

  return false; // not locked
}

lock_handle_t create_lock_file(const file_path_t file) {
  HANDLE fd = ::CreateFileW(
    file,
    GENERIC_WRITE, // write access
    0, // prevent access from other processes
    NULL, // default security attributes
    CREATE_ALWAYS, // always create file 
    FILE_FLAG_DELETE_ON_CLOSE, // allow OS to remove file when process finished
    NULL);

  if (INVALID_HANDLE_VALUE == fd) {
    auto path = boost::locale::conv::utf_to_utf<char>(file);
    IR_FRMT_ERROR("Unable to create lock file: '%s', error: %d", path.c_str(), GetLastError());
    return nullptr;
  }
  
  lock_handle_t handle(reinterpret_cast<void*>(fd));
  char buf[256];

  // write hostname to lock file
  if (const int err = get_host_name(buf, sizeof buf-1)) {
    IR_FRMT_ERROR("Unable to get hostname, error: %d", err);
    return nullptr;
  }

  if (!file_utils::write(fd, buf, strlen(buf)+1)) { // include terminate 0
    auto path = boost::locale::conv::utf_to_utf<char>(file);
    IR_FRMT_ERROR("Unable to write lock file: '%s', error: %d", path.c_str(), GetLastError());
    return nullptr;
  }

#ifdef _WIN32
  #pragma warning(disable : 4996)
#endif // _WIN32 

  // write PID to lock file 
  const size_t size = sprintf(buf, "%d", get_pid());
  if (!file_utils::write(fd, buf, size)) {
    auto path = boost::locale::conv::utf_to_utf<char>(file);
    IR_FRMT_ERROR("Unable to write lock file: '%s', error: %d", path.c_str(), GetLastError());
    return nullptr;
  }

#ifdef _WIN32
  #pragma warning(default: 4996)
#endif // _WIN32 

  // flush buffers
  if (::FlushFileBuffers(fd) <= 0) {
    auto path = boost::locale::conv::utf_to_utf<char>(file);
    IR_FRMT_ERROR("Unable to flush lock file: '%s', error: %d ", path.c_str(), GetLastError());
    return nullptr;
  }

  return handle;
}

bool file_sync(const file_path_t file) NOEXCEPT {
  HANDLE handle = ::CreateFileW(
    file, GENERIC_WRITE,
    FILE_SHARE_WRITE, NULL,
    OPEN_EXISTING,
    0, NULL
  );

  // try other sharing modes since MSFT Windows fails sometimes
  if (INVALID_HANDLE_VALUE == handle) {
      handle = ::CreateFileW(
      file, GENERIC_WRITE,
      FILE_SHARE_READ, NULL,
      OPEN_EXISTING,
      0, NULL
    );
  }

  // try other sharing modes since MSFT Windows fails sometimes
  if (INVALID_HANDLE_VALUE == handle) {
      handle = ::CreateFileW(
      file, GENERIC_WRITE,
      FILE_SHARE_DELETE, NULL,
      OPEN_EXISTING,
      0, NULL
    );
  }

  // try other sharing modes since MSFT Windows fails sometimes
  if (INVALID_HANDLE_VALUE == handle) {
      handle = ::CreateFileW(
      file, GENERIC_WRITE,
      0, NULL,
      OPEN_EXISTING,
      0, NULL
    );
  }

  if (INVALID_HANDLE_VALUE == handle) {
    return false;
  }

  const bool res = ::FlushFileBuffers(handle) > 0;
  ::CloseHandle(handle);
  return res;
}

bool file_sync(int fd) NOEXCEPT {
  // Attempt to convert file descriptor into an operating system file handle
  HANDLE handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd));

  // An invalid file system handle was returned.
  if (handle == INVALID_HANDLE_VALUE) {
    return false;
  }

  return !FlushFileBuffers(handle);
}

#else

void lock_file_deleter::operator()(void* handle) const {
  if (handle) {
    const int fd = static_cast<int>(reinterpret_cast<size_t>(handle));
    if (fd < 0) {
      return; // invalid desriptor
    }
    flock(fd, LOCK_UN); // unlock file
    close(fd); // close file
  }
}

bool exists(const file_path_t file) {
  return 0 == access(file, F_OK);
}

bool write(int fd, const char* buf, size_t size) {
  const ssize_t written = ::write(fd, buf, size);
  return written >= 0 && size_t(written) == size;
}

bool verify_lock_file(const file_path_t file) {
  if (!exists(file)) {
    return false; // not locked
  }

  const int fd = ::open(file, O_RDONLY);

  if (fd < 0) {
    IR_FRMT_ERROR("Unable to open lock file '%s' for verification, error: %d", file, errno);
    return false; // not locked
  }

  char buf[256];
  ssize_t size;
  // read file content
  {
    lock_handle_t handle(reinterpret_cast<void*>(fd));

    // try to apply advisory lock on lock file
    if (flock(fd, LOCK_EX | LOCK_NB)) {
      if (EWOULDBLOCK == errno) {
        IR_FRMT_ERROR("Lock file '%s' is already locked", file);
        return true; // locked
      } else {
        IR_FRMT_ERROR("Unable to apply lock on lock file: '%s', error: %d", file, errno);
        return false; // not locked
      }
    }

    size = read(fd, buf, sizeof buf);
  }

  if (size <= 0 || sizeof buf == size) {
    // invalid file format or string too long
    return false; // not locked
  }
  buf[size] = 0; // set termination 0

  // check hostname
  const size_t len = strlen(buf); // hostname length 
  if (!is_same_hostname(buf, len)) {
    IR_FRMT_INFO("Index locked by another host, hostname: '%s', file: '%s'", buf, file);
    return true; // locked
  }

  if (len >= size_t(size)) {
    // no PID in buf
    return false; // not locked
  }

  // check pid
  const char* pid = buf+len+1;
  if (is_valid_pid(pid)) {
    IR_FRMT_INFO("Index locked by another process, PID: '%s', file: '%s'", pid, file);
    return true; // locked
  }

  return false; // not locked
}

lock_handle_t create_lock_file(const file_path_t file) {
  const int fd = ::open(file, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);

  if (fd < 0) {
    IR_FRMT_ERROR("Unable to create lock file: '%s', error: %d", file, errno);
    return nullptr;
  }

  lock_handle_t handle(reinterpret_cast<void*>(fd));
  char buf[256];

  // write hostname to lock file
  if (const int err = get_host_name(buf, sizeof buf-1)) {
    IR_FRMT_ERROR("Unable to get hostname, error: %d", err);
    return nullptr;
  }

  if (!file_utils::write(fd, buf, strlen(buf)+1)) { // include terminated 0
    IR_FRMT_ERROR("Unable to write lock file: '%s', error: %d", file, errno);
    return nullptr;
  }

  // write PID to lock file 
  size_t size = sprintf(buf, "%d", get_pid());
  if (!file_utils::write(fd, buf, size)) {
    IR_FRMT_ERROR("Unable to write lock file: '%s', error: %d", file, errno);
    return nullptr;
  }

  // flush buffers
  if (fsync(fd)) {
    IR_FRMT_ERROR("Unable to flush lock file: '%s', error: %d", file, errno);
    return nullptr;
  }

  // try to apply advisory lock on lock file
  if (flock(fd, LOCK_EX)) {
    IR_FRMT_ERROR("Unable to apply lock on lock file: '%s', error: %d", file, errno);
    return nullptr;
  }

  return handle;
}

bool file_sync(const file_path_t file) NOEXCEPT {
  const int handle = ::open(file, O_WRONLY, S_IRWXU);
  if (handle < 0) {
    return false;
  }

  const bool res = (0 == fsync(handle));
  close(handle);
  return res;
}

bool file_sync(int fd) NOEXCEPT {
  return 0 == fsync(fd);
}

#endif // _WIN32

// -----------------------------------------------------------------------------
// --SECTION--                                                             stats
// -----------------------------------------------------------------------------

bool absolute(bool& result, const file_path_t path) NOEXCEPT {
  if (!path) {
    return false;
  }

  #ifdef _WIN32
    if (MAX_PATH > wcslen(path)) {
      result = !PathIsRelativeW(path);
    } else {
      // ensure that PathIsRelativeW(...) is given a value shorter than MAX_PATH
      // still ok since to determine if absolute only need the start of the path
      std::basic_string<wchar_t> buf(path, MAX_PATH - 1); // -1 for '\0'

      result = !PathIsRelativeW(buf.c_str());
    }
  #else
    result = path[0] == '/'; // a null terminated string is at least size 1
  #endif

  return true;
}

bool block_size(file_blksize_t& result, const file_path_t file) NOEXCEPT {
  assert(file != nullptr);
#ifdef _WIN32
  // TODO FIXME find a workaround
  UNUSED(file);
  result = 512;

  return true;
#else
  file_stat_t info;

  if (0 != path_stats(info, file)) {
    return false;
  }

  result = info.st_blksize;

  return true;
#endif // _WIN32
}

bool block_size(file_blksize_t& result, int fd) NOEXCEPT {
#ifdef _WIN32
  // TODO FIXME find a workaround
  UNUSED(fd);
  result = 512;

  return true;
#else
  file_stat_t info;

  if (0 != file_fstat(fd, &info)) {
    return false;
  }

  result = info.st_blksize;

  return true;
#endif // _WIN32
}

bool byte_size(uint64_t& result, const file_path_t file) NOEXCEPT {
  assert(file != nullptr);
  file_stat_t info;

  if (0 != path_stats(info, file)) {
    return false;
  }

  result = info.st_size;

  return true;
}

bool byte_size(uint64_t& result, int fd) NOEXCEPT {
  file_stat_t info;

  if (0 != file_fstat(fd, &info)) {
    return false;
  }

  result = info.st_size;

  return true;
}

bool exists(bool& result, const file_path_t file) NOEXCEPT {
  assert(file != nullptr);
  file_stat_t info;

  result = 0 == path_stats(info, file);

  if (!result && ENOENT != errno) {
    auto path = boost::locale::conv::utf_to_utf<char>(file);

    IR_FRMT_ERROR("Failed to get stat, error %d path: %s", errno, path.c_str());
  }

  return true;
}

bool exists_directory(bool& result, const file_path_t name) NOEXCEPT {
  assert(name != nullptr);
  file_stat_t info;

  result = 0 == path_stats(info, name);

  if (result) {
    #ifdef _WIN32
      result = (info.st_mode & _S_IFDIR) > 0;
    #else
      result = (info.st_mode & S_IFDIR) > 0;
    #endif
  } else if (ENOENT != errno) {
    auto path = boost::locale::conv::utf_to_utf<char>(name);

    IR_FRMT_ERROR("Failed to get stat, error %d path: %s", errno, path.c_str());
  }

  return true;
}

bool exists_file(bool& result, const file_path_t name) NOEXCEPT {
  assert(name != nullptr);
  file_stat_t info;

  result = 0 == path_stats(info, name);

  if (result) {
    #ifdef _WIN32
      result = (info.st_mode & _S_IFREG) > 0;
    #else
      result = (info.st_mode & S_IFREG) > 0;
    #endif
  } else if (ENOENT != errno) {
    auto path = boost::locale::conv::utf_to_utf<char>(name);

    IR_FRMT_ERROR("Failed to get stat, error %d path: %s", errno, path.c_str());
  }

  return true;
}

bool mtime(time_t& result, const file_path_t file) NOEXCEPT {
  assert(file != nullptr);
  file_stat_t info;

  if (0 != path_stats(info, file)) {
    return false;
  }

  result = info.st_mtime;

  return true;
}

bool mtime(time_t& result, int fd) NOEXCEPT {
  file_stat_t info;

  if (0 != file_fstat(fd, &info)) {
    return false;
  }

  result = info.st_mtime;

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                         open file
// -----------------------------------------------------------------------------

handle_t open(const file_path_t path, const file_path_t mode) NOEXCEPT {
  #ifdef _WIN32
    #pragma warning(disable: 4996) // '_wfopen': This function or variable may be unsafe.
    handle_t handle(::_wfopen(path ? path : IR_WSTR("NUL:"), mode));
    #pragma warning(default: 4996)
  #else
    handle_t handle(::fopen(path ? path : "/dev/null", mode));
  #endif

  if (!handle) {
    // even win32 uses 'errno' for error codes in calls to _wfopen(...)
    IR_FRMT_ERROR("Failed to open file, error: %d, path: " IR_FILEPATH_SPECIFIER, errno, path);
    IR_LOG_STACK_TRACE();
  }

  return handle;
}

handle_t open(FILE* file, const file_path_t mode) NOEXCEPT {
  #ifdef _WIN32
    // win32 approach is to get the original filename of the handle and open it again
    // due to a bug from the 1980's the file name is garanteed to not change while the file is open
    HANDLE handle = HANDLE(::_get_osfhandle(::_fileno(file)));

    if (INVALID_HANDLE_VALUE == HANDLE(handle)) {
      IR_FRMT_ERROR("Failed to get system handle from file handle, error %d", errno);
      return nullptr;
    }

    const auto size = MAX_PATH + 1; // +1 for \0
    TCHAR path[size];
    auto length = GetFinalPathNameByHandle(handle, path, size - 1, VOLUME_NAME_DOS); // -1 for \0

    if (!length) {
      IR_FRMT_ERROR("Failed to get filename from file handle, error %d", GetLastError());

      return nullptr;
    }

    if(length < size) {
      path[length] = '\0';

      return open(path, mode);
    }

    IR_FRMT_WARN(
      "Required file path buffer size of %d is greater than the expected size of %d, malloc necessary",
      length + 1, size // +1 for \0
    );

    auto buf_size = length + 1; // +1 for \0
    auto buf = irs::memory::make_unique<TCHAR[]>(buf_size);

    length = GetFinalPathNameByHandle(handle, buf.get(), buf_size - 1, VOLUME_NAME_DOS); // -1 for \0

    if(length && length < buf_size) {
      buf[length] = '\0';

      return open(buf.get(), mode);
    }

    IR_FRMT_ERROR(
      "Failed to get filename from file handle, inconsistent length detected, first %d then %d",
      buf_size, length + 1 // +1 for \0
    );

    return nullptr;
  #elif defined(__APPLE__)
    // MacOS approach is to open the original file via the file descriptor link under /dev/fd
    // the link is garanteed to point to the original inode even if the original file was removed
    // unfortunatly this results in the original file descriptor being dup()'ed
    // therefore try to get the original file name from the existing descriptor and reopen it
    // FIXME TODO assume that the file has not moved and was not deleted
    auto fd = ::fileno(file);
    char path[MAXPATHLEN + 1]; // F_GETPATH requires a buffer of size at least MAXPATHLEN, +1 for \0

    if (0 > fd || 0 > fcntl(fd, F_GETPATH, path)) {
      IR_FRMT_ERROR("Failed to get file path from file handle, error %d", errno);
      return nullptr;
    }

    return open(path, mode);
  #else
    // posix approach is to open the original file via the file descriptor link under /proc/self/fd
    // the link is garanteed to point to the original inode even if the original file was removed
    auto fd = ::fileno(file);
    char path[strlen("/proc/self/fd/") + sizeof(fd)*3 + 1]; // approximate maximum number of chars, +1 for \0

    if (0 > fd || 0 > sprintf(path, "/proc/self/fd/%d", fd)) {
      IR_FRMT_ERROR("Failed to get system handle from file handle, error %d", errno);
      return nullptr;
    }

    return open(path, mode);
  #endif
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        path utils
// -----------------------------------------------------------------------------

bool mkdir(const file_path_t path) NOEXCEPT {
  bool result;

  if (!exists_directory(result, path)) {
    return false; // failure checking directory existence
  }

  if (result) {
    return true; // already exists
  }

  if (!exists(result, path) || result) {
    return false; // failure checking existence or something else exists with the same name
  }

  auto parts = path_parts(path);

  if (!parts.dirname.empty()) {
    // need a null terminated string for use with ::mkdir()/::CreateDirectoryW()
    std::basic_string<std::remove_pointer<file_path_t>::type> parent(parts.dirname);

    if (!mkdir(parent.c_str())) {
      return false; // failed to create parent
    }
  }

  #ifdef _WIN32
    bool abs;

    if (!absolute(abs, path)) {
      return false;
    }

    // '\\?\' cannot be used with relative paths
    if (!abs) {
      return 0 != CreateDirectoryW(path, nullptr);
    }

    // workaround for path MAX_PATH
    auto dirname = path_prefix + path;

    // 'path_prefix' cannot be used with paths containing a mix of native and non-native path separators
    std::replace(
      &dirname[0], &dirname[0] + dirname.size(), L'/', file_path_delimiter
    );

    return 0 != ::CreateDirectoryW(dirname.c_str(), nullptr);
  #else
    return 0 == ::mkdir(path, S_IRWXU|S_IRWXG|S_IRWXO);
  #endif
}

bool move(const file_path_t src_path, const file_path_t dst_path) NOEXCEPT {
  // FIXME TODO ensure both functions lead to the same result in all cases @see utf8_path_tests::rename() tests
  #ifdef _WIN32
    return 0 != ::MoveFileExW(src_path, dst_path, MOVEFILE_REPLACE_EXISTING|MOVEFILE_COPY_ALLOWED);
  #else
    return 0 == ::rename(src_path, dst_path);
  #endif
}

path_parts_t path_parts(const file_path_t path) NOEXCEPT {
  if (!path) {
    return path_parts_t();
  }

  bool have_extension = false;
  path_parts_t::ref_t dirname = path_parts_t::ref_t::NIL;
  size_t stem_end = 0;

  for(size_t i = 0;; ++i) {
    switch (*(path + i)) {
    #ifdef _WIN32
     case L'\\': // fall through
     case L'/':
    #else
     case '/':
    #endif
      have_extension = false;
      dirname = path_parts_t::ref_t(path, i);
      break;
    #ifdef _WIN32
     case L'.':
    #else
     case '.':
    #endif
      have_extension = true;
      stem_end = i;
      break;
    #ifdef _WIN32
     case L'\0': {
    #else
     case '\0': {
    #endif
      path_parts_t result;

      if (have_extension) {
        result.extension =
          path_parts_t::ref_t(path + stem_end + 1, i - stem_end - 1); // +1/-1 for delimiter
      } else {
        stem_end = i;
      }

      if (dirname.null()) {
        result.basename = path_parts_t::ref_t(path, i);
        result.stem = path_parts_t::ref_t(path, stem_end);
      } else {
        auto stem_start = dirname.size() + 1; // +1 for delimiter

        result.basename =
          path_parts_t::ref_t(path + stem_start, i - stem_start);
        result.stem =
          path_parts_t::ref_t(path + stem_start, stem_end - stem_start);
      }

      result.dirname = std::move(dirname);

      return result;
     }
     default: {} // NOOP
    }
  }
}

bool read_cwd(
    std::basic_string<std::remove_pointer<file_path_t>::type>& result
) NOEXCEPT {
  try {
    #ifdef _WIN32
      auto size = GetCurrentDirectory(0, nullptr);

      if (!size) {
        IR_FRMT_ERROR("Failed to get length of the current working directory, error %d", GetLastError());

        return false;
      }

      if (size >= result.size()) {
        result.resize(size + 1); // allocate space for cwd, +1 for '\0'
      }

      size = GetCurrentDirectory(size, &result[0]);

      // if error or more space required than available
      if (!size || size >= result.size()) {
        IR_FRMT_ERROR("Failed to get the current working directory, error %d", GetLastError());

        return false;
      }

      if (result.size() >= path_prefix.size() &&
          !result.compare(0, path_prefix.size(), path_prefix)) {
        result = result.substr(path_prefix.size());
        size -= uint32_t(path_prefix.size()); // path_prefix size <= DWORD max
      }

      result.resize(size); // truncate buffer to size of cwd
    #else
      result.resize(result.capacity()); // use up the entire buffer (noexcept)

      if (result.empty()) {
        // workaround for implementations of std::basic_string without a buffer
        char buf[PATH_MAX];

        if (nullptr != getcwd(buf, PATH_MAX)) {
          result.assign(buf);

          return true;
        }
      } else if (nullptr != getcwd(&result[0], result.size())) {
        result.resize(std::strlen(&result[0])); // truncate buffer to size of cwd

        return true;
      }

      if (ERANGE != errno) {
        IR_FRMT_ERROR("Failed to get the current working directory, error %d", errno);

        return false;
      }

      struct deleter_t {
        void operator()(char* ptr) const { free(ptr); }
      };
      std::unique_ptr<char, deleter_t> pcwd(getcwd(nullptr, 0));

      if (!pcwd) {
        IR_FRMT_ERROR("Failed to allocate the current working directory, error %d", errno);

        return false;
      }

      result.assign(pcwd.get());
    #endif

    return true;
  } catch (std::bad_alloc& e) {
    IR_FRMT_ERROR("Memory allocation failure while getting the current working directory: %s", e.what());
  } catch (std::exception& e) {
    IR_FRMT_ERROR("Caught exception while getting the current working directory: %s", e.what());
  }

  return false;
}

bool remove(const file_path_t path) NOEXCEPT {
  try {
    // a reusable buffer for a full path used during recursive removal
    std::basic_string<std::remove_pointer<file_path_t>::type> buf;

    // must remove each directory entry recursively (ignore result, check final ::remove() instead)
    visit_directory(
      path,
      [path, &buf](const file_path_t name)->bool {
        buf.assign(path);
        buf += path_separator;
        buf += name;
        remove(buf.c_str());

        return true;
      },
      false
    );
  } catch(...) {
    return false; // possibly a malloc error
  }

  #ifdef _WIN32
    bool abs;

    if (!absolute(abs, path)) {
      return false;
    }

    // '\\?\' cannot be used with relative paths
    if (!abs) {
      bool result;
      auto res = exists_directory(result, path) && result
               ? ::RemoveDirectoryW(path)
               : ::DeleteFileW(path)
               ;

      if (!res) { // 0 == error
        auto utf8path = boost::locale::conv::utf_to_utf<char>(path);
        IR_FRMT_ERROR("Failed to remove path: '%s', error %d", utf8path.c_str(), GetLastError());

        return false;
      }

      return true;
    }

    // workaround for path MAX_PATH
    auto fullpath = path_prefix + path;

    // 'path_prefix' cannot be used with paths containing a mix of native and non-native path separators
    std::replace(
      &fullpath[0], &fullpath[0] + fullpath.size(), L'/', file_path_delimiter
    );

    bool result;
    auto res = exists_directory(result, path) && result
             ? ::RemoveDirectoryW(fullpath.c_str())
             : ::DeleteFileW(fullpath.c_str())
             ;

    if (!res) { // 0 == error
      auto utf8path = boost::locale::conv::utf_to_utf<char>(path);
      IR_FRMT_ERROR("Failed to remove path: '%s', error %d", utf8path.c_str(), GetLastError());

      return false;
    }
  #else
    auto res = ::remove(path);

    if (res) { // non-0 == error
      IR_FRMT_ERROR("Failed to remove path: '%s', error %d", path, errno);

      return false;
    }
  #endif

  return true;
}

bool set_cwd(const file_path_t path) NOEXCEPT {
  #ifdef _WIN32
    bool abs;

    if (!absolute(abs, path)) {
      return false;
    }

    // '\\?\' cannot be used with relative paths
    if (!abs) {
      return 0 != SetCurrentDirectory(path);
    }

    // workaround for path MAX_PATH
    auto fullpath = path_prefix + path;

    // 'path_prefix' cannot be used with paths containing a mix of native and non-native path separators
    std::replace(
      &fullpath[0], &fullpath[0] + fullpath.size(), L'/', file_path_delimiter
    );

    return 0 != SetCurrentDirectory(fullpath.c_str());
  #else
    return 0 == chdir(path);
  #endif
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   directory utils
// -----------------------------------------------------------------------------

bool visit_directory(
  const file_path_t name,
  const std::function<bool(const file_path_t name)>& visitor,
  bool include_dot_dir /*= true*/
) {
  #ifdef _WIN32
    std::wstring dirname(name);

    if (dirname.empty()) {
      return false; // match Posix behaviour for empty-string directory
    }

    dirname += L"\\*"; // FindFirstFile requires name to end with '\*'

    WIN32_FIND_DATA direntry;
    HANDLE dir = ::FindFirstFile(dirname.c_str(), &direntry);

    if(dir == INVALID_HANDLE_VALUE) {
      return false;
    }

    bool terminate = false;

    do {
      if (include_dot_dir || !(
          (direntry.cFileName[0] == L'.' && direntry.cFileName[1] == L'\0') ||
          (direntry.cFileName[0] == L'.' && direntry.cFileName[1] == L'.' && direntry.cFileName[2] == L'\0'))) {
        terminate = !visitor(direntry.cFileName);
      }
    } while(!terminate && ::FindNextFile(dir, &direntry));

    ::FindClose(dir);

    return !terminate;
  #else
    DIR* dir = opendir(name);

    if(!dir) {
      return false;
    }

    struct dirent* direntry;
    bool terminate = false;

    while (!terminate && (direntry = readdir(dir))) {
      if (include_dot_dir || !(
          (direntry->d_name[0] == '.' && direntry->d_name[1] == '\0') ||
          (direntry->d_name[0] == '.' && direntry->d_name[1] == '.' && direntry->d_name[2] == '\0'))) {
        terminate = !visitor(direntry->d_name);
      }
    }

    closedir(dir);

    return !terminate;
  #endif
}

NS_END // file_utils
NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------