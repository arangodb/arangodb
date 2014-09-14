////////////////////////////////////////////////////////////////////////////////
/// @brief replication functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "replication-common.h"

#include "Basics/files.h"
#include "Basics/tri-strings.h"
#include "VocBase/collection.h"
#include "VocBase/vocbase.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                       REPLICATION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a timestamp string in a target buffer
////////////////////////////////////////////////////////////////////////////////

void TRI_GetTimeStampReplication (char* dst,
                                  size_t maxLength) {
  time_t tt;
  struct tm tb;

  tt = time(0);
  TRI_gmtime(tt, &tb);

  strftime(dst, maxLength, "%Y-%m-%dT%H:%M:%SZ", &tb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determine whether a collection should be included in replication
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExcludeCollectionReplication (const char* name) {
  if (name == nullptr) {
    // name invalid
    return true;
  }

  if (*name != '_') {
    // all regular collections are included
    return false;
  }

  if (TRI_EqualString(name, TRI_COL_NAME_REPLICATION) ||
      TRI_EqualString(name, TRI_COL_NAME_TRANSACTION) ||
      TRI_EqualString(name, TRI_COL_NAME_USERS) ||
      TRI_IsPrefixString(name, TRI_COL_NAME_STATISTICS) ||
      TRI_EqualString(name, "_aal") ||
      TRI_EqualString(name, "_configuration") ||
      TRI_EqualString(name, "_cluster_kickstarter_plans") ||
      TRI_EqualString(name, "_fishbowl") ||
      TRI_EqualString(name, "_modules") ||
      TRI_EqualString(name, "_routing")) {
    // these system collections will be excluded
    return true;
  }

  return false;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
