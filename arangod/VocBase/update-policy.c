////////////////////////////////////////////////////////////////////////////////
/// @brief document update / delete policies
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

#include "update-policy.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise a policy object
////////////////////////////////////////////////////////////////////////////////

void TRI_InitUpdatePolicy (TRI_doc_update_policy_t* const policy,
                           TRI_doc_update_policy_e type,
                           TRI_voc_rid_t expectedRid,
                           TRI_voc_rid_t* previousRid) {
  policy->_policy = type;
  policy->_expectedRid = expectedRid;
  policy->_previousRid = previousRid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare revision of found document with revision specified in policy
/// this will also store the actual revision id found in the database in the
/// context variable _previousRid, but only if this is not NULL
////////////////////////////////////////////////////////////////////////////////

int TRI_CheckUpdatePolicy (const TRI_doc_update_policy_t* const policy,
                           const TRI_voc_rid_t actualRid) {

  if (policy == NULL) {
    return TRI_ERROR_NO_ERROR;
  }

  // store previous revision
  if (policy->_previousRid != NULL) {
    *(policy->_previousRid) = actualRid;
  }

  // check policy
  switch (policy->_policy) {
    case TRI_DOC_UPDATE_ERROR:
      if (policy->_expectedRid != 0 && policy->_expectedRid != actualRid) {
        return TRI_ERROR_ARANGO_CONFLICT;
      }
      break;

    case TRI_DOC_UPDATE_CONFLICT:
      return TRI_ERROR_NOT_IMPLEMENTED;

    case TRI_DOC_UPDATE_ILLEGAL:
      return TRI_ERROR_INTERNAL;

    case TRI_DOC_UPDATE_LAST_WRITE:
      return TRI_ERROR_NO_ERROR;
  }

  return TRI_ERROR_NO_ERROR;
}


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
