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

#ifndef ARANGODB_V8SERVER_V8__CURSOR_H
#define ARANGODB_V8SERVER_V8__CURSOR_H 1

#include "Basics/Common.h"
#include "VocBase/server.h"
#include "v8-vocbase.h"
#include "Ahuacatl/ahuacatl-context.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief function that encapsulates execution of an AQL query
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> ExecuteQueryCursorAhuacatl (TRI_vocbase_t* const vocbase,
                                                  TRI_aql_context_t* const context,
                                                  TRI_json_t const* parameters,
                                                  bool doCount,
                                                  uint32_t batchSize,
                                                  double cursorTtl);

void TRI_InitV8indexArangoDB (v8::Handle<v8::Context> context,
                              TRI_server_t* server,
                              TRI_vocbase_t* vocbase,
                              triagens::arango::JSLoader* loader,
                              const size_t threadNumber,
                              TRI_v8_global_t* v8g,
                              v8::Handle<v8::ObjectTemplate>  ArangoDBNS);

extern void TRI_InitV8cursor (v8::Handle<v8::Context> context,
                              TRI_server_t* server,
                              TRI_vocbase_t* vocbase,
                              triagens::arango::JSLoader* loader,
                              const size_t threadNumber,
                              TRI_v8_global_t* v8g);

v8::Handle<v8::Value> TRI_WrapGeneralCursor (void* cursor);

#endif
