////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "files.h"

#ifdef _WIN32
#include <tchar.h>
#include <Shlwapi.h>
#include <chrono>
#include <thread>
#include <unicode/locid.h>
#endif

#include <algorithm>

#include "Basics/directories.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/ReadLocker.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/WriteLocker.h"
#include "Basics/conversions.h"
#include "Basics/hashes.h"
#include "Basics/tri-strings.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"

using namespace arangodb::basics;
using namespace arangodb;

namespace {

/// @brief names of blocking files
#ifdef TRI_HAVE_WIN32_FILE_LOCKING
std::vector<std::pair<std::string, HANDLE>> OpenedFiles;
#else
std::vector<std::pair<std::string, int>> OpenedFiles;
#endif

/// @brief lock for protected access to vector OpenedFiles
static basics::ReadWriteLock OpenedFilesLock;

/// @brief struct to remove all opened lockfiles on shutdown
struct LockfileRemover {
  LockfileRemover() {}

  ~LockfileRemover() {
    WRITE_LOCKER(locker, OpenedFilesLock);

    for (auto const& it : OpenedFiles) {
#ifdef TRI_HAVE_WIN32_FILE_LOCKING
      HANDLE fd = it.second;
      CloseHandle(fd);
#else
      int fd = it.second;
      TRI_CLOSE(fd);
#endif

      TRI_UnlinkFile(it.first.c_str());
    }

    OpenedFiles.clear();
  }
};

/// @brief this instance will remove all lockfiles in its dtor
static LockfileRemover remover;
}

/// @brief read buffer size (used for bulk file reading)
#define READBUFFER_SIZE 8192

/// @brief a static buffer of zeros, used to initialize files
static char NullBuffer[4096];

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the character is a directory separator
////////////////////////////////////////////////////////////////////////////////

static constexpr bool IsDirSeparatorChar(char c) {
  // the check for c != TRI_DIR_SEPARATOR_CHAR is required
  // for g++6. otherwise it will warn about equal expressions
  // in the two branches
  return (c == TRI_DIR_SEPARATOR_CHAR ||
          (TRI_DIR_SEPARATOR_CHAR != '/' && c == '/'));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalizes path
///
/// @note path will be modified in-place
////////////////////////////////////////////////////////////////////////////////

static void NormalizePath(std::string& path) {
  for (auto& it : path) {
    if (IsDirSeparatorChar(it)) {
      it = TRI_DIR_SEPARATOR_CHAR;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lists the directory tree
////////////////////////////////////////////////////////////////////////////////

static void ListTreeRecursively(char const* full, char const* path,
                                std::vector<std::string>& result) {
  std::vector<std::string> dirs = TRI_FilesDirectory(full);

  for (size_t j = 0; j < 2; ++j) {
    for (auto const& filename : dirs) {
      std::string const newFull =
          arangodb::basics::FileUtils::buildFilename(full, filename);
      std::string newPath;

      if (*path) {
        newPath = arangodb::basics::FileUtils::buildFilename(path, filename);
      } else {
        newPath = filename;
      }

      if (j == 0) {
        if (TRI_IsDirectory(newFull.c_str())) {
          result.push_back(newPath);

          if (!TRI_IsSymbolicLink(newFull.c_str())) {
            ListTreeRecursively(newFull.c_str(), newPath.c_str(), result);
          }
        }
      } else {
        if (!TRI_IsDirectory(newFull.c_str())) {
          result.push_back(newPath);
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locates a environment given configuration directory
////////////////////////////////////////////////////////////////////////////////

static std::string LocateConfigDirectoryEnv() {
  std::string r;
  if (!TRI_GETENV("ARANGODB_CONFIG_PATH", r)) {
    return std::string();
  }
  NormalizePath(r);
  while (!r.empty() && IsDirSeparatorChar(r[r.size() - 1])) {
    r.pop_back();
  }

  r.push_back(TRI_DIR_SEPARATOR_CHAR);

  return r;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the size of a file
///
/// Will return a negative error number on error, typically -1
////////////////////////////////////////////////////////////////////////////////

int64_t TRI_SizeFile(char const* path) {
  TRI_stat_t stbuf;
  int res = TRI_STAT(path, &stbuf);

  if (res != 0) {
    // an error occurred
    return (int64_t)res;
  }

  return (int64_t)stbuf.st_size;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if file or directory is writable
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

bool TRI_IsWritable(char const* path) {
  // ..........................................................................
  // will attempt the following:
  //   if path is a directory, then attempt to create temporary file
  //   if path is a file, then attempt to open it in read/write mode
  // ..........................................................................

  // #error "TRI_IsWritable needs to be implemented for Windows"
  // TODO: implementation for seems to be non-trivial
  return true;
}

#else

bool TRI_IsWritable(char const* path) {
  // we can use POSIX access() from unistd.h to check for write permissions
  return (access(path, W_OK) == 0);
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if path is a directory
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsDirectory(char const* path) {
  TRI_stat_t stbuf;
  int res;

  res = TRI_STAT(path, &stbuf);

  return (res == 0) && ((stbuf.st_mode & S_IFMT) == S_IFDIR);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if path is a regular file
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsRegularFile(char const* path) {
  TRI_stat_t stbuf;
  int res;

  res = TRI_STAT(path, &stbuf);

  return (res == 0) && ((stbuf.st_mode & S_IFMT) == S_IFREG);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if path is a symbolic link
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

bool TRI_IsSymbolicLink(char const* path) {
  // TODO : check if a file is a symbolic link - without opening the file
  return false;
}

#else

bool TRI_IsSymbolicLink(char const* path) {
  struct stat stbuf;
  int res;

  res = lstat(path, &stbuf);

  return (res == 0) && ((stbuf.st_mode & S_IFMT) == S_IFLNK);
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if file or directory exists
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

bool TRI_ExistsFile(char const* path) {
  if (path == nullptr) {
    return false;
  }

  TRI_stat_t stbuf;
  int res;

  size_t len = strlen(path);

  // path must not end with a \ on Windows, other stat() will return -1
  if (len > 0 && path[len - 1] == TRI_DIR_SEPARATOR_CHAR) {
    std::string copy(path);

    // remove trailing slash
    while (!copy.empty() && IsDirSeparatorChar(copy[copy.size() - 1])) {
      copy.pop_back();
    }

    res = TRI_STAT(copy.c_str(), &stbuf);
  } else {
    res = TRI_STAT(path, &stbuf);
  }

  return res == 0;
}

#else

bool TRI_ExistsFile(char const* path) {
  if (path == nullptr) {
    return false;
  }

  struct stat stbuf;
  int res = TRI_STAT(path, &stbuf);

  return res == 0;
}

#endif

int TRI_ChMod(char const* path, long mode, std::string& err) {
  int res;
#ifdef _WIN32
  UnicodeString wpath(path);
  res = _wchmod(wpath.getTerminatedBuffer(), static_cast<int>(mode));
#else
  res = chmod(path, mode);
#endif

  if (res != 0) {
    err = "error setting desired mode " + std::to_string(mode) + " for file " +
          path + ": " + strerror(errno);
    return errno;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the last modification date of a file
////////////////////////////////////////////////////////////////////////////////

int TRI_MTimeFile(char const* path, int64_t* mtime) {
  TRI_stat_t stbuf;
  int res = TRI_STAT(path, &stbuf);

  if (res == 0) {
    *mtime = static_cast<int64_t>(stbuf.st_mtime);
    return TRI_ERROR_NO_ERROR;
  }

  res = errno;
  if (res == ENOENT) {
    return TRI_ERROR_FILE_NOT_FOUND;
  }

  TRI_set_errno(TRI_ERROR_SYS_ERROR);
  return TRI_errno();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a directory, recursively
////////////////////////////////////////////////////////////////////////////////

int TRI_CreateRecursiveDirectory(char const* path, long& systemError,
                                 std::string& systemErrorStr) {
  char const* p;
  char const* s;
          
  int res = TRI_ERROR_NO_ERROR;
  std::string copy = std::string(path);
  p = s = copy.data();

  while (*p != '\0') {
    if (*p == TRI_DIR_SEPARATOR_CHAR) {
      if (p - s > 0) {
#ifdef _WIN32
        // Don't try to create the drive letter as directory:
        if ((p - copy.data() == 2) && (s[1] == ':')) {
          s = p + 1;
          continue;
        }
#endif
        // *p = '\0';
        copy[p - copy.data()] = '\0';
        res = TRI_CreateDirectory(copy.c_str(), systemError, systemErrorStr);

        if (res == TRI_ERROR_FILE_EXISTS || res == TRI_ERROR_NO_ERROR) {
          systemErrorStr.clear();
          res = TRI_ERROR_NO_ERROR;
          // *p = TRI_DIR_SEPARATOR_CHAR;
          copy[p - copy.data()] = TRI_DIR_SEPARATOR_CHAR;
          s = p + 1;
        } else {
          break;
        }
      }
    }

    p++;
  }

  if ((res == TRI_ERROR_FILE_EXISTS || res == TRI_ERROR_NO_ERROR) &&
      (p - s > 0)) {
    res = TRI_CreateDirectory(copy.c_str(), systemError, systemErrorStr);
        
    if (res == TRI_ERROR_FILE_EXISTS) {
      systemErrorStr.clear();
      res = TRI_ERROR_NO_ERROR;
    }
  }

  TRI_ASSERT(res != TRI_ERROR_FILE_EXISTS);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a directory
////////////////////////////////////////////////////////////////////////////////

int TRI_CreateDirectory(char const* path, long& systemError,
                        std::string& systemErrorStr) {
  TRI_ERRORBUF;

  // reset error flag
  TRI_set_errno(TRI_ERROR_NO_ERROR);

  int res = TRI_MKDIR(path, 0777);

  if (res == TRI_ERROR_NO_ERROR) {
    return res;
  }

  // check errno
  TRI_SYSTEM_ERROR();
  res = errno;
  if (res != TRI_ERROR_NO_ERROR) {
    systemErrorStr = std::string("failed to create directory '") + path + "': " +
                     TRI_GET_ERRORBUF;
    systemError = res;

    if (res == ENOENT) {
      return TRI_ERROR_FILE_NOT_FOUND;
    }
    if (res == EEXIST) {
      return TRI_ERROR_FILE_EXISTS;
    }
    if (res == EPERM) {
      return TRI_ERROR_FORBIDDEN;
    }
  }

  return TRI_ERROR_SYS_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an empty directory
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveEmptyDirectory(char const* filename) {
  int res = TRI_RMDIR(filename);

  if (res != 0) {
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "cannot remove directory '" << filename
               << "': " << TRI_LAST_ERROR_STR;
    return TRI_set_errno(TRI_ERROR_SYS_ERROR);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a directory recursively
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveDirectory(char const* filename) {
  if (TRI_IsSymbolicLink(filename)) {
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "removing symbolic link '" << filename << "'";
    return TRI_UnlinkFile(filename);
  } else if (TRI_IsDirectory(filename)) {
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "removing directory '" << filename << "'";

    int res = TRI_ERROR_NO_ERROR;
    std::vector<std::string> files = TRI_FilesDirectory(filename);
    for (auto const& dir : files) {
      std::string full = arangodb::basics::FileUtils::buildFilename(filename, dir);

      int subres = TRI_RemoveDirectory(full.c_str());

      if (subres != TRI_ERROR_NO_ERROR) {
        res = subres;
      }
    }

    if (res == TRI_ERROR_NO_ERROR) {
      res = TRI_RemoveEmptyDirectory(filename);
    }

    return res;
  } else if (TRI_ExistsFile(filename)) {
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "removing file '" << filename << "'";

    return TRI_UnlinkFile(filename);
  } else {
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "attempt to remove non-existing file/directory '" << filename
               << "'";

    // TODO: why do we actually return "no error" here?
    return TRI_ERROR_NO_ERROR;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a directory recursively
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveDirectoryDeterministic(char const* filename) {
  std::vector<std::string> files = TRI_FullTreeDirectory(filename);
  // start removing files from long names to short names
  std::sort(files.begin(), files.end(), [](std::string const& lhs, std::string const& rhs) -> bool {
    if (lhs.size() == rhs.size()) {
      // equal lengths. now compare the file/directory names
      return lhs < rhs;
    }
    return lhs.size() > rhs.size();
  });

  // LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "removing files in directory '" << filename << "' in this order: " << files;

  int res = TRI_ERROR_NO_ERROR;

  for (auto const& it : files) {
    if (it.empty()) {
      // TRI_FullTreeDirectory returns "" as its first member
      continue;
    }

    std::string full = arangodb::basics::FileUtils::buildFilename(filename, it);
    int subres = TRI_RemoveDirectory(full.c_str());

    if (subres != TRI_ERROR_NO_ERROR) {
      res = subres;
    }
  }

  int subres = TRI_RemoveDirectory(filename);
  if (subres != TRI_ERROR_NO_ERROR) {
    res = subres;
  }
  
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the dirname
////////////////////////////////////////////////////////////////////////////////

std::string TRI_Dirname(std::string const& path) {
  size_t n = path.size();
  size_t m = 0;

  if (1 < n) {
    if (path[n - 1] == TRI_DIR_SEPARATOR_CHAR) {
      m = 1;
    }
  }

  if (n == 0) {
    return std::string(".");
  } else if (n == 1 && path[0] == TRI_DIR_SEPARATOR_CHAR) {
    return std::string(TRI_DIR_SEPARATOR_STR);
  } else if (n - m == 1 && path[0] == '.') {
    return std::string(".");
  } else if (n - m == 2 && path[0] == '.' && path[1] == '.') {
    return std::string("..");
  }

  char const* p;
  for (p = path.data() + (n - m - 1); path.data() < p; --p) {
    if (*p == TRI_DIR_SEPARATOR_CHAR) {
      break;
    }
  }

  if (path.data() == p) {
    if (*p == TRI_DIR_SEPARATOR_CHAR) {
      return std::string(TRI_DIR_SEPARATOR_STR);
    } else {
      return std::string(".");
    }
  }

  n = p - path.data();

  return path.substr(0, n);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the basename
////////////////////////////////////////////////////////////////////////////////

std::string TRI_Basename(char const* path) {
  size_t n = strlen(path);

  if (1 < n) {
    if (IsDirSeparatorChar(path[n - 1])) {
      n -= 1;
    }
  }

  if (n == 0) {
    return std::string();
  } else if (n == 1) {
    if (IsDirSeparatorChar(*path)) {
      return std::string(TRI_DIR_SEPARATOR_STR);
    }
    return std::string(path, n);
  } else {
    char const* p;

    for (p = path + (n - 2); path < p; --p) {
      if (IsDirSeparatorChar(*p)) {
        break;
      }
    }

    if (path == p) {
      if (IsDirSeparatorChar(*p)) {
        return std::string(path + 1, n - 1);
      }
      return std::string(path, n);
    } else {
      n -= p - path;

      return std::string(p + 1, n - 1);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a list of files in path
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_WIN32_LIST_FILES

std::vector<std::string> TRI_FilesDirectory(char const* path) {
  std::vector<std::string> result;
  std::string filter(path);
  filter.append("\\*");

  struct _wfinddata_t fd;

  UnicodeString wfilter(filter.c_str());
  
  intptr_t handle = _wfindfirst(wfilter.getTerminatedBuffer(), &fd);

  if (handle == -1) {
    return result;
  }

  std::string ufn;
  UnicodeString fn;
  do {
    if (wcscmp(fd.name, L".") != 0 && wcscmp(fd.name, L"..") != 0) {
      ufn.clear();
      fn = fd.name;
      fn.toUTF8String<std::string>(ufn);
      result.emplace_back(ufn);
    }
  } while (_wfindnext(handle, &fd) != -1);

  _findclose(handle);

  return result;
}

#else

std::vector<std::string> TRI_FilesDirectory(char const* path) {
  std::vector<std::string> result;

  DIR* d = opendir(path);

  if (d == nullptr) {
    return result;
  }

  auto guard = scopeGuard([&d]() { closedir(d); });

  struct dirent* de = readdir(d);

  while (de != nullptr) {
    if (strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0) {
      // may throw
      result.emplace_back(std::string(de->d_name));
    }

    de = readdir(d);
  }
  return result;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief lists the directory tree including files and directories
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> TRI_FullTreeDirectory(char const* path) {
  std::vector<std::string> result;

  result.push_back("");
  ListTreeRecursively(path, "", result);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a file
////////////////////////////////////////////////////////////////////////////////

int TRI_RenameFile(char const* old, char const* filename, long* systemError,
                   std::string* systemErrorStr) {
  int res;
  TRI_ERRORBUF;
#ifdef _WIN32
  BOOL moveResult = 0;

  UnicodeString oldf(old);
  UnicodeString newf(filename);

  moveResult = MoveFileExW(oldf.getTerminatedBuffer(), newf.getTerminatedBuffer(),
                           MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING);

  if (!moveResult) {
    TRI_SYSTEM_ERROR();

    if (systemError != nullptr) {
      *systemError = errno;
    }
    if (systemErrorStr != nullptr) {
      *systemErrorStr = windowsErrorBuf;
    }
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "cannot rename file from '" << old << "' to '" << filename
               << "': " << errno << " - " << windowsErrorBuf;
    res = -1;
  } else {
    res = 0;
  }
#else
  res = rename(old, filename);
#endif

  if (res != 0) {
    if (systemError != nullptr) {
      *systemError = errno;
    }
    if (systemErrorStr != nullptr) {
      *systemErrorStr = TRI_LAST_ERROR_STR;
    }
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "cannot rename file from '" << old << "' to '" << filename
               << "': " << TRI_LAST_ERROR_STR;
    return TRI_set_errno(TRI_ERROR_SYS_ERROR);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlinks a file
////////////////////////////////////////////////////////////////////////////////

int TRI_UnlinkFile(char const* filename) {
  int res = TRI_UNLINK(filename);

  if (res != 0) {
    int e = errno;
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "cannot unlink file '" << filename
               << "': " << TRI_LAST_ERROR_STR;
    if (e == ENOENT) {
      return TRI_ERROR_FILE_NOT_FOUND;
    } 
    if (e == EPERM) {
      return TRI_ERROR_FORBIDDEN;
    }
    return TRI_ERROR_SYS_ERROR;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads into a buffer from a file
////////////////////////////////////////////////////////////////////////////////

bool TRI_ReadPointer(int fd, void* buffer, size_t length) {
  char* ptr = static_cast<char*>(buffer);

  while (0 < length) {
    ssize_t n = TRI_READ(fd, ptr, static_cast<TRI_read_t>(length));

    if (n < 0) {
      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot read: " << TRI_LAST_ERROR_STR;
      return false;
    } else if (n == 0) {
      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot read, end-of-file";
      return false;
    }

    ptr += n;
    length -= n;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes buffer to a file
////////////////////////////////////////////////////////////////////////////////

bool TRI_WritePointer(int fd, void const* buffer, size_t length) {
  char const* ptr = static_cast<char const*>(buffer);

  while (0 < length) {
    ssize_t n = TRI_WRITE(fd, ptr, static_cast<TRI_write_t>(length));

    if (n < 0) {
      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot write: " << TRI_LAST_ERROR_STR;
      return false;
    }

    ptr += n;
    length -= n;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves data to a file
////////////////////////////////////////////////////////////////////////////////

int TRI_WriteFile(char const* filename, char const* data, size_t length) {
  int fd;
  bool result;

  fd = TRI_CREATE(filename, O_CREAT | O_EXCL | O_RDWR | TRI_O_CLOEXEC,
                  S_IRUSR | S_IWUSR);

  if (fd == -1) {
    return TRI_set_errno(TRI_ERROR_SYS_ERROR);
  }

  result = TRI_WritePointer(fd, data, length);

  TRI_CLOSE(fd);

  if (!result) {
    return TRI_errno();
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fsyncs a file
////////////////////////////////////////////////////////////////////////////////

bool TRI_fsync(int fd) {
  int res = fsync(fd);

#ifdef __APPLE__

  if (res == 0) {
    res = fcntl(fd, F_FULLFSYNC, 0);
  }

#endif

  if (res == 0) {
    return true;
  } else {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief slurps in a file
////////////////////////////////////////////////////////////////////////////////

char* TRI_SlurpFile(char const* filename,
                    size_t* length) {
  TRI_set_errno(TRI_ERROR_NO_ERROR);
  int fd = TRI_OPEN(filename, O_RDONLY | TRI_O_CLOEXEC);

  if (fd == -1) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    return nullptr;
  }

  TRI_string_buffer_t result;
  TRI_InitStringBuffer(&result, false);

  while (true) {
    int res = TRI_ReserveStringBuffer(&result, READBUFFER_SIZE);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_CLOSE(fd);
      TRI_AnnihilateStringBuffer(&result);

      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }

    ssize_t n = TRI_READ(fd, (void*)TRI_EndStringBuffer(&result), READBUFFER_SIZE);

    if (n == 0) {
      break;
    }

    if (n < 0) {
      TRI_CLOSE(fd);

      TRI_AnnihilateStringBuffer(&result);

      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      return nullptr;
    }

    TRI_IncreaseLengthStringBuffer(&result, (size_t)n);
  }

  if (length != nullptr) {
    *length = TRI_LengthStringBuffer(&result);
  }

  TRI_CLOSE(fd);
  return result._buffer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a lock file based on the PID
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_WIN32_FILE_LOCKING

int TRI_CreateLockFile(char const* filename) {
  TRI_ERRORBUF;
  OVERLAPPED ol;

  WRITE_LOCKER(locker, OpenedFilesLock);

  for (size_t i = 0; i < OpenedFiles.size(); ++i) {
    if (OpenedFiles[i].first == filename) {
      // file already exists
      return TRI_ERROR_NO_ERROR;
    }
  }

  UnicodeString fn(filename);
  HANDLE fd = CreateFileW(fn.getTerminatedBuffer(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, NULL);

  if (fd == INVALID_HANDLE_VALUE) {
    TRI_SYSTEM_ERROR();
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot create lockfile '" << filename
             << "': " << TRI_GET_ERRORBUF;
    return TRI_set_errno(TRI_ERROR_SYS_ERROR);
  }

  TRI_pid_t pid = Thread::currentProcessId();
  std::string buf = std::to_string(pid);
  DWORD len;

  BOOL r = WriteFile(fd, buf.c_str(), static_cast<unsigned int>(buf.size()), &len, NULL);

  if (!r || len != buf.size()) {
    TRI_SYSTEM_ERROR();
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot write lockfile '" << filename
             << "': " << TRI_GET_ERRORBUF;
    int res = TRI_set_errno(TRI_ERROR_SYS_ERROR);

    if (r) {
      CloseHandle(fd);
    }

    TRI_UNLINK(filename);

    return res;
  }

  memset(&ol, 0, sizeof(ol));
  r = LockFileEx(fd, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 0,
                 128, &ol);

  if (!r) {
    TRI_SYSTEM_ERROR();
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot set lockfile status '" << filename
             << "': " << TRI_GET_ERRORBUF;
    int res = TRI_set_errno(TRI_ERROR_SYS_ERROR);

    CloseHandle(fd);
    TRI_UNLINK(filename);

    return res;
  }

  OpenedFiles.push_back(std::make_pair(filename, fd));

  return TRI_ERROR_NO_ERROR;
}

#else

int TRI_CreateLockFile(char const* filename) {
  WRITE_LOCKER(locker, OpenedFilesLock);

  for (size_t i = 0; i < OpenedFiles.size(); ++i) {
    if (OpenedFiles[i].first == filename) {
      // file already exists
      return TRI_ERROR_NO_ERROR;
    }
  }
  
  int fd = TRI_CREATE(filename, O_CREAT | O_EXCL | O_RDWR | TRI_O_CLOEXEC,
                      S_IRUSR | S_IWUSR);

  if (fd == -1) {
    return TRI_set_errno(TRI_ERROR_SYS_ERROR);
  }

  TRI_pid_t pid = Thread::currentProcessId();
  std::string buf = std::to_string(pid);

  int rv = TRI_WRITE(fd, buf.c_str(), static_cast<TRI_write_t>(buf.size()));

  if (rv == -1) {
    int res = TRI_set_errno(TRI_ERROR_SYS_ERROR);

    TRI_CLOSE(fd);
    TRI_UNLINK(filename);

    return res;
  }

  struct flock lock;

  lock.l_start = 0;
  lock.l_len = 0;
  lock.l_type = F_WRLCK;
  lock.l_whence = SEEK_SET;
  // try to lock pid file
  rv = fcntl(fd, F_SETLK, &lock);

  if (rv == -1) {
    int res = TRI_set_errno(TRI_ERROR_SYS_ERROR);

    TRI_CLOSE(fd);
    TRI_UNLINK(filename);

    return res;
  }

  OpenedFiles.push_back(std::make_pair(filename, fd));

  return TRI_ERROR_NO_ERROR;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief verifies a lock file based on the PID
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_WIN32_FILE_LOCKING

int TRI_VerifyLockFile(char const* filename) {
  HANDLE fd;

  if (!TRI_ExistsFile(filename)) {
    return TRI_ERROR_NO_ERROR;
  }

  fd = CreateFile(filename, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                  FILE_ATTRIBUTE_NORMAL, NULL);

  if (fd == INVALID_HANDLE_VALUE) {
    if (GetLastError() == ERROR_SHARING_VIOLATION) {
      return TRI_ERROR_ARANGO_DATADIR_LOCKED;
    }

    return TRI_ERROR_NO_ERROR;
  }

  CloseHandle(fd);
  TRI_UnlinkFile(filename);

  return TRI_ERROR_NO_ERROR;
}

#else

int TRI_VerifyLockFile(char const* filename) {
  if (!TRI_ExistsFile(filename)) {
    return TRI_ERROR_NO_ERROR;
  }

  int fd = TRI_OPEN(filename, O_RDWR | TRI_O_CLOEXEC);

  if (fd < 0) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "cannot open lockfile '" << filename << "' in write mode: " << TRI_last_error();
    
    if (errno == EACCES) {
      return TRI_ERROR_CANNOT_WRITE_FILE;
    }

    return TRI_ERROR_INTERNAL;
  }

  char buffer[128];
  memset(buffer, 0,
         sizeof(buffer));  // not really necessary, but this shuts up valgrind
  ssize_t n = TRI_READ(fd, buffer, static_cast<TRI_read_t>(sizeof(buffer)));

  TRI_DEFER(TRI_CLOSE(fd));

  if (n <= 0 || n == sizeof(buffer)) {
    return TRI_ERROR_NO_ERROR;
  }

  uint32_t fc = TRI_UInt32String(buffer);
  int res = TRI_errno();

  if (res != TRI_ERROR_NO_ERROR) {
    // invalid pid value
    return TRI_ERROR_NO_ERROR;
  }

  TRI_pid_t pid = fc;
    
  // check for the existence of previous process via kill command

  // from man 2 kill:
  //   If sig is 0, then no signal is sent, but existence and permission checks are still performed; 
  //   this can be used to check for the existence of a process ID or process group ID that the caller 
  //   is permitted to signal.
  if (kill(pid, 0) == -1) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "found existing lockfile '" << filename << "' of previous process with pid " << pid << ", but that process seems to be dead already";
  } else { 
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "found existing lockfile '" << filename << "' of previous process with pid " << pid << ", and that process seems to be still running";
  }

  struct flock lock;
  
  lock.l_start = 0;
  lock.l_len = 0;
  lock.l_type = F_WRLCK;
  lock.l_whence = SEEK_SET;
  // try to lock pid file
  int canLock = fcntl(fd, F_SETLK, &lock);  // Exclusive (write) lock
      
  // file was not yet locked; could be locked
  if (canLock == 0) {
    //lock.l_type = F_UNLCK;
    res = fcntl(fd, F_GETLK, &lock);
  
    if (res != TRI_ERROR_NO_ERROR) {
      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "fcntl on lockfile '" << filename
                << "' failed: " << TRI_last_error();
    }

    return TRI_ERROR_NO_ERROR;
  }

  // error!
  canLock = errno;
  TRI_set_errno(TRI_ERROR_SYS_ERROR);

  // from man 2 fcntl: "If a conflicting lock is held by another process, 
  // this call returns -1 and sets errno to EACCES or EAGAIN."
  if (canLock != EACCES && canLock != EAGAIN) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "fcntl on lockfile '" << filename
              << "' failed: " << TRI_last_error()
              << ". a possible reason is that the filesystem does not support file-locking";
  }
  
  return TRI_ERROR_ARANGO_DATADIR_LOCKED;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a lock file based on the PID
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_WIN32_FILE_LOCKING

int TRI_DestroyLockFile(char const* filename) {
  WRITE_LOCKER(locker, OpenedFilesLock);
  for (size_t i = 0; i < OpenedFiles.size(); ++i) {
    if (OpenedFiles[i].first == filename) {
      HANDLE fd = OpenedFiles[i].second;
      CloseHandle(fd);
      TRI_UnlinkFile(filename);

      OpenedFiles.erase(OpenedFiles.begin() + i);
      break;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

#else

int TRI_DestroyLockFile(char const* filename) {
  WRITE_LOCKER(locker, OpenedFilesLock);
  for (size_t i = 0; i < OpenedFiles.size(); ++i) {
    if (OpenedFiles[i].first == filename) {

      int fd = TRI_OPEN(filename, O_RDWR | TRI_O_CLOEXEC);

      if (fd < 0) {
        return TRI_ERROR_NO_ERROR;
      }

      struct flock lock;

      lock.l_start = 0;
      lock.l_len = 0;
      lock.l_type = F_UNLCK;
      lock.l_whence = SEEK_SET;
      // release the lock
      int res = fcntl(fd, F_SETLK, &lock);
      TRI_CLOSE(fd);

      if (res == 0) {
        TRI_UnlinkFile(filename);
      }

      // close lock file descriptor
      fd = OpenedFiles[i].second;
      TRI_CLOSE(fd);

      OpenedFiles.erase(OpenedFiles.begin() + i);

      return res;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief return the filename component of a file (without path)
////////////////////////////////////////////////////////////////////////////////

char* TRI_GetFilename(char const* filename) {
  char const* p;
  char const* s;

  p = s = filename;

  while (*p != '\0') {
    if (*p == '\\' || *p == '/' || *p == ':') {
      s = p + 1;
    }
    p++;
  }

  return TRI_DuplicateString(s);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the absolute path of a file
/// in contrast to realpath() this function can also be used to determine the
/// full path for files & directories that do not exist. realpath() would fail
/// for those cases.
/// It is the caller's responsibility to free the string created by this
/// function
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

char* TRI_GetAbsolutePath(char const* fileName,
                          char const* currentWorkingDirectory) {
  char* result;
  size_t cwdLength;
  size_t fileLength;
  bool ok;

  // ...........................................................................
  // Check that fileName actually makes some sense
  // ...........................................................................

  if (fileName == nullptr || *fileName == '\0') {
    return nullptr;
  }

  // ...........................................................................
  // Under windows we can assume that fileName is absolute if fileName starts
  // with a letter A-Z followed by a colon followed by either a forward or
  // backslash.
  // ...........................................................................

  if ((fileName[0] > 64 && fileName[0] < 91) ||
      (fileName[0] > 96 && fileName[0] < 123)) {
    if (fileName[1] == ':') {
      if (fileName[2] == '/' || fileName[2] == '\\') {
        return TRI_DuplicateString(fileName);
      }
    }
  }

  // ...........................................................................
  // The fileName itself was not absolute, so we attempt to amalgamate the
  // currentWorkingDirectory with the fileName
  // ...........................................................................

  // ...........................................................................
  // Check that the currentWorkingDirectory makes sense
  // ...........................................................................

  if (currentWorkingDirectory == nullptr || *currentWorkingDirectory == '\0') {
    return nullptr;
  }

  // ...........................................................................
  // Under windows the currentWorkingDirectory should start
  // with a letter A-Z followed by a colon followed by either a forward or
  // backslash.
  // ...........................................................................

  ok = false;
  if ((currentWorkingDirectory[0] > 64 && currentWorkingDirectory[0] < 91) ||
      (currentWorkingDirectory[0] > 96 && currentWorkingDirectory[0] < 123)) {
    if (currentWorkingDirectory[1] == ':') {
      if (currentWorkingDirectory[2] == '/' ||
          currentWorkingDirectory[2] == '\\') {
        ok = true;
      }
    }
  }

  if (!ok) {
    // directory name can also start with a backslash
    if (currentWorkingDirectory[0] == '/' ||
        currentWorkingDirectory[0] == '\\') {
      ok = true;
    }
  }

  if (!ok) {
    return nullptr;
  }

  // ...........................................................................
  // Determine the total legnth of the new string
  // ...........................................................................

  cwdLength = strlen(currentWorkingDirectory);
  fileLength = strlen(fileName);

  if (currentWorkingDirectory[cwdLength - 1] == '\\' ||
      currentWorkingDirectory[cwdLength - 1] == '/' || fileName[0] == '\\' ||
      fileName[0] == '/') {
    // we do not require a backslash
    result = static_cast<char*>(
        TRI_Allocate(                     (cwdLength + fileLength + 1) * sizeof(char)));
    if (result == nullptr) {
      return nullptr;
    }
    memcpy(result, currentWorkingDirectory, cwdLength);
    memcpy(result + cwdLength, fileName, fileLength);
    result[cwdLength + fileLength] = '\0';
  } else {
    // we do require a backslash
    result = static_cast<char*>(
        TRI_Allocate(                     (cwdLength + fileLength + 2) * sizeof(char)));
    if (result == nullptr) {
      return nullptr;
    }
    memcpy(result, currentWorkingDirectory, cwdLength);
    result[cwdLength] = '\\';
    memcpy(result + cwdLength + 1, fileName, fileLength);
    result[cwdLength + fileLength + 1] = '\0';
  }

  return result;
}

#else

char* TRI_GetAbsolutePath(char const* file, char const* cwd) {
  char* ptr;
  size_t cwdLength;

  if (file == nullptr || *file == '\0') {
    return nullptr;
  }

  // name is absolute if starts with either forward or backslash
  bool isAbsolute = (*file == '/' || *file == '\\');

  // file is also absolute if contains a colon
  for (ptr = (char*)file; *ptr; ++ptr) {
    if (*ptr == ':') {
      isAbsolute = true;
      break;
    }
  }

  if (isAbsolute) {
    return TRI_DuplicateString(file);
  }

  if (cwd == nullptr || *cwd == '\0') {
    // no absolute path given, must abort
    return nullptr;
  }

  cwdLength = strlen(cwd);
  TRI_ASSERT(cwdLength > 0);

  char* result = static_cast<char*>(
      TRI_Allocate(                   (cwdLength + strlen(file) + 2) * sizeof(char)));

  if (result != nullptr) {
    ptr = result;
    memcpy(ptr, cwd, cwdLength);
    ptr += cwdLength;

    if (cwd[cwdLength - 1] != '/') {
      *(ptr++) = '/';
    }
    memcpy(ptr, file, strlen(file));
    ptr += strlen(file);
    *ptr = '\0';
  }

  return result;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the binary name without any path or suffix
////////////////////////////////////////////////////////////////////////////////

std::string TRI_BinaryName(char const* argv0) {
  std::string result = TRI_Basename(argv0);
  if (result.size() > 4 && result.substr(result.size() - 4, 4) == ".exe") {
    result = result.substr(0, result.size() - 4);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locates the directory containing the program
////////////////////////////////////////////////////////////////////////////////

std::string TRI_LocateBinaryPath(char const* argv0) {
#if _WIN32
  wchar_t buff[4096];
  int res = GetModuleFileNameW(NULL, buff, sizeof(buff));

  if (res != 0) {
    buff[4095] = '\0';

    wchar_t* q = buff + res;

    while (buff < q) {
      if (*q == '\\' || *q == '/') {
        *q = '\0';
        break;
      }

      --q;
    }

    size_t len = q - buff;

    UnicodeString fn(buff, static_cast<int32_t>(len));
    std::string ufn;
    fn.toUTF8String<std::string>(ufn);
    
    return ufn;
  }

  return std::string();

#else

  std::string binaryPath;

  // check if name contains a '/' ( or '\' for windows)
  char const* p = argv0;

  for (; *p && *p != TRI_DIR_SEPARATOR_CHAR; ++p) {
  }

  // contains a path
  if (*p) {
    binaryPath = TRI_Dirname(argv0);
  }

  // check PATH variable
  else {
    std::string pv;
    if (TRI_GETENV("PATH", pv)) {
      std::vector<std::string> files = basics::StringUtils::split(pv, ':', '\0');

      for (auto const& prefix : files) {
        std::string full;
        if (!prefix.empty()) {
          full = arangodb::basics::FileUtils::buildFilename(prefix, argv0);
        } else {
          full = arangodb::basics::FileUtils::buildFilename(".", argv0);
        }

        if (TRI_ExistsFile(full.c_str())) {
          binaryPath = prefix;
          break;
        }
      }
    }
  }

  return binaryPath;
#endif
}

std::string TRI_GetInstallRoot(std::string const& binaryPath,
                               char const* installBinaryPath) {
  // First lets remove trailing (back) slashes from the bill:
  size_t installPathLength = strlen(installBinaryPath);

  if (installBinaryPath[installPathLength - 1] == TRI_DIR_SEPARATOR_CHAR) {
    --installPathLength;
  }
  
  size_t binaryPathLength = binaryPath.size();
  char const* p = binaryPath.c_str();

  if (p[binaryPathLength - 1] == TRI_DIR_SEPARATOR_CHAR) {
    --binaryPathLength;
  }
  
  if (installPathLength > binaryPathLength) {
    return TRI_DIR_SEPARATOR_STR;
  }

  for (size_t i = 1; i < installPathLength; ++i) {
    if (p[binaryPathLength - i] != installBinaryPath[installPathLength - i]) {
      return TRI_DIR_SEPARATOR_STR;
    }
  }
  return std::string(p, binaryPathLength - installPathLength);
}

static bool CopyFileContents(int srcFD, int dstFD, ssize_t fileSize,
                             std::string& error) {
  bool rc = true;
#if TRI_LINUX_SPLICE
  bool enableSplice = true;
  if (enableSplice) {
    int splicePipe[2];
    ssize_t pipeSize = 0;
    long chunkSendRemain = fileSize;
    loff_t totalSentAlready = 0;

    if (pipe(splicePipe) != 0) {
      error = std::string("splice failed to create pipes: ") + strerror(errno);
      return false;
    }
    while (chunkSendRemain > 0) {
      if (pipeSize == 0) {
        pipeSize = splice(srcFD, &totalSentAlready, splicePipe[1], nullptr,
                          chunkSendRemain, SPLICE_F_MOVE);
        if (pipeSize == -1) {
          error = std::string("splice read failed: ") + strerror(errno);
          rc = false;
          break;
        }
      }
      ssize_t sent = splice(splicePipe[0], nullptr, dstFD, nullptr, pipeSize,
                            SPLICE_F_MORE | SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
      if (sent == -1) {
        error = std::string("splice read failed: ") + strerror(errno);
        rc = false;
        break;
      }
      pipeSize -= sent;
      chunkSendRemain -= sent;
    }
    close(splicePipe[0]);
    close(splicePipe[1]);
  } else
#endif
  {
    // 128k:
    constexpr size_t C128 = 128 * 1024;
    char* buf =
        static_cast<char*>(TRI_Allocate(C128));

    if (buf == nullptr) {
      error = "failed to allocate temporary buffer";
      rc = false;
    }
    
    size_t chunkRemain = fileSize;
    while (rc && (chunkRemain > 0)) {
      size_t readChunk = (std::min)(C128, chunkRemain);
      ssize_t nRead = TRI_READ(srcFD, buf, static_cast<TRI_read_t>(readChunk));

      if (nRead < 0) {
        error = std::string("failed to read a chunk: ") + strerror(errno);
        rc = false;
        break;
      }

      if (nRead == 0) {
        // EOF. done
        break;
      }

      size_t writeOffset = 0;
      size_t writeRemaining = static_cast<size_t>(nRead);
      while (writeRemaining > 0) {
        // write can write less data than requested. so we must go on writing until
        // we have written out all data
        ssize_t nWritten = TRI_WRITE(dstFD, buf + writeOffset, static_cast<TRI_write_t>(writeRemaining));

        if (nWritten < 0) {
          // error during write
          error = std::string("failed to read a chunk: ") + strerror(errno);
          rc = false;
          break;
        }

        writeOffset += static_cast<size_t>(nWritten);
        writeRemaining -= static_cast<size_t>(nWritten);
      }

      chunkRemain -= nRead;
    }

    TRI_Free(buf);
  }
  return rc;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copies the contents of a file
////////////////////////////////////////////////////////////////////////////////

bool TRI_CopyFile(std::string const& src, std::string const& dst,
                  std::string& error) {
#ifdef _WIN32
  TRI_ERRORBUF;

  UnicodeString s(src.c_str());
  UnicodeString d(dst.c_str());

  bool rc = CopyFileW(s.getTerminatedBuffer(), d.getTerminatedBuffer(), true) != 0;
  if (!rc) {
    TRI_SYSTEM_ERROR();
    error = "failed to copy " + src + " to " + dst + ": " + TRI_GET_ERRORBUF;
  }
  return rc;
#else
  size_t dsize;
  int srcFD, dstFD;
  struct stat statbuf;

  srcFD = open(src.c_str(), O_RDONLY);
  if (srcFD < 0) {
    error = "failed to open source file " + src + ": " + strerror(errno);
    return false;
  }
  dstFD = open(dst.c_str(), O_EXCL | O_CREAT | O_NONBLOCK | O_WRONLY,
               S_IRUSR | S_IWUSR);
  if (dstFD < 0) {
    close(srcFD);
    error = "failed to open destination file " + dst + ": " + strerror(errno);
    return false;
  }

  TRI_FSTAT(srcFD, &statbuf);
  dsize = statbuf.st_size;

  bool rc = CopyFileContents(srcFD, dstFD, dsize, error);
  timeval times[2];
  memset(times, 0, sizeof(times));
  times[0].tv_sec = TRI_STAT_ATIME_SEC(statbuf);
  times[1].tv_sec = TRI_STAT_MTIME_SEC(statbuf);

  if (fchown(dstFD, -1 /*statbuf.st_uid*/, statbuf.st_gid) != 0) {
    error = std::string("failed to chown ") + dst + ": " + strerror(errno);
    // rc = false;  no, this is not fatal...
  }
  if (fchmod(dstFD, statbuf.st_mode) != 0) {
    error = std::string("failed to chmod ") + dst + ": " + strerror(errno);
    rc = false;
  }

#ifdef HAVE_FUTIMES
  if (futimes(dstFD, times) != 0) {
    error =
        std::string("failed to adjust age: ") + dst + ": " + strerror(errno);
    rc = false;
  }
#else
  if (utimes(dst.c_str(), times) != 0) {
    error =
        std::string("failed to adjust age: ") + dst + ": " + strerror(errno);
    rc = false;
  }
#endif
  close(srcFD);
  close(dstFD);
  return rc;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copies the filesystem attributes of a file
////////////////////////////////////////////////////////////////////////////////

bool TRI_CopyAttributes(std::string const& srcItem, std::string const& dstItem,
                        std::string& error) {
#ifndef _WIN32
  struct stat statbuf;

  TRI_STAT(srcItem.c_str(), &statbuf);

  if (chown(dstItem.c_str(), -1 /*statbuf.st_uid*/, statbuf.st_gid) != 0) {
    error = std::string("failed to chown ") + dstItem + ": " + strerror(errno);
    //    return false;
  }
  if (chmod(dstItem.c_str(), statbuf.st_mode) != 0) {
    error = std::string("failed to chmod ") + dstItem + ": " + strerror(errno);
    return false;
  }

  timeval times[2];
  memset(times, 0, sizeof(times));
  times[0].tv_sec = TRI_STAT_ATIME_SEC(statbuf);
  times[1].tv_sec = TRI_STAT_MTIME_SEC(statbuf);

  if (utimes(dstItem.c_str(), times) != 0) {
    error = std::string("failed to adjust age: ") + dstItem + ": " +
            strerror(errno);
    return false;
  }

#endif
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copies a symlink; the link target is not altered.
////////////////////////////////////////////////////////////////////////////////

bool TRI_CopySymlink(std::string const& srcItem, std::string const& dstItem,
                     std::string& error) {
#ifndef _WIN32
  char buffer[PATH_MAX];
  ssize_t rc = readlink(srcItem.c_str(), buffer, sizeof(buffer) - 1);
  if (rc == -1) {
    error = std::string("failed to read symlink ") + srcItem + ": " +
            strerror(errno);
    return false;
  }
  buffer[rc] = '\0';
  if (symlink(buffer, dstItem.c_str()) != 0) {
    error = std::string("failed to create symlink ") + dstItem + " -> " +
            buffer + ": " + strerror(errno);
    return false;
  }
#endif
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locates the home directory
///
/// Under windows there is no 'HOME' directory as such so getenv("HOME") may
/// return NULL -- which it does under windows.  A safer approach below
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

std::string TRI_HomeDirectory() {
  std::string drive;
  std::string path;

  if (! TRI_GETENV("HOMEDRIVE", drive) ||
      ! TRI_GETENV("HOMEPATH", path)) {
    return std::string();
  }
  
  return drive + path;
}

#else

std::string TRI_HomeDirectory() {
  char const* result = getenv("HOME");

  if (result == nullptr) {
    return std::string(".");
  }

  return std::string(result);
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief calculate the crc32 checksum of a file
////////////////////////////////////////////////////////////////////////////////

int TRI_Crc32File(char const* path, uint32_t* crc) {
  FILE* fin;
  void* buffer;
  int bufferSize;
  int res;
  int res2;

  *crc = TRI_InitialCrc32();

  bufferSize = 4096;
  buffer = TRI_Allocate((size_t)bufferSize);

  if (buffer == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  fin = TRI_FOPEN(path, "rb");

  if (fin == nullptr) {
    TRI_Free(buffer);

    return TRI_ERROR_FILE_NOT_FOUND;
  }

  res = TRI_ERROR_NO_ERROR;

  while (true) {
    int sizeRead = (int)fread(buffer, 1, bufferSize, fin);

    if (sizeRead < bufferSize) {
      if (feof(fin) == 0) {
        res = errno;
        break;
      }
    }

    if (sizeRead > 0) {
      *crc = TRI_BlockCrc32(*crc, static_cast<char const*>(buffer), sizeRead);
    } else /* if (sizeRead <= 0) */ {
      break;
    }
  }

  TRI_Free(buffer);

  res2 = fclose(fin);
  if (res2 != TRI_ERROR_NO_ERROR && res2 != EOF) {
    if (res == TRI_ERROR_NO_ERROR) {
      res = res2;
    }
    // otherwise keep original error
  }

  *crc = TRI_FinalCrc32(*crc);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the application's name, should be called before the first
/// call to TRI_GetTempPath
////////////////////////////////////////////////////////////////////////////////

static std::string TRI_ApplicationName = "arangodb";

void TRI_SetApplicationName(std::string const& name) {
  TRI_ApplicationName = name;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the system's temporary path
////////////////////////////////////////////////////////////////////////////////
#ifdef _WIN32
static std::string getTempPath() {
  // ..........................................................................
  // Unfortunately we generally have little control on whether or not the
  // application will be compiled with UNICODE defined. In some cases such as
  // this one, we attempt to cater for both. MS provides some methods which are
  // 'defined' for both, for example, GetTempPath (below) actually converts to
  // GetTempPathA (ascii) or GetTempPathW (wide characters or what MS call
  // unicode).
  // ..........................................................................

#define LOCAL_MAX_PATH_BUFFER 2049
  wchar_t tempPathName[LOCAL_MAX_PATH_BUFFER];
  DWORD dwReturnValue = 0;
  // ..........................................................................
  // Attempt to locate the path where the users temporary files are stored
  // Note we are imposing a limit of 2048+1 characters for the maximum size of a
  // possible path
  // ..........................................................................

  /* from MSDN:
     The GetTempPath function checks for the existence of environment variables
     in the following order and uses the first path found:

     The path specified by the TMP environment variable.
     The path specified by the TEMP environment variable.
     The path specified by the USERPROFILE environment variable.
     The Windows directory.
  */
  dwReturnValue = GetTempPathW(LOCAL_MAX_PATH_BUFFER, tempPathName);

  if ((dwReturnValue > LOCAL_MAX_PATH_BUFFER) || (dwReturnValue == 0)) {
    // something wrong
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "GetTempPathW failed: LOCAL_MAX_PATH_BUFFER="
                                              << LOCAL_MAX_PATH_BUFFER << ":dwReturnValue=" << dwReturnValue;
  }

  UnicodeString tmpPathW(tempPathName, dwReturnValue);
  std::string result;
  tmpPathW.toUTF8String<std::string>(result);
  // ...........................................................................
  // Whether or not UNICODE is defined, we assume that the temporary file name
  // fits in the ascii set of characters. This is a small compromise so that
  // temporary file names can be extra long if required.
  // ...........................................................................

  for (auto const& it : result) {
    if (static_cast<uint8_t>(it) > 127) {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "Invalid characters in temporary path name: '" <<
        result << "'";
      FATAL_ERROR_ABORT();
    }
  }
  if (result.empty() || (result.back() != TRI_DIR_SEPARATOR_CHAR)) {
    result += TRI_DIR_SEPARATOR_STR;
  }
  return result;
}

static int mkDTemp(char* s, size_t bufferSize) {
  std::string out;
  UnicodeString sw(s);
  // this will overwrite the _XXX part of the string:
  auto rc = _wmktemp_s((wchar_t*)sw.getTerminatedBuffer(), bufferSize);
  if (rc == 0) {
    // if it worked out, we need to return the utf8 version:
    sw.toUTF8String<std::string>(out);
    memcpy(s, out.c_str(), bufferSize);
    rc = TRI_MKDIR(s, 0700);
  }
  return rc;
}

#else

static std::string getTempPath() {
  std::string system = "";
  char const* v = getenv("TMPDIR");

  if (v == nullptr || *v == '\0') {
    system = "/tmp/";
  } else if (v[strlen(v) - 1] == '/') {
    system = v;
  } else {
    system = std::string(v) + "/";
  }
  return system;
}

static int mkDTemp(char* s, size_t /*bufferSize*/) {
  if (mkdtemp(s) != nullptr) {
    return TRI_ERROR_NO_ERROR;
  }
  return errno;
}

#endif

/// @brief the actual temp path used
static std::unique_ptr<char[]> SystemTempPath;

/// @brief user-defined temp path
static std::string UserTempPath;

class SystemTempPathSweeper {
  std::string _systemTempPath;
public:
  SystemTempPathSweeper() : _systemTempPath(){};

  ~SystemTempPathSweeper(void) {
    if (!_systemTempPath.empty()) {
      // delete directory iff directory is empty
      TRI_RMDIR(_systemTempPath.c_str());
    }
  }
  void init(const char *path) {
    _systemTempPath = path;
  }
};

SystemTempPathSweeper SystemTempPathSweeperInstance;

// The TempPath is set but not created
void TRI_SetTempPath(std::string const& temp) {
  UserTempPath = temp;
}

std::string TRI_GetTempPath() {
  char* path = SystemTempPath.get();

  if (path == nullptr) {
    std::string system;
    if (UserTempPath.empty()) {
      system = getTempPath();
    } else {
      system = UserTempPath;
    }
  
    // Strip any double back/slashes from the string.
    while (system.find(TRI_DIR_SEPARATOR_STR TRI_DIR_SEPARATOR_STR) !=  std::string::npos) {
      system = StringUtils::replace(system,
                                    std::string(TRI_DIR_SEPARATOR_STR TRI_DIR_SEPARATOR_STR),
                                    std::string(TRI_DIR_SEPARATOR_STR)
                                    );
    }
  
    // remove trailing DIR_SEPARATOR
    while (!system.empty() && IsDirSeparatorChar(system[system.size() - 1])) {
      system.pop_back();
    }

    // and re-append it 
    system.push_back(TRI_DIR_SEPARATOR_CHAR);

    if (UserTempPath.empty()) {
      system += TRI_ApplicationName + "_XXXXXX";
    }
  
    int tries = 0;
    while (true) {
      // copy to a character array
      SystemTempPath.reset(new char[system.size() + 1]);
      path = SystemTempPath.get();
      TRI_CopyString(path, system.c_str(), system.size());

      int res;
      if (!UserTempPath.empty()) {
        // --temp.path was specified
        if (TRI_IsDirectory(system.c_str())) {
          // temp directory already exists. now simply use it
          break;
        }
    
        res = TRI_MKDIR(UserTempPath.c_str(), 0700);
      } else {
        // no --temp.path was specified
        // fill template and create directory
        tries = 9;
        res = mkDTemp(SystemTempPath.get(), system.size() + 1);
      }

      if (res == TRI_ERROR_NO_ERROR) {
        break;
      }

      // directory could not be created
      // this may be a race, a permissions problem or something else
      if (++tries >= 10) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
        LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "UserTempPath: " << UserTempPath << ", system: " << system << ", user temp path exists: " << TRI_IsDirectory(UserTempPath.c_str()) << ", res: " << res << ", SystemTempPath: " << SystemTempPath.get();
#endif
        LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "failed to create a temporary directory - giving up!";
        FATAL_ERROR_ABORT();
      }
      // sleep for a random amout of time and try again soon
      // with this, we try to avoid races between multiple processes
      // that try to create temp directories at the same time
      std::this_thread::sleep_for(std::chrono::milliseconds(5 + RandomGenerator::interval(uint64_t(20))));
    }

    SystemTempPathSweeperInstance.init(SystemTempPath.get());
  }

  return std::string(path);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief get a temporary file name
////////////////////////////////////////////////////////////////////////////////

int TRI_GetTempName(char const* directory, std::string& result, bool createFile,
                    long& systemError, std::string& errorMessage) {
  std::string temp = TRI_GetTempPath();

  std::string dir;
  if (directory != nullptr) {
    dir = arangodb::basics::FileUtils::buildFilename(temp, directory);
  } else {
    dir = temp;
  }

  // remove trailing DIR_SEPARATOR
  while (!dir.empty() && IsDirSeparatorChar(dir[dir.size() - 1])) {
    dir.pop_back();
  }
 
  int res = TRI_CreateRecursiveDirectory(dir.c_str(), systemError, errorMessage);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  if (!TRI_IsDirectory(dir.c_str())) {
    errorMessage = dir + " exists and is not a directory!";
    return TRI_ERROR_CANNOT_CREATE_DIRECTORY;
  }

  int tries = 0;
  while (tries++ < 10) {
    TRI_pid_t pid = Thread::currentProcessId();

    std::string tempName = "tmp-" + std::to_string(pid) + '-' + std::to_string(RandomGenerator::interval(UINT32_MAX));
    
    std::string filename = arangodb::basics::FileUtils::buildFilename(dir, tempName);

    if (TRI_ExistsFile(filename.c_str())) {
      errorMessage = std::string("Tempfile already exists! ") + filename;
    } else {
      if (createFile) {
        FILE* fd = TRI_FOPEN(filename.c_str(), "wb");

        if (fd != nullptr) {
          fclose(fd);
          result = filename;
          return TRI_ERROR_NO_ERROR;
        }
      } else {
        result = filename;
        return TRI_ERROR_NO_ERROR;
      }
    }

    // next try
  }

  return TRI_ERROR_CANNOT_CREATE_TEMP_FILE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locate the installation directory
//
/// Will always end in a directory separator.
////////////////////////////////////////////////////////////////////////////////

std::string TRI_LocateInstallDirectory(char const* argv0, char const* binaryPath) {
  std::string thisPath = TRI_LocateBinaryPath(argv0);
  return TRI_GetInstallRoot(thisPath, binaryPath) + TRI_DIR_SEPARATOR_CHAR; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locates the configuration directory
///
/// Will always end in a directory separator.
////////////////////////////////////////////////////////////////////////////////

#if _WIN32

std::string TRI_LocateConfigDirectory(char const* binaryPath) {
  std::string v = LocateConfigDirectoryEnv();

  if (!v.empty()) {
    return v;
  }

  std::string r = TRI_LocateInstallDirectory(nullptr, binaryPath);

  r += _SYSCONFDIR_;
  r += std::string(1, TRI_DIR_SEPARATOR_CHAR);

  return r;
}

#elif defined(_SYSCONFDIR_)

std::string TRI_LocateConfigDirectory(char const* binaryPath) {
  std::string v = LocateConfigDirectoryEnv();

  if (!v.empty()) {
    return v;
  }

  char const* dir = _SYSCONFDIR_;

  if (*dir == '\0') {
    return std::string();
  }

  size_t len = strlen(dir);

  if (dir[len - 1] != TRI_DIR_SEPARATOR_CHAR) {
    return std::string(dir) + "/";
  } else {
    return std::string(dir);
  }
}

#else

std::string TRI_LocateConfigDirectory(char const*) { return LocateConfigDirectoryEnv(); }

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief get the address of the null buffer
////////////////////////////////////////////////////////////////////////////////

char* TRI_GetNullBufferFiles() { return &NullBuffer[0]; }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the size of the null buffer
////////////////////////////////////////////////////////////////////////////////

size_t TRI_GetNullBufferSizeFiles() { return sizeof(NullBuffer); }

/// @brief creates a new datafile
/// returns the file descriptor or -1 if the file cannot be created
int TRI_CreateDatafile(std::string const& filename, size_t maximalSize) {
  TRI_ERRORBUF;

  // open the file
  int fd = TRI_CREATE(filename.c_str(), O_CREAT | O_EXCL | O_RDWR | TRI_O_CLOEXEC | TRI_NOATIME,
                      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

  TRI_IF_FAILURE("CreateDatafile1") {
    // intentionally fail
    TRI_CLOSE(fd);
    fd = -1;
    errno = ENOSPC;
  }

  if (fd < 0) {
    if (errno == ENOSPC) {
      TRI_set_errno(TRI_ERROR_ARANGO_FILESYSTEM_FULL);
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot create datafile '" << filename << "': " << TRI_last_error();
    } else {
      TRI_SYSTEM_ERROR();

      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot create datafile '" << filename << "': " << TRI_GET_ERRORBUF;
    }
    return -1;
  }

  // no fallocate present, or at least pretend it's not there...
  int res = TRI_ERROR_NOT_IMPLEMENTED;

#ifdef __linux__
#ifdef FALLOC_FL_ZERO_RANGE
  // try fallocate
  res = fallocate(fd, FALLOC_FL_ZERO_RANGE, 0, maximalSize);
#endif
#endif

  if (res != TRI_ERROR_NO_ERROR) {
    // either fallocate failed or it is not there...

    // fill file with zeros from FileNullBuffer
    size_t writeSize = TRI_GetNullBufferSizeFiles();
    size_t written = 0;
    while (written < maximalSize) {
      if (writeSize + written > maximalSize) {
        writeSize = maximalSize - written;
      }

      ssize_t writeResult =
          TRI_WRITE(fd, TRI_GetNullBufferFiles(), static_cast<TRI_write_t>(writeSize));

      TRI_IF_FAILURE("CreateDatafile2") {
        // intentionally fail
        writeResult = -1;
        errno = ENOSPC;
      }

      if (writeResult < 0) {
        if (errno == ENOSPC) {
          TRI_set_errno(TRI_ERROR_ARANGO_FILESYSTEM_FULL);
          LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot create datafile '" << filename << "': " << TRI_last_error();
        } else {
          TRI_SYSTEM_ERROR();
          TRI_set_errno(TRI_ERROR_SYS_ERROR);
          LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot create datafile '" << filename << "': " << TRI_GET_ERRORBUF;
        }

        TRI_CLOSE(fd);
        TRI_UnlinkFile(filename.c_str());

        return -1;
      }

      written += static_cast<size_t>(writeResult);
    }
  }

  // go back to offset 0
  TRI_lseek_t offset = TRI_LSEEK(fd, (TRI_lseek_t)0, SEEK_SET);

  if (offset == (TRI_lseek_t)-1) {
    TRI_SYSTEM_ERROR();
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    TRI_CLOSE(fd);

    // remove empty file
    TRI_UnlinkFile(filename.c_str());

    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "cannot seek in datafile '" << filename << "': '" << TRI_GET_ERRORBUF << "'";
    return -1;
  }

  return fd;
}


bool TRI_PathIsAbsolute(std::string const& path) {
#if _WIN32
  UnicodeString upath(path.c_str(), (uint16_t) path.length());
  return !PathIsRelativeW(upath.getTerminatedBuffer());
#else  
  return (!path.empty()) && path.c_str()[0] == '/';
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize the files subsystem
////////////////////////////////////////////////////////////////////////////////

void TRI_InitializeFiles() {
  // fill buffer with 0 bytes
  memset(TRI_GetNullBufferFiles(), 0, TRI_GetNullBufferSizeFiles());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown the files subsystem
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownFiles() {}


bool TRI_GETENV(char const* which, std::string &value) {
#ifdef _WIN32
  UnicodeString uwhich(which);
  wchar_t const *v = _wgetenv(uwhich.getTerminatedBuffer());

  if (v == nullptr) {
    return false;
  }
  value.clear();
  UnicodeString vu(v);
  vu.toUTF8String<std::string>(value);
  return true;
#else
  char const* v = getenv(which);

  if (v == nullptr) {
    return false;
  }
  value.clear();
  value = v;
  return true;
#endif
}
