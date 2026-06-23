////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <gmock/gmock.h>

#include "Cache/ICacheManagerProvider.h"
#include "Metrics/ICollector.h"
#include "RestServer/IDatabasePathProvider.h"
#include "RestServer/IDatabaseProvider.h"
#include "RestServer/IDumpLimitsProvider.h"
#include "RestServer/IFlushControl.h"
#include "RocksDBEngine/IIndexCacheRefill.h"
#include "RocksDBEngine/ISortingPolicy.h"
#include "VectorIndex/IVectorIndexProvider.h"

namespace arangodb::tests {

struct MockDatabasePathProvider : IDatabasePathProvider {
  MOCK_METHOD(std::string const&, directory, (), (const, override));
  MOCK_METHOD(std::string, subdirectoryName, (std::string const&),
              (const, override));
};

struct MockVectorIndexProvider : IVectorIndexProvider {
  MOCK_METHOD(bool, isVectorIndexEnabled, (), (const, noexcept, override));
};

struct MockFlushControl : IFlushControl {
  MOCK_METHOD(bool, isEnabled, (), (const, noexcept, override));
  MOCK_METHOD((std::tuple<std::size_t, std::size_t, TRI_voc_tick_t>),
              releaseUnusedTicks, (), (override));
  MOCK_METHOD(void, registerFlushSubscription,
              (std::shared_ptr<FlushSubscription> const&), (override));
};

struct MockDumpLimitsProvider : IDumpLimitsProvider {
  MOCK_METHOD(DumpLimitsFeatureOptions const&, limits, (),
              (const, noexcept, override));
};

struct MockDatabaseProvider : IDatabaseProvider {
  MOCK_METHOD(VocbasePtr, useDatabase, (std::string_view), (const, override));
  MOCK_METHOD(VocbasePtr, useDatabase, (TRI_voc_tick_t), (const, override));
  MOCK_METHOD(void, enumerateDatabases,
              (std::function<void(TRI_vocbase_t&)> const&), (override));
  MOCK_METHOD(void, inventory,
              (velocypack::Builder&, TRI_voc_tick_t,
               std::function<bool(LogicalCollection const*)> const&),
              (override));
  MOCK_METHOD(replication::Version, defaultReplicationVersion, (),
              (const, noexcept, override));
  MOCK_METHOD(bool, extendedNames, (), (const, noexcept, override));
  MOCK_METHOD(void, extendedNames, (bool), (noexcept, override));
};

struct MockCacheManagerProvider : ICacheManagerProvider {
  MOCK_METHOD(cache::Manager*, manager, (), (override));
  MOCK_METHOD(std::size_t, minValueSizeForEdgeCompression, (),
              (const, noexcept, override));
  MOCK_METHOD(std::uint32_t, accelerationFactorForEdgeCompression, (),
              (const, noexcept, override));
};

struct MockSortingPolicy : ISortingPolicy {
  MOCK_METHOD(basics::VelocyPackHelper::SortingMethod, getSortingMethod, (),
              (const, noexcept, override));
};

struct MockIndexCacheRefill : IIndexCacheRefill {
  MOCK_METHOD(void, scheduleFullIndexRefill,
              (std::string const&, std::string const&, IndexId), (override));
  MOCK_METHOD(bool, autoRefill, (), (const, noexcept, override));
  MOCK_METHOD(bool, autoRefillOnFollowers, (), (const, noexcept, override));
  MOCK_METHOD(void, waitForCatchup, (), (override));
};

struct MockMetricsCollector : metrics::ICollector {
  MOCK_METHOD(std::shared_ptr<metrics::Metric>, doAdd, (metrics::Builder&),
              (override));
};

}  // namespace arangodb::tests
