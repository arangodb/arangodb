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
#include <filesystem>

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

// makes a path absolute
std::filesystem::path absolutePath(std::filesystem::path path);

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

// strip extension
std::string stripExtension(std::string const& path,
                           std::string const& extension);

// returns the home directory
std::string homeDirectory();

// returns the config directory
std::string configDirectory(char const* binaryPath);

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

// This is specific to cgroupFiles
std::optional<int64_t> readCgroupFileValue(const std::string& path);

}  // namespace arangodb::basics::FileUtils
