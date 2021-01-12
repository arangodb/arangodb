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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOCBASE_METHODS_TRANSACTIONS_HANDLER_H
#define ARANGOD_VOCBASE_METHODS_TRANSACTIONS_HANDLER_H 1

#include <v8.h>
#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>
#include "Basics/ReadWriteLock.h"
#include "Basics/Result.h"
#include "VocBase/vocbase.h"

namespace arangodb {

Result executeTransaction(v8::Isolate*, basics::ReadWriteLock& cancelLock,
                          std::atomic<bool>& canceled, VPackSlice transaction,
                          std::string const& requestPortType, VPackBuilder& result);

Result executeTransactionJS(v8::Isolate*, v8::Handle<v8::Value> const& arg,
                            v8::Handle<v8::Value>& result, v8::TryCatch&);

}  // namespace arangodb
#endif
