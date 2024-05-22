////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#ifndef USE_V8
#error this file is not supposed to be used in builds with -DUSE_V8=Off
#endif

#include "Utils/CollectionNameResolver.h"
#include "V8/v8-globals.h"
#include "V8Server/v8-vocbase.h"

namespace arangodb {
class Index;
class LogicalCollection;
}  // namespace arangodb

void TRI_InitV8IndexArangoDB(v8::Isolate* isolate,
                             v8::Handle<v8::ObjectTemplate> ArangoDBNS);

void TRI_InitV8IndexCollection(v8::Isolate* isolate,
                               v8::Handle<v8::ObjectTemplate> rt);
