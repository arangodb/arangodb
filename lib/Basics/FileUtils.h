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

#ifndef ARANGODB_BASICS_FILE_UTILS_H
#define ARANGODB_BASICS_FILE_UTILS_H 1

#include "Basics/Common.h"

#include "Basics/FileResult.h"
#include "Basics/FileResultString.h"
#include "Basics/files.h"

namespace arangodb {
namespace basics {
class StringBuffer;

namespace FileUtils {

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

template <typename... Args>
inline std::string buildFilename(std::string const& path, std::string const& name, Args... args) {
  return buildFilename(buildFilename(path, name), args...);
}

// reads file into string
std::string slurp(std::string const& filename);
void slurp(std::string const& filename, StringBuffer&);

// creates file and writes string to it
void spit(std::string const& filename, char const* ptr, size_t len, bool sync = false);
void spit(std::string const& filename, std::string const& content, bool sync = false);
void spit(std::string const& filename, StringBuffer const& content, bool sync = false);

// returns true if a file could be removed
bool remove(std::string const& fileName, int* errorNumber = 0);

// returns true if a file could be renamed
bool rename(std::string const& oldName, std::string const& newName,
            int* errorNumber = 0);

// creates a new directory
bool createDirectory(std::string const& name, int* errorNumber = 0);
bool createDirectory(std::string const& name, int mask, int* errorNumber = 0);

// copies directories / files recursive
bool copyRecursive(std::string const& source, std::string const& target,
                   std::string& error);
bool copyDirectoryRecursive(std::string const& source,
                            std::string const& target, std::string& error);

// returns list of files
std::vector<std::string> listFiles(std::string const& directory);

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
}
}
}

#endif
