////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_BASICS_ARANGO_GLOBAL_CONTEXT_H
#define ARANGODB_BASICS_ARANGO_GLOBAL_CONTEXT_H 1

#include <string>
#include <vector>

#include "Basics/Common.h"

namespace arangodb {
class ArangoGlobalContext {
 public:
  static ArangoGlobalContext* CONTEXT;

 public:
  ArangoGlobalContext(int argc, char* argv[], char const* installDirectory);
  ~ArangoGlobalContext();

 public:
  std::string binaryName() const { return _binaryName; }
  std::string runRoot() const { return _runRoot; }
  void createMiniDumpFilename();
  void normalizePath(std::vector<std::string>& path, char const* whichPath, bool fatal);
  void normalizePath(std::string& path, char const* whichPath, bool fatal);
  std::string const& getBinaryPath() const { return _binaryPath; }
  int exit(int ret);
  void installHup();

 private:
  std::string const _binaryName;
  std::string const _binaryPath;
  std::string const _runRoot;
  int _ret;
};
}  // namespace arangodb

#endif
