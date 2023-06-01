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
    RocksDBDumpManager& manager, std::shared_ptr<RocksDBDumpContext> ctx)
    : _manager(manager), _ctx(std::move(ctx)) {
  TRI_ASSERT(_ctx != nullptr);
}

RocksDBDumpContextGuard::RocksDBDumpContextGuard(
    RocksDBDumpContextGuard&& other) noexcept
    : _manager(other._manager), _ctx(std::move(other._ctx)) {
  TRI_ASSERT(_ctx != nullptr);
}

RocksDBDumpContextGuard::~RocksDBDumpContextGuard() {
  TRI_ASSERT(_ctx != nullptr);
  _ctx->extendLifetime();
}

RocksDBDumpContext* RocksDBDumpContextGuard::operator->() {
  TRI_ASSERT(_ctx != nullptr);
  return _ctx.get();
}

}  // namespace arangodb
