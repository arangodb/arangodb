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

#include "FileUtils.h"

#ifdef TRI_HAVE_DIRENT_H
#include <dirent.h>
#endif

#ifdef TRI_HAVE_DIRECT_H
#include <direct.h>
#endif

#include "Basics/Exceptions.h"
#include "Basics/OpenFilesTracker.h"
#include "Basics/StringBuffer.h"
#include "Basics/files.h"
#include "Basics/tri-strings.h"
#include "Logger/Logger.h"

#if defined(_WIN32) && defined(_MSC_VER)

#define TRI_DIR_FN(item) item.name

#else

#define TRI_DIR_FN(item) item->d_name

#endif

namespace arangodb {
namespace basics {
namespace FileUtils {

////////////////////////////////////////////////////////////////////////////////
/// @brief removes trailing path separators from path
///
/// path will be modified in-place
////////////////////////////////////////////////////////////////////////////////

std::string removeTrailingSeparator(std::string const& name) {
  size_t endpos = name.find_last_not_of(TRI_DIR_SEPARATOR_CHAR);
  if (endpos != std::string::npos) {
    return name.substr(0, endpos + 1);
  }

  return name;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalizes path
///
/// path will be modified in-place
////////////////////////////////////////////////////////////////////////////////

void normalizePath(std::string& name) {
  std::replace(name.begin(), name.end(), '/', TRI_DIR_SEPARATOR_CHAR);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a filename
////////////////////////////////////////////////////////////////////////////////

std::string buildFilename(char const* path, char const* name) {
  TRI_ASSERT(path != nullptr);
  TRI_ASSERT(name != nullptr);

  std::string result(path);

  if (!result.empty()) {
    result = removeTrailingSeparator(result) + TRI_DIR_SEPARATOR_CHAR;
  }

  if (!result.empty() && *name == TRI_DIR_SEPARATOR_CHAR) {
    // skip initial forward slash in name to avoid having two forward slashes in
    // result
    result.append(name + 1);
  } else {
    result.append(name);
  }
  normalizePath(result);  // in place

  return result;
}

std::string buildFilename(std::string const& path, std::string const& name) {
  std::string result(path);

  if (!result.empty()) {
    result = removeTrailingSeparator(result) + TRI_DIR_SEPARATOR_CHAR;
  }

  if (!result.empty() && !name.empty() && name[0] == TRI_DIR_SEPARATOR_CHAR) {
    // skip initial forward slash in name to avoid having two forward slashes in
    // result
    result.append(name.c_str() + 1, name.size() - 1);
  } else {
    result.append(name);
  }
  normalizePath(result);  // in place

  return result;
}

static void throwFileReadError(int fd, std::string const& filename) {
  TRI_set_errno(TRI_ERROR_SYS_ERROR);
  int res = TRI_errno();

  if (fd >= 0) {
    TRI_TRACKED_CLOSE_FILE(fd);
  }

  std::string message("read failed for file '" + filename + "': " +
                      strerror(res));
  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "" << message;

  THROW_ARANGO_EXCEPTION(TRI_ERROR_SYS_ERROR);
}

std::string slurp(std::string const& filename) {
  int fd = TRI_TRACKED_OPEN_FILE(filename.c_str(), O_RDONLY | TRI_O_CLOEXEC);

  if (fd == -1) {
    throwFileReadError(fd, filename);
  }

  char buffer[10240];
  StringBuffer result(TRI_CORE_MEM_ZONE);

  while (true) {
    ssize_t n = TRI_READ(fd, buffer, sizeof(buffer));

    if (n == 0) {
      break;
    }

    if (n < 0) {
      throwFileReadError(fd, filename);
    }

    result.appendText(buffer, n);
  }

  TRI_TRACKED_CLOSE_FILE(fd);

  std::string r(result.c_str(), result.length());

  return r;
}

void slurp(std::string const& filename, StringBuffer& result) {
  int fd = TRI_TRACKED_OPEN_FILE(filename.c_str(), O_RDONLY | TRI_O_CLOEXEC);

  if (fd == -1) {
    throwFileReadError(fd, filename);
  }

  // reserve space in the output buffer
  off_t fileSize = size(filename);
  if (fileSize > 0) {
    result.reserve((size_t)fileSize);
  }

  char buffer[10240];

  while (true) {
    ssize_t n = TRI_READ(fd, buffer, sizeof(buffer));

    if (n == 0) {
      break;
    }

    if (n < 0) {
      throwFileReadError(fd, filename);
    }

    result.appendText(buffer, n);
  }

  TRI_TRACKED_CLOSE_FILE(fd);
}

static void throwFileWriteError(int fd, std::string const& filename) {
  TRI_set_errno(TRI_ERROR_SYS_ERROR);
  int res = TRI_errno();

  if (fd >= 0) {
    TRI_TRACKED_CLOSE_FILE(fd);
  }

  std::string message("write failed for file '" + filename + "': " +
                      strerror(res));
  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "" << message;

  THROW_ARANGO_EXCEPTION(TRI_ERROR_SYS_ERROR);
}

void spit(std::string const& filename, char const* ptr, size_t len, bool sync) {
  int fd = TRI_TRACKED_CREATE_FILE(filename.c_str(),
                                   O_WRONLY | O_CREAT | O_TRUNC | TRI_O_CLOEXEC,
                                   S_IRUSR | S_IWUSR | S_IRGRP);

  if (fd == -1) {
    throwFileWriteError(fd, filename);
  }

  while (0 < len) {
    ssize_t n = TRI_WRITE(fd, ptr, (TRI_write_t)len);

    if (n < 0) {
      throwFileWriteError(fd, filename);
    }

    ptr += n;
    len -= n;
  }

  if (sync) {
    // intentionally ignore this error as there is nothing we can do about it
    TRI_fsync(fd);

  }

  TRI_TRACKED_CLOSE_FILE(fd);
}

void spit(std::string const& filename, std::string const& content, bool sync) {
  spit(filename, content.c_str(), content.size(), sync);
}

void spit(std::string const& filename, StringBuffer const& content, bool sync) {
  spit(filename, content.c_str(), content.length(), sync);
}

bool remove(std::string const& fileName, int* errorNumber) {
  if (errorNumber != nullptr) {
    *errorNumber = 0;
  }

  int result = std::remove(fileName.c_str());

  if (errorNumber != nullptr) {
    *errorNumber = errno;
  }

  return (result != 0) ? false : true;
}

bool rename(std::string const& oldName, std::string const& newName,
            int* errorNumber) {
  if (errorNumber != nullptr) {
    *errorNumber = 0;
  }

  int result = std::rename(oldName.c_str(), newName.c_str());

  if (errorNumber != nullptr) {
    *errorNumber = errno;
  }

  return (result != 0) ? false : true;
}

bool createDirectory(std::string const& name, int* errorNumber) {
  if (errorNumber != nullptr) {
    *errorNumber = 0;
  }

  return createDirectory(name, 0777, errorNumber);
}

bool createDirectory(std::string const& name, int mask, int* errorNumber) {
  if (errorNumber != nullptr) {
    *errorNumber = 0;
  }

  int result = TRI_MKDIR(name.c_str(), mask);

  int res = errno;
  if (result != 0 && res == EEXIST && isDirectory(name)) {
    result = 0;
  } else if (res != 0) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
  }

  if (errorNumber != nullptr) {
    *errorNumber = res;
  }

  return (result != 0) ? false : true;
}

bool copyRecursive(std::string const& source, std::string const& target,
                   std::string& error) {
  if (isDirectory(source)) {
    return copyDirectoryRecursive(source, target, error);
  }

  return TRI_CopyFile(source, target, error);
}

bool copyDirectoryRecursive(std::string const& source,
                            std::string const& target, std::string& error) {
  bool rc = true;

  auto isSubDirectory = [](std::string const& name) -> bool {
    return isDirectory(name);
  };
#ifdef TRI_HAVE_WIN32_LIST_FILES
  struct _finddata_t oneItem;
  intptr_t handle;

  std::string filter = source + "\\*";
  handle = _findfirst(filter.c_str(), &oneItem);

  if (handle == -1) {
    error = "directory " + source + "not found";
    return false;
  }

  do {
#else
  DIR* filedir = opendir(source.c_str());

  if (filedir == nullptr) {
    error = "directory " + source + "not found";
    return false;
  }

  struct dirent* oneItem = nullptr;

  // do not use readdir_r() here anymore as it is not safe and deprecated
  // in newer versions of libc:
  // http://man7.org/linux/man-pages/man3/readdir_r.3.html
  // the man page recommends to use plain readdir() because it can be expected
  // to be thread-safe in reality, and newer versions of POSIX may require its
  // thread-safety formally, and in addition obsolete readdir_r() altogether
  while ((oneItem = (readdir(filedir))) != nullptr) {
#endif
    // Now iterate over the items.
    // check its not the pointer to the upper directory:
    if (!strcmp(TRI_DIR_FN(oneItem), ".") ||
        !strcmp(TRI_DIR_FN(oneItem), "..")) {
      continue;
    }

    std::string dst = target + TRI_DIR_SEPARATOR_STR + TRI_DIR_FN(oneItem);
    std::string src = source + TRI_DIR_SEPARATOR_STR + TRI_DIR_FN(oneItem);

    // Handle subdirectories:
    if (isSubDirectory(src)) {
      long systemError;
      int rc = TRI_CreateDirectory(dst.c_str(), systemError, error);
      if (rc != TRI_ERROR_NO_ERROR) {
        break;
      }
      if (!copyDirectoryRecursive(src, dst, error)) {
        break;
      }
      if (!TRI_CopyAttributes(src, dst, error)) {
        break;
      }
#ifndef _WIN32
    } else if (isSymbolicLink(src)) {
      if (!TRI_CopySymlink(src, dst, error)) {
        break;
      }
#endif
    } else {
      if (!TRI_CopyFile(src, dst, error)) {
        break;
      }
    }
#ifdef TRI_HAVE_WIN32_LIST_FILES
  } while (_findnext(handle, &oneItem) != -1);

  _findclose(handle);

#else
  }
  closedir(filedir);

#endif
  return rc;
}

std::vector<std::string> listFiles(std::string const& directory) {
  std::vector<std::string> result;

#ifdef TRI_HAVE_WIN32_LIST_FILES

  struct _finddata_t fd;
  intptr_t handle;

  std::string filter = directory + "\\*";
  handle = _findfirst(filter.c_str(), &fd);

  if (handle == -1) {
    return result;
  }

  do {
    if (strcmp(fd.name, ".") != 0 && strcmp(fd.name, "..") != 0) {
      result.push_back(fd.name);
    }
  } while (_findnext(handle, &fd) != -1);

  _findclose(handle);

#else

  DIR* d = opendir(directory.c_str());

  if (d == nullptr) {
    return result;
  }

  dirent* de = readdir(d);

  while (de != nullptr) {
    if (strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0) {
      result.push_back(de->d_name);
    }

    de = readdir(d);
  }

  closedir(d);

#endif

  return result;
}

bool isDirectory(std::string const& path) {
  TRI_stat_t stbuf;
  int res = TRI_STAT(path.c_str(), &stbuf);

#ifdef _WIN32
  return (res == 0) && ((stbuf.st_mode & S_IFMT) == S_IFDIR);
#else
  return (res == 0) && S_ISDIR(stbuf.st_mode);
#endif
}

bool isSymbolicLink(std::string const& path) {
#ifdef TRI_HAVE_WIN32_SYMBOLIC_LINK

  // .....................................................................
  // TODO: On the NTFS file system, there are the following file links:
  // hard links -
  // junctions -
  // symbolic links -
  // .....................................................................
  return false;

#else

  struct stat stbuf;
  int res = TRI_STAT(path.c_str(), &stbuf);

  return (res == 0) && S_ISLNK(stbuf.st_mode);

#endif
}

bool isRegularFile(std::string const& path) {
  TRI_stat_t stbuf;
  int res = TRI_STAT(path.c_str(), &stbuf);
  return (res == 0) && ((stbuf.st_mode & S_IFMT) == S_IFREG);
}

bool exists(std::string const& path) {
  TRI_stat_t stbuf;
  int res = TRI_STAT(path.c_str(), &stbuf);

  return res == 0;
}

off_t size(std::string const& path) {
  int64_t result = TRI_SizeFile(path.c_str());

  if (result < 0) {
    return (off_t)0;
  }

  return (off_t)result;
}

std::string stripExtension(std::string const& path,
                           std::string const& extension) {
  size_t pos = path.rfind(extension);
  if (pos == std::string::npos) {
    return path;
  }

  std::string last = path.substr(pos);
  if (last == extension) {
    return path.substr(0, pos);
  }

  return path;
}

FileResult changeDirectory(std::string const& path) {
  int res = TRI_CHDIR(path.c_str());

  if (res == 0) {
    return FileResult();
  } else {
    return FileResult(errno);
  }
}

FileResultString currentDirectory() {
  size_t len = 1000;
  std::unique_ptr<char[]> current(new char[len]);

  while (TRI_GETCWD(current.get(), (int)len) == nullptr) {
    if (errno == ERANGE) {
      len += 1000;
      current.reset(new char[len]);
    } else {
      return FileResultString(errno, ".");
    }
  }

  std::string result = current.get();

  return FileResultString(result);
}

std::string homeDirectory() {
  char* dir = TRI_HomeDirectory();
  std::string result = dir;
  TRI_FreeString(TRI_CORE_MEM_ZONE, dir);

  return result;
}

std::string configDirectory(char const* binaryPath) {
  char* dir = TRI_LocateConfigDirectory(binaryPath);

  if (dir == nullptr) {
    return currentDirectory().result();
  }

  std::string result = dir;
  TRI_FreeString(TRI_CORE_MEM_ZONE, dir);

  return result;
}

std::string dirname(std::string const& name) {
  char* result = TRI_Dirname(name.c_str());
  std::string base;

  if (result != nullptr) {
    base = result;
    TRI_FreeString(TRI_CORE_MEM_ZONE, result);
  }

  return base;
}

void makePathAbsolute(std::string& path) {
  std::string cwd = FileUtils::currentDirectory().result();

  if (path.empty()) {
    path = cwd;
  } else {
    char* p = TRI_GetAbsolutePath(path.c_str(), cwd.c_str());

    if (p != nullptr) {
      path = p;
      TRI_FreeString(TRI_CORE_MEM_ZONE, p);
    }
  }
}
}
}
}
