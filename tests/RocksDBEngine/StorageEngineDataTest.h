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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RocksDBEngine/StorageEngineFixture.h"

#include "Basics/StaticStrings.h"
#include "Metrics/CounterBuilder.h"
#include "RestServer/ServerIdFeature.h"
#include "Sharding/ShardingFeature.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeatureOptions.h"
#include "Utils/ExecContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/VocbaseInfo.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <memory>
#include <string>
#include <string_view>

namespace arangodb::tests {

// Counter handed to the transaction manager (tracks expired transactions). We
// declare our own because the production counter is file-local to
// ManagerFeature.cpp.
DECLARE_COUNTER(arangodb_storage_engine_test_transactions_expired_total,
                "Expired transactions (storage-engine test)");

// A fixture that adds the scaffolding needed to exercise database, collection
// and document operations through the storage engine. The helpers stay
// deliberately thin: they only use what the engine already exposes.
//
// Parameterized on the base storage-engine fixture so the same scaffolding and
// helpers serve both the plain engine (StorageEngineFixture) and the
// time-travel engine (TimeTravelStorageEngineFixture) without duplication.
template<class BaseFixture>
class BasicStorageEngineDataTest : public BaseFixture {
 protected:
  static void SetUpTestSuite() {
    BaseFixture::SetUpTestSuite();
    // Minting a collection's GUID reads the global server id, which is normally
    // populated by the ServerIdFeature at startup. That feature is not part of
    // this setup, so we seed the id directly. The value must be large enough
    // that its hex form yields a GUID longer than three characters.
    ServerIdFeature::setId(ServerId{1234567890123ULL});

    // Building a LogicalCollection constructs a ShardingInfo, which resolves
    // its sharding strategy through the ShardingFeature. Register it on our own
    // server and run prepare() so the strategy factories are available.
    suite().server.addFeature<ShardingFeature>().prepare();

    // Transactions resolve their manager through engine().transactionManager(),
    // which is only valid once the manager has been created. In production the
    // ManagerFeature does this at startup; here we create it directly (the
    // engine holds only a weak reference, so we keep it alive as a member).
    _transactionManager = suite().engine.createTransactionManager(
        transaction::ManagerFeatureOptions{},
        suite().metricsRegistry.add(
            arangodb_storage_engine_test_transactions_expired_total{}));
  }

  static void TearDownTestSuite() {
    _transactionManager.reset();
    BaseFixture::TearDownTestSuite();
  }

  inline static std::shared_ptr<transaction::Manager> _transactionManager;

  // Build an in-memory Database object. We construct the database directly
  // with the fixture's injected database provider rather than going through
  // engine().openDatabase(): the direct path keeps the test in full control of
  // the collaborators.
  std::unique_ptr<Database> makeDatabase(std::string_view name, uint64_t id) {
    CreateDatabaseInfo info{suite().server, ExecContext::superuser()};
    // Name validation still reaches into the DatabaseFeature (extendedNames()),
    // which is not available here. Disable it (gap 2 in the gap report).
    info.validateNames(false);
    auto res = info.load(name, id);
    EXPECT_TRUE(res.ok()) << res.errorMessage();
    return std::make_unique<Database>(std::move(info), this->engine(),
                                      suite().dbProvider);
  }

  // Persist the create-database marker so the database is discoverable through
  // the engine's inventory API (getDatabases).
  void persistDatabase(Database const& database) {
    VPackBuilder builder;
    builder.openObject();
    builder.add(StaticStrings::DatabaseId,
                VPackValue(std::to_string(database.id())));
    builder.add("name", VPackValue(database.name()));
    builder.close();
    auto res = this->engine().writeCreateDatabaseMarker(database.id(),
                                                        builder.slice());
    ASSERT_TRUE(res.ok()) << res.errorMessage();
  }

  // Create a collection and persist its create marker through the engine.
  // database.createCollection() both builds the collection object and registers
  // it in the database's lookup tables, so transactions can resolve it by name;
  // engine().createCollection() then writes the on-disk marker that the
  // inventory API reports. When `timeTravel` is set, the collection is created
  // with the immutable enableTimeTravel property so its primary index is backed
  // by the PrimaryIndex_TT column family.
  std::shared_ptr<LogicalCollection> makeCollection(Database& database,
                                                    std::string_view name,
                                                    bool timeTravel = false) {
    VPackBuilder builder;
    builder.openObject();
    builder.add(StaticStrings::DataSourceName, VPackValue(name));
    if (timeTravel) {
      builder.add(StaticStrings::EnableTimeTravel, VPackValue(true));
    }
    builder.close();
    auto collection = database.createCollection(builder.slice());
    this->engine().createCollection(database, *collection);
    return collection;
  }

 private:
  static StorageEngineFixtureSuite& suite() { return *BaseFixture::_suite; }
};

using StorageEngineDataTest = BasicStorageEngineDataTest<StorageEngineFixture>;

using TimeTravelStorageEngineDataTest =
    BasicStorageEngineDataTest<TimeTravelStorageEngineFixture>;

}  // namespace arangodb::tests
