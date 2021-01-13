////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_V8_SERVER_V8_ACTIONS_H
#define ARANGOD_V8_SERVER_V8_ACTIONS_H 1

#include "Basics/Common.h"

#include <v8.h>

class TRI_action_t;
struct TRI_v8_global_t;

namespace arangodb {
class GeneralRequest;
}

v8::Handle<v8::Object> TRI_RequestCppToV8(v8::Isolate* isolate,
                                          TRI_v8_global_t const* v8g,
                                          arangodb::GeneralRequest* request,
                                          TRI_action_t const* action);

void TRI_InitV8Actions(v8::Isolate* isolate);

void TRI_InitV8ServerUtils(v8::Isolate* isolate);

#endif
