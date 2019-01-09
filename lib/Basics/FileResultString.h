////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_BASICS_FILE_RESULT_STRING_H
#define ARANGODB_BASICS_FILE_RESULT_STRING_H 1

#include "Basics/Result.h"

namespace arangodb {
class FileResultString : public FileResult {
 public:
  FileResultString(std::string const& result) : FileResult(), _result(result) {}

  FileResultString(int sysErrorNumber, std::string const& result)
      : FileResult(sysErrorNumber), _result(result) {}

  FileResultString(int sysErrorNumber)
      : FileResult(sysErrorNumber), _result() {}

 public:
  std::string const& result() const { return _result; }

 protected:
  std::string const _result;
};
}  // namespace arangodb

#endif
