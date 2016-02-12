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

#ifdef TRI_HAVE_DIRENT_H
#include <dirent.h>
#endif

#ifdef TRI_HAVE_DIRECT_H
#include <direct.h>
#endif

#ifdef TRI_HAVE_SYS_FILE_H
#include <sys/file.h>
#endif

#include "Basics/Mutex.h"
#include "Basics/MutexLocker.h"
#include "Basics/conversions.h"
#include "Basics/hashes.h"
#include "Basics/locks.h"
#include "Basics/Logger.h"
#include "Basics/random.h"
#include "Basics/StringBuffer.h"
#include "Basics/threads.h"
#include "Basics/tri-strings.h"

#ifdef _WIN32
#include <tchar.h>
#endif

using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief read buffer size (used for bulk file reading)
////////////////////////////////////////////////////////////////////////////////

#define READBUFFER_SIZE 8192

////////////////////////////////////////////////////////////////////////////////
/// @brief a static buffer of zeros, used to initialize files
////////////////////////////////////////////////////////////////////////////////

static char NullBuffer[4096];

////////////////////////////////////////////////////////////////////////////////
/// @brief already initialized
////////////////////////////////////////////////////////////////////////////////

static bool Initialized = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief user-defined temporary path
////////////////////////////////////////////////////////////////////////////////

static std::string TempPath;

////////////////////////////////////////////////////////////////////////////////
/// @brief names of blocking files
////////////////////////////////////////////////////////////////////////////////

static TRI_vector_string_t FileNames;

////////////////////////////////////////////////////////////////////////////////
/// @brief descriptors of blocking files
////////////////////////////////////////////////////////////////////////////////

static TRI_vector_t FileDescriptors;

////////////////////////////////////////////////////////////////////////////////
/// @brief lock for protected access to vector FileNames
////////////////////////////////////////////////////////////////////////////////

static TRI_read_write_lock_t FileNamesLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief removes trailing path separators from path
///
/// @note path will be modified in-place
////////////////////////////////////////////////////////////////////////////////

static void RemoveTrailingSeparator(char* path) {
  char const* s;
  size_t n;

  n = strlen(path);
  s = path;

  if (n > 0) {
    char* p = path + n - 1;

    while (p > s && (*p == TRI_DIR_SEPARATOR_CHAR || *p == '/')) {
      *p = '\0';
      --p;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalizes path
///
/// @note path will be modified in-place
////////////////////////////////////////////////////////////////////////////////

static void NormalizePath(char* path) {
  RemoveTrailingSeparator(path);

  char* p = path;
  char* e = path + strlen(p);

  for (; p < e; ++p) {
    if (*p == TRI_DIR_SEPARATOR_CHAR || *p == '/') {
      *p = TRI_DIR_SEPARATOR_CHAR;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief linear search of the giving element
///
/// @return index of the element in the vector
///         -1 when the element was not found
////////////////////////////////////////////////////////////////////////////////

static ssize_t LookupElementVectorString(TRI_vector_string_t* vector,
                                         char const* element) {
  int idx = -1;

  TRI_ReadLockReadWriteLock(&FileNamesLock);

  for (size_t i = 0; i < vector->_length; i++) {
    if (TRI_EqualString(element, vector->_buffer[i])) {
      // theoretically this might cap the value of i, but it is highly unlikely
      idx = (ssize_t)i;
      break;
    }
  }

  TRI_ReadUnlockReadWriteLock(&FileNamesLock);
  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief callback function for removing all locked files by a process
////////////////////////////////////////////////////////////////////////////////

static void RemoveAllLockedFiles(void) {
  TRI_WriteLockReadWriteLock(&FileNamesLock);

  for (size_t i = 0; i < FileNames._length; i++) {
#ifdef TRI_HAVE_WIN32_FILE_LOCKING
    HANDLE fd = *(HANDLE*)TRI_AtVector(&FileDescriptors, i);
    CloseHandle(fd);
#else
    int fd = *(int*)TRI_AtVector(&FileDescriptors, i);
    TRI_CLOSE(fd);
#endif

    TRI_UnlinkFile(FileNames._buffer[i]);
  }

  TRI_DestroyVectorString(&FileNames);
  TRI_DestroyVector(&FileDescriptors);

  TRI_WriteUnlockReadWriteLock(&FileNamesLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes some structures which are needed by the file functions
////////////////////////////////////////////////////////////////////////////////

static void InitializeLockFiles(void) {
  if (Initialized) {
    return;
  }

  TRI_InitVectorString(&FileNames, TRI_CORE_MEM_ZONE);

#ifdef TRI_HAVE_WIN32_FILE_LOCKING
  TRI_InitVector(&FileDescriptors, TRI_CORE_MEM_ZONE, sizeof(HANDLE));
#else
  TRI_InitVector(&FileDescriptors, TRI_CORE_MEM_ZONE, sizeof(int));
#endif

  TRI_InitReadWriteLock(&FileNamesLock);

  atexit(&RemoveAllLockedFiles);
  Initialized = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lists the directory tree
////////////////////////////////////////////////////////////////////////////////

static void ListTreeRecursively(char const* full, char const* path,
                                TRI_vector_string_t* result) {
  size_t j;
  std::vector<std::string> dirs = TRI_FilesDirectory(full);

  for (j = 0; j < 2; ++j) {
    for (auto const& filename : dirs) {
      char* newfull = TRI_Concatenate2File(full, filename.c_str());
      char* newpath;

      if (*path) {
        newpath = TRI_Concatenate2File(path, filename.c_str());
      } else {
        newpath = TRI_DuplicateString(filename.c_str());
      }

      if (j == 0) {
        if (TRI_IsDirectory(newfull)) {
          TRI_PushBackVectorString(result, newpath);

          if (!TRI_IsSymbolicLink(newfull)) {
            ListTreeRecursively(newfull, newpath, result);
          }
        } else {
          TRI_FreeString(TRI_CORE_MEM_ZONE, newpath);
        }
      } else {
        if (!TRI_IsDirectory(newfull)) {
          TRI_PushBackVectorString(result, newpath);
        } else {
          TRI_FreeString(TRI_CORE_MEM_ZONE, newpath);
        }
      }

      TRI_FreeString(TRI_CORE_MEM_ZONE, newfull);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locates a environment given configuration directory
////////////////////////////////////////////////////////////////////////////////

static char* LocateConfigDirectoryEnv(void) {
  char const* v = getenv("ARANGODB_CONFIG_PATH");

  if (v == nullptr) {
    return nullptr;
  }

  char* r = TRI_DuplicateString(v);

  NormalizePath(r);

  TRI_AppendString(&r, TRI_DIR_SEPARATOR_STR);

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
  } else {
    TRI_stat_t stbuf;
    size_t len;
    int res;

    len = strlen(path);

    // path must not end with a \ on Windows, other stat() will return -1
    if (len > 0 && path[len - 1] == TRI_DIR_SEPARATOR_CHAR) {
      char* copy = TRI_DuplicateString(TRI_CORE_MEM_ZONE, path);

      if (copy == nullptr) {
        return false;
      }

      // remove trailing slash
      RemoveTrailingSeparator(copy);

      res = TRI_STAT(copy, &stbuf);
      TRI_FreeString(TRI_CORE_MEM_ZONE, copy);
    } else {
      res = TRI_STAT(path, &stbuf);
    }

    return res == 0;
  }
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
  res = _chmod(path, static_cast<int>(mode));
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

bool TRI_CreateRecursiveDirectory(char const* path, long& systemError,
                                  std::string& systemErrorStr) {
  char* copy;
  char* p;
  char* s;
  int res;

  res = TRI_ERROR_NO_ERROR;
  p = s = copy = TRI_DuplicateString(path);

  while (*p != '\0') {
    if (*p == TRI_DIR_SEPARATOR_CHAR) {
      if (p - s > 0) {
#ifdef _WIN32
        // Don't try to create the drive letter as directory:
        if ((p - copy == 2) && (s[1] == ':')) {
          s = p + 1;
          continue;
        }
#endif
        *p = '\0';
        res = TRI_CreateDirectory(copy, systemError, systemErrorStr);
        if ((res == TRI_ERROR_FILE_EXISTS) || (res == TRI_ERROR_NO_ERROR)) {
          res = TRI_ERROR_NO_ERROR;
          *p = TRI_DIR_SEPARATOR_CHAR;
          s = p + 1;
        } else {
          break;
        }
      }
    }
    p++;
  }

  if (((res == TRI_ERROR_FILE_EXISTS) || (res == TRI_ERROR_NO_ERROR)) &&
      (p - s > 0)) {
    res = TRI_CreateDirectory(copy, systemError, systemErrorStr);
    if (res == TRI_ERROR_FILE_EXISTS) {
      res = TRI_ERROR_NO_ERROR;
    }
  }

  TRI_Free(TRI_CORE_MEM_ZONE, copy);

  return res == TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a directory
////////////////////////////////////////////////////////////////////////////////

int TRI_CreateDirectory(char const* path, long& systemError,
                        std::string& systemErrorStr) {
  TRI_ERRORBUF;
  int res;

  // reset error flag
  TRI_set_errno(TRI_ERROR_NO_ERROR);

  res = TRI_MKDIR(path, 0777);

  if (res == TRI_ERROR_NO_ERROR) {
    return res;
  }

  // check errno
  TRI_SYSTEM_ERROR();
  res = errno;
  if (res != TRI_ERROR_NO_ERROR) {
    systemErrorStr = std::string("Failed to create directory [") + path + "] " +
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
    LOG(TRACE) << "cannot remove directory '" << filename << "': " << TRI_LAST_ERROR_STR;
    return TRI_set_errno(TRI_ERROR_SYS_ERROR);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a directory recursively
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveDirectory(char const* filename) {
  if (TRI_IsDirectory(filename)) {
    int res;

    LOG(TRACE) << "removing directory '" << filename << "'";

    res = TRI_ERROR_NO_ERROR;
    std::vector<std::string> files = TRI_FilesDirectory(filename);
    for (auto const& dir : files) {
      char* full;
      int subres;

      full = TRI_Concatenate2File(filename, dir.c_str());

      subres = TRI_RemoveDirectory(full);
      TRI_FreeString(TRI_CORE_MEM_ZONE, full);

      if (subres != TRI_ERROR_NO_ERROR) {
        res = subres;
      }
    }

    if (res == TRI_ERROR_NO_ERROR) {
      res = TRI_RemoveEmptyDirectory(filename);
    }

    return res;
  } else if (TRI_ExistsFile(filename)) {
    LOG(TRACE) << "removing file '" << filename << "'";

    return TRI_UnlinkFile(filename);
  } else {
    LOG(TRACE) << "attempt to remove non-existing file/directory '" << filename << "'";

    return TRI_ERROR_NO_ERROR;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the dirname
////////////////////////////////////////////////////////////////////////////////

char* TRI_Dirname(char const* path) {
  size_t n;
  size_t m;
  char const* p;

  n = strlen(path);
  m = 0;

  if (1 < n) {
    if (path[n - 1] == TRI_DIR_SEPARATOR_CHAR) {
      m = 1;
    }
  }

  if (n == 0) {
    return TRI_DuplicateString(".");
  } else if (n == 1 && *path == TRI_DIR_SEPARATOR_CHAR) {
    return TRI_DuplicateString(TRI_DIR_SEPARATOR_STR);
  } else if (n - m == 1 && *path == '.') {
    return TRI_DuplicateString(".");
  } else if (n - m == 2 && path[0] == '.' && path[1] == '.') {
    return TRI_DuplicateString("..");
  }

  for (p = path + (n - m - 1); path < p; --p) {
    if (*p == TRI_DIR_SEPARATOR_CHAR) {
      break;
    }
  }

  if (path == p) {
    if (*p == TRI_DIR_SEPARATOR_CHAR) {
      return TRI_DuplicateString(TRI_DIR_SEPARATOR_STR);
    } else {
      return TRI_DuplicateString(".");
    }
  }

  n = p - path;

  return TRI_DuplicateString(path, n);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the basename
////////////////////////////////////////////////////////////////////////////////

char* TRI_Basename(char const* path) {
  size_t n;

  n = strlen(path);

  if (1 < n) {
    if (path[n - 1] == TRI_DIR_SEPARATOR_CHAR || path[n - 1] == '/') {
      n -= 1;
    }
  }

  if (n == 0) {
    return TRI_DuplicateString("");
  } else if (n == 1) {
    if (*path == TRI_DIR_SEPARATOR_CHAR || *path == '/') {
      return TRI_DuplicateString(TRI_DIR_SEPARATOR_STR);
    } else {
      return TRI_DuplicateString(path, n);
    }
  } else {
    char const* p;

    for (p = path + (n - 2); path < p; --p) {
      if (*p == TRI_DIR_SEPARATOR_CHAR || *p == '/') {
        break;
      }
    }

    if (path == p) {
      if (*p == TRI_DIR_SEPARATOR_CHAR || *p == '/') {
        return TRI_DuplicateString(path + 1, n - 1);
      } else {
        return TRI_DuplicateString(path, n);
      }
    } else {
      n -= p - path;

      return TRI_DuplicateString(p + 1, n - 1);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a filename
////////////////////////////////////////////////////////////////////////////////

char* TRI_Concatenate2File(char const* path, char const* name) {
  size_t len = strlen(path);
  char* result;

  if (0 < len) {
    result = TRI_DuplicateString(path);
    RemoveTrailingSeparator(result);

    TRI_AppendString(&result, TRI_DIR_SEPARATOR_STR);
  } else {
    result = TRI_DuplicateString("");
  }

  TRI_AppendString(&result, name);
  NormalizePath(result);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a filename
////////////////////////////////////////////////////////////////////////////////

char* TRI_Concatenate3File(char const* path1, char const* path2,
                           char const* name) {
  char* tmp;
  char* result;

  tmp = TRI_Concatenate2File(path1, path2);
  result = TRI_Concatenate2File(tmp, name);

  TRI_FreeString(TRI_CORE_MEM_ZONE, tmp);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a list of files in path
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_WIN32_LIST_FILES

std::vector<std::string> TRI_FilesDirectory(char const* path) {
  std::vector<std::string> result;

  struct _finddata_t fd;
  intptr_t handle;
  char* filter;

  filter = TRI_Concatenate2String(path, "\\*");
  if (!filter) {
    return result;
  }

  handle = _findfirst(filter, &fd);
  TRI_FreeString(TRI_CORE_MEM_ZONE, filter);

  if (handle == -1) {
    return result;
  }

  do {
    if (strcmp(fd.name, ".") != 0 && strcmp(fd.name, "..") != 0) {
      result.emplace_back(fd.name);
    }
  } while (_findnext(handle, &fd) != -1);

  _findclose(handle);

  return result;
}

#else

std::vector<std::string> TRI_FilesDirectory(char const* path) {
  std::vector<std::string> result;

  DIR* d;
  struct dirent* de;

  d = opendir(path);

  if (d == 0) {
    return result;
  }

  de = readdir(d);

  while (de != 0) {
    if (strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0) {
      result.emplace_back(de->d_name);
    }

    de = readdir(d);
  }

  closedir(d);

  return result;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief lists the directory tree including files and directories
////////////////////////////////////////////////////////////////////////////////

TRI_vector_string_t TRI_FullTreeDirectory(char const* path) {
  TRI_vector_string_t result;

  TRI_InitVectorString(&result, TRI_CORE_MEM_ZONE);

  TRI_PushBackVectorString(&result, TRI_DuplicateString(""));
  ListTreeRecursively(path, "", &result);

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
  moveResult = MoveFileExA(old, filename,
                           MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING);

  if (!moveResult) {
    TRI_SYSTEM_ERROR();

    if (systemError != nullptr) {
      *systemError = errno;
    }
    if (systemErrorStr != nullptr) {
      *systemErrorStr = windowsErrorBuf;
    }
    LOG(TRACE) << "cannot rename file from '" << old << "' to '" << filename << "': " << errno << " - " << windowsErrorBuf;
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
    LOG(TRACE) << "cannot rename file from '" << old << "' to '" << filename << "': " << TRI_LAST_ERROR_STR;
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
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG(TRACE) << "cannot unlink file '" << filename << "': " << TRI_LAST_ERROR_STR;
    int e = TRI_errno();
    if (e == ENOENT) {
      return TRI_ERROR_FILE_NOT_FOUND;
    }
    return e;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads into a buffer from a file
////////////////////////////////////////////////////////////////////////////////

bool TRI_ReadPointer(int fd, void* buffer, size_t length) {
  char* ptr = static_cast<char*>(buffer);

  while (0 < length) {
    ssize_t n = TRI_READ(fd, ptr, (unsigned int)length);

    if (n < 0) {
      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      LOG(ERR) << "cannot read: " << TRI_LAST_ERROR_STR;
      return false;
    } else if (n == 0) {
      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      LOG(ERR) << "cannot read, end-of-file";
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
    ssize_t n = TRI_WRITE(fd, ptr, (TRI_write_t)length);

    if (n < 0) {
      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      LOG(ERR) << "cannot write: " << TRI_LAST_ERROR_STR;
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

char* TRI_SlurpFile(TRI_memory_zone_t* zone, char const* filename,
                    size_t* length) {
  TRI_string_buffer_t result;
  int fd;

  fd = TRI_OPEN(filename, O_RDONLY | TRI_O_CLOEXEC);

  if (fd == -1) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    return nullptr;
  }

  TRI_InitStringBuffer(&result, zone);

  while (true) {
    int res;
    ssize_t n;

    res = TRI_ReserveStringBuffer(&result, READBUFFER_SIZE);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_CLOSE(fd);
      TRI_AnnihilateStringBuffer(&result);

      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      return nullptr;
    }

    n = TRI_READ(fd, (void*)TRI_EndStringBuffer(&result), READBUFFER_SIZE);

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
  BOOL r;
  DWORD len;
  HANDLE fd;
  OVERLAPPED ol;
  TRI_pid_t pid;
  char* buf;
  char* fn;
  int res;

  InitializeLockFiles();

  if (0 <= LookupElementVectorString(&FileNames, filename)) {
    return TRI_ERROR_NO_ERROR;
  }

  fd = CreateFile(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                  FILE_ATTRIBUTE_NORMAL, NULL);

  if (fd == INVALID_HANDLE_VALUE) {
    TRI_SYSTEM_ERROR();
    LOG(ERR) << "cannot create Lockfile '" << filename << "': " << TRI_GET_ERRORBUF;
    return TRI_set_errno(TRI_ERROR_SYS_ERROR);
  }

  pid = TRI_CurrentProcessId();
  buf = TRI_StringUInt32(pid);

  r = WriteFile(fd, buf, (unsigned int)strlen(buf), &len, NULL);

  if (!r || len != strlen(buf)) {
    TRI_SYSTEM_ERROR();
    LOG(ERR) << "cannot write Lockfile '" << filename << "': " << TRI_GET_ERRORBUF;
    res = TRI_set_errno(TRI_ERROR_SYS_ERROR);

    TRI_FreeString(TRI_CORE_MEM_ZONE, buf);

    if (r) {
      CloseHandle(fd);
    }

    TRI_UNLINK(filename);

    return res;
  }

  TRI_FreeString(TRI_CORE_MEM_ZONE, buf);

  memset(&ol, 0, sizeof(ol));
  r = LockFileEx(fd, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 0,
                 128, &ol);

  if (!r) {
    TRI_SYSTEM_ERROR();
    LOG(ERR) << "cannot set Lockfile status '" << filename << "': " << TRI_GET_ERRORBUF;
    res = TRI_set_errno(TRI_ERROR_SYS_ERROR);

    CloseHandle(fd);
    TRI_UNLINK(filename);

    return res;
  }

  fn = TRI_DuplicateString(filename);

  TRI_WriteLockReadWriteLock(&FileNamesLock);
  TRI_PushBackVectorString(&FileNames, fn);
  TRI_PushBackVector(&FileDescriptors, &fd);
  TRI_WriteUnlockReadWriteLock(&FileNamesLock);

  return TRI_ERROR_NO_ERROR;
}

#else

int TRI_CreateLockFile(char const* filename) {
  InitializeLockFiles();

  if (0 <= LookupElementVectorString(&FileNames, filename)) {
    return TRI_ERROR_NO_ERROR;
  }

  int fd = TRI_CREATE(filename, O_CREAT | O_EXCL | O_RDWR | TRI_O_CLOEXEC,
                      S_IRUSR | S_IWUSR);

  if (fd == -1) {
    return TRI_set_errno(TRI_ERROR_SYS_ERROR);
  }

  TRI_pid_t pid = TRI_CurrentProcessId();
  char* buf = TRI_StringUInt32(pid);

  int rv = TRI_WRITE(fd, buf, (TRI_write_t)strlen(buf));

  if (rv == -1) {
    int res = TRI_set_errno(TRI_ERROR_SYS_ERROR);

    TRI_FreeString(TRI_CORE_MEM_ZONE, buf);

    TRI_CLOSE(fd);
    TRI_UNLINK(filename);

    return res;
  }

  TRI_FreeString(TRI_CORE_MEM_ZONE, buf);
  
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

  char* fn = TRI_DuplicateString(filename);

  TRI_WriteLockReadWriteLock(&FileNamesLock);
  TRI_PushBackVectorString(&FileNames, fn);
  TRI_PushBackVector(&FileDescriptors, &fd);
  TRI_WriteUnlockReadWriteLock(&FileNamesLock);

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

  int fd = TRI_OPEN(filename, O_RDONLY | TRI_O_CLOEXEC);

  if (fd < 0) {
    return TRI_ERROR_NO_ERROR;
  }

  char buffer[128];
  memset(buffer, 0,
         sizeof(buffer));  // not really necessary, but this shuts up valgrind
  ssize_t n = TRI_READ(fd, buffer, sizeof(buffer));

  if (n < 0) {
    TRI_CLOSE(fd);
    return TRI_ERROR_NO_ERROR;
  }

  // pid too long
  if (n == sizeof(buffer)) {
    TRI_CLOSE(fd);
    return TRI_ERROR_NO_ERROR;
  }

  // file empty
  if (n == 0) {
    TRI_CLOSE(fd);
    return TRI_ERROR_NO_ERROR;
  }

  uint32_t fc = TRI_UInt32String(buffer);
  int res = TRI_errno();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_CLOSE(fd);
    return TRI_ERROR_NO_ERROR;
  }

  TRI_pid_t pid = fc;

  if (kill(pid, 0) == -1) {
    TRI_CLOSE(fd);
    return TRI_ERROR_NO_ERROR;
  }
  struct flock lock;

  lock.l_start = 0;
  lock.l_len = 0;
  lock.l_type = F_WRLCK;
  lock.l_whence = SEEK_SET;
  // try to lock pid file
  int canLock = fcntl(fd, F_SETLK, &lock);  // Exclusive (write) lock

  // file was not yet locken; could be locked
  if (canLock == 0) {
    lock.l_type = F_UNLCK;
    fcntl(fd, F_GETLK, &lock);
    TRI_CLOSE(fd);

    return TRI_ERROR_NO_ERROR;
  }

  canLock = errno;

  TRI_CLOSE(fd);

  LOG(WARN) << "fcntl on lockfile '" << filename << "' failed: " << TRI_errno_string(canLock);

  return TRI_ERROR_ARANGO_DATADIR_LOCKED;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a lock file based on the PID
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_WIN32_FILE_LOCKING

int TRI_DestroyLockFile(char const* filename) {
  InitializeLockFiles();
  ssize_t n = LookupElementVectorString(&FileNames, filename);

  if (n < 0) {
    return TRI_ERROR_NO_ERROR;
  }

  HANDLE fd = *(HANDLE*)TRI_AtVector(&FileDescriptors, n);

  CloseHandle(fd);

  TRI_UnlinkFile(filename);

  TRI_WriteLockReadWriteLock(&FileNamesLock);
  TRI_RemoveVectorString(&FileNames, n);
  TRI_RemoveVector(&FileDescriptors, n);
  TRI_WriteUnlockReadWriteLock(&FileNamesLock);

  return TRI_ERROR_NO_ERROR;
}

#else

int TRI_DestroyLockFile(char const* filename) {
  InitializeLockFiles();
  ssize_t n = LookupElementVectorString(&FileNames, filename);

  if (n < 0) {
    return TRI_ERROR_NO_ERROR;
  }

  int fd = TRI_OPEN(filename, O_RDWR | TRI_O_CLOEXEC);

  if (fd < 0) {
    return TRI_ERROR_NO_ERROR;
  }

  struct flock lock;

  lock.l_start = 0;
  lock.l_len = 0;
  lock.l_type = F_UNLCK;
  lock.l_whence = SEEK_SET;
  // relesae the lock
  int res = fcntl(fd, F_GETLK, &lock);
  TRI_CLOSE(fd);

  if (res == 0) {
    TRI_UnlinkFile(filename);
  }

  // close lock file descriptor
  fd = *(int*)TRI_AtVector(&FileDescriptors, n);
  TRI_CLOSE(fd);

  TRI_WriteLockReadWriteLock(&FileNamesLock);
  TRI_RemoveVectorString(&FileNames, n);
  TRI_RemoveVector(&FileDescriptors, n);
  TRI_WriteUnlockReadWriteLock(&FileNamesLock);

  return res;
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
        return TRI_DuplicateString(TRI_UNKNOWN_MEM_ZONE, fileName);
      }
    }
  }

  // ...........................................................................
  // The fileName itself was not absolute, so we attempt to amalagmate the
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
    return nullptr;
  }

  // ...........................................................................
  // Determine the total legnth of the new string
  // ...........................................................................

  cwdLength = strlen(currentWorkingDirectory);
  fileLength = strlen(fileName);

  if (currentWorkingDirectory[cwdLength - 1] == '\\' ||
      currentWorkingDirectory[cwdLength - 1] == '/') {
    // we do not require a backslash
    result = static_cast<char*>(
        TRI_Allocate(TRI_UNKNOWN_MEM_ZONE,
                     (cwdLength + fileLength + 1) * sizeof(char), false));
    if (result == nullptr) {
      return nullptr;
    }
    memcpy(result, currentWorkingDirectory, cwdLength);
    memcpy(result + cwdLength, fileName, fileLength);
    result[cwdLength + fileLength] = '\0';
  } else {
    // we do require a backslash
    result = static_cast<char*>(
        TRI_Allocate(TRI_UNKNOWN_MEM_ZONE,
                     (cwdLength + fileLength + 2) * sizeof(char), false));
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
    return TRI_DuplicateString(TRI_UNKNOWN_MEM_ZONE, file);
  }

  if (cwd == nullptr || *cwd == '\0') {
    // no absolute path given, must abort
    return nullptr;
  }

  cwdLength = strlen(cwd);
  TRI_ASSERT(cwdLength > 0);

  char* result = static_cast<char*>(
      TRI_Allocate(TRI_UNKNOWN_MEM_ZONE,
                   (cwdLength + strlen(file) + 2) * sizeof(char), false));

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
  char* name;
  char* p;
  char* e;

  name = TRI_Basename(argv0);

  p = name;
  e = name + strlen(name);

  if (p < e - 4) {
    if (TRI_CaseEqualString(e - 4, ".exe")) {
      e[-4] = '\0';
    }
  }

  std::string result = name;
  TRI_FreeString(TRI_CORE_MEM_ZONE, name);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locates the directory containing the program
////////////////////////////////////////////////////////////////////////////////

std::string TRI_LocateBinaryPath(char const* argv0) {
  char const* p;
  char* binaryPath = nullptr;

#if _WIN32

  if (argv0 == nullptr) {
    char buff[4096];
    int res = GetModuleFileName(NULL, buff, sizeof(buff));

    if (res != 0) {
      buff[4095] = '\0';

      char* q = buff + res;

      while (buff < q) {
        if (*q == '\\' || *q == '/') {
          *q = '\0';
          break;
        }

        --q;
      }

      return std::string(buff);
    }

    return std::string();
  }

#endif

  // check if name contains a '/' ( or '\' for windows)
  p = argv0;

  for (; *p && *p != TRI_DIR_SEPARATOR_CHAR; ++p) {
  }

  // contains a path
  if (*p) {
    char* dir = TRI_Dirname(argv0);

    if (dir == nullptr) {
      binaryPath = TRI_DuplicateString("");
      TRI_FreeString(TRI_CORE_MEM_ZONE, dir);
    }
  }

  // check PATH variable
  else {
    p = getenv("PATH");

    if (p != nullptr) {
      TRI_vector_string_t files;
      size_t i;

      files = TRI_SplitString(p, ':');

      for (i = 0; i < files._length; ++i) {
        char* full;
        char* prefix;

        prefix = files._buffer[i];

        if (*prefix) {
          full = TRI_Concatenate2File(prefix, argv0);
        } else {
          full = TRI_Concatenate2File(".", argv0);
        }

        if (TRI_ExistsFile(full)) {
          TRI_FreeString(TRI_CORE_MEM_ZONE, full);
          binaryPath = TRI_DuplicateString(files._buffer[i]);
          break;
        }

        TRI_FreeString(TRI_CORE_MEM_ZONE, full);
      }

      TRI_DestroyVectorString(&files);
    }
  }

  std::string result = (binaryPath == nullptr) ? "" : binaryPath;

  if (binaryPath != nullptr) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, binaryPath);
  }

  return result;
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
#define C128 131072
    TRI_write_t nRead;
    TRI_read_t chunkRemain = fileSize;
    char* buf =
        static_cast<char*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, C128, false));

    if (buf == nullptr) {
      error = "failed to allocate temporary buffer";
      rc = false;
    }
    while (rc && (chunkRemain > 0)) {
      TRI_read_t readChunk;
      if (chunkRemain > C128) {
        readChunk = C128;
      } else {
        readChunk = chunkRemain;
      }

      nRead = TRI_READ(srcFD, buf, readChunk);
      if (nRead < 1) {
        error = std::string("failed to read a chunk: ") + strerror(errno);
        break;
      }

      if ((TRI_read_t)TRI_WRITE(dstFD, buf, nRead) != nRead) {
        rc = false;
        break;
      }

      chunkRemain -= nRead;
    }

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, buf);
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
  bool rc = CopyFile(src.c_str(), dst.c_str(), false) != 0;
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
  ssize_t rc;
  rc = readlink(srcItem.c_str(), buffer, sizeof(buffer));
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

char* TRI_HomeDirectory() {
  char const* drive = getenv("HOMEDRIVE");
  char const* path = getenv("HOMEPATH");
  char* result;

  if (drive != 0 && path != 0) {
    result = TRI_Concatenate2String(drive, path);
  } else {
    result = TRI_DuplicateString("");
  }

  return result;
}

#else

char* TRI_HomeDirectory() {
  char const* result = getenv("HOME");

  if (result == 0) {
    result = ".";
  }

  return TRI_DuplicateString(TRI_CORE_MEM_ZONE, result);
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
  buffer = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, (size_t)bufferSize, false);

  if (buffer == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  fin = fopen(path, "rb");

  if (fin == nullptr) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, buffer);

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
    } else if (sizeRead <= 0) {
      break;
    }
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, buffer);

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

static char const* TRI_ApplicationName = nullptr;

void TRI_SetApplicationName(char const* name) {
  TRI_ASSERT(strlen(name) <= 13);
  TRI_ApplicationName = name;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the system's temporary path
////////////////////////////////////////////////////////////////////////////////

#ifndef _WIN32

static std::unique_ptr<char[]> SystemTempPath;

static void SystemTempPathCleaner(void) {
  char* path = SystemTempPath.get();

  if (path != nullptr) {
    rmdir(path);
  }
}

std::string TRI_GetTempPath() {
  char* path = SystemTempPath.get();

  if (path == nullptr) {
    std::string system = "";
    char const* v = getenv("TMPDIR");

    // create the template
    if (v == nullptr || *v == '\0') {
      system = "/tmp/";
    } else if (v[strlen(v) - 1] == '/') {
      system = v;
    } else {
      system = std::string(v) + "/";
    }

    system += std::string(TRI_ApplicationName) + "_XXXXXX";

    // copy to a character array
    SystemTempPath.reset(new char[system.size() + 1]);
    path = SystemTempPath.get();
    TRI_CopyString(path, system.c_str(), system.size());

    // fill template
    char* res = mkdtemp(SystemTempPath.get());

    if (res == nullptr) {
      system = "/tmp/arangodb";
      SystemTempPath.reset(new char[system.size() + 1]);
      path = SystemTempPath.get();
      TRI_CopyString(path, system.c_str(), system.size());
    }

    atexit(SystemTempPathCleaner);
  }

  return std::string(path);
}

#else

std::string TRI_GetTempPath() {
// ..........................................................................
// Unfortunately we generally have little control on whether or not the
// application will be compiled with UNICODE defined. In some cases such as
// this one, we attempt to cater for both. MS provides some methods which are
// 'defined' for both, for example, GetTempPath (below) actually converts to
// GetTempPathA (ascii) or GetTempPathW (wide characters or what MS call
// unicode).
// ..........................................................................

#define LOCAL_MAX_PATH_BUFFER 2049
  TCHAR tempFileName[LOCAL_MAX_PATH_BUFFER];
  TCHAR tempPathName[LOCAL_MAX_PATH_BUFFER];
  DWORD dwReturnValue = 0;
  UINT uReturnValue = 0;
  HANDLE tempFileHandle = INVALID_HANDLE_VALUE;
  BOOL ok;
  char* result;

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
  dwReturnValue = GetTempPath(LOCAL_MAX_PATH_BUFFER, tempPathName);

  if ((dwReturnValue > LOCAL_MAX_PATH_BUFFER) || (dwReturnValue == 0)) {
    // something wrong
    LOG(TRACE) << "GetTempPathA failed: LOCAL_MAX_PATH_BUFFER=" << LOCAL_MAX_PATH_BUFFER << ":dwReturnValue=" << dwReturnValue;
    // attempt to simply use the current directory
    _tcscpy(tempFileName, TEXT("."));
  }

  // ...........................................................................
  // Having obtained the temporary path, we have to determine if we can actually
  // write to that directory
  // ...........................................................................

  uReturnValue = GetTempFileName(tempPathName, TEXT("TRI_"), 0, tempFileName);

  if (uReturnValue == 0) {
    LOG(TRACE) << "GetTempFileNameA failed";
    _tcscpy(tempFileName, TEXT("TRI_tempFile"));
  }

  tempFileHandle = CreateFile((LPTSTR)tempFileName,   // file name
                              GENERIC_WRITE,          // open for write
                              0,                      // do not share
                              NULL,                   // default security
                              CREATE_ALWAYS,          // overwrite existing
                              FILE_ATTRIBUTE_NORMAL,  // normal file
                              NULL);                  // no template

  if (tempFileHandle == INVALID_HANDLE_VALUE) {
    LOG(FATAL) << "Can not create a temporary file"; FATAL_ERROR_EXIT();
  }

  ok = CloseHandle(tempFileHandle);

  if (!ok) {
    LOG(FATAL) << "Can not close the handle of a temporary file"; FATAL_ERROR_EXIT();
  }

  ok = DeleteFile(tempFileName);

  if (!ok) {
    LOG(FATAL) << "Can not destroy a temporary file"; FATAL_ERROR_EXIT();
  }

  // ...........................................................................
  // Whether or not UNICODE is defined, we assume that the temporary file name
  // fits in the ascii set of characters. This is a small compromise so that
  // temporary file names can be extra long if required.
  // ...........................................................................
  {
    size_t j;
    size_t pathSize = _tcsclen(tempPathName);
    char* temp = static_cast<char*>(
        TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, pathSize + 1, false));

    if (temp == nullptr) {
      LOG(FATAL) << "Out of memory"; FATAL_ERROR_EXIT();
    }

    for (j = 0; j < pathSize; ++j) {
      if (tempPathName[j] > 127) {
        LOG(FATAL) << "Invalid characters in temporary path name"; FATAL_ERROR_EXIT();
      }
      temp[j] = (char)(tempPathName[j]);
    }
    temp[pathSize] = 0;

    // remove trailing directory separator
    RemoveTrailingSeparator(temp);

    // ok = (WideCharToMultiByte(CP_UTF8, WC_NO_BEST_FIT_CHARS, tempPathName,
    // -1, temp, pathSize + 1,  NULL, NULL) != 0);

    result = TRI_DuplicateString(temp);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, temp);
  }

  std::string r = result;
  TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  return r;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief get a temporary file name
////////////////////////////////////////////////////////////////////////////////

int TRI_GetTempName(char const* directory, char** result, bool const createFile,
                    long& systemError, std::string& errorMessage) {
  char* dir;
  int tries;

  std::string temp = TRI_GetUserTempPath();

  if (directory != nullptr) {
    dir = TRI_Concatenate2File(temp.c_str(), directory);
  } else {
    dir = TRI_DuplicateString(temp.c_str());
  }

  // remove trailing PATH_SEPARATOR
  RemoveTrailingSeparator(dir);

  bool res = TRI_CreateRecursiveDirectory(dir, systemError, errorMessage);

  if (!res) {
    TRI_Free(TRI_CORE_MEM_ZONE, dir);
    return TRI_ERROR_SYS_ERROR;
  }

  if (!TRI_IsDirectory(dir)) {
    errorMessage = std::string(dir) + " exists and is not a directory!";
    TRI_Free(TRI_CORE_MEM_ZONE, dir);
    return TRI_ERROR_CANNOT_CREATE_DIRECTORY;
  }

  tries = 0;
  while (tries++ < 10) {
    TRI_pid_t pid;
    char* tempName;
    char* pidString;
    char* number;
    char* filename;

    pid = TRI_CurrentProcessId();

    number = TRI_StringUInt32(TRI_UInt32Random());
    pidString = TRI_StringUInt32(pid);
    tempName = TRI_Concatenate4String("tmp-", pidString, "-", number);
    TRI_Free(TRI_CORE_MEM_ZONE, number);
    TRI_Free(TRI_CORE_MEM_ZONE, pidString);

    filename = TRI_Concatenate2File(dir, tempName);
    TRI_Free(TRI_CORE_MEM_ZONE, tempName);

    if (TRI_ExistsFile(filename)) {
      errorMessage = std::string("Tempfile already exists! ") + filename;
      TRI_Free(TRI_CORE_MEM_ZONE, filename);
    } else {
      if (createFile) {
        FILE* fd = fopen(filename, "wb");

        if (fd != nullptr) {
          fclose(fd);
          TRI_Free(TRI_CORE_MEM_ZONE, dir);
          *result = filename;
          return TRI_ERROR_NO_ERROR;
        }
      } else {
        TRI_Free(TRI_CORE_MEM_ZONE, dir);
        *result = filename;
        return TRI_ERROR_NO_ERROR;
      }

      TRI_Free(TRI_CORE_MEM_ZONE, filename);
    }

    // next try
  }

  TRI_Free(TRI_CORE_MEM_ZONE, dir);

  return TRI_ERROR_CANNOT_CREATE_TEMP_FILE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the user-defined temp path, with a fallback to the system's
/// temp path if none is specified
////////////////////////////////////////////////////////////////////////////////

std::string TRI_GetUserTempPath() {
  if (TempPath.empty()) {
    return TRI_GetTempPath();
  }

  return TempPath;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set a new user-defined temp path
////////////////////////////////////////////////////////////////////////////////

void TRI_SetUserTempPath(std::string const& path) { TempPath = path; }

////////////////////////////////////////////////////////////////////////////////
/// @brief locate the installation directory
//
/// Will always end in a directory separator.
////////////////////////////////////////////////////////////////////////////////

#if _WIN32

std::string TRI_LocateInstallDirectory() {
  return TRI_LocateBinaryPath(nullptr) +
         std::string(1, TRI_DIR_SEPARATOR_CHAR) + ".." +
         std::string(1, TRI_DIR_SEPARATOR_CHAR);
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief locates the configuration directory
///
/// Will always end in a directory separator.
////////////////////////////////////////////////////////////////////////////////

#if _WIN32

char* TRI_LocateConfigDirectory() {
  char* v = LocateConfigDirectoryEnv();

  if (v != nullptr) {
    return v;
  }

  std::string r = TRI_LocateInstallDirectory();

#ifdef _SYSCONFDIR_
  r += _SYSCONFDIR_;
#else
  r += "etc\\arangodb";
#endif

  r += std::string(1, TRI_DIR_SEPARATOR_CHAR);

  return TRI_DuplicateString(r.c_str());
}

#elif defined(_SYSCONFDIR_)

char* TRI_LocateConfigDirectory() {
  size_t len;
  char const* dir = _SYSCONFDIR_;
  char* v;

  v = LocateConfigDirectoryEnv();

  if (v != nullptr) {
    return v;
  }

  if (*dir == '\0') {
    return nullptr;
  }

  len = strlen(dir);

  if (dir[len - 1] != TRI_DIR_SEPARATOR_CHAR) {
    return TRI_Concatenate2String(dir, "/");
  } else {
    return TRI_DuplicateString(dir);
  }
}

#else

char* TRI_LocateConfigDirectory() { return LocateConfigDirectoryEnv(); }

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief get the address of the null buffer
////////////////////////////////////////////////////////////////////////////////

char* TRI_GetNullBufferFiles() { return &NullBuffer[0]; }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the size of the null buffer
////////////////////////////////////////////////////////////////////////////////

size_t TRI_GetNullBufferSizeFiles() { return sizeof(NullBuffer); }

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize the files subsystem
////////////////////////////////////////////////////////////////////////////////

void TRI_InitializeFiles() {
  memset(TRI_GetNullBufferFiles(), 0, TRI_GetNullBufferSizeFiles());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown the files subsystem
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownFiles() {}
