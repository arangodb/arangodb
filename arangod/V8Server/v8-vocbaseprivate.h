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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_V8_SERVER_V8_VOCBASEPRIVATE_H
#define ARANGOD_V8_SERVER_V8_VOCBASEPRIVATE_H 1

#include "Basics/Common.h"
#include "V8/v8-utils.h"
#include "V8Server/v8-vocbase.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for TRI_vocbase_t
///
/// Layout:
/// - SLOT_CLASS_TYPE
/// - SLOT_CLASS
////////////////////////////////////////////////////////////////////////////////

extern int32_t const WRP_VOCBASE_TYPE;

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for LogicalCollection
///
/// Layout:
/// - SLOT_CLASS_TYPE
/// - SLOT_CLASS
/// - SLOT_COLLECTION
////////////////////////////////////////////////////////////////////////////////

extern int32_t const WRP_VOCBASE_COL_TYPE;

////////////////////////////////////////////////////////////////////////////////
/// @brief macro to make sure we won't continue if we are inside a transaction
////////////////////////////////////////////////////////////////////////////////

#define PREVENT_EMBEDDED_TRANSACTION()                                  \
  if (arangodb::V8TransactionContext::IsEmbedded()) {                   \
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION); \
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief create a v8 revision id value from the internal revision id
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8RevisionId(v8::Isolate* isolate, TRI_voc_rid_t rid);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the vocbase pointer from the current V8 context
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t* GetContextVocBase(v8::Isolate* isolate);

////////////////////////////////////////////////////////////////////////////////
/// @brief create a v8 tick id value from the internal tick id
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8TickId(v8::Isolate* isolate, TRI_voc_tick_t tick);

////////////////////////////////////////////////////////////////////////////////
/// @brief parse document or document handle from a v8 value (string | object)
/// Note that the builder must already be open with an object and will remain
/// open afterwards.
////////////////////////////////////////////////////////////////////////////////

bool ExtractDocumentHandle(v8::Isolate* isolate,
                           v8::Handle<v8::Value> const val,
                           std::string& collectionName,
                           arangodb::velocypack::Builder& builder,
                           bool includeRev);

#endif
