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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include <time.h>

#include "Basics/system-functions.h"
#include "Replication/common-defines.h"

namespace arangodb {

/// @brief generate a timestamp string in a target buffer
void TRI_GetTimeStampReplication(char* dst, size_t maxLength) {
  struct tm tb;
  time_t tt = time(nullptr);
  TRI_gmtime(tt, &tb);

  strftime(dst, maxLength, "%Y-%m-%dT%H:%M:%SZ", &tb);
}

/// @brief generate a timestamp string in a target buffer
void TRI_GetTimeStampReplication(double timeStamp, char* dst, size_t maxLength) {
  struct tm tb;
  time_t tt = static_cast<time_t>(timeStamp);
  TRI_gmtime(tt, &tb);

  strftime(dst, maxLength, "%Y-%m-%dT%H:%M:%SZ", &tb);
}

bool TRI_ExcludeCollectionReplication(std::string const& name, bool includeSystem,
                                      bool includeFoxxQueues) {
  if (name.empty()) {
    // should not happen...
    return true;
  }

  if (name[0] != '_') {
    // all regular collections are included
    return false;
  }

  if (!includeSystem) {
    // do not include any system collections
    return true;
  }

  if (name.compare(0, 11, "_statistics") == 0 ||
      name == "_routing") {
    // these system collections will always be excluded
    return true;
  } 
  
  if (!includeFoxxQueues && (name == "_jobs" || name == "_queues")) {
    return true;
  }

  return false;
}

}  // namespace arangodb
