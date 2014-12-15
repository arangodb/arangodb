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

#ifndef ARANGODB_V8SERVER_V8__INDEX_H
#define ARANGODB_V8SERVER_V8__INDEX_H 1

#include "Basics/Common.h"
#include "VocBase/server.h"
#include "V8/v8-globals.h"
#include "VocBase/index.h"
#include "Utils/CollectionNameResolver.h"
#include "v8-vocbase.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a index identifier
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupIndexByHandle (v8::Isolate* isolate,
                                      triagens::arango::CollectionNameResolver const* resolver,
                                      TRI_vocbase_col_t const* collection,
                                      v8::Handle<v8::Value> const val,
                                      bool ignoreNotFound);

void TRI_InitV8indexArangoDB (v8::Isolate* isolate,
                              v8::Handle<v8::ObjectTemplate>  ArangoDBNS);

void TRI_InitV8indexCollection (v8::Isolate* isolate,
                                v8::Handle<v8::ObjectTemplate> rt);
#endif
