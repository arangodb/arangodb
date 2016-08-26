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

#include "replication-common.h"
#include "Basics/tri-strings.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a timestamp string in a target buffer
////////////////////////////////////////////////////////////////////////////////

void TRI_GetTimeStampReplication(char* dst, size_t maxLength) {
  struct tm tb;
  time_t tt = time(nullptr);
  TRI_gmtime(tt, &tb);

  strftime(dst, maxLength, "%Y-%m-%dT%H:%M:%SZ", &tb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a timestamp string in a target buffer
////////////////////////////////////////////////////////////////////////////////

void TRI_GetTimeStampReplication(double timeStamp, char* dst,
                                 size_t maxLength) {
  struct tm tb;
  time_t tt = static_cast<time_t>(timeStamp);
  TRI_gmtime(tt, &tb);

  strftime(dst, maxLength, "%Y-%m-%dT%H:%M:%SZ", &tb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determine whether a collection should be included in replication
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExcludeCollectionReplication(char const* name, bool includeSystem) {
  if (name == nullptr) {
    // name invalid
    return true;
  }

  if (*name != '_') {
    // all regular collections are included
    return false;
  }

  if (!includeSystem) {
    // do not include any system collections
    return true;
  }

  if (TRI_IsPrefixString(name, "_statistics") ||
      TRI_EqualString(name, "_apps") ||
      TRI_EqualString(name, "_configuration") ||
      TRI_EqualString(name, "_cluster_kickstarter_plans") ||
      TRI_EqualString(name, "_foxxlog") || TRI_EqualString(name, "_jobs") ||
      TRI_EqualString(name, "_queues") || TRI_EqualString(name, "_sessions")) {
    // these system collections will always be excluded
    return true;
  }

  return false;
}
