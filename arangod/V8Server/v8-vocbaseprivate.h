////////////////////////////////////////////////////////////////////////////////
/// @brief V8-vocbase bridge
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_V8SERVER_V8__VOCBASE_PRIVATE_H
#define ARANGODB_V8SERVER_V8__VOCBASE_PRIVATE_H

#include "Basics/Common.h"
#include "V8/v8-utils.h"
#include "v8-vocbase.h"
#include "Ahuacatl/ahuacatl-error.h"


////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for TRI_vocbase_t
///
/// Layout:
/// - SLOT_CLASS_TYPE
/// - SLOT_CLASS
////////////////////////////////////////////////////////////////////////////////

extern int32_t const WRP_VOCBASE_TYPE;


////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for TRI_vocbase_col_t
///
/// Layout:
/// - SLOT_CLASS_TYPE
/// - SLOT_CLASS
/// - SLOT_COLLECTION
////////////////////////////////////////////////////////////////////////////////

extern int32_t const WRP_VOCBASE_COL_TYPE;



// -----------------------------------------------------------------------------
// --SECTION--                                                   private defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief macro to make sure we won't continue if we are inside a transaction
////////////////////////////////////////////////////////////////////////////////

#define PREVENT_EMBEDDED_TRANSACTION(scope)                               \
  if (V8TransactionContext<true>::IsEmbedded()) {                         \
    TRI_V8_EXCEPTION(scope, TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION);  \
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief free a string if defined, nop otherwise
////////////////////////////////////////////////////////////////////////////////

#define FREE_STRING(zone, what)                                           \
  if (what != nullptr) {                                                  \
    TRI_Free(zone, what);                                                 \
    what = nullptr;                                                       \
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a v8 document id value from the parameters
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8DocumentId (std::string const& collectionName,
                                    std::string const& key);

////////////////////////////////////////////////////////////////////////////////
/// @brief create a v8 revision id value from the internal revision id
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8RevisionId (TRI_voc_rid_t rid);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the vocbase pointer from the current V8 context
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* GetContextVocBase ();

////////////////////////////////////////////////////////////////////////////////
/// @brief create a v8 tick id value from the internal tick id
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8TickId (TRI_voc_tick_t tick);

////////////////////////////////////////////////////////////////////////////////
/// @brief create an Ahuacatl error in a javascript object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> CreateErrorObjectAhuacatl (TRI_aql_error_t* error);

////////////////////////////////////////////////////////////////////////////////
/// @brief parse document or document handle from a v8 value (string | object)
////////////////////////////////////////////////////////////////////////////////

bool ExtractDocumentHandle (v8::Handle<v8::Value> const val,
                            std::string& collectionName,
                            TRI_voc_key_t& key,
                            TRI_voc_rid_t& rid);

#endif
