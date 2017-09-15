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
#include "file_utils.hpp"
#include "process_utils.hpp"
#include "network_utils.hpp"

#include "string.hpp"
#include "error/error.hpp"
#include "utils/log.hpp"

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
    IR_FRMT_INFO("Index locked by another host, hostname: %s", buf);
    return true; // locked
  }

  // check pid
  const char* pid = buf + len + 1;
  if (is_valid_pid(pid)) {
    IR_FRMT_INFO("Index locked by another process, PID: %s", pid);
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
    IR_FRMT_INFO("Index locked by another host, hostname: %s", buf);
    return true; // locked
  }

  if (len >= size_t(size)) {
    // no PID in buf
    return false; // not locked
  }

  // check pid
  const char* pid = buf+len+1;
  if (is_valid_pid(pid)) {
    IR_FRMT_INFO("Index locked by another process, PID: %s", pid);
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

#endif // _WIN32

ptrdiff_t file_size(const file_path_t file) NOEXCEPT {
  assert(file != nullptr);
  file_stat_t info;
  return 0 == file_stat(file, &info) ? info.st_size : -1;
}

ptrdiff_t file_size(int fd) NOEXCEPT {
  file_stat_t info;
  return 0 == file_fstat(fd, &info) ? info.st_size : -1;
}

ptrdiff_t block_size(int fd) NOEXCEPT {
#ifdef _WIN32
  // fixme
  return 512;
#else
  file_stat_t info;
  return 0 == file_fstat(fd, &info) ? info.st_blksize : -1;
#endif // _WIN32
}

bool is_directory(const file_path_t name) NOEXCEPT {
  file_stat_t info;

  if (file_stat(name, &info) != 0) {
    auto path = boost::locale::conv::utf_to_utf<char>(name);

    IR_FRMT_ERROR("Failed to get stat, error %d path: %s", errno, path.c_str());

    return false; 
  }

#ifdef _WIN32
  return (info.st_mode & _S_IFDIR) > 0;
#else
  return (info.st_mode & S_IFDIR) > 0;
#endif
}

handle_t open(const file_path_t path, const file_path_t mode) NOEXCEPT {
  #ifdef _WIN32
    #pragma warning(disable: 4996) // '_wfopen': This function or variable may be unsafe.
    return handle_t(::_wfopen(path ? path : _T("NUL:"), mode));
    #pragma warning(default: 4996)
  #else
    return handle_t(::fopen(path ? path : "/dev/null", mode));
  #endif
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

    const auto size = sizeof(FILE_NAME_INFO) + sizeof(WCHAR)*MAX_PATH + 1; // +1 for \0
    char info_buf[size];
    PFILE_NAME_INFO info = reinterpret_cast<PFILE_NAME_INFO>(info_buf);

    if (!GetFileInformationByHandleEx(handle, FileNameInfo, info, size)) {
      IR_FRMT_ERROR("Failed to get filename from file handle, error %d", GetLastError());
      return nullptr;
    }

    auto path = info->FileName;
    auto length = info->FileNameLength >> (sizeof(path[0]) - 1); // FileNameLength is in bytes

    path[length] = '\0';

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

bool visit_directory(
  const file_path_t name,
  const std::function<bool(const file_path_t name)>& visitor,
  bool include_dot_dir /*= true*/
) {
  #ifdef _WIN32
    std::wstring dirname(name);

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
    } while(::FindNextFile(dir, &direntry)); 

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

NS_END
NS_END
