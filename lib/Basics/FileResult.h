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

#ifndef ARANGODB_BASICS_FILE_RESULT_H
#define ARANGODB_BASICS_FILE_RESULT_H 1

#include "Basics/Result.h"

namespace arangodb {
class FileResult {
 public:
  FileResult() : _result(), _sysErrorNumber(0) {}

  explicit FileResult(int sysErrorNumber);

  // forwarded methods
  bool ok() const { return _result.ok(); }
  bool fail() const { return _result.fail(); }
  int errorNumber() const { return _result.errorNumber(); }
  std::string errorMessage() const { return _result.errorMessage(); }

 public:
  int sysErrorNumber() const { return _sysErrorNumber; }

 protected:
  Result _result;
  int const _sysErrorNumber;
};
}  // namespace arangodb

#endif
