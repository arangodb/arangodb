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

#ifndef ARANGODB_BASICS_FILE_RESULT_H
#define ARANGODB_BASICS_FILE_RESULT_H 1

#include "Basics/Result.h"

namespace arangodb {
class FileResult : public Result {
 public:
  FileResult() : Result(), _sysErrorNumber(0) {}

  explicit FileResult(int sysErrorNumber)
      : Result(TRI_ERROR_SYS_ERROR, strerror(sysErrorNumber)),
        _sysErrorNumber(sysErrorNumber) {}

 public:
  int sysErrorNumber() const { return _sysErrorNumber; }

 protected:
  int const _sysErrorNumber;
};
}  // namespace arangodb

#endif
