////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <algorithm>
#include <chrono>
#include <memory>
#include <thread>
#include <type_traits>
#include <utility>

#include "Basics/operating-system.h"

#include <sys/statvfs.h>

#ifdef TRI_HAVE_DIRENT_H
#include <dirent.h>
#endif

#ifdef TRI_HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef TRI_HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "files.h"

#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/Thread.h"
#include "Basics/WriteLocker.h"
#include "Basics/application-exit.h"
#include "Basics/conversions.h"
#include "Basics/debugging.h"
#include "Basics/directories.h"
#include "Basics/error.h"
#include "Basics/hashes.h"
#include "Basics/memory.h"
#include "Basics/threads.h"
#include "Basics/tri-strings.h"
#include "Basics/voc-errors.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Random/RandomGenerator.h"

using namespace arangodb::basics;
using namespace arangodb;

namespace {

// whether or not we can use the splice system call on Linux
bool canUseSplice = true;

/// @brief names of blocking files
std::vector<std::pair<std::string, int>> OpenedFiles;

/// @brief lock for protected access to vector OpenedFiles
static basics::ReadWriteLock OpenedFilesLock;

/// @brief struct to remove all opened lockfiles on shutdown
struct LockfileRemover {
  LockfileRemover() {}

  ~LockfileRemover() {
    WRITE_LOCKER(locker, OpenedFilesLock);

    for (auto const& it : OpenedFiles) {
      int fd = it.second;
      TRI_CLOSE(fd);

      TRI_UnlinkFile(it.first.c_str());
    }

    OpenedFiles.clear();
  }
};

/// @brief this instance will remove all lockfiles in its dtor
static LockfileRemover remover;

}  // namespace

/// @brief read buffer size (used for bulk file reading)
#define READBUFFER_SIZE 8192

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the character is a directory separator
////////////////////////////////////////////////////////////////////////////////

static constexpr bool IsDirSeparatorChar(char c) {
  // the check for c != TRI_DIR_SEPARATOR_CHAR is required
  // for g++6. otherwise it will warn about equal expressions
  // in the two branches
  if constexpr (TRI_DIR_SEPARATOR_CHAR == '/') {
    return c == TRI_DIR_SEPARATOR_CHAR;
  } else {
    return (c == TRI_DIR_SEPARATOR_CHAR || c == '/');
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalizes path
///
/// @note path will be modified in-place
////////////////////////////////////////////////////////////////////////////////

void TRI_NormalizePath(std::string& path) {
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
  TRI_NormalizePath(r);
  while (!r.empty() && IsDirSeparatorChar(r[r.size() - 1])) {
    r.pop_back();
  }

  r.push_back(TRI_DIR_SEPARATOR_CHAR);

  return r;
}

void TRI_SetCanUseSplice(bool value) noexcept { ::canUseSplice = value; }

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

bool TRI_IsWritable(char const* path) {
  // we can use POSIX access() from unistd.h to check for write permissions
  return (access(path, W_OK) == 0);
}

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

bool TRI_IsSymbolicLink(char const* path) {
  struct stat stbuf;
  int res;

  res = lstat(path, &stbuf);

  return (res == 0) && ((stbuf.st_mode & S_IFMT) == S_IFLNK);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a symbolic link
////////////////////////////////////////////////////////////////////////////////

bool TRI_CreateSymbolicLink(std::string const& target,
                            std::string const& linkpath, std::string& error) {
  int res = symlink(target.c_str(), linkpath.c_str());

  if (res < 0) {
    error = StringUtils::concatT("failed to create a symlink ", target, " -> ",
                                 linkpath, " - ", strerror(errno));
  }
  return res == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resolves a symbolic link
////////////////////////////////////////////////////////////////////////////////

namespace {
static bool IsSymbolicLink(char const* path, struct stat* stbuf) {
  int res = lstat(path, stbuf);

  return (res == 0) && ((stbuf->st_mode & S_IFMT) == S_IFLNK);
}
}  // namespace

std::string TRI_ResolveSymbolicLink(std::string path, bool& hadError,
                                    bool recursive) {
  struct stat sb;
  while (IsSymbolicLink(path.data(), &sb)) {
    // if file is a symlink this contains the targets file name length
    // instead of the file size
    auto buffsize = sb.st_size + 1;

    // resolve symlinks
    std::vector<char> buff;
    buff.resize(buffsize);
    auto written = ::readlink(path.c_str(), buff.data(), buff.size());

    if (written) {
      path = std::string(buff.data(), buff.size());
    } else {
      // error occured while resolving
      hadError = true;
      break;
    }
    if (!recursive) {
      break;
    }
  }
  return path;
}

std::string TRI_ResolveSymbolicLink(std::string path, bool recursive) {
  bool ignore;
  return TRI_ResolveSymbolicLink(std::move(path), ignore, recursive);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if file or directory exists
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExistsFile(char const* path) {
  if (path == nullptr) {
    return false;
  }

  struct stat stbuf;
  int res = TRI_STAT(path, &stbuf);

  return res == 0;
}

ErrorCode TRI_ChMod(char const* path, long mode, std::string& err) {
  int res = chmod(path, mode);

  if (res != 0) {
    auto res2 = TRI_set_errno(TRI_ERROR_SYS_ERROR);
    err = StringUtils::concatT("error setting desired mode ",
                               std::to_string(mode), " for file ", path, ": ",
                               TRI_last_error());
    return res2;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the last modification date of a file
////////////////////////////////////////////////////////////////////////////////

ErrorCode TRI_MTimeFile(char const* path, int64_t* mtime) {
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

ErrorCode TRI_CreateRecursiveDirectory(char const* path, long& systemError,
                                       std::string& systemErrorStr) {
  char const* p;
  char const* s;

  auto res = TRI_ERROR_NO_ERROR;
  std::string copy = std::string(path);
  p = s = copy.data();

  while (*p != '\0') {
    if (*p == TRI_DIR_SEPARATOR_CHAR) {
      if (p - s > 0) {
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

ErrorCode TRI_CreateDirectory(char const* path, long& systemError,
                              std::string& systemErrorStr) {
  // reset error flag
  TRI_set_errno(TRI_ERROR_NO_ERROR);

  int res = TRI_MKDIR(path, 0777);

  if (res == 0) {
    return TRI_ERROR_NO_ERROR;
  }

  // check errno
  res = errno;
  if (res != 0) {
    systemErrorStr = std::string("failed to create directory '") + path +
                     "': " + TRI_LAST_ERROR_STR;
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

ErrorCode TRI_RemoveEmptyDirectory(char const* filename) {
  int res = TRI_RMDIR(filename);

  if (res != 0) {
    LOG_TOPIC("a6c6a", TRACE, arangodb::Logger::FIXME)
        << "cannot remove directory '" << filename
        << "': " << TRI_LAST_ERROR_STR;
    return TRI_set_errno(TRI_ERROR_SYS_ERROR);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a directory recursively
////////////////////////////////////////////////////////////////////////////////

ErrorCode TRI_RemoveDirectory(char const* filename) {
  if (TRI_IsSymbolicLink(filename)) {
    LOG_TOPIC("abdb4", TRACE, arangodb::Logger::FIXME)
        << "removing symbolic link '" << filename << "'";
    return TRI_UnlinkFile(filename);
  } else if (TRI_IsDirectory(filename)) {
    LOG_TOPIC("0207a", TRACE, arangodb::Logger::FIXME)
        << "removing directory '" << filename << "'";

    auto res = TRI_ERROR_NO_ERROR;
    std::vector<std::string> files = TRI_FilesDirectory(filename);
    for (auto const& dir : files) {
      std::string full =
          arangodb::basics::FileUtils::buildFilename(filename, dir);

      auto subres = TRI_RemoveDirectory(full.c_str());

      if (subres != TRI_ERROR_NO_ERROR) {
        res = subres;
      }
    }

    if (res == TRI_ERROR_NO_ERROR) {
      res = TRI_RemoveEmptyDirectory(filename);
    }

    return res;
  } else if (TRI_ExistsFile(filename)) {
    LOG_TOPIC("f103f", TRACE, arangodb::Logger::FIXME)
        << "removing file '" << filename << "'";

    return TRI_UnlinkFile(filename);
  } else {
    LOG_TOPIC("08df8", TRACE, arangodb::Logger::FIXME)
        << "attempt to remove non-existing file/directory '" << filename << "'";

    // TODO: why do we actually return "no error" here?
    return TRI_ERROR_NO_ERROR;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a directory recursively
////////////////////////////////////////////////////////////////////////////////

ErrorCode TRI_RemoveDirectoryDeterministic(char const* filename) {
  std::vector<std::string> files = TRI_FullTreeDirectory(filename);
  // start removing files from long names to short names
  std::sort(files.begin(), files.end(),
            [](std::string const& lhs, std::string const& rhs) -> bool {
              if (lhs.size() == rhs.size()) {
                // equal lengths. now compare the file/directory names
                return lhs < rhs;
              }
              return lhs.size() > rhs.size();
            });

  // LOG_TOPIC("0d9b9", TRACE, arangodb::Logger::FIXME) << "removing files in
  // directory
  // '" << filename << "' in this order: " << files;

  auto res = TRI_ERROR_NO_ERROR;

  for (auto const& it : files) {
    if (it.empty()) {
      // TRI_FullTreeDirectory returns "" as its first member
      continue;
    }

    std::string full = arangodb::basics::FileUtils::buildFilename(filename, it);
    auto subres = TRI_RemoveDirectory(full.c_str());

    if (subres != TRI_ERROR_NO_ERROR) {
      res = subres;
    }
  }

  auto subres = TRI_RemoveDirectory(filename);
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

  if (n == 0) {
    // "" => "."
    return std::string(".");
  }

  if (n > 1 && path[n - 1] == TRI_DIR_SEPARATOR_CHAR) {
    // .../ => ...
    return path.substr(0, n - 1);
  }

  if (n == 1 && path[0] == TRI_DIR_SEPARATOR_CHAR) {
    // "/" => "/"
    return std::string(TRI_DIR_SEPARATOR_STR);
  } else if (n == 1 && path[0] == '.') {
    return std::string(".");
  } else if (n == 2 && path[0] == '.' && path[1] == '.') {
    return std::string("..");
  }

  char const* p;
  for (p = path.data() + (n - 1); path.data() < p; --p) {
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

std::string TRI_Basename(std::string const& path) {
  return TRI_Basename(path.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a list of files in path
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> TRI_FilesDirectory(char const* path) {
  std::vector<std::string> result;

  DIR* d = opendir(path);

  if (d == nullptr) {
    return result;
  }

  auto guard = scopeGuard([&d]() noexcept { closedir(d); });

  struct dirent* de = readdir(d);

  while (de != nullptr) {
    if (strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0) {
      // may throw
      result.emplace_back(de->d_name);
    }

    de = readdir(d);
  }
  return result;
}

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

ErrorCode TRI_RenameFile(char const* old, char const* filename,
                         long* systemError, std::string* systemErrorStr) {
  int res = rename(old, filename);

  if (res != 0) {
    if (systemError != nullptr) {
      *systemError = errno;
    }
    if (systemErrorStr != nullptr) {
      *systemErrorStr = TRI_LAST_ERROR_STR;
    }
    return TRI_set_errno(TRI_ERROR_SYS_ERROR);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlinks a file
////////////////////////////////////////////////////////////////////////////////

ErrorCode TRI_UnlinkFile(char const* filename) {
  int res = TRI_UNLINK(filename);

  if (res != 0) {
    int e = errno;
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG_TOPIC("80e57", TRACE, arangodb::Logger::FIXME)
        << "cannot unlink file '" << filename << "': " << TRI_LAST_ERROR_STR;
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

TRI_read_return_t TRI_ReadPointer(int fd, char* buffer, size_t length) {
  char* ptr = buffer;
  size_t remainLength = length;

  while (0 < remainLength) {
    TRI_read_return_t n =
        TRI_READ(fd, ptr, static_cast<TRI_read_t>(remainLength));

    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      LOG_TOPIC("c9c0c", ERR, arangodb::Logger::FIXME)
          << "cannot read: " << TRI_LAST_ERROR_STR;
      return n;  // always negative
    } else if (n == 0) {
      break;
    }

    ptr += n;
    remainLength -= n;
  }

  TRI_ASSERT(ptr >= buffer);
  return static_cast<TRI_read_return_t>(ptr - buffer);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief writes buffer to a file
////////////////////////////////////////////////////////////////////////////////

bool TRI_WritePointer(int fd, void const* buffer, size_t length) {
  char const* ptr = static_cast<char const*>(buffer);

  while (0 < length) {
    auto n = TRI_WRITE(fd, ptr, static_cast<TRI_write_t>(length));

    if (n < 0) {
      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      LOG_TOPIC("420b1", ERR, arangodb::Logger::FIXME)
          << "cannot write: " << TRI_LAST_ERROR_STR;
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

ErrorCode TRI_WriteFile(char const* filename, char const* data, size_t length) {
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

char* TRI_SlurpFile(char const* filename, size_t* length) {
  TRI_set_errno(TRI_ERROR_NO_ERROR);
  int fd = TRI_OPEN(filename, O_RDONLY | TRI_O_CLOEXEC);

  if (fd == -1) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    return nullptr;
  }

  TRI_string_buffer_t result;
  TRI_InitStringBuffer(&result, false);

  while (true) {
    auto res = TRI_ReserveStringBuffer(&result, READBUFFER_SIZE);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_CLOSE(fd);
      TRI_AnnihilateStringBuffer(&result);

      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }

    TRI_read_return_t n =
        TRI_READ(fd, (void*)TRI_EndStringBuffer(&result), READBUFFER_SIZE);

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
/// @brief Read a file and pass contents to user function:  true if entire file
///        processed
////////////////////////////////////////////////////////////////////////////////

bool TRI_ProcessFile(
    char const* filename,
    std::function<bool(char const* block, size_t size)> const& reader) {
  TRI_set_errno(TRI_ERROR_NO_ERROR);
  int fd = TRI_OPEN(filename, O_RDONLY | TRI_O_CLOEXEC);

  if (fd == -1) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    return false;
  }

  auto guard = scopeGuard([&fd]() noexcept { TRI_CLOSE(fd); });

  char buffer[16384];

  while (true) {
    TRI_read_return_t n = TRI_READ(fd, &buffer[0], sizeof(buffer));

    if (n == 0) {
      return true;
    }

    if (n < 0) {
      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      return false;
    }

    if (!reader(&buffer[0], n)) {
      return false;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a lock file based on the PID
////////////////////////////////////////////////////////////////////////////////

ErrorCode TRI_CreateLockFile(char const* filename) {
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
    auto res = TRI_set_errno(TRI_ERROR_SYS_ERROR);

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
    auto res = TRI_set_errno(TRI_ERROR_SYS_ERROR);

    TRI_CLOSE(fd);
    TRI_UNLINK(filename);

    return res;
  }

  OpenedFiles.push_back(std::make_pair(filename, fd));

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief verifies a lock file based on the PID
////////////////////////////////////////////////////////////////////////////////

ErrorCode TRI_VerifyLockFile(char const* filename) {
  if (!TRI_ExistsFile(filename)) {
    return TRI_ERROR_NO_ERROR;
  }

  int fd = TRI_OPEN(filename, O_RDWR | TRI_O_CLOEXEC);

  if (fd < 0) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG_TOPIC("51db5", WARN, arangodb::Logger::FIXME)
        << "cannot open lockfile '" << filename
        << "' in write mode: " << TRI_last_error();

    if (errno == EACCES) {
      return TRI_ERROR_CANNOT_WRITE_FILE;
    }

    return TRI_ERROR_INTERNAL;
  }

  auto sg = arangodb::scopeGuard([&]() noexcept { TRI_CLOSE(fd); });

  char buffer[128];
  memset(buffer, 0,
         sizeof(buffer));  // not really necessary, but this shuts up valgrind
  TRI_read_return_t n =
      TRI_READ(fd, buffer, static_cast<TRI_read_t>(sizeof(buffer)));

  if (n <= 0 || n == sizeof(buffer)) {
    return TRI_ERROR_NO_ERROR;
  }

  uint32_t fc = basics::StringUtils::uint32(&buffer[0], n);

  if (fc == 0) {
    // invalid pid value
    return TRI_ERROR_NO_ERROR;
  }

  TRI_pid_t pid = fc;

  // check for the existence of previous process via kill command

  // from man 2 kill:
  //   If sig is 0, then no signal is sent, but existence and permission checks
  //   are still performed; this can be used to check for the existence of a
  //   process ID or process group ID that the caller is permitted to signal.
  if (kill(pid, 0) == -1) {
    LOG_TOPIC("b387d", WARN, arangodb::Logger::FIXME)
        << "found existing lockfile '" << filename
        << "' of previous process with pid " << pid
        << ", but that process seems to be dead already";
  } else {
    LOG_TOPIC("ad4b2", WARN, arangodb::Logger::FIXME)
        << "found existing lockfile '" << filename
        << "' of previous process with pid " << pid
        << ", and that process seems to be still running";
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
    // lock.l_type = F_UNLCK;
    if (0 != fcntl(fd, F_GETLK, &lock)) {
      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      LOG_TOPIC("c960a", WARN, arangodb::Logger::FIXME)
          << "fcntl on lockfile '" << filename
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
    LOG_TOPIC("94b6d", WARN, arangodb::Logger::FIXME)
        << "fcntl on lockfile '" << filename << "' failed: " << TRI_last_error()
        << ". a possible reason is that the filesystem does not support "
           "file-locking";
  }

  return TRI_ERROR_ARANGO_DATADIR_LOCKED;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a lock file based on the PID
////////////////////////////////////////////////////////////////////////////////

ErrorCode TRI_DestroyLockFile(char const* filename) {
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
      auto res = TRI_ERROR_NO_ERROR;
      if (0 != fcntl(fd, F_SETLK, &lock)) {
        res = TRI_set_errno(TRI_ERROR_SYS_ERROR);
      }
      TRI_CLOSE(fd);

      if (res == TRI_ERROR_NO_ERROR) {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief return the filename component of a file (without path)
////////////////////////////////////////////////////////////////////////////////

std::string TRI_GetFilename(std::string const& filename) {
  size_t pos = filename.find_last_of("\\/:");
  if (pos == std::string::npos) {
    return filename;
  }
  return filename.substr(pos + 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the absolute path of a file
/// in contrast to realpath() this function can also be used to determine the
/// full path for files & directories that do not exist. realpath() would fail
/// for those cases.
/// It is the caller's responsibility to free the string created by this
/// function
////////////////////////////////////////////////////////////////////////////////

std::string TRI_GetAbsolutePath(std::string const& fileName,
                                std::string const& currentWorkingDirectory) {
  if (fileName.empty()) {
    return std::string();
  }

  // name is absolute if starts with either forward or backslash
  // file is also absolute if contains a colon
  bool isAbsolute = (fileName[0] == '/' || fileName[0] == '\\' ||
                     fileName.find(':') != std::string::npos);

  if (isAbsolute) {
    return fileName;
  }

  std::string result;

  if (!currentWorkingDirectory.empty()) {
    result.reserve(currentWorkingDirectory.size() + fileName.size() + 1);

    result.append(currentWorkingDirectory);
    if (currentWorkingDirectory.back() != '/') {
      result.push_back('/');
    }
    result.append(fileName);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the binary name without any path or suffix
////////////////////////////////////////////////////////////////////////////////

std::string TRI_BinaryName(char const* argv0) {
  auto result = TRI_Basename(argv0);
  if (result.size() > 4 && result.ends_with(".exe")) {
    result.resize(result.size() - 4);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locates the directory containing the program
////////////////////////////////////////////////////////////////////////////////

std::string TRI_LocateBinaryPath(char const* argv0) {
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
      std::vector<std::string> files = basics::StringUtils::split(pv, ':');

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

[[maybe_unused]] static bool CopyFileContents(int srcFD, int dstFD,
                                              TRI_read_t fileSize,
                                              std::string& error) {
  TRI_ASSERT(fileSize > 0);

  bool rc = true;

  if (::canUseSplice) {
    // Linux-specific file-copying code based on splice()
    // The splice() system call first appeared in Linux 2.6.17; library support
    // was added to glibc in version 2.5. libmusl also has bindings for it. so
    // we simply assume it is there on Linux.
    int splicePipe[2];
    ssize_t pipeSize = 0;
    long chunkSendRemain = fileSize;
    loff_t totalSentAlready = 0;

    if (pipe(splicePipe) != 0) {
      error = std::string("splice failed to create pipes: ") + strerror(errno);
      return false;
    }
    try {
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

        auto sent = splice(splicePipe[0], nullptr, dstFD, nullptr, pipeSize,
                           SPLICE_F_MORE | SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
        if (sent == -1) {
          auto e = errno;
          error = std::string("splice read failed: ") + strerror(e);
          if (e == EINVAL) {
            error +=
                ", please check if the target filesystem supports splicing, "
                "and if not, turn if off by restarting with option "
                "`--use-splice-syscall false`";
          }
          rc = false;
          break;
        }
        pipeSize -= sent;
        chunkSendRemain -= sent;
      }
    } catch (...) {
      // make sure we always close the pipes
      rc = false;
    }
    close(splicePipe[0]);
    close(splicePipe[1]);

    return rc;
  }

  // systems other than Linux use regular file-copying.
  // note: regular file copying will also be used on Linux
  // if we cannot use the splice() system call

  size_t bufferSize = std::min<size_t>(fileSize, 128 * 1024);
  char* buf = static_cast<char*>(TRI_Allocate(bufferSize));

  if (buf == nullptr) {
    error = "failed to allocate temporary buffer";
    return false;
  }

  try {
    TRI_read_t chunkRemain = fileSize;
    while (rc && (chunkRemain > 0)) {
      auto readChunk = static_cast<TRI_read_t>(
          std::min(bufferSize, static_cast<size_t>(chunkRemain)));
      TRI_read_return_t nRead = TRI_READ(srcFD, buf, readChunk);

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
        // write can write less data than requested. so we must go on writing
        // until we have written out all data
        auto nWritten = TRI_WRITE(dstFD, buf + writeOffset,
                                  static_cast<TRI_write_t>(writeRemaining));

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
  } catch (...) {
    // make sure we always close the buffer
    rc = false;
  }

  TRI_Free(buf);
  return rc;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copies the contents of a file
////////////////////////////////////////////////////////////////////////////////

bool TRI_CopyFile(std::string const& src, std::string const& dst,
                  std::string& error, struct stat* statbuf /*= nullptr*/) {
  int srcFD = open(src.c_str(), O_RDONLY);
  if (srcFD < 0) {
    error = "failed to open source file " + src + ": " + strerror(errno);
    return false;
  }
  int dstFD = open(dst.c_str(), O_EXCL | O_CREAT | O_NONBLOCK | O_WRONLY,
                   S_IRUSR | S_IWUSR);
  if (dstFD < 0) {
    close(srcFD);
    error = "failed to open destination file " + dst + ": " + strerror(errno);
    return false;
  }

  bool rc = true;
  try {
    struct stat localStat;

    if (statbuf == nullptr) {
      TRI_FSTAT(srcFD, &localStat);
      statbuf = &localStat;
    }
    TRI_ASSERT(statbuf != nullptr);

    size_t dsize = statbuf->st_size;

    if (dsize > 0) {
      // only copy non-empty files
      rc = CopyFileContents(srcFD, dstFD, dsize, error);
    }
    timeval times[2];
    memset(times, 0, sizeof(times));
    times[0].tv_sec = TRI_STAT_ATIME_SEC(*statbuf);
    times[1].tv_sec = TRI_STAT_MTIME_SEC(*statbuf);

    if (fchown(dstFD, -1 /*statbuf.st_uid*/, statbuf->st_gid) != 0) {
      error = std::string("failed to chown ") + dst + ": " + strerror(errno);
      // rc = false;  no, this is not fatal...
    }
    if (fchmod(dstFD, statbuf->st_mode) != 0) {
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
  } catch (...) {
    // make sure we always close file handles
    rc = false;
  }

  close(srcFD);
  close(dstFD);
  return rc;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copies the filesystem attributes of a file
////////////////////////////////////////////////////////////////////////////////

bool TRI_CopyAttributes(std::string const& srcItem, std::string const& dstItem,
                        std::string& error) {
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

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copies a symlink; the link target is not altered.
////////////////////////////////////////////////////////////////////////////////

bool TRI_CopySymlink(std::string const& srcItem, std::string const& dstItem,
                     std::string& error) {
  char buffer[PATH_MAX];
  auto rc = readlink(srcItem.c_str(), buffer, sizeof(buffer) - 1);
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
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a hard link; the link target is not altered.
////////////////////////////////////////////////////////////////////////////////

bool TRI_CreateHardlink(std::string const& existingFile,
                        std::string const& newFile, std::string& error) {
  int rc = link(existingFile.c_str(), newFile.c_str());

  if (rc == -1) {
    error = std::string("failed to create hard link ") + newFile + ": " +
            strerror(errno);
  }  // if

  return 0 == rc;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locates the home directory
///
/// Under windows there is no 'HOME' directory as such so getenv("HOME") may
/// return NULL -- which it does under windows.  A safer approach below
////////////////////////////////////////////////////////////////////////////////

std::string TRI_HomeDirectory() {
  char const* result = getenv("HOME");

  if (result == nullptr) {
    return std::string(".");
  }

  return std::string(result);
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

static ErrorCode mkDTemp(char* s, size_t /*bufferSize*/) {
  if (mkdtemp(s) != nullptr) {
    return TRI_ERROR_NO_ERROR;
  }
  return TRI_set_errno(TRI_ERROR_SYS_ERROR);
}

/// @brief the actual temp path used
static std::unique_ptr<char[]> SystemTempPath;

/// @brief user-defined temp path
static std::string UserTempPath;

class SystemTempPathSweeper {
  std::string _systemTempPath;

 public:
  SystemTempPathSweeper() : _systemTempPath() {}

  ~SystemTempPathSweeper() {
    if (!_systemTempPath.empty()) {
      // delete directory iff directory is empty
      TRI_RMDIR(_systemTempPath.c_str());
    }
  }

  void init(const char* path) { _systemTempPath = path; }
};

SystemTempPathSweeper SystemTempPathSweeperInstance;

// The TempPath is set but not created
void TRI_SetTempPath(std::string const& temp) { UserTempPath = temp; }

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
    while (system.find(TRI_DIR_SEPARATOR_STR TRI_DIR_SEPARATOR_STR) !=
           std::string::npos) {
      system = StringUtils::replace(
          system, std::string(TRI_DIR_SEPARATOR_STR TRI_DIR_SEPARATOR_STR),
          std::string(TRI_DIR_SEPARATOR_STR));
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

      auto res = TRI_ERROR_NO_ERROR;
      if (!UserTempPath.empty()) {
        // --temp.path was specified
        if (TRI_IsDirectory(system.c_str())) {
          // temp directory already exists. now simply use it
          break;
        }

        res = 0 == TRI_MKDIR(UserTempPath.c_str(), 0700)
                  ? TRI_ERROR_NO_ERROR
                  : TRI_set_errno(TRI_ERROR_SYS_ERROR);
      } else {
        // no --temp.path was specified
        // fill template and create directory
        tries = 9;

        // create base directories of the new directory (but ignore any failures
        // if they already exist. if this fails, the following mkDTemp will
        // either succeed or fail and return an error
        try {
          long systemError;
          std::string systemErrorStr;
          std::string baseDirectory = TRI_Dirname(SystemTempPath.get());
          if (baseDirectory.size() <= 1) {
            baseDirectory = SystemTempPath.get();
          }
          // create base directory if it does not yet exist
          TRI_CreateRecursiveDirectory(baseDirectory.data(), systemError,
                                       systemErrorStr);
        } catch (...) {
        }

        // fill template string (XXXXXX) with some pseudo-random value and
        // create the directory
        res = mkDTemp(SystemTempPath.get(), system.size() + 1);
      }

      if (res == TRI_ERROR_NO_ERROR) {
        break;
      }

      // directory could not be created
      // this may be a race, a permissions problem or something else
      if (++tries >= 10) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
        LOG_TOPIC("6656e", ERR, arangodb::Logger::FIXME)
            << "UserTempPath: " << UserTempPath << ", system: " << system
            << ", user temp path exists: "
            << TRI_IsDirectory(UserTempPath.c_str()) << ", res: " << res
            << ", SystemTempPath: " << SystemTempPath.get();
#endif
        LOG_TOPIC("88da0", FATAL, arangodb::Logger::FIXME)
            << "failed to create a temporary directory - giving up!";
        FATAL_ERROR_ABORT();
      }
      // sleep for a random amout of time and try again soon
      // with this, we try to avoid races between multiple processes
      // that try to create temp directories at the same time
      std::this_thread::sleep_for(std::chrono::milliseconds(
          5 + RandomGenerator::interval(uint64_t(20))));
    }

    SystemTempPathSweeperInstance.init(SystemTempPath.get());
  }

  return std::string(path);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a temporary file name
////////////////////////////////////////////////////////////////////////////////

ErrorCode TRI_GetTempName(char const* directory, std::string& result,
                          bool createFile, long& systemError,
                          std::string& errorMessage) {
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

  auto res =
      TRI_CreateRecursiveDirectory(dir.c_str(), systemError, errorMessage);

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

    std::string tempName =
        "tmp-" + std::to_string(pid) + '-' +
        std::to_string(RandomGenerator::interval(UINT32_MAX));

    std::string filename =
        arangodb::basics::FileUtils::buildFilename(dir, tempName);

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

std::string TRI_LocateInstallDirectory(char const* argv0,
                                       char const* binaryPath) {
  std::string thisPath = TRI_LocateBinaryPath(argv0);
  std::string ret = TRI_GetInstallRoot(thisPath, binaryPath);
  if (ret.length() != 1 || ret != TRI_DIR_SEPARATOR_STR) {
    ret += TRI_DIR_SEPARATOR_CHAR;
  }
  return ret;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locates the configuration directory
///
/// Will always end in a directory separator.
////////////////////////////////////////////////////////////////////////////////

#if defined(_SYSCONFDIR_)

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

std::string TRI_LocateConfigDirectory(char const*) {
  return LocateConfigDirectoryEnv();
}

#endif

bool TRI_PathIsAbsolute(std::string const& path) {
  return path.starts_with('/');
}

/// @brief return the amount of total and free disk space for the given path
arangodb::Result TRI_GetDiskSpaceInfo(std::string const& path,
                                      uint64_t& totalSpace,
                                      uint64_t& freeSpace) {
  struct statvfs stat;

  if (statvfs(path.c_str(), &stat) == -1) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    return {TRI_errno(), TRI_last_error()};
  }

  auto const factor = static_cast<uint64_t>(stat.f_bsize);

  totalSpace = factor * static_cast<uint64_t>(stat.f_blocks);

  // sbuf.bfree is total free space available to root
  // sbuf.bavail is total free space available to unprivileged user
  // sbuf.bavail <= sbuf.bfree ... pick correct based upon effective user id
  if (geteuid()) {
    // non-zero user is unprivileged, or -1 if error. take more conservative
    // size
    freeSpace = factor * static_cast<uint64_t>(stat.f_bavail);
  } else {
    // root user can access all disk space
    freeSpace = factor * static_cast<uint64_t>(stat.f_bfree);
  }
  return {};
}

/// @brief return the amount of total and free inodes for the given path.
/// always returns 0 on Windows
arangodb::Result TRI_GetINodesInfo(std::string const& path,
                                   uint64_t& totalINodes,
                                   uint64_t& freeINodes) {
  struct statvfs stat;

  if (statvfs(path.c_str(), &stat) == -1) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    return {TRI_errno(), TRI_last_error()};
  }
  totalINodes = static_cast<uint64_t>(stat.f_files);
  freeINodes = static_cast<uint64_t>(stat.f_ffree);
  return {};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads an environment variable. returns false if env var was not set.
/// if env var was set, returns env variable value in "value" and returns true.
////////////////////////////////////////////////////////////////////////////////

bool TRI_GETENV(char const* which, std::string& value) {
  char const* v = getenv(which);

  if (v == nullptr) {
    return false;
  }
  value.clear();
  value = v;
  return true;
}
