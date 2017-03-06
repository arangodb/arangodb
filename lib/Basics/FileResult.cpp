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

#include "FileResult.h"

namespace arangodb {
FileResult::FileResult(bool state)
    : Result(), _state(state), _sysErrorNumber(0) {}

FileResult::FileResult(bool state, int sysErrorNumber)
    : Result(TRI_ERROR_SYS_ERROR, strerror(sysErrorNumber)),
      _state(state), _sysErrorNumber(sysErrorNumber) {}
}
