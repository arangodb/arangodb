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

#ifndef ARANGOD_V8_SERVER_V8_COLLECTION_H
#define ARANGOD_V8_SERVER_V8_COLLECTION_H 1

#include "Basics/Common.h"
#include "Utils/CollectionNameResolver.h"
#include "v8-vocbase.h"
#include "VocBase/server.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a collection
////////////////////////////////////////////////////////////////////////////////

void ReleaseCollection(TRI_vocbase_col_t const* collection);

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a collection info into a TRI_vocbase_col_t
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* CoordinatorCollection(TRI_vocbase_t* vocbase,
                                         arangodb::CollectionInfo const& ci);

///////////////////////////////////////////////////////////////////////////////
/// @brief check if a name belongs to a collection
////////////////////////////////////////////////////////////////////////////////

bool EqualCollection(arangodb::CollectionNameResolver const* resolver,
                     std::string const& collectionName,
                     TRI_vocbase_col_t const* collection);

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_vocbase_col_t
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> WrapCollection(v8::Isolate* isolate,
                                      TRI_vocbase_col_t const* collection);

void TRI_InitV8collection(v8::Handle<v8::Context> context, TRI_server_t* server,
                          TRI_vocbase_t* vocbase, arangodb::JSLoader* loader,
                          size_t const threadNumber, TRI_v8_global_t* v8g,
                          v8::Isolate* isolate,
                          v8::Handle<v8::ObjectTemplate> ArangoDBNS);

#endif
