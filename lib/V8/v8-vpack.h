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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef USE_V8
#error this file is not supposed to be used in builds with -DUSE_V8=Off
#endif

#include "V8/v8-globals.h"

#include <velocypack/Options.h>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack
}  // namespace arangodb

/// @brief converts a VPack value into a V8 object
v8::Handle<v8::Value> TRI_VPackToV8(
    v8::Isolate* isolate, arangodb::velocypack::Slice,
    arangodb::velocypack::Options const* options =
        &arangodb::velocypack::Options::Defaults,
    arangodb::velocypack::Slice const* base = nullptr);

/// @brief convert a V8 value to VPack value. can throw an exception in case the
/// conversion goes wrong
void TRI_V8ToVPack(v8::Isolate* isolate, arangodb::velocypack::Builder& builder,
                   v8::Local<v8::Value> value, bool keepTopLevelOpen,
                   bool convertFunctionsToNull = false);
