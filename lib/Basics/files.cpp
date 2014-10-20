////////////////////////////////////////////////////////////////////////////////
/// @brief file functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
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


#include "Basics/conversions.h"
#include "Basics/hashes.h"
#include "Basics/locks.h"
#include "Basics/logging.h"
#include "Basics/random.h"
#include "Basics/string-buffer.h"
#include "Basics/threads.h"
#include "Basics/tri-strings.h"

#ifdef _WIN32
#include <tchar.h>
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                   private defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief read buffer size (used for bulk file reading)
////////////////////////////////////////////////////////////////////////////////

#define READBUFFER_SIZE 8192

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief already initialised
////////////////////////////////////////////////////////////////////////////////

static bool Initialised = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief user-defined temporary path
////////////////////////////////////////////////////////////////////////////////

static char* TempPath;

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

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief removes trailing path separators from path
///
/// @note path will be modified in-place
////////////////////////////////////////////////////////////////////////////////

static void RemoveTrailingSeparator (char* path) {
  const char* s;
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

static void NormalizePath (char* path) {
  char* p;
  char *e;

  RemoveTrailingSeparator(path);

  p = path;
  e = path + strlen(p);

  for (; p < e;  ++p) {
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

static ssize_t LookupElementVectorString (TRI_vector_string_t * vector, char const * element) {
  size_t i;
  int idx = -1;

  TRI_ReadLockReadWriteLock(&FileNamesLock);

  for (i = 0;  i < vector->_length;  i++) {
    if (TRI_EqualString(element, vector->_buffer[i])) {
      // theoretically this might cap the value of i, but it is highly unlikely
      idx = (ssize_t) i;
      break;
    }
  }

  TRI_ReadUnlockReadWriteLock(&FileNamesLock);
  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief callback function for removing all locked files by a process
////////////////////////////////////////////////////////////////////////////////

static void RemoveAllLockedFiles (void) {
  size_t i;

  TRI_WriteLockReadWriteLock(&FileNamesLock);

  for (i = 0;  i < FileNames._length;  i++) {
#ifdef TRI_HAVE_WIN32_FILE_LOCKING
    HANDLE fd;

    fd = * (HANDLE*) TRI_AtVector(&FileDescriptors, i);
    CloseHandle(fd);
#else
    int fd;

    fd = * (int*) TRI_AtVector(&FileDescriptors, i);
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

static void InitialiseLockFiles (void) {
  if (Initialised) {
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
  Initialised = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lists the directory tree
////////////////////////////////////////////////////////////////////////////////

static void ListTreeRecursively (char const* full,
                                 char const* path,
                                 TRI_vector_string_t* result) {
  size_t i;
  size_t j;
  TRI_vector_string_t dirs = TRI_FilesDirectory(full);

  for (j = 0;  j < 2;  ++j) {
    for (i = 0;  i < dirs._length;  ++i) {
      char const* filename = dirs._buffer[i];
      char* newfull = TRI_Concatenate2File(full, filename);
      char* newpath;

      if (*path) {
        newpath = TRI_Concatenate2File(path, filename);
      }
      else {
        newpath = TRI_DuplicateString(filename);
      }

      if (j == 0) {
        if (TRI_IsDirectory(newfull)) {
          TRI_PushBackVectorString(result, newpath);

          if (! TRI_IsSymbolicLink(newfull)) {
            ListTreeRecursively(newfull, newpath, result);
          }
        }
        else {
          TRI_FreeString(TRI_CORE_MEM_ZONE, newpath);
        }
      }
      else {
        if (! TRI_IsDirectory(newfull)) {
          TRI_PushBackVectorString(result, newpath);
        }
        else {
          TRI_FreeString(TRI_CORE_MEM_ZONE, newpath);
        }
      }

      TRI_FreeString(TRI_CORE_MEM_ZONE, newfull);
    }
  }

  TRI_DestroyVectorString(&dirs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locates a environment given configuration directory
////////////////////////////////////////////////////////////////////////////////

static char* LocateConfigDirectoryEnv (void) {
  char const* v;
  char* r;

  v = getenv("ARANGODB_CONFIG_PATH");

  if (v == NULL) {
    return NULL;
  }

  r = TRI_DuplicateString(v);

  NormalizePath(r);

  TRI_AppendString(&r, TRI_DIR_SEPARATOR_STR);

  return r;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief sets close-on-exit for a socket
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_WIN32_CLOSE_ON_EXEC

bool TRI_SetCloseOnExitFile (int fileDescriptor) {
  return true;
}

#else

bool TRI_SetCloseOnExitFile (int fileDescriptor) {
  long flags = fcntl(fileDescriptor, F_GETFD, 0);

  if (flags < 0) {
    return false;
  }

  flags = fcntl(fileDescriptor, F_SETFD, flags | FD_CLOEXEC);

  if (flags < 0) {
    return false;
  }

  return true;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the size of a file
///
/// Will return a negative error number on error, typically -1
////////////////////////////////////////////////////////////////////////////////

int64_t TRI_SizeFile (char const* path) {
  TRI_stat_t stbuf;
  int res = TRI_STAT(path, &stbuf);

  if (res != 0) {
    // an error occurred
    return (int64_t) res;
  }

  return (int64_t) stbuf.st_size;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if file or directory is writable
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

bool TRI_IsWritable (char const* path) {

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

bool TRI_IsWritable (char const* path) {
  // we can use POSIX access() from unistd.h to check for write permissions
  return (access(path, W_OK) == 0);
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if path is a directory
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsDirectory (char const* path) {
  TRI_stat_t stbuf;
  int res;

  res = TRI_STAT(path, &stbuf);

  return (res == 0) && ((stbuf.st_mode & S_IFMT) == S_IFDIR);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if path is a symbolic link
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

bool TRI_IsSymbolicLink (char const* path) {
  // TODO : check if a file is a symbolic link - without opening the file
  return false;
}

#else

bool TRI_IsSymbolicLink (char const* path) {
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

bool TRI_ExistsFile (char const* path) {
  if (path == NULL) {
    return false;
  }
  else {
    TRI_stat_t stbuf;
    size_t len;
    int res;

    len  = strlen(path);

    // path must not end with a \ on Windows, other stat() will return -1
    if (len > 0 && path[len - 1] == TRI_DIR_SEPARATOR_CHAR) {
      char* copy = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, path);

      if (copy == NULL) {
        return false;
      }

      // remove trailing slash
      RemoveTrailingSeparator(copy);

      res = TRI_STAT(copy, &stbuf);
      TRI_FreeString(TRI_CORE_MEM_ZONE, copy);
    }
    else {
      res = TRI_STAT(path, &stbuf);
    }

    return res == 0;
  }
}

#else

bool TRI_ExistsFile (char const* path) {
  if (path == nullptr) {
    return false;
  }
    
  struct stat stbuf;
  int res = TRI_STAT(path, &stbuf);

  return res == 0;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the last modification date of a file
////////////////////////////////////////////////////////////////////////////////

int TRI_MTimeFile (char const* path, int64_t* mtime) {
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

int TRI_CreateRecursiveDirectory (char const* path) {
  char* copy;
  char* p;
  char* s;
  int res;
  int m;

  res = TRI_ERROR_NO_ERROR;
  p = s = copy = TRI_DuplicateString(path);

  while (*p != '\0') {
    if (*p == TRI_DIR_SEPARATOR_CHAR) {
      if (p - s > 0) {

        *p = '\0';
        m = TRI_MKDIR(copy, 0777);

        *p = TRI_DIR_SEPARATOR_CHAR;
        s = p + 1;

        if (m != 0 && errno != EEXIST) {
          res = errno;
          break;
        }
      }
    }
    p++;
  }

  if (res == TRI_ERROR_NO_ERROR && (p - s > 0)) {
    m = TRI_MKDIR(copy, 0777);

    if (m != 0 && errno != EEXIST) {
      res = errno;
    }
  }

  TRI_Free(TRI_CORE_MEM_ZONE, copy);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a directory
////////////////////////////////////////////////////////////////////////////////

int TRI_CreateDirectory (char const* path) {
  int res;

  // reset error flag
  TRI_set_errno(TRI_ERROR_NO_ERROR);

  res = TRI_MKDIR(path, 0777);

  if (res != TRI_ERROR_NO_ERROR) {
    // check errno
    res = errno;
    if (res != TRI_ERROR_NO_ERROR) {
      if (res == ENOENT) {
        res = TRI_ERROR_FILE_NOT_FOUND;
      }
      else if (res == EEXIST) {
        res = TRI_ERROR_FILE_EXISTS;
      }
      else if (res == EPERM) {
        res = TRI_ERROR_FORBIDDEN;
      }
      else {
        // an unknown error type. will be translated into system error below
        res = TRI_ERROR_NO_ERROR;
      }
    }

    // if errno doesn't indicate an error, return a system error
    if (res == TRI_ERROR_NO_ERROR) {
      res = TRI_ERROR_SYS_ERROR;
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an empty directory
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveEmptyDirectory (char const* filename) {
  int res;

  res = TRI_RMDIR(filename);

  if (res != 0) {
    LOG_TRACE("cannot remove directory '%s': %s", filename, TRI_LAST_ERROR_STR);
    return TRI_set_errno(TRI_ERROR_SYS_ERROR);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a directory recursively
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveDirectory (char const* filename) {
  if (TRI_IsDirectory(filename)) {
    TRI_vector_string_t files;
    size_t i;
    int res;

    LOG_TRACE("removing directory '%s'", filename);

    res = TRI_ERROR_NO_ERROR;
    files = TRI_FilesDirectory(filename);

    for (i = 0;  i < files._length;  ++i) {
      char* full;
      int subres;

      full = TRI_Concatenate2File(filename, files._buffer[i]);

      subres = TRI_RemoveDirectory(full);
      TRI_FreeString(TRI_CORE_MEM_ZONE, full);

      if (subres != TRI_ERROR_NO_ERROR) {
        res = subres;
      }
    }

    TRI_DestroyVectorString(&files);

    if (res == TRI_ERROR_NO_ERROR) {
      res = TRI_RemoveEmptyDirectory(filename);
    }

    return res;
  }
  else if (TRI_ExistsFile(filename)) {
    LOG_TRACE("removing file '%s'", filename);

    return TRI_UnlinkFile(filename);
  }
  else {
    LOG_TRACE("attempt to remove non-existing file/directory '%s'", filename);

    return TRI_ERROR_NO_ERROR;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the dirname
////////////////////////////////////////////////////////////////////////////////

char* TRI_Dirname (char const* path) {
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
  }
  else if (n == 1 && *path == TRI_DIR_SEPARATOR_CHAR) {
    return TRI_DuplicateString(TRI_DIR_SEPARATOR_STR);
  }
  else if (n - m == 1 && *path == '.') {
    return TRI_DuplicateString(".");
  }
  else if (n - m == 2 && path[0] == '.' && path[1] == '.') {
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
    }
    else {
      return TRI_DuplicateString(".");
    }
  }

  n = p - path;

  return TRI_DuplicateString2(path, n);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the basename
////////////////////////////////////////////////////////////////////////////////

char* TRI_Basename (char const* path) {
  size_t n;

  n = strlen(path);

  if (1 < n) {
    if (path[n - 1] == TRI_DIR_SEPARATOR_CHAR || path[n - 1] == '/') {
      n -= 1;
    }
  }

  if (n == 0) {
    return TRI_DuplicateString("");
  }
  else if (n == 1) {
    if (*path == TRI_DIR_SEPARATOR_CHAR || *path == '/') {
      return TRI_DuplicateString(TRI_DIR_SEPARATOR_STR);
    }
    else {
      return TRI_DuplicateString2(path, n);
    }
  }
  else {
    char const* p;

    for (p = path + (n - 2);  path < p;  --p) {
      if (*p == TRI_DIR_SEPARATOR_CHAR || *p == '/') {
        break;
      }
    }

    if (path == p) {
      if (*p == TRI_DIR_SEPARATOR_CHAR || *p == '/') {
        return TRI_DuplicateString2(path + 1, n - 1);
      }
      else {
        return TRI_DuplicateString2(path, n);
      }
    }
    else {
      n -= p - path;

      return TRI_DuplicateString2(p + 1, n - 1);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a filename
////////////////////////////////////////////////////////////////////////////////

char* TRI_Concatenate2File (char const* path, char const* name) {
  size_t len = strlen(path);
  char* result;

  if (0 < len) {
    result = TRI_DuplicateString(path);
    RemoveTrailingSeparator(result);

    TRI_AppendString(&result, TRI_DIR_SEPARATOR_STR);
  }
  else {
    result = TRI_DuplicateString("");
  }

  TRI_AppendString(&result, name);
  NormalizePath(result);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a filename
////////////////////////////////////////////////////////////////////////////////

char* TRI_Concatenate3File (char const* path1, char const* path2, char const* name) {
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

TRI_vector_string_t TRI_FilesDirectory (char const* path) {
  TRI_vector_string_t result;

  struct _finddata_t fd;
  intptr_t handle;
  char* filter;

  TRI_InitVectorString(&result, TRI_CORE_MEM_ZONE);

  filter = TRI_Concatenate2String(path, "\\*");
  if (! filter) {
    return result;
  }

  handle = _findfirst(filter, &fd);
  TRI_FreeString(TRI_CORE_MEM_ZONE, filter);

  if (handle == -1) {
    return result;
  }

  do {
    if (strcmp(fd.name, ".") != 0 && strcmp(fd.name, "..") != 0) {
      TRI_PushBackVectorString(&result, TRI_DuplicateString(fd.name));
    }
  }
  while (_findnext(handle, &fd) != -1);

  _findclose(handle);

  return result;
}

#else

TRI_vector_string_t TRI_FilesDirectory (char const* path) {
  TRI_vector_string_t result;

  DIR * d;
  struct dirent * de;

  TRI_InitVectorString(&result, TRI_CORE_MEM_ZONE);

  d = opendir(path);

  if (d == 0) {
    return result;
  }

  de = readdir(d);

  while (de != 0) {
    if (strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0) {

      TRI_PushBackVectorString(&result, TRI_DuplicateString(de->d_name));
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

TRI_vector_string_t TRI_FullTreeDirectory (char const* path) {
  TRI_vector_string_t result;

  TRI_InitVectorString(&result, TRI_CORE_MEM_ZONE);

  TRI_PushBackVectorString(&result, TRI_DuplicateString(""));
  ListTreeRecursively(path, "", &result);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a file
////////////////////////////////////////////////////////////////////////////////

int TRI_RenameFile (char const* old, char const* filename) {
  int res;

#ifdef _WIN32
  BOOL moveResult = 0;
  moveResult = MoveFileExA(old, filename, MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING);

  if (! moveResult) {
    DWORD errorCode = GetLastError();
    LOG_TRACE("cannot rename file from '%s' to '%s': %d", old, filename, (int) errorCode);
    res = -1;
  }
  else {
    res = 0;
  }
#else
  res = rename(old, filename);
#endif

  if (res != 0) {
    LOG_TRACE("cannot rename file from '%s' to '%s': %s", old, filename, TRI_LAST_ERROR_STR);
    return TRI_set_errno(TRI_ERROR_SYS_ERROR);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlinks a file
////////////////////////////////////////////////////////////////////////////////

int TRI_UnlinkFile (char const* filename) {
  int res = TRI_UNLINK(filename);

  if (res != 0) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG_TRACE("cannot unlink file '%s': %s", filename, TRI_LAST_ERROR_STR);
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

bool TRI_ReadPointer (int fd, void* buffer, size_t length) {
  char* ptr = static_cast<char*>(buffer);

  while (0 < length) {
    ssize_t n = TRI_READ(fd, ptr, (unsigned int) length);

    if (n < 0) {
      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      LOG_ERROR("cannot read: %s", TRI_LAST_ERROR_STR);
      return false;
    }
    else if (n == 0) {
      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      LOG_ERROR("cannot read, end-of-file");
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

bool TRI_WritePointer (int fd, void const* buffer, size_t length) {
  char const* ptr = static_cast<char const*>(buffer);

  while (0 < length) {
    ssize_t n = TRI_WRITE(fd, ptr, (TRI_write_t) length);

    if (n < 0) {
      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      LOG_ERROR("cannot write: %s", TRI_LAST_ERROR_STR);
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

int TRI_WriteFile (const char* filename, const char* data, size_t length) {
  int fd;
  bool result;

  fd = TRI_CREATE(filename, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);

  if (fd == -1) {
    return TRI_set_errno(TRI_ERROR_SYS_ERROR);
  }

  result = TRI_WritePointer(fd, data, length);

  TRI_CLOSE(fd);

  if (! result) {
    return TRI_errno();
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fsyncs a file
////////////////////////////////////////////////////////////////////////////////

bool TRI_fsync (int fd) {
  int res = fsync(fd);

#ifdef __APPLE__

  if (res == 0) {
    res = fcntl(fd, F_FULLFSYNC, 0);
  }

#endif

  if (res == 0) {
    return true;
  }
  else {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief slurps in a file
////////////////////////////////////////////////////////////////////////////////

char* TRI_SlurpFile (TRI_memory_zone_t* zone,
                     char const* filename,
                     size_t* length) {
  TRI_string_buffer_t result;
  int fd;

  fd = TRI_OPEN(filename, O_RDONLY);

  if (fd == -1) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    return NULL;
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
      return NULL;
    }

    n = TRI_READ(fd, (void*) TRI_EndStringBuffer(&result), READBUFFER_SIZE);

    if (n == 0) {
      break;
    }

    if (n < 0) {
      TRI_CLOSE(fd);

      TRI_AnnihilateStringBuffer(&result);

      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      return NULL;
    }

    TRI_IncreaseLengthStringBuffer(&result, (size_t) n);
  }

  if (length != NULL) {
    *length = TRI_LengthStringBuffer(&result);
  }

  TRI_CLOSE(fd);
  return result._buffer;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a lock file based on the PID
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_WIN32_FILE_LOCKING

int TRI_CreateLockFile (char const* filename) {
  BOOL r;
  DWORD len;
  HANDLE fd;
  OVERLAPPED ol;
  TRI_pid_t pid;
  char* buf;
  char* fn;
  int res;

  InitialiseLockFiles();

  if (0 <= LookupElementVectorString(&FileNames, filename)) {
    return TRI_ERROR_NO_ERROR;
  }

  fd = CreateFile(filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

  if (fd == INVALID_HANDLE_VALUE) {
    return TRI_set_errno(TRI_ERROR_SYS_ERROR);
  }

  pid = TRI_CurrentProcessId();
  buf = TRI_StringUInt32(pid);

  r = WriteFile(fd, buf, (unsigned int) strlen(buf), &len, NULL);

  if (! r || len != strlen(buf)) {
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
  r = LockFileEx(fd, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 0, 128, &ol);

  if (! r) {
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

int TRI_CreateLockFile (char const* filename) {
  TRI_pid_t pid;
  char* buf;
  char* fn;
  int fd;
  int rv;
  int res;

  InitialiseLockFiles();

  if (0 <= LookupElementVectorString(&FileNames, filename)) {
    return TRI_ERROR_NO_ERROR;
  }

  fd = TRI_CREATE(filename, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);

  if (fd == -1) {
    return TRI_set_errno(TRI_ERROR_SYS_ERROR);
  }

  pid = TRI_CurrentProcessId();
  buf = TRI_StringUInt32(pid);

  rv = TRI_WRITE(fd, buf, (TRI_write_t) strlen(buf));

  if (rv == -1) {
    res = TRI_set_errno(TRI_ERROR_SYS_ERROR);

    TRI_FreeString(TRI_CORE_MEM_ZONE, buf);

    TRI_CLOSE(fd);
    TRI_UNLINK(filename);

    return res;
  }

  TRI_FreeString(TRI_CORE_MEM_ZONE, buf);
  TRI_CLOSE(fd);

  // try to open pid file
  fd = TRI_OPEN(filename, O_RDONLY);

  if (fd < 0) {
    return TRI_set_errno(TRI_ERROR_SYS_ERROR);
  }

  // try to lock pid file
  rv = flock(fd, LOCK_EX);

  if (rv == -1) {
    res = TRI_set_errno(TRI_ERROR_SYS_ERROR);

    TRI_CLOSE(fd);
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

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief verifies a lock file based on the PID
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_WIN32_FILE_LOCKING

int TRI_VerifyLockFile (char const* filename) {
  HANDLE fd;

  if (! TRI_ExistsFile(filename)) {
    return TRI_ERROR_NO_ERROR;
  }

  fd = CreateFile(filename, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

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

int TRI_VerifyLockFile (char const* filename) {
  TRI_pid_t pid;
  char buffer[128];
  int can_lock;
  int fd;
  int res;
  ssize_t n;
  uint32_t fc;

  if (! TRI_ExistsFile(filename)) {
    return TRI_ERROR_NO_ERROR;
  }

  fd = TRI_OPEN(filename, O_RDONLY);

  if (fd < 0) {
    return TRI_ERROR_NO_ERROR;
  }

  n = TRI_READ(fd, buffer, sizeof(buffer));

  TRI_CLOSE(fd);

  if (n < 0) {
    return TRI_ERROR_NO_ERROR;
  }

  // file empty or pid too long
  if (n == 0 || n == sizeof(buffer)) {
    return TRI_ERROR_NO_ERROR;
  }

  // 0-terminate buffer
  buffer[n] = '\0';

  fc = TRI_UInt32String(buffer);
  res = TRI_errno();

  if (res != TRI_ERROR_NO_ERROR) {
    return TRI_ERROR_NO_ERROR;
  }

  pid = fc;

  if (kill(pid, 0) == -1) {
    return TRI_ERROR_NO_ERROR;
  }

  fd = TRI_OPEN(filename, O_RDONLY);

  if (fd < 0) {
    return TRI_ERROR_NO_ERROR;
  }

  can_lock = flock(fd, LOCK_EX | LOCK_NB);

  // file was not yet be locked
  if (can_lock == 0) {
    flock(fd, LOCK_UN);
    TRI_CLOSE(fd);

    return TRI_ERROR_NO_ERROR;
  }

  TRI_CLOSE(fd);
  return TRI_ERROR_ARANGO_DATADIR_LOCKED;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a lock file based on the PID
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_WIN32_FILE_LOCKING

int TRI_DestroyLockFile (char const* filename) {
  HANDLE fd;
  ssize_t n;

  InitialiseLockFiles();
  n = LookupElementVectorString(&FileNames, filename);

  if (n < 0) {
    return TRI_ERROR_NO_ERROR;
  }


  fd = * (HANDLE*) TRI_AtVector(&FileDescriptors, n);

  CloseHandle(fd);

  TRI_UnlinkFile(filename);

  TRI_WriteLockReadWriteLock(&FileNamesLock);
  TRI_RemoveVectorString(&FileNames, n);
  TRI_RemoveVector(&FileDescriptors, n);
  TRI_WriteUnlockReadWriteLock(&FileNamesLock);

  return TRI_ERROR_NO_ERROR;
}

#else

int TRI_DestroyLockFile (char const* filename) {
  int fd;
  int res;
  ssize_t n;

  InitialiseLockFiles();
  n = LookupElementVectorString(&FileNames, filename);

  if (n < 0) {
    // TODO: what happens if the file does not exist?
    return TRI_ERROR_NO_ERROR;
  }

  fd = TRI_OPEN(filename, O_RDWR);

  if (fd < 0) {
    // TODO: what happens if the file does not exist?
    return TRI_ERROR_NO_ERROR;
  }

  res = flock(fd, LOCK_UN);
  TRI_CLOSE(fd);

  if (res == 0) {
    TRI_UnlinkFile(filename);
  }

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

char* TRI_GetFilename (char const* filename) {
  const char* p;
  const char* s;

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

char* TRI_GetAbsolutePath (char const* fileName, char const* currentWorkingDirectory) {
  char* result;
  size_t cwdLength;
  size_t fileLength;
  bool ok;

  // ...........................................................................
  // Check that fileName actually makes some sense
  // ...........................................................................

  if (fileName == NULL || *fileName == '\0') {
    return NULL;
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
        return TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, fileName);
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

  if (currentWorkingDirectory == NULL || *currentWorkingDirectory == '\0') {
    return NULL;
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
      if (currentWorkingDirectory[2] == '/' || currentWorkingDirectory[2] == '\\') {
        ok = true;
      }
    }
  }

  if (! ok) {
    return NULL;
  }


  // ...........................................................................
  // Determine the total legnth of the new string
  // ...........................................................................

  cwdLength  = strlen(currentWorkingDirectory);
  fileLength = strlen(fileName);

  if (currentWorkingDirectory[cwdLength - 1] == '\\' ||
      currentWorkingDirectory[cwdLength - 1] == '/') {
    // we do not require a backslash
    result = static_cast<char*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, (cwdLength + fileLength + 1) * sizeof(char), false));
    if (result == nullptr) {
      return nullptr;
    }
    memcpy(result, currentWorkingDirectory, cwdLength);
    memcpy(result + cwdLength, fileName, fileLength);
    result[cwdLength + fileLength] = '\0';
  }
  else {
    // we do require a backslash
    result = static_cast<char*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, (cwdLength + fileLength + 2) * sizeof(char), false));
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

char* TRI_GetAbsolutePath (char const* file, char const* cwd) {
  char* ptr;
  size_t cwdLength;
  bool isAbsolute;

  if (file == NULL || *file == '\0') {
    return NULL;
  }

  // name is absolute if starts with either forward or backslash
  isAbsolute = (*file == '/' || *file == '\\');

  // file is also absolute if contains a colon
  for (ptr = (char*) file; *ptr; ++ptr) {
    if (*ptr == ':') {
      isAbsolute = true;
      break;
    }
  }

  if (isAbsolute){
    return TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, file);
  }

  if (cwd == NULL || *cwd == '\0') {
    // no absolute path given, must abort
    return NULL;
  }

  cwdLength = strlen(cwd);
  TRI_ASSERT(cwdLength > 0);

  char* result = static_cast<char*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, (cwdLength + strlen(file) + 2) * sizeof(char), false));

  if (result != NULL) {
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

char* TRI_BinaryName (const char* argv0) {
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

  return name;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locates the directory containing the program
////////////////////////////////////////////////////////////////////////////////

char* TRI_LocateBinaryPath (char const* argv0) {
  char const* p;
  char* binaryPath = NULL;

  // check if name contains a '/' ( or '\' for windows)
  p = argv0;

  for (;  *p && *p != TRI_DIR_SEPARATOR_CHAR;  ++p) {
  }

  // contains a path
  if (*p) {
    binaryPath = TRI_Dirname(argv0);

    if (binaryPath == 0) {
      binaryPath = TRI_DuplicateString("");
    }
  }

  // check PATH variable
  else {
    p = getenv("PATH");

    if (p == 0) {
      binaryPath = TRI_DuplicateString("");
    }
    else {
      TRI_vector_string_t files;
      size_t i;

      files = TRI_SplitString(p, ':');

      for (i = 0;  i < files._length;  ++i) {
        char* full;
        char* prefix;

        prefix = files._buffer[i];

        if (*prefix) {
          full = TRI_Concatenate2File(prefix, argv0);
        }
        else {
          full = TRI_Concatenate2File(".", argv0);
        }

        if (TRI_ExistsFile(full)) {
          TRI_FreeString(TRI_CORE_MEM_ZONE, full);
          binaryPath = TRI_DuplicateString(files._buffer[i]);
          break;
        }

        TRI_FreeString(TRI_CORE_MEM_ZONE, full);
      }

      if (binaryPath == NULL) {
        binaryPath = TRI_DuplicateString(".");
      }

      TRI_DestroyVectorString(&files);
    }
  }

  return binaryPath;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locates the home directory
///
/// Under windows there is no 'HOME' directory as such so getenv("HOME") may
/// return NULL -- which it does under windows.  A safer approach below
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

char* TRI_HomeDirectory () {
  char const* drive = getenv("HOMEDRIVE");
  char const* path = getenv("HOMEPATH");
  char* result;

  if (drive != 0 && path != 0) {
    result = TRI_Concatenate2String(drive, path);
  }
  else {
    result = TRI_DuplicateString("");
  }

  return result;
}

#else

char* TRI_HomeDirectory () {
  char const* result = getenv("HOME");

  if (result == 0) {
    result = ".";
  }

  return TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, result);
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief calculate the crc32 checksum of a file
////////////////////////////////////////////////////////////////////////////////

int TRI_Crc32File (char const* path, uint32_t* crc) {
  FILE* fin;
  void* buffer;
  int bufferSize;
  int res;
  int res2;

  *crc = TRI_InitialCrc32();

  bufferSize = 4096;
  buffer = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, (size_t) bufferSize, false);

  if (buffer == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  fin = fopen(path, "rb");

  if (fin == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, buffer);

    return TRI_ERROR_FILE_NOT_FOUND;
  }

  res = TRI_ERROR_NO_ERROR;

  while (true) {
    int sizeRead = (int) fread(buffer, 1, bufferSize, fin);

    if (sizeRead < bufferSize) {
      if (feof(fin) == 0) {
        res = errno;
        break;
      }
    }

    if (sizeRead > 0) {
      *crc = TRI_BlockCrc32(*crc, static_cast<char const*>(buffer), sizeRead);
    }
    else if (sizeRead <= 0) {
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

static char const* TRI_ApplicationName = NULL;

void TRI_SetApplicationName (char const* name) {
  TRI_ASSERT(strlen(name) <= 13);
  TRI_ApplicationName = name;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the system's temporary path
////////////////////////////////////////////////////////////////////////////////

#ifndef _WIN32
// This must be exactly 14 Ys and 6 Xs because it will be overwritten
// and these are the maximum lengths!
static char TRI_TempPath[] = "/tmp/YYYYYYYYYYYYYYXXXXXX";
static bool TRI_TempPathIsSet = false;

static void TRI_TempPathCleaner (void) {
  if (TRI_TempPathIsSet) {
    rmdir(TRI_TempPath);
    TRI_TempPathIsSet = false;
  }
}
#endif

char* TRI_GetTempPath () {

#ifdef _WIN32

  // ..........................................................................
  // Unfortunately we generally have little control on whether or not the
  // application will be compiled with UNICODE defined. In some cases such as
  // this one, we attempt to cater for both. MS provides some methods which are
  // 'defined' for both, for example, GetTempPath (below) actually converts to
  // GetTempPathA (ascii) or GetTempPathW (wide characters or what MS call unicode).
  // ..........................................................................

  #define LOCAL_MAX_PATH_BUFFER 2049
  TCHAR   tempFileName[LOCAL_MAX_PATH_BUFFER];
  TCHAR   tempPathName[LOCAL_MAX_PATH_BUFFER];
  DWORD   dwReturnValue  = 0;
  UINT    uReturnValue   = 0;
  HANDLE  tempFileHandle = INVALID_HANDLE_VALUE;
  BOOL    ok;
  char* result;

  // ..........................................................................
  // Attempt to locate the path where the users temporary files are stored
  // Note we are imposing a limit of 2048+1 characters for the maximum size of a
  // possible path
  // ..........................................................................

  dwReturnValue = GetTempPath(LOCAL_MAX_PATH_BUFFER, tempPathName);

  if ( (dwReturnValue > LOCAL_MAX_PATH_BUFFER) || (dwReturnValue == 0)) {
    // something wrong
    LOG_TRACE("GetTempPathA failed: LOCAL_MAX_PATH_BUFFER=%d:dwReturnValue=%d", LOCAL_MAX_PATH_BUFFER, dwReturnValue);
    // attempt to simply use the current directory
    _tcscpy(tempFileName,TEXT("."));
  }


  // ...........................................................................
  // Having obtained the temporary path, we have to determine if we can actually
  // write to that directory
  // ...........................................................................

  uReturnValue = GetTempFileName(tempPathName, TEXT("TRI_"), 0, tempFileName);

  if (uReturnValue == 0) {
    LOG_TRACE("GetTempFileNameA failed");
    _tcscpy(tempFileName,TEXT("TRI_tempFile"));
  }

  tempFileHandle = CreateFile((LPTSTR) tempFileName, // file name
                              GENERIC_WRITE,         // open for write
                              0,                     // do not share
                              NULL,                  // default security
                              CREATE_ALWAYS,         // overwrite existing
                              FILE_ATTRIBUTE_NORMAL, // normal file
                              NULL);                 // no template

  if (tempFileHandle == INVALID_HANDLE_VALUE) {
    LOG_FATAL_AND_EXIT("Can not create a temporary file");
  }

  ok = CloseHandle(tempFileHandle);

  if (! ok) {
    LOG_FATAL_AND_EXIT("Can not close the handle of a temporary file");
  }

  ok = DeleteFile(tempFileName);

  if (! ok) {
    LOG_FATAL_AND_EXIT("Can not destroy a temporary file");
  }


  // ...........................................................................
  // Whether or not UNICODE is defined, we assume that the temporary file name
  // fits in the ascii set of characters. This is a small compromise so that
  // temporary file names can be extra long if required.
  // ...........................................................................
  {
    size_t j;
    size_t pathSize = _tcsclen(tempPathName);
    char* temp = static_cast<char*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, pathSize + 1, false));

    if (temp == NULL) {
      LOG_FATAL_AND_EXIT("Out of memory");
    }

    for (j = 0; j < pathSize; ++j) {
      if (tempPathName[j] > 127) {
        LOG_FATAL_AND_EXIT("Invalid characters in temporary path name");
      }
      temp[j] = (char)(tempPathName[j]);
    }
    temp[pathSize] = 0;

    // remove trailing directory separator
    RemoveTrailingSeparator(temp);

    //ok = (WideCharToMultiByte(CP_UTF8, WC_NO_BEST_FIT_CHARS, tempPathName, -1, temp, pathSize + 1,  NULL, NULL) != 0);

    result = TRI_DuplicateString(temp);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, temp);
  }

  return result;
#else
  char* res;
  if (! TRI_TempPathIsSet) {
    // Note that TRI_TempPath has space for /tmp/YYYYYYYYYYXXXXXX
    if (TRI_ApplicationName != NULL) {
      strcpy(TRI_TempPath, "/tmp/");
      strncat(TRI_TempPath, TRI_ApplicationName, 13);
      strcat(TRI_TempPath, "_XXXXXX");
    }
    else {
      strcpy(TRI_TempPath, "/tmp/arangodb_XXXXXX");
    }
    res = mkdtemp(TRI_TempPath);
    if (res == NULL) {
      strcpy(TRI_TempPath, "/tmp/arangodb");
      return TRI_DuplicateString(TRI_TempPath);
    }
    atexit(TRI_TempPathCleaner);
    TRI_TempPathIsSet = true;
  }
  return TRI_DuplicateString(TRI_TempPath);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a temporary file name
////////////////////////////////////////////////////////////////////////////////

int TRI_GetTempName (char const* directory,
                     char** result,
                     const bool createFile) {
  char* dir;
  char* temp;
  int tries;

  temp = TRI_GetUserTempPath();

  if (directory != NULL) {
    dir = TRI_Concatenate2File(temp, directory);
  }
  else {
    dir = TRI_DuplicateString(temp);
  }

  TRI_Free(TRI_CORE_MEM_ZONE, temp);

  // remove trailing PATH_SEPARATOR
  RemoveTrailingSeparator(dir);

  TRI_CreateRecursiveDirectory(dir);

  if (! TRI_IsDirectory(dir)) {
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
      TRI_Free(TRI_CORE_MEM_ZONE, filename);
    }
    else {
      if (createFile) {
        FILE* fd = fopen(filename, "wb");

        if (fd != NULL) {
          fclose(fd);
          TRI_Free(TRI_CORE_MEM_ZONE, dir);
          *result = filename;
          return TRI_ERROR_NO_ERROR;
        }
      }
      else {
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

char* TRI_GetUserTempPath (void) {
  if (TempPath == NULL) {
    return TRI_GetTempPath();
  }

  return TRI_DuplicateString(TempPath);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set a new user-defined temp path
////////////////////////////////////////////////////////////////////////////////

void TRI_SetUserTempPath (char* path) {
  if (TempPath != NULL) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, TempPath);
  }

  if (path == NULL) {
    // unregister user-defined temp path
    TempPath = NULL;
  }
  else {
    // copy the user-defined temp path
    TempPath = TRI_DuplicateString(path);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locate the installation directory in the given rootKey
///
/// rootKey should be: either HKEY_CURRENT_USER or HKEY_LOCAL_MASCHINE
/// Will always end in a directory separator.
////////////////////////////////////////////////////////////////////////////////

#if _WIN32
char * __LocateInstallDirectory_In(HKEY rootKey) {
  DWORD dwType;
  char  szPath[1023];
  DWORD dwDataSize;
  HKEY key;

  dwDataSize = sizeof(szPath);
  memset(szPath, 0, dwDataSize);

  // open the key for reading
  long lResult = RegOpenKeyEx(
                              rootKey,
                              "SOFTWARE\\triAGENS GmbH\\ArangoDB " TRI_VERSION,
                               0,
                               KEY_READ,
                               &key);

  if (lResult == ERROR_SUCCESS) {
      // read the version value
      lResult = RegQueryValueEx(key, "", NULL, &dwType, (BYTE*)szPath, &dwDataSize);

      if (lResult == ERROR_SUCCESS) {
        return TRI_Concatenate2String(szPath, "\\"); // TODO check if it already ends in \\ or /
      }

      RegCloseKey(key);
    }

  return NULL;

}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief locate the installation directory
//
/// Will always end in a directory separator.
////////////////////////////////////////////////////////////////////////////////

#if _WIN32
char* TRI_LocateInstallDirectory () {
  // We look for the configuration first in HKEY_CURRENT_USER (arango was
  // installed for a single user). When we don't find  anything when look in
  // HKEY_LOCAL_MACHINE (arango was installed as service).

  char * directory = __LocateInstallDirectory_In(HKEY_CURRENT_USER);

  if (!directory) {
    directory = __LocateInstallDirectory_In(HKEY_LOCAL_MACHINE);
  }
  return directory;
}

#else

char* TRI_LocateInstallDirectory () {
  return NULL;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief locates the configuration directory
///
/// Will always end in a directory separator.
////////////////////////////////////////////////////////////////////////////////

#if _WIN32

char* TRI_LocateConfigDirectory () {
  char* v;

  v = LocateConfigDirectoryEnv();

  if (v != NULL) {
    return v;
  }

  v = TRI_LocateInstallDirectory();

  if (v != NULL) {
    TRI_AppendString(&v, "etc\\arangodb\\");
  }

  return v;
}

#elif defined(_SYSCONFDIR_)

char* TRI_LocateConfigDirectory () {
  size_t len;
  const char* dir = _SYSCONFDIR_;
  char* v;

  v = LocateConfigDirectoryEnv();

  if (v != NULL) {
    return v;
  }

  if (*dir == '\0') {
    return NULL;
  }

  len = strlen(dir);

  if (dir[len - 1] != TRI_DIR_SEPARATOR_CHAR) {
    return TRI_Concatenate2String(dir, "/");
  }
  else {
    return TRI_DuplicateString(dir);
  }
}

#else

char* TRI_LocateConfigDirectory () {
  return LocateConfigDirectoryEnv();
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                  module functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise the files subsystem
///
/// TODO: inialise logging here?
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseFiles (void) {
  // clear user-defined temp path
  TempPath = NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown the files subsystem
///
/// TODO: inialise logging here?
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownFiles (void) {
  if (TempPath != NULL) {
    // free any user-defined temp-path
    TRI_FreeString(TRI_CORE_MEM_ZONE, TempPath);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
