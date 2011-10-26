////////////////////////////////////////////////////////////////////////////////
/// @brief file functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011-2010, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "files.h"

#ifdef TRI_HAVE_DIRENT_H
#include <dirent.h>
#endif

#ifdef TRI_HAVE_DIRECT_H
#include <direct.h>
#endif

#include <Basics/logging.h>
#include <Basics/strings.h>

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Files File Utilities
/// @{
////////////////////////////////////////////////////////////////////////////////

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
/// @brief checks if file exists
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExistsFile (char const* path) {
  struct stat stbuf;
  int res;

  res = stat(path, &stbuf);

  return res == 0;
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
/// @brief extracts the dirname
////////////////////////////////////////////////////////////////////////////////

char* TRI_Dirname (char const* path) {
  size_t n;
  size_t m;
  char const* p;

  n = strlen(path);
  m = 0;

  if (1 < n) {
    if (path[n - 1] == '/') {
      m = 1;
    }
  }

  if (n == 0) {
    return TRI_DuplicateString(".");
  }
  else if (n == 1 && *path == '/') {
    return TRI_DuplicateString("/");
  }
  else if (n - m == 1 && *path == '.') {
    return TRI_DuplicateString(".");
  }
  else if (n - m == 2 && path[0] == '.' && path[1] == '.') {
    return TRI_DuplicateString("..");
  }

  for (p = path + (n - m - 1);  path < p;  --p) {
    if (*p == '/') {
      break;
    }
  }

  if (path == p) {
    if (*p == '/') {
      return TRI_DuplicateString("/");
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
    if (path[n - 1] == '/') {
      m = 1;
    }
  }

  if (n == 0) {
    return TRI_DuplicateString("");
  }
  else if (n == 1 && *path == '/') {
    return TRI_DuplicateString("");
  }
  else if (n - m == 1 && *path == '.') {
    return TRI_DuplicateString("");
  }
  else if (n - m == 2 && path[0] == '.' && path[1] == '.') {
    return TRI_DuplicateString("");
  }

  for (p = path + (n - m - 1);  path < p;  --p) {
    if (*p == '/') {
      break;
    }
  }

  if (path == p) {
    if (*p == '/') {
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
  return TRI_Concatenate3String(path, "/", name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a filename
////////////////////////////////////////////////////////////////////////////////

char* TRI_Concatenate3File (char const* path1, char const* path2, char const* name) {
  return TRI_Concatenate5String(path1, "/", path2, "/", name);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a list of files in path
////////////////////////////////////////////////////////////////////////////////

TRI_vector_string_t TRI_FilesDirectory (char const* path) {
  TRI_vector_string_t result;

#ifdef TRI_HAVE_WIN32_LIST_FILES

  struct _finddata_t fd;
  intptr_t handle;
  string filter;

  TRI_InitVectorString(&result);

  filter = directory + "\\*";
  handle = _findfirst(filter.c_str(), &fd);

  if (handle == -1) {
    return result;
  }

  do {
    if (strcmp(fd.name, ".") != 0 && strcmp(fd.name, "..") != 0) {
      result.push_back(fd.name);
    }
  } while(_findnext(handle, &fd) != -1);

  _findclose(handle);

#else

  DIR * d;
  struct dirent * de;

  TRI_InitVectorString(&result);

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

#endif

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a file
////////////////////////////////////////////////////////////////////////////////

bool TRI_RenameFile (char const* old, char const* filename) {
  int res;

  res = rename(old, filename);

  if (res != 0) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG_TRACE("cannot rename file from '%s' to '%s': %s", old, filename, TRI_LAST_ERROR_STR);
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlinks a file
////////////////////////////////////////////////////////////////////////////////

bool TRI_UnlinkFile (char const* filename) {
  int res;

  res = unlink(filename);

  if (res != 0) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    LOG_TRACE("cannot unlink file '%s': %s", filename, TRI_LAST_ERROR_STR);
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads into a buffer from a file
////////////////////////////////////////////////////////////////////////////////

bool TRI_ReadPointer (int fd, void* buffer, size_t length) {
  char* ptr;

  ptr = buffer;

  while (0 < length) {
    ssize_t n = read(fd, ptr, length);

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
    ssize_t n = write(fd, ptr, length);

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
    res = = fcntl(fd, F_FULLFSYNC, 0);
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
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
