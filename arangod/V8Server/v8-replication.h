////////////////////////////////////////////////////////////////////////////////
/// @brief V8-replication Replication functions
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

#ifndef ARANGODB_V8SERVER_V8__REPLICATION_H
#define ARANGODB_V8SERVER_V8__REPLICATION_H

#include "Basics/Common.h"
#include "v8-vocbase.h"
#include "VocBase/server.h"


void TRI_InitV8replication (v8::Handle<v8::Context> context,
                            TRI_server_t* server,
                            TRI_vocbase_t* vocbase,
                            triagens::arango::JSLoader* loader,
                            const size_t threadNumber,
                            TRI_v8_global_t* v8g);

#endif
