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

#pragma once

#include <memory>

namespace arangodb {
class RocksDBDumpContext;
class RocksDBDumpManager;

class RocksDBDumpContextGuard {
 public:
  RocksDBDumpContextGuard(RocksDBDumpManager& manager,
                          std::shared_ptr<RocksDBDumpContext> ctx);

  RocksDBDumpContextGuard(RocksDBDumpContextGuard&& other) noexcept;

  ~RocksDBDumpContextGuard();

  RocksDBDumpContext* operator->();

 private:
  // TODO: _manager is currently unused. maybe remove it?
  [[maybe_unused]] RocksDBDumpManager& _manager;

  std::shared_ptr<RocksDBDumpContext> _ctx;
};
}  // namespace arangodb
