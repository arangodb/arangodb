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

#ifndef TRIAGENS_VOC_BASE_DOCUMENT_POLICY_H
#define TRIAGENS_VOC_BASE_DOCUMENT_POLICY_H 1

#include "BasicsC/common.h"
#include "VocBase/vocbase.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief update and delete policy
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_DOC_UPDATE_ERROR,
  TRI_DOC_UPDATE_LAST_WRITE,
  TRI_DOC_UPDATE_CONFLICT,
  TRI_DOC_UPDATE_ILLEGAL
}
TRI_doc_update_policy_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief policy container
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_doc_update_policy_s {
  TRI_voc_rid_t                    _expectedRid;  // the expected revision id of a document. only used if set and for update/delete
  TRI_voc_rid_t*                   _previousRid;  // a variable that the previous revsion id found in the database will be pushed into. only used if set and for update/delete
  TRI_doc_update_policy_e          _policy;       // the update policy
}
TRI_doc_update_policy_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

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

void TRI_InitUpdatePolicy (TRI_doc_update_policy_t* const,
                           TRI_doc_update_policy_e,
                           TRI_voc_rid_t,
                           TRI_voc_rid_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief compare revision of found document with revision specified in policy
/// this will also store the actual revision id found in the database in the
/// context variable _previousRid, but only if this is not NULL
////////////////////////////////////////////////////////////////////////////////

int TRI_CheckUpdatePolicy (const TRI_doc_update_policy_t* const,
                           const TRI_voc_rid_t);


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
