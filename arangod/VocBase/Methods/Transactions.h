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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef USE_V8
#error this file is not supposed to be used in builds with -DUSE_V8=Off
#endif

#include "Basics/ReadWriteLock.h"
#include "Basics/Result.h"
#include "VocBase/vocbase.h"

#include <v8.h>
#include <atomic>
#include <string>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack
class V8Executor;

Result executeTransaction(V8Executor* executor,
                          basics::ReadWriteLock& cancelLock,
                          std::atomic<bool>& canceled,
                          velocypack::Slice transaction,
                          std::string const& requestPortType,
                          velocypack::Builder& result);

Result executeTransactionJS(v8::Isolate*, v8::Handle<v8::Value> arg,
                            v8::Handle<v8::Value>& result, v8::TryCatch&);

}  // namespace arangodb
