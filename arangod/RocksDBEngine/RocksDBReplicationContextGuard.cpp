////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBEngine/RocksDBReplicationContextGuard.h"

#include "Assertions/Assert.h"
#include "Basics/Exceptions.h"
#include "RocksDBEngine/RocksDBReplicationContext.h"
#include "RocksDBEngine/RocksDBReplicationManager.h"

namespace arangodb {

RocksDBReplicationContextGuard::RocksDBReplicationContextGuard(
    RocksDBReplicationManager& manager)
    : RocksDBReplicationContextGuard(manager, nullptr) {}

RocksDBReplicationContextGuard::RocksDBReplicationContextGuard(
    RocksDBReplicationManager& manager,
    std::shared_ptr<RocksDBReplicationContext> ctx)
    : _manager(manager), _ctx(std::move(ctx)), _deleted(false) {}

RocksDBReplicationContextGuard::RocksDBReplicationContextGuard(
    RocksDBReplicationContextGuard&& other) noexcept
    : _manager(other._manager),
      _ctx(std::move(other._ctx)),
      _deleted(std::exchange(other._deleted, false)) {}

RocksDBReplicationContextGuard::~RocksDBReplicationContextGuard() { release(); }

RocksDBReplicationContext* RocksDBReplicationContextGuard::operator->() {
  if (_ctx == nullptr || _deleted) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "no context in RocksDBReplicationContextGuard");
  }
  return _ctx.get();
}

void RocksDBReplicationContextGuard::release() noexcept {
  if (_ctx != nullptr) {
    _manager.release(std::move(_ctx), _deleted);
  }
}

}  // namespace arangodb
