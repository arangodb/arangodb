////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <utility>

#include "FileUtils.h"

#include "Basics/operating-system.h"

#ifdef TRI_HAVE_DIRENT_H
#include <dirent.h>
#endif

#ifdef TRI_HAVE_DIRECT_H
#include <direct.h>
#endif
#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/stat.h>

#include <unicode/unistr.h>

#include "Basics/Exceptions.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StringBuffer.h"
#include "Basics/debugging.h"
#include "Basics/error.h"
#include "Basics/files.h"
#include "Basics/voc-errors.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

namespace {
std::function<bool(std::string const&)> const passAllFilter =
    [](std::string const&) { return false; };
}

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

#ifdef _WIN32
  // for Windows the situation is a bit more complicated,
  // as a mere replacement of all forward slashes to backslashes
  // may leave us with a double backslash for sequences like "bla/\foo".
  // in this case we collapse duplicate dir separators to a single one.
  // we intentionally ignore the first 2 characters, because they may
  // contain a network share filename such as "\\foo\bar"

  size_t const n = name.size();
  size_t out = 0;

  for (size_t i = 0; i < n; ++i) {
    if (name[i] == TRI_DIR_SEPARATOR_CHAR && out > 1 && name[out - 1] == TRI_DIR_SEPARATOR_CHAR) {
      continue;
    }
    name[out++] = name[i];
  }

  if (out != n) {
    name.resize(out);
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a filename
////////////////////////////////////////////////////////////////////////////////

std::string buildFilename(char const* path, char const* name) {
  TRI_ASSERT(path != nullptr);
  TRI_ASSERT(name != nullptr);

  std::string result(path);

  if (!result.empty()) {
    result = removeTrailingSeparator(result);
    if (result.length() != 1 || result[0] != TRI_DIR_SEPARATOR_CHAR) {
      result += TRI_DIR_SEPARATOR_CHAR;
    }
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
    result = removeTrailingSeparator(result);
    if (result.length() != 1 || result[0] != TRI_DIR_SEPARATOR_CHAR) {
      result += TRI_DIR_SEPARATOR_CHAR;
    }
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

static void throwFileReadError(std::string const& filename) {
  TRI_set_errno(TRI_ERROR_SYS_ERROR);
  int res = TRI_errno();

  std::string message("read failed for file '" + filename + "': " + strerror(res));
  LOG_TOPIC("a0898", TRACE, arangodb::Logger::FIXME) << message;

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_SYS_ERROR, message);
}

static void fillStringBuffer(int fd, std::string const& filename,
                             StringBuffer& result, size_t chunkSize) {
  while (true) {
    if (result.reserve(chunkSize) != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
    TRI_read_return_t n = TRI_READ(fd, result.end(), static_cast<TRI_read_t>(chunkSize));

    if (n == 0) {
      break;
    }

    if (n < 0) {
      throwFileReadError(filename);
    }

    result.increaseLength(n);
  }
}

std::string slurp(std::string const& filename) {
  int fd = TRI_OPEN(filename.c_str(), O_RDONLY | TRI_O_CLOEXEC);

  if (fd == -1) {
    throwFileReadError(filename);
  }

  TRI_DEFER(TRI_CLOSE(fd));

  constexpr size_t chunkSize = 8192;
  StringBuffer buffer(chunkSize, false);

  fillStringBuffer(fd, filename, buffer, chunkSize);

  return std::string(buffer.data(), buffer.length());
}

void slurp(std::string const& filename, StringBuffer& result) {
  int fd = TRI_OPEN(filename.c_str(), O_RDONLY | TRI_O_CLOEXEC);

  if (fd == -1) {
    throwFileReadError(filename);
  }

  TRI_DEFER(TRI_CLOSE(fd));

  result.reset();
  constexpr size_t chunkSize = 8192;

  fillStringBuffer(fd, filename, result, chunkSize);
}

Result slurpNoEx(std::string const& filename, StringBuffer& result) {
  int fd = TRI_OPEN(filename.c_str(), O_RDONLY | TRI_O_CLOEXEC);

  if (fd == -1) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    int res = TRI_errno();
    std::string message("read failed for file '" + filename + "': " + strerror(res));
    LOG_TOPIC("a1898", TRACE, arangodb::Logger::FIXME) << message;
    return {TRI_ERROR_SYS_ERROR, message};
  }

  TRI_DEFER(TRI_CLOSE(fd));

  result.reset();
  constexpr size_t chunkSize = 8192;
  fillStringBuffer(fd, filename, result, chunkSize);
  return {};
}

Result slurp(std::string const& filename, std::string& result) {
  constexpr size_t chunkSize = 8192;
  StringBuffer buffer(chunkSize, false);

  auto status = slurpNoEx(filename, buffer);

  result = std::string(buffer.data(), buffer.length());
  return status;
}

static void throwFileWriteError(std::string const& filename) {
  TRI_set_errno(TRI_ERROR_SYS_ERROR);

  std::string message("write failed for file '" + filename + "': " + TRI_last_error());
  LOG_TOPIC("a8930", TRACE, arangodb::Logger::FIXME) << "" << message;

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_SYS_ERROR, message);
}

void spit(std::string const& filename, char const* ptr, size_t len, bool sync) {
  int fd = TRI_CREATE(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC | TRI_O_CLOEXEC,
                      S_IRUSR | S_IWUSR | S_IRGRP);

  if (fd == -1) {
    throwFileWriteError(filename);
  }

  TRI_DEFER(TRI_CLOSE(fd));

  while (0 < len) {
    ssize_t n = TRI_WRITE(fd, ptr, static_cast<TRI_write_t>(len));

    if (n < 0) {
      throwFileWriteError(filename);
    }

    ptr += n;
    len -= n;
  }

  if (sync) {
    // intentionally ignore this error as there is nothing we can do about it
    TRI_fsync(fd);
  }
}

void spit(std::string const& filename, std::string const& content, bool sync) {
  spit(filename, content.data(), content.size(), sync);
}

void spit(std::string const& filename, StringBuffer const& content, bool sync) {
  spit(filename, content.data(), content.length(), sync);
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

  auto result = TRI_MKDIR(name.c_str(), static_cast<mode_t>(mask));

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

/// @brief will not copy files/directories for which the filter function
/// returns true (now wrapper for version below with TRI_copy_recursive_e filter)
bool copyRecursive(std::string const& source, std::string const& target,
                   std::function<bool(std::string const&)> const& filter,
                   std::string& error) {

  // "auto lambda" will not work here
  std::function<TRI_copy_recursive_e(std::string const&)> lambda =
    [&filter] (std::string const& pathname) -> TRI_copy_recursive_e {
    return filter(pathname) ? TRI_COPY_IGNORE : TRI_COPY_COPY;
  };

  return copyRecursive(source, target, lambda, error);

} // copyRecursive (bool filter())


/// @brief will not copy files/directories for which the filter function
/// returns true
bool copyRecursive(std::string const& source, std::string const& target,
                   std::function<TRI_copy_recursive_e(std::string const&)> const& filter,
                   std::string& error) {

  bool ret_bool = false;

  if (isDirectory(source)) {
    ret_bool = copyDirectoryRecursive(source, target, filter, error);
  } else {
    switch (filter(source)) {
      case TRI_COPY_IGNORE:
        ret_bool = true;  // original TRI_ERROR_NO_ERROR implies "false", seems wrong
        break;

      case TRI_COPY_COPY:
        ret_bool = TRI_CopyFile(source, target, error);
        break;

      case TRI_COPY_LINK:
        ret_bool = TRI_CreateHardlink(source, target, error);
        break;

      default:
        ret_bool = false; // TRI_ERROR_BAD_PARAMETER seems wrong since returns "true"
        break;
    } // switch
  } // else

  return ret_bool;
} // copyRecursive (TRI_copy_recursive_e filter())


/// @brief will not copy files/directories for which the filter function
/// returns true
bool copyDirectoryRecursive(std::string const& source, std::string const& target,
                            std::function<TRI_copy_recursive_e(std::string const&)> const& filter,
                            std::string& error) {
  char* fn = nullptr;
  bool rc_bool = true;

  auto isSubDirectory = [](std::string const& name) -> bool {
    return isDirectory(name);
  };
#ifdef TRI_HAVE_WIN32_LIST_FILES
  struct _wfinddata_t oneItem;
  intptr_t handle;

  std::string rcs;
  std::string flt = source + "\\*";

  icu::UnicodeString f(flt.c_str());

  handle = _wfindfirst(reinterpret_cast<wchar_t const*>(f.getTerminatedBuffer()), &oneItem);

  if (handle == -1) {
    error = "directory " + source + " not found";
    return false;
  }

  do {
    rcs.clear();
    icu::UnicodeString d((wchar_t*)oneItem.name, static_cast<int32_t>(wcslen(oneItem.name)));
    d.toUTF8String<std::string>(rcs);
    fn = (char*)rcs.c_str();
#else
  DIR* filedir = opendir(source.c_str());

  if (filedir == nullptr) {
    error = "directory " + source + " not found";
    return false;
  }

  struct dirent* oneItem = nullptr;

  // do not use readdir_r() here anymore as it is not safe and deprecated
  // in newer versions of libc:
  // http://man7.org/linux/man-pages/man3/readdir_r.3.html
  // the man page recommends to use plain readdir() because it can be expected
  // to be thread-safe in reality, and newer versions of POSIX may require its
  // thread-safety formally, and in addition obsolete readdir_r() altogether
  while ((oneItem = (readdir(filedir))) != nullptr && rc_bool) {
    fn = oneItem->d_name;
#endif

    // Now iterate over the items.
    // check its not the pointer to the upper directory:
    if (!strcmp(fn, ".") || !strcmp(fn, "..")) {
      continue;
    }
    std::string dst = target + TRI_DIR_SEPARATOR_STR + fn;
    std::string src = source + TRI_DIR_SEPARATOR_STR + fn;

    switch (filter(src)) {
      case TRI_COPY_IGNORE:
        break;

      case TRI_COPY_COPY:
        // Handle subdirectories:
        if (isSubDirectory(src)) {
          long systemError;
          int rc = TRI_CreateDirectory(dst.c_str(), systemError, error);
          if (rc != TRI_ERROR_NO_ERROR && rc != TRI_ERROR_FILE_EXISTS) {
            rc_bool = false;
            break;
          }
          if (!copyDirectoryRecursive(src, dst, filter, error)) {
            rc_bool = false;
            break;
          }
          if (!TRI_CopyAttributes(src, dst, error)) {
            rc_bool = false;
            break;
          }
#ifndef _WIN32
        } else if (isSymbolicLink(src)) {
          if (!TRI_CopySymlink(src, dst, error)) {
            rc_bool = false;
          }
#endif
        } else {
          if (!TRI_CopyFile(src, dst, error)) {
            rc_bool = false;
          }
        }
        break;

      case TRI_COPY_LINK:
        if (!TRI_CreateHardlink(src, dst, error)) {
          rc_bool = false;
        } // if
        break;
    } // switch
#ifdef TRI_HAVE_WIN32_LIST_FILES
  } while (_wfindnext(handle, &oneItem) != -1 && rc_bool);

  _findclose(handle);

#else
  }
  closedir(filedir);

#endif
  return rc_bool;
}

std::vector<std::string> listFiles(std::string const& directory) {
  std::vector<std::string> result;

#ifdef TRI_HAVE_WIN32_LIST_FILES
  char* fn = nullptr;

  struct _wfinddata_t oneItem;
  intptr_t handle;
  std::string rcs;

  std::string filter = directory + "\\*";
  icu::UnicodeString f(filter.c_str());
  handle = _wfindfirst(reinterpret_cast<wchar_t const*>(f.getTerminatedBuffer()), &oneItem);

  if (handle == -1) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    int res = TRI_errno();

    std::string message("failed to enumerate files in directory '" + directory +
                        "': " + strerror(res));
    THROW_ARANGO_EXCEPTION_MESSAGE(res, message);
  }

  do {
    rcs.clear();
    icu::UnicodeString d((wchar_t*)oneItem.name, static_cast<int32_t>(wcslen(oneItem.name)));
    d.toUTF8String<std::string>(rcs);
    fn = (char*)rcs.c_str();

    if (!strcmp(fn, ".") || !strcmp(fn, "..")) {
      continue;
    }

    result.push_back(rcs);
  } while (_wfindnext(handle, &oneItem) != -1);

  _findclose(handle);

#else

  DIR* d = opendir(directory.c_str());

  if (d == nullptr) {
    TRI_set_errno(TRI_ERROR_SYS_ERROR);
    int res = TRI_errno();

    std::string message("failed to enumerate files in directory '" + directory +
                        "': " + strerror(res));
    THROW_ARANGO_EXCEPTION_MESSAGE(res, message);
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

std::string stripExtension(std::string const& path, std::string const& extension) {
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

std::string homeDirectory() { return TRI_HomeDirectory(); }

std::string configDirectory(char const* binaryPath) {
  std::string dir = TRI_LocateConfigDirectory(binaryPath);

  if (dir.empty()) {
    return currentDirectory().result();
  }

  return dir;
}

std::string dirname(std::string const& name) { return TRI_Dirname(name); }

void makePathAbsolute(std::string& path) {
  std::string cwd = FileUtils::currentDirectory().result();

  if (path.empty()) {
    path = cwd;
  } else {
    std::string p = TRI_GetAbsolutePath(path, cwd);
    if (!p.empty()) {
      path = p;
    }
  }
}

static void throwProgramError(std::string const& filename) {
  TRI_set_errno(TRI_ERROR_SYS_ERROR);
  int res = TRI_errno();

  std::string message("open failed for file '" + filename + "': " + strerror(res));
  LOG_TOPIC("a557b", TRACE, arangodb::Logger::FIXME) << message;

  THROW_ARANGO_EXCEPTION(TRI_ERROR_SYS_ERROR);
}

std::string slurpProgram(std::string const& program) {
#ifdef _WIN32
  icu::UnicodeString uprog(program.c_str(), static_cast<int32_t>(program.length()));
  FILE* fp = _wpopen(reinterpret_cast<wchar_t const*>(uprog.getTerminatedBuffer()), L"r");
#else
  FILE* fp = popen(program.c_str(), "r");
#endif

  constexpr size_t chunkSize = 8192;
  StringBuffer buffer(chunkSize, false);

  if (fp) {
    int c;

    while ((c = getc(fp)) != EOF) {
      buffer.appendChar((char)c);
    }

#ifdef _WIN32
    int res = _pclose(fp);
#else
    int res = pclose(fp);
#endif

    if (res != 0) {
      throwProgramError(program);
    }
  } else {
    throwProgramError(program);
  }

  return std::string(buffer.data(), buffer.length());
}

int slurpProgramWithExitcode(std::string const& program, std::string& output) {
#ifdef _WIN32
  icu::UnicodeString uprog(program.c_str(), static_cast<int32_t>(program.length()));
  FILE* fp = _wpopen(reinterpret_cast<wchar_t const*>(uprog.getTerminatedBuffer()), L"r");
#else
  FILE* fp = popen(program.c_str(), "r");
#endif

  constexpr size_t chunkSize = 8192;
  StringBuffer buffer(chunkSize, false);

  if (fp) {
    int c;

    while ((c = getc(fp)) != EOF) {
      buffer.appendChar((char)c);
    }

#ifdef _WIN32
    int res = _pclose(fp);
#else
    int res = pclose(fp);
#endif

    output = std::string(buffer.data(), buffer.length());
    return res;
  }

  throwProgramError(program);
  return 1;   // Just to please the compiler.
};

}  // namespace FileUtils
}  // namespace basics
}  // namespace arangodb
