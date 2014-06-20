////////////////////////////////////////////////////////////////////////////////
/// @brief document update / delete policies
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_VOC_BASE_UPDATE__POLICY_H
#define ARANGODB_VOC_BASE_UPDATE__POLICY_H 1

#include "Basics/Common.h"
#include "VocBase/vocbase.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief update and delete policy
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_DOC_UPDATE_ERROR,
  TRI_DOC_UPDATE_LAST_WRITE,
  TRI_DOC_UPDATE_ILLEGAL
}
TRI_doc_update_policy_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief policy container
////////////////////////////////////////////////////////////////////////////////

class TRI_doc_update_policy_t {

  public:

    TRI_doc_update_policy_t (TRI_doc_update_policy_e policy,
                             TRI_voc_rid_t expectedRid,
                             TRI_voc_rid_t* previousRid)
      : _expectedRid(expectedRid),
        _previousRid(previousRid),
        _policy(policy) {
    }

    TRI_doc_update_policy_t () = delete;

  public:

////////////////////////////////////////////////////////////////////////////////
/// @brief compare revision of found document with revision specified in policy
/// this will also store the actual revision id found in the database in the
/// context variable _previousRid, but only if this is not NULL
////////////////////////////////////////////////////////////////////////////////

    int check (TRI_voc_rid_t actualRid) const {
      // store previous revision
      if (_previousRid != nullptr) {
        *(_previousRid) = actualRid;
      }

      // check policy
      switch (_policy) {
        case TRI_DOC_UPDATE_ERROR:
          if (_expectedRid != 0 && _expectedRid != actualRid) {
            return TRI_ERROR_ARANGO_CONFLICT;
          }
          break;

        case TRI_DOC_UPDATE_ILLEGAL:
          return TRI_ERROR_INTERNAL;

        case TRI_DOC_UPDATE_LAST_WRITE:
          return TRI_ERROR_NO_ERROR;
      }

      return TRI_ERROR_NO_ERROR;
    }

  private:

    TRI_voc_rid_t                    _expectedRid;  // the expected revision id of a document. only used if set and for update/delete
    TRI_voc_rid_t*                   _previousRid;  // a variable that the previous revsion id found in the database will be pushed into. only used if set and for update/delete
    TRI_doc_update_policy_e          _policy;       // the update policy
};

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
