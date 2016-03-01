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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_V8_SERVER_V8_VPACK_WRAPPER_H
#define ARANGOD_V8_SERVER_V8_VPACK_WRAPPER_H 1

#include "Basics/Common.h"
#include "V8/v8-globals.h"
#include "VocBase/voc-types.h"

#include <v8.h>

struct TRI_df_marker_t;
struct TRI_doc_mptr_t;
struct TRI_document_collection_t;

namespace arangodb {
class DocumentDitch;
class Transaction;

namespace V8VPackWrapper {

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a VPackSlice
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> wrap(v8::Isolate*, arangodb::Transaction*,
                           TRI_voc_cid_t cid, arangodb::DocumentDitch* ditch,
                           struct TRI_doc_mptr_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief generate the VPack object template
////////////////////////////////////////////////////////////////////////////////

void initialize(v8::Isolate*, v8::Handle<v8::Context>, TRI_v8_global_t*);

}
}

#endif
