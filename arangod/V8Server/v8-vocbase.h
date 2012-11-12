////////////////////////////////////////////////////////////////////////////////
/// @brief V8-vocbase bridge
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_V8_V8_VOCBASE_H
#define TRIAGENS_V8_V8_VOCBASE_H 1

#include "V8/v8-globals.h"
#include "ShapedJson/shaped-json.h"
#include "VocBase/document-collection.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the collection, but doesn't lock it
///
/// it is the caller's responsibility to acquire and release the required locks
/// the collection must also have the correct status already. don't use this
/// function if you're unsure about it!
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t* TRI_ExtractSimpleCollection (v8::Arguments const& argv,
                                                        TRI_vocbase_col_t const*& collection,
                                                        v8::Handle<v8::Object>* err);

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts and locks the collection
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t* TRI_ExtractAndUseSimpleCollection (v8::Arguments const&,
                                                              TRI_vocbase_col_t const*&,
                                                              v8::Handle<v8::Object>*);

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a collection
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseCollection (TRI_vocbase_col_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief parse document or document handle
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_ParseDocumentOrDocumentHandle (TRI_vocbase_t*,
                                                         TRI_vocbase_col_t const*&,
                                                         TRI_voc_key_t&,
                                                         TRI_voc_rid_t&,
                                                         const bool,
                                                         v8::Handle<v8::Value>);

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a index identifier
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupIndexByHandle (TRI_vocbase_t*,
                                      TRI_vocbase_col_t const*&,
                                      v8::Handle<v8::Value>,
                                      bool,
                                      v8::Handle<v8::Object>*);

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_vocbase_t
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_WrapVocBase (TRI_vocbase_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_vocbase_t for edges
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_WrapEdges (TRI_vocbase_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_vocbase_col_t
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_WrapCollection (TRI_vocbase_col_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_vocbase_col_t for edges
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_WrapEdgesCollection (TRI_vocbase_col_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_shaped_json_t
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_WrapShapedJson (TRI_vocbase_col_t const*,
                                          TRI_doc_mptr_t const*,
                                          TRI_barrier_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a TRI_vocbase_t global context
////////////////////////////////////////////////////////////////////////////////

TRI_v8_global_t* TRI_InitV8VocBridge (v8::Handle<v8::Context>, TRI_vocbase_t*, const size_t);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
