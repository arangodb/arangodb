////////////////////////////////////////////////////////////////////////////////
/// @brief primary index of a collection
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2012, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_DURHAM_VOC_BASE_PRIMARY_INDEX_H
#define TRIAGENS_DURHAM_VOC_BASE_PRIMARY_INDEX_H 1

#include "BasicsC/common.h"

#include "VocBase/primary-collection.h"
#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                     PRIMARY INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constraint container
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_revision_constraint_s {
  TRI_doc_update_policy_e     _policy;
  TRI_transaction_local_id_t  _expectedRevision;
  TRI_transaction_local_id_t  _previousRevision;
}
TRI_revision_constraint_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief transactional master pointer
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_transaction_doc_mptr_s {
  TRI_transaction_local_id_t _validFrom; // valid from trx id
  TRI_transaction_local_id_t _validTo;   // valid to trx id
  char*                      _key;       // document identifier (string)
  TRI_voc_fid_t              _fid;       // datafile identifier
  void const*                _data;      // pointer to the raw marker (NULL for deleted documents)
}
TRI_transaction_doc_mptr_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief primary index of a collection
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_primary_index_s {
  TRI_read_write_lock_t        _lock;

  TRI_transaction_doc_mptr_t** _table;
  uint64_t                     _nrAlloc;
  uint64_t                     _nrUsed;
}
TRI_primary_index_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create the primary index
////////////////////////////////////////////////////////////////////////////////

TRI_primary_index_t* TRI_CreatePrimaryIndex (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief free the primary index
////////////////////////////////////////////////////////////////////////////////

void TRI_FreePrimaryIndex (TRI_primary_index_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
