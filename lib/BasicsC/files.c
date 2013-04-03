////////////////////////////////////////////////////////////////////////////////
/// @brief file functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
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


#include "BasicsC/conversions.h"
#include "BasicsC/hashes.h"
#include "BasicsC/locks.h"
#include "BasicsC/logging.h"
#include "BasicsC/random.h"
#include "BasicsC/string-buffer.h"
#include "BasicsC/tri-strings.h"
#include "BasicsC/threads.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Files
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief already initialised
////////////////////////////////////////////////////////////////////////////////

static bool Initialised = false;

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
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Files
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief remove trailing path separators from path.
/// path will be modified in-place
////////////////////////////////////////////////////////////////////////////////

static void RemoveTrailingSeparator (char* path) {
  size_t n;

  n = strlen(path);

  if (n > 0) {
    char* p = path + n - 1;
    while (*p == TRI_DIR_SEPARATOR_CHAR) {
      *p = '\0';
      --p;
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
  int idx = -1;
  int i;

  TRI_ReadLockReadWriteLock(&FileNamesLock);

  for (i = 0;  i < vector->_length;  i++) {
    if (TRI_EqualString(element, vector->_buffer[i])) {
      idx = i;
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
  int fd;

  TRI_WriteLockReadWriteLock(&FileNamesLock);

  for (i = 0;  i < FileNames._length;  i++) {
    TRI_UnlinkFile(FileNames._buffer[i]);
    TRI_RemoveVectorString(&FileNames, i);

    fd = * (int*) TRI_AtVector(&FileDescriptors, i);

    TRI_CLOSE(fd);
    TRI_RemoveVector(&FileDescriptors, i);
  }

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
  TRI_InitVector(&FileDescriptors, TRI_CORE_MEM_ZONE, sizeof(int));

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
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Files
/// @{
////////////////////////////////////////////////////////////////////////////////

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
  struct stat stbuf;
  int res = stat(path, &stbuf);

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
  // implementation for seems to be non-trivial
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
  struct stat stbuf;
  int res;

  res = stat(path, &stbuf);

  return (res == 0) && ((stbuf.st_mode & S_IFMT) == S_IFDIR);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if path is a symbolic link
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

bool TRI_IsSymbolicLink (char const* path) {
  // todo : check if a file is a symbolic link - without opening the file
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

bool TRI_ExistsFile (char const* path) {
  struct stat stbuf;
  int res;

  res = stat(path, &stbuf);

  return res == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a directory, recursively
////////////////////////////////////////////////////////////////////////////////

bool TRI_CreateRecursiveDirectory (char const* path) {
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

bool TRI_CreateDirectory (char const* path) {
  int res;

  res = TRI_MKDIR(path, 0777);

  if (res != 0) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    return false;
  }

  return true;
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
    int subres;

    LOG_TRACE("removing directory '%s'", filename);

    res = TRI_ERROR_NO_ERROR;
    files = TRI_FilesDirectory(filename);

    for (i = 0;  i < files._length;  ++i) {
      char* full;

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
    LOG_TRACE("removing non-existing file '%s'", filename);

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
    return TRI_DuplicateString("");
  }
  else if (n == 1 && *path == TRI_DIR_SEPARATOR_CHAR) {
    return TRI_DuplicateString("");
  }
  else if (n - m == 1 && *path == '.') {
    return TRI_DuplicateString("");
  }
  else if (n - m == 2 && path[0] == '.' && path[1] == '.') {
    return TRI_DuplicateString("");
  }

  for (p = path + (n - m - 1); path < p; --p) {
    if (*p == TRI_DIR_SEPARATOR_CHAR) {
      break;
    }
  }

  if (path == p) {
    if (*p == TRI_DIR_SEPARATOR_CHAR) {
      return TRI_DuplicateString2(path + 1, n - m);
    }
  }

  n -= p - path;

  return TRI_DuplicateString2(p + 1, n - 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a filename
////////////////////////////////////////////////////////////////////////////////

char* TRI_Concatenate2File (char const* path, char const* name) {
  return TRI_Concatenate3String(path, TRI_DIR_SEPARATOR_STR, name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a filename
////////////////////////////////////////////////////////////////////////////////

char* TRI_Concatenate3File (char const* path1, char const* path2, char const* name) {
  return TRI_Concatenate5String(path1, TRI_DIR_SEPARATOR_STR, path2, TRI_DIR_SEPARATOR_STR, name);
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
      TRI_PushBackVectorString(&result, TRI_DuplicateString(fd.name));
    }
  }
  while(_findnext(handle, &fd) != -1);

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
  if (!moveResult) {
    DWORD errorCode = GetLastError();
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
  int res;

  res = TRI_UNLINK(filename);

  if (res != 0) {
    LOG_TRACE("cannot unlink file '%s': %s", filename, TRI_LAST_ERROR_STR);
    return TRI_set_errno(TRI_ERROR_SYS_ERROR);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads into a buffer from a file
////////////////////////////////////////////////////////////////////////////////

bool TRI_ReadPointer (int fd, void* buffer, size_t length) {
  char* ptr;

  ptr = buffer;


  while (0 < length) {
    ssize_t n = TRI_READ(fd, ptr, length);


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
  char const* ptr;

  ptr = buffer;

  while (0 < length) {
    ssize_t n = TRI_WRITE(fd, ptr, length);

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

char* TRI_SlurpFile (TRI_memory_zone_t* zone, char const* filename, size_t* length) {
  TRI_string_buffer_t result;
  char buffer[10240];
  int fd;
  int res;

  fd = TRI_OPEN(filename, O_RDONLY);

  if (fd == -1) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    return NULL;
  }

  TRI_InitStringBuffer(&result, zone);

  while (true) {
    ssize_t n;

    n = TRI_READ(fd, buffer, sizeof(buffer));

    if (n == 0) {
      break;
    }

    if (n < 0) {
      TRI_CLOSE(fd);

      TRI_AnnihilateStringBuffer(&result);

      TRI_set_errno(TRI_ERROR_SYS_ERROR);
      return NULL;
    }

    res = TRI_AppendString2StringBuffer(&result, buffer, n);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_CLOSE(fd);

      TRI_AnnihilateStringBuffer(&result);

      return NULL;
    }
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

  rv = TRI_WRITE(fd, buf, strlen(buf));

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

  // ..........................................................................
  // TODO: use windows LockFile to lock the file
  // ..........................................................................
  //rv = LockFileEx(fd, LOCKFILE_EXCLUSIVE_LOCK, 0, 0, 0, 0);
  // rv = flock(fd, LOCK_EX);
  rv = true;

  if (!rv) {
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

  rv = TRI_WRITE(fd, buf, strlen(buf));

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
  TRI_pid_t pid;
  char buffer[128];
  int can_lock;
  int fd;
  int res;
  ssize_t n;
  uint32_t fc;

  if (! TRI_ExistsFile(filename)) {
    return TRI_set_errno(TRI_ERROR_SYS_ERROR);
  }

  fd = TRI_OPEN(filename, O_RDONLY);
  if (fd < 0) {
    // this method if checking whether or not the database is locked is not suitable with the manner
    // in which it is coded.
    // windows assigns ownership of the file to the process for exclusive use
    // the file exists, yet we can not open it, so being here we can only assume that the
    // database is locked.
    return TRI_ERROR_NO_ERROR;
  }
  n = TRI_READ(fd, buffer, sizeof(buffer));
  TRI_CLOSE(fd);

  // file empty
  if (n == 0) {
    return TRI_set_errno(TRI_ERROR_ILLEGAL_NUMBER);
  }

  // not really necessary, but this shuts up valgrind
  memset(buffer, 0, sizeof(buffer));

  fc = TRI_UInt32String(buffer);
  res = TRI_errno();

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  pid = fc;


  // ..........................................................................
  // determine if a process with pid exists
  // ..........................................................................
  // use OpenProcess / TerminateProcess as a replacement for kill
  //if (kill(pid, 0) == -1) {
  //  return TRI_set_errno(TRI_ERROR_DEAD_PID);
  //}

  fd = TRI_OPEN(filename, O_RDONLY);

  if (fd < 0) {
    return TRI_set_errno(TRI_ERROR_SYS_ERROR);
  }

  // ..........................................................................
  // TODO: Use windows LockFileEx to determine if file can be locked
  // ..........................................................................
  // = LockFileEx(fd, LOCKFILE_EXCLUSIVE_LOCK, 0, 0, 0, 0);
  //can_lock = flock(fd, LOCK_EX | LOCK_NB);
  can_lock = true;


  // file was not yet be locked
  if (can_lock == 0) {
    res = TRI_set_errno(TRI_ERROR_SYS_ERROR);

    // ........................................................................
    // TODO: Use windows LockFileEx to determine if file can be locked
    // ........................................................................
    //flock(fd, LOCK_UN);

    TRI_CLOSE(fd);

    return res;
  }

  TRI_CLOSE(fd);
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
    return TRI_set_errno(TRI_ERROR_SYS_ERROR);
  }

  fd = TRI_OPEN(filename, O_RDONLY);
  n = TRI_READ(fd, buffer, sizeof(buffer));
  TRI_CLOSE(fd);

  // file empty
  if (n == 0) {
    return TRI_set_errno(TRI_ERROR_ILLEGAL_NUMBER);
  }

  // not really necessary, but this shuts up valgrind
  memset(buffer, 0, sizeof(buffer));

  fc = TRI_UInt32String(buffer);
  res = TRI_errno();

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  pid = fc;

  if (kill(pid, 0) == -1) {
    return TRI_set_errno(TRI_ERROR_DEAD_PID);
  }

  fd = TRI_OPEN(filename, O_RDONLY);

  if (fd < 0) {
    return TRI_set_errno(TRI_ERROR_SYS_ERROR);
  }

  can_lock = flock(fd, LOCK_EX | LOCK_NB);

  // file was not yet be locked
  if (can_lock == 0) {
    res = TRI_set_errno(TRI_ERROR_SYS_ERROR);

    flock(fd, LOCK_UN);
    TRI_CLOSE(fd);

    return res;
  }

  TRI_CLOSE(fd);
  return TRI_ERROR_NO_ERROR;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a lock file based on the PID
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_WIN32_FILE_LOCKING

int TRI_DestroyLockFile (char const* filename) {
  int fd;
  int n;
  int res;

  InitialiseLockFiles();
  n = LookupElementVectorString(&FileNames, filename);

  if (n < 0) {
    return false;
  }


  fd = TRI_OPEN(filename, O_RDWR);

  if (fd < 0) {
    return false;
  }

  // ..........................................................................
  // TODO: Use windows LockFileEx to determine if file can be locked
  // ..........................................................................
  //flock(fd, LOCK_UN);
  //res = flock(fd, LOCK_UN);
  res = 0;

  TRI_CLOSE(fd);

  if (res == 0) {
    TRI_UnlinkFile(filename);

    TRI_WriteLockReadWriteLock(&FileNamesLock);

    TRI_RemoveVectorString(&FileNames, n);
    fd = * (int*) TRI_AtVector(&FileDescriptors, n);
    TRI_CLOSE(fd);

    TRI_WriteUnlockReadWriteLock(&FileNamesLock);
  }

  return res;
}

#else

int TRI_DestroyLockFile (char const* filename) {
  int fd;
  int n;
  int res;

  InitialiseLockFiles();
  n = LookupElementVectorString(&FileNames, filename);

  if (n < 0) {
    return false;
  }

  fd = TRI_OPEN(filename, O_RDWR);
  // TODO: what happens if the file does not exist?
  res = flock(fd, LOCK_UN);
  TRI_CLOSE(fd);

  if (res == 0) {
    TRI_UnlinkFile(filename);

    TRI_WriteLockReadWriteLock(&FileNamesLock);

    TRI_RemoveVectorString(&FileNames, n);
    fd = * (int*) TRI_AtVector(&FileDescriptors, n);
    TRI_CLOSE(fd);

    TRI_WriteUnlockReadWriteLock(&FileNamesLock);
  }

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

  if ((fileName[0] > 64 && fileName[0] < 91) || (fileName[0] > 96 && fileName[0] < 123)) {
    if ((fileName[1] != '\0') && (fileName[1] == ':')) {
      if ((fileName[2] != '\0') && (fileName[2] == '/' || fileName[2] == '\\')) {
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
  if ((currentWorkingDirectory[0] > 64 && currentWorkingDirectory[0] < 91) || (currentWorkingDirectory[0] > 96 && currentWorkingDirectory[0] < 123)) {
    if ((currentWorkingDirectory[1] != '\0') && (currentWorkingDirectory[1] == ':')) {
      if ((currentWorkingDirectory[2] != '\0') && (currentWorkingDirectory[2] == '/' || currentWorkingDirectory[2] == '\\')) {
        ok = true;
      }
    }
  }

  if (!ok) {
    return NULL;
  }


  // ...........................................................................
  // Determine the total legnth of the new string
  // ...........................................................................

  cwdLength  = strlen(currentWorkingDirectory);
  fileLength = strlen(fileName);

  if (currentWorkingDirectory[cwdLength - 1] != '\\' && currentWorkingDirectory[cwdLength - 1] != '/') {
    // we do not require a backslash
    result = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, (cwdLength + fileLength + 1) * sizeof(char), false);
    memcpy(result, currentWorkingDirectory, cwdLength);
    memcpy(result + cwdLength, fileName, fileLength);
    result[cwdLength + fileLength] = '\0';
  }
  else {
    // we do require a backslash
    result = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, (cwdLength + fileLength + 2) * sizeof(char), false);
    memcpy(result, currentWorkingDirectory, cwdLength);
    result[cwdLength] = '\\';
    memcpy(result + cwdLength + 1, fileName, fileLength);
    result[cwdLength + fileLength + 1] = '\0';
  }


  return result;
}


#else

char* TRI_GetAbsolutePath (char const* file, char const* cwd) {
  char* result;
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
  assert(cwdLength > 0);

  result = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, (cwdLength + strlen(file) + 2) * sizeof(char), false);
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
/// @brief locates the directory containing the program
////////////////////////////////////////////////////////////////////////////////

char* TRI_LocateBinaryPath (char const* argv0) {
  char const* p;
  char* dir;
  char* binaryPath = NULL;
  size_t i;

  // check if name contains a '/' ( or '\' for windows)
  p = argv0;

  for (;  *p && *p != TRI_DIR_SEPARATOR_CHAR;  ++p) {
  }

  // contains a path
  if (*p) {
    dir = TRI_Dirname(argv0);

    if (dir == 0) {
      binaryPath = TRI_DuplicateString("");
    }
    else {
      binaryPath = TRI_DuplicateString(dir);
      TRI_FreeString(TRI_CORE_MEM_ZONE, dir);
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

  retun result;
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
  size_t bufferSize;
  int res;

  *crc = TRI_InitialCrc32();

  bufferSize = 4096;
  buffer = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, bufferSize, false);

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
      *crc = TRI_BlockCrc32(*crc, buffer, sizeRead);
    }
    else if (sizeRead <= 0) {
      break;
    }
  }

  fclose(fin);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, buffer);

  *crc = TRI_FinalCrc32(*crc);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the system's temporary path
////////////////////////////////////////////////////////////////////////////////

char* TRI_GetTempPath () {
#ifdef _WIN32
#warning TRI_GetTempPath not implemented for Windows
  return TRI_DuplicateString(".");
#else
  return TRI_DuplicateString("/tmp/arangodb");
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

  temp = TRI_GetTempPath();
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
    return TRI_ERROR_INTERNAL;
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

  return TRI_ERROR_INTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
