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

#pragma once

#include <stddef.h>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "Basics/Common.h"
#include "Basics/FileResult.h"
#include "Basics/FileResultString.h"
#include "Basics/Result.h"
#include "Basics/operating-system.h"

#ifdef ARANGODB_HAVE_GETGRGID
#include <grp.h>
#endif

#ifdef ARANGODB_HAVE_GETPWNAM
#include <pwd.h>
#endif

namespace arangodb::basics::FileUtils {

// removes trailing path separators from path, path will be modified in-place
std::string removeTrailingSeparator(std::string const& name);

// normalizes path, path will be modified in-place
void normalizePath(std::string& name);

// makes a path absolute, path will be modified in-place
void makePathAbsolute(std::string& path);

// creates a filename
std::string buildFilename(char const* path, char const* name);

// creates a filename
std::string buildFilename(std::string const& path, std::string const& name);

template<typename... Args>
inline std::string buildFilename(std::string const& path,
                                 std::string const& name, Args... args) {
  return buildFilename(buildFilename(path, name), args...);
}

// reads file into string or buffer
void slurp(std::string const& filename, std::string& result);
std::string slurp(std::string const& filename);

// creates file and writes string to it
void spit(std::string const& filename, char const* ptr, size_t len,
          bool sync = false);
void spit(std::string const& filename, std::string const& content,
          bool sync = false);

// appends to an existing file
void appendToFile(std::string const& filename, char const* ptr, size_t len,
                  bool sync = false);
void appendToFile(std::string const& filename, std::string_view s,
                  bool sync = false);

// if a file could be removed returns TRI_ERROR_NO_ERROR.
// otherwise, returns TRI_ERROR_SYS_ERROR and sets LastError.
[[nodiscard]] ErrorCode remove(std::string const& fileName);

// creates a new directory
bool createDirectory(std::string const& name, ErrorCode* errorNumber = nullptr);
bool createDirectory(std::string const& name, int mask,
                     ErrorCode* errorNumber = nullptr);

/// @brief copies directories / files recursive
/// will not copy files/directories for which the filter function
/// returns true (now wrapper for version below with TRI_copy_recursive_e
/// filter)
bool copyRecursive(std::string const& source, std::string const& target,
                   std::function<bool(std::string const&)> const& filter,
                   std::string& error);

enum TRI_copy_recursive_e { TRI_COPY_IGNORE, TRI_COPY_COPY, TRI_COPY_LINK };

/// @brief copies directories / files recursive
/// will not copy files/directories for which the filter function
/// returns true
bool copyRecursive(
    std::string const& source, std::string const& target,
    std::function<TRI_copy_recursive_e(std::string const&)> const& filter,
    std::string& error);

/// @brief will not copy files/directories for which the filter function
/// returns true
bool copyDirectoryRecursive(
    std::string const& source, std::string const& target,
    std::function<TRI_copy_recursive_e(std::string const&)> const& filter,
    std::string& error);

// returns list of files / subdirectories / links in a directory.
// does not recurse into subdirectories. will throw an exception in
// case the directory cannot be opened for iteration.
std::vector<std::string> listFiles(std::string const& directory);

// returns the number of files / subdirectories / links in a directory.
// does not recurse into subdirectories. will throw an exception in
// case the directory cannot be opened for iteration.
size_t countFiles(std::string const& directory);

// checks if path is a directory
bool isDirectory(std::string const& path);

// checks if path is a symbolic link
bool isSymbolicLink(std::string const& path);

// checks if path is a regular file
bool isRegularFile(std::string const& path);

// checks if path exists
bool exists(std::string const& path);

// returns the size of a file. will return 0 for non-existing files
/// the caller should check first if the file exists via the exists() method
off_t size(std::string const& path);

// strip extension
std::string stripExtension(std::string const& path,
                           std::string const& extension);

// changes into directory
FileResult changeDirectory(std::string const& path);

// returns the current directory
FileResultString currentDirectory();

// returns the home directory
std::string homeDirectory();

// returns the config directory
std::string configDirectory(char const* binaryPath);

// returns the dir name of a path
std::string dirname(std::string const&);

// returns the output of a program
std::string slurpProgram(std::string const& program);

#ifdef ARANGODB_HAVE_GETPWUID
std::optional<uid_t> findUser(std::string const& nameOrId) noexcept;
std::optional<std::string> findUserName(uid_t id) noexcept;
#endif
#ifdef ARANGODB_HAVE_GETGRGID
std::optional<gid_t> findGroup(std::string const& nameOrId) noexcept;
#endif
#ifdef ARANGODB_HAVE_INITGROUPS
void initGroups(std::string const& userName, gid_t groupId) noexcept;
#endif

}  // namespace arangodb::basics::FileUtils
