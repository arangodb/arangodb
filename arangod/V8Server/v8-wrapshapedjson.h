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

#ifndef ARANGODB_V8SERVER_V8__WRAPSHAPEDJSON_H
#define ARANGODB_V8SERVER_V8__WRAPSHAPEDJSON_H

#include "Basics/Common.h"
#include "v8-vocbase.h"
#include "VocBase/server.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_shaped_json_t
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_WrapShapedJson (v8::Isolate* isolate,
                                          triagens::arango::CollectionNameResolver const*,
                                          TRI_barrier_t*,
                                          TRI_voc_cid_t,
                                          TRI_document_collection_t*,
                                          void const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_shaped_json_t
////////////////////////////////////////////////////////////////////////////////

template<class T>
v8::Handle<v8::Value> TRI_WrapShapedJson (v8::Isolate* isolate,
                                          T& trx,
                                          TRI_voc_cid_t cid,
                                          void const* data) {
  TRI_barrier_t* barrier = trx.barrier(cid);
  TRI_ASSERT(barrier != nullptr);

  triagens::arango::CollectionNameResolver const* resolver = trx.resolver();
  TRI_document_collection_t* collection = trx.documentCollection(cid);

  TRI_ASSERT(collection != nullptr);

  return TRI_WrapShapedJson(isolate, resolver, barrier, cid, collection, data);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate the TRI_shaped_json_t template
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8ShapedJson (v8::Isolate *isolate, 
                           v8::Handle<v8::Context> context,
                           size_t threadNumber,
                           TRI_v8_global_t* v8g);
#endif
