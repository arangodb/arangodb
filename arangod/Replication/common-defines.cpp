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

#include "Replication/common-defines.h"
#include "Basics/tri-strings.h"
#include <time.h>

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

  if (TRI_IsPrefixString(name.c_str(), "_statistics") ||
      name == "_configuration" || name == "_frontend" ||
      name == "_cluster_kickstarter_plans" || name == "_routing" ||
      name == "_fishbowl" || name == "_foxxlog" || name == "_sessions") {
    // these system collections will always be excluded
    return true;
  } else if (!includeFoxxQueues && (name == "_jobs" || name == "_queues")) {
    return true;
  }

  return false;
}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
char const* TRI_TranslateMarkerTypeReplication(TRI_replication_operation_e type) {
  switch (type) {
    case REPLICATION_DATABASE_CREATE: return "REPLICATION_DATABASE_CREATE";
    case REPLICATION_DATABASE_DROP: return "REPLICATION_DATABASE_DROP";
    case REPLICATION_COLLECTION_CREATE: return "REPLICATION_COLLECTION_CREATE";
    case REPLICATION_COLLECTION_DROP: return "REPLICATION_COLLECTION_DROP";
    case REPLICATION_COLLECTION_RENAME: return "REPLICATION_COLLECTION_RENAME";
    case REPLICATION_COLLECTION_CHANGE: return "REPLICATION_COLLECTION_CHANGE";
    case REPLICATION_COLLECTION_TRUNCATE: return "REPLICATION_COLLECTION_TRUNCATE";
    case REPLICATION_INDEX_CREATE: return "REPLICATION_INDEX_CREATE";
    case REPLICATION_INDEX_DROP: return "REPLICATION_INDEX_DROP";
    case REPLICATION_VIEW_CREATE: return "REPLICATION_VIEW_CREATE";
    case REPLICATION_VIEW_DROP: return "REPLICATION_VIEW_DROP";
    case REPLICATION_VIEW_CHANGE: return "REPLICATION_VIEW_CHANGE";
    case REPLICATION_TRANSACTION_START: return "REPLICATION_TRANSACTION_START";
    case REPLICATION_TRANSACTION_COMMIT: return "REPLICATION_TRANSACTION_COMMIT";
    case REPLICATION_TRANSACTION_ABORT: return "REPLICATION_TRANSACTION_ABORT";
    case REPLICATION_MARKER_DOCUMENT: return "REPLICATION_MARKER_DOCUMENT";
    case REPLICATION_MARKER_REMOVE: return "REPLICATION_MARKER_REMOVE";
    default: return "INVALID";
  }
}
#endif

}  // namespace arangodb
