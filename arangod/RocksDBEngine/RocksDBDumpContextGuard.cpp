////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBEngine/RocksDBDumpContextGuard.h"

#include "Basics/debugging.h"
#include "RocksDBEngine/RocksDBDumpContext.h"
#include "RocksDBEngine/RocksDBDumpManager.h"

namespace arangodb {

RocksDBDumpContextGuard::RocksDBDumpContextGuard(
    std::shared_ptr<RocksDBDumpContext> ctx)
    : _ctx(std::move(ctx)) {
  _ctx->extendLifetime();
  TRI_ASSERT(_ctx != nullptr);
}

RocksDBDumpContext* RocksDBDumpContextGuard::operator->() {
  TRI_ASSERT(_ctx != nullptr);
  return _ctx.get();
}

RocksDBDumpContextGuard::~RocksDBDumpContextGuard() {
  if (_ctx) {
    _ctx->extendLifetime();
  }
}

}  // namespace arangodb
