////////////////////////////////////////////////////////////////////////////////
/// @brief replication functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "replication-common.h"

#include "BasicsC/files.h"
#include "BasicsC/tri-strings.h"
#include "VocBase/collection.h"
#include "VocBase/vocbase.h"


#ifdef TRI_ENABLE_REPLICATION

// -----------------------------------------------------------------------------
// --SECTION--                                                       REPLICATION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Replication
/// {
////////////////////////////////////////////////////////////////////////////////

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
  if (TRI_EqualString(name, TRI_COL_NAME_DATABASES)) {
    return true;
  }
  
  if (TRI_EqualString(name, TRI_COL_NAME_ENDPOINTS)) {
    return true;
  }
  
  if (TRI_EqualString(name, TRI_COL_NAME_PREFIXES)) {
    return true;
  }

  if (TRI_EqualString(name, TRI_COL_NAME_REPLICATION)) {
    return true;
  }

  if (TRI_EqualString(name, TRI_COL_NAME_TRANSACTION)) {
    return true;
  }
  
  if (TRI_EqualString(name, TRI_COL_NAME_USERS)) {
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
