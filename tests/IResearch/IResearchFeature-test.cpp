////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include <Agency/AsyncAgencyComm.h>
#include "gtest/gtest.h"

#include "utils/misc.hpp"
#include "utils/string.hpp"
#include "utils/thread_utils.hpp"
#include "utils/utf8_path.hpp"
#include "utils/version_defines.hpp"

#include "IResearch/AgencyMock.h"
#include "IResearch/common.h"
#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"
#include "Mocks/StorageEngineMock.h"
#include "Mocks/TemplateSpecializer.h"

#include "Agency/Store.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "Aql/AqlFunctionFeature.h"
#include "Basics/NumberOfCores.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "ClusterEngine/ClusterEngine.h"
#include "Cluster/ClusterTypes.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "IResearch/ApplicationServerHelper.h"
#include "IResearch/Containers.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLinkCoordinator.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchView.h"
#include "Rest/Version.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/MetricsFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/UpgradeFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Indexes.h"
#include "VocBase/Methods/Upgrade.h"
#include "VocBase/Methods/Version.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

using namespace std::chrono_literals;

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchFeatureTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AGENCY, arangodb::LogLevel::FATAL>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::CLUSTER, arangodb::LogLevel::FATAL> {
 protected:

  arangodb::tests::mocks::MockV8Server server;

  IResearchFeatureTest() : server(false) {
    arangodb::tests::init();

    server.addFeature<arangodb::iresearch::IResearchAnalyzerFeature>(false);
    server.addFeature<arangodb::FlushFeature>(false);
    server.addFeature<arangodb::QueryRegistryFeature>(false);
    server.addFeature<arangodb::ServerSecurityFeature>(false);
    server.startFeatures();
  }

  // version 0 data-source path
  irs::utf8_path getPersistedPath0(arangodb::LogicalView const& view) {
    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
    irs::utf8_path dataPath(dbPathFeature.directory());
    dataPath /= "databases";
    dataPath /= "database-";
    dataPath += std::to_string(view.vocbase().id());
    dataPath /= arangodb::iresearch::DATA_SOURCE_TYPE.name();
    dataPath += "-";
    dataPath += std::to_string(view.id().id());
    return dataPath;
  };

  // version 1 data-source path
  irs::utf8_path getPersistedPath1(arangodb::iresearch::IResearchLink const& link) {
    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
    irs::utf8_path dataPath(dbPathFeature.directory());
    dataPath /= "databases";
    dataPath /= "database-";
    dataPath += std::to_string(link.collection().vocbase().id());
    dataPath /= arangodb::iresearch::DATA_SOURCE_TYPE.name();
    dataPath += "-";
    dataPath += std::to_string(link.collection().id().id());
    dataPath += "_";
    dataPath += std::to_string(link.id().id());
    return dataPath;
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchFeatureTest, test_options_default) {
  using namespace arangodb::options;
  using namespace arangodb::iresearch;

  IResearchFeature feature(server.server());
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_1));

  auto waitForStats = [&](std::tuple<size_t, size_t, size_t> expectedStats,
                          arangodb::iresearch::ThreadGroup group,
                          std::chrono::steady_clock::duration timeout = 10s) {
    auto const end = std::chrono::steady_clock::now() + timeout;
    while (expectedStats != feature.stats(group)) {
      std::this_thread::sleep_for(10ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  };

  auto opts = std::make_shared<ProgramOptions>("", "", "", "");
  feature.collectOptions(opts);
  auto* threads = opts->get<UInt32Parameter>("--arangosearch.threads");
  ASSERT_NE(nullptr, threads);
  ASSERT_EQ(0, *threads->ptr);
  auto* threadsLimit = opts->get<UInt32Parameter>("--arangosearch.threads-limit");
  ASSERT_NE(nullptr, threadsLimit);
  ASSERT_EQ(0, *threadsLimit->ptr);
  auto* consolidationThreads = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads");
  ASSERT_NE(nullptr, consolidationThreads);
  ASSERT_EQ(0, *consolidationThreads->ptr);
  auto* consolidationThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads-idle");
  ASSERT_NE(nullptr, consolidationThreadsIdle);
  ASSERT_EQ(0, *consolidationThreadsIdle->ptr);
  auto* commitThreads = opts->get<UInt32Parameter>("--arangosearch.commit-threads");
  ASSERT_NE(nullptr, commitThreads);
  ASSERT_EQ(0, *commitThreads->ptr);
  auto* commitThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.commit-threads-idle");
  ASSERT_NE(nullptr, commitThreadsIdle);
  ASSERT_EQ(0, *commitThreadsIdle->ptr);

  uint32_t const expectedNumThreads = std::max(1U, uint32_t(arangodb::NumberOfCores::getValue()) / 6);
  feature.validateOptions(opts);
  ASSERT_EQ(0, *threads->ptr);
  ASSERT_EQ(0, *threadsLimit->ptr);
  ASSERT_EQ(expectedNumThreads, *consolidationThreads->ptr);
  ASSERT_EQ(expectedNumThreads, *consolidationThreadsIdle->ptr);
  ASSERT_EQ(expectedNumThreads, *commitThreads->ptr);
  ASSERT_EQ(expectedNumThreads, *commitThreadsIdle->ptr);

  feature.prepare();
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_1));

  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_1));
  feature.start();
  ASSERT_EQ(std::make_pair(size_t(*commitThreads->ptr), size_t(*commitThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(*consolidationThreads->ptr), size_t(*consolidationThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_1));
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_0);
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_1);
  feature.stop();
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_1));
}

TEST_F(IResearchFeatureTest, test_options_commit_threads_default_set) {
  using namespace arangodb::options;
  using namespace arangodb::iresearch;

  IResearchFeature feature(server.server());
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_1));

  auto waitForStats = [&](std::tuple<size_t, size_t, size_t> expectedStats,
                          arangodb::iresearch::ThreadGroup group,
                          std::chrono::steady_clock::duration timeout = 10s) {
    auto const end = std::chrono::steady_clock::now() + timeout;
    while (expectedStats != feature.stats(group)) {
      std::this_thread::sleep_for(10ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  };

  auto opts = std::make_shared<ProgramOptions>("", "", "", "");
  feature.collectOptions(opts);
  auto* threads = opts->get<UInt32Parameter>("--arangosearch.threads");
  ASSERT_NE(nullptr, threads);
  ASSERT_EQ(0, *threads->ptr);
  auto* threadsLimit = opts->get<UInt32Parameter>("--arangosearch.threads-limit");
  ASSERT_NE(nullptr, threadsLimit);
  ASSERT_EQ(0, *threadsLimit->ptr);
  auto* consolidationThreads = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads");
  ASSERT_NE(nullptr, consolidationThreads);
  ASSERT_EQ(0, *consolidationThreads->ptr);
  auto* consolidationThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads-idle");
  ASSERT_NE(nullptr, consolidationThreadsIdle);
  ASSERT_EQ(0, *consolidationThreadsIdle->ptr);
  auto* commitThreads = opts->get<UInt32Parameter>("--arangosearch.commit-threads");
  ASSERT_NE(nullptr, commitThreads);
  ASSERT_EQ(0, *commitThreads->ptr);
  auto* commitThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.commit-threads-idle");
  ASSERT_NE(nullptr, commitThreadsIdle);
  ASSERT_EQ(0, *commitThreadsIdle->ptr);

  uint32_t const expectedConsolidationThreads
    = std::max(1U, uint32_t(arangodb::NumberOfCores::getValue()) / 6);
  uint32_t const expectedCommitThreads = expectedConsolidationThreads;

  opts->processingResult().touch("arangosearch.commit-threads");
  *commitThreads->ptr = 0;

  feature.validateOptions(opts);
  ASSERT_EQ(0, *threads->ptr);
  ASSERT_EQ(0, *threadsLimit->ptr);
  ASSERT_EQ(expectedConsolidationThreads, *consolidationThreads->ptr);
  ASSERT_EQ(expectedConsolidationThreads, *consolidationThreadsIdle->ptr);
  ASSERT_EQ(expectedCommitThreads, *commitThreads->ptr);
  ASSERT_EQ(expectedCommitThreads, *commitThreadsIdle->ptr);

  feature.prepare();
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_1));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_1));

  feature.start();
  ASSERT_EQ(std::make_pair(size_t(*commitThreads->ptr), size_t(*commitThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(*consolidationThreads->ptr), size_t(*consolidationThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_1));
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_0);
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_1);
  feature.stop();
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_1));
}

TEST_F(IResearchFeatureTest, test_options_commit_threads_min) {
  using namespace arangodb::options;
  using namespace arangodb::iresearch;

  IResearchFeature feature(server.server());
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_1));

  auto waitForStats = [&](std::tuple<size_t, size_t, size_t> expectedStats,
                          arangodb::iresearch::ThreadGroup group,
                          std::chrono::steady_clock::duration timeout = 10s) {
    auto const end = std::chrono::steady_clock::now() + timeout;
    while (expectedStats != feature.stats(group)) {
      std::this_thread::sleep_for(10ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  };

  auto opts = std::make_shared<ProgramOptions>("", "", "", "");
  feature.collectOptions(opts);
  auto* threads = opts->get<UInt32Parameter>("--arangosearch.threads");
  ASSERT_NE(nullptr, threads);
  ASSERT_EQ(0, *threads->ptr);
  auto* threadsLimit = opts->get<UInt32Parameter>("--arangosearch.threads-limit");
  ASSERT_NE(nullptr, threadsLimit);
  ASSERT_EQ(0, *threadsLimit->ptr);
  auto* consolidationThreads = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads");
  ASSERT_NE(nullptr, consolidationThreads);
  ASSERT_EQ(0, *consolidationThreads->ptr);
  auto* consolidationThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads-idle");
  ASSERT_NE(nullptr, consolidationThreadsIdle);
  ASSERT_EQ(0, *consolidationThreadsIdle->ptr);
  auto* commitThreads = opts->get<UInt32Parameter>("--arangosearch.commit-threads");
  ASSERT_NE(nullptr, commitThreads);
  ASSERT_EQ(0, *commitThreads->ptr);
  auto* commitThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.commit-threads-idle");
  ASSERT_NE(nullptr, commitThreadsIdle);
  ASSERT_EQ(0, *commitThreadsIdle->ptr);


  uint32_t const expectedConsolidationThreads
    = std::max(1U, uint32_t(arangodb::NumberOfCores::getValue()) / 6);
  uint32_t const expectedCommitThreads = 1;

  opts->processingResult().touch("arangosearch.commit-threads");
  *commitThreads->ptr = expectedCommitThreads;

  feature.validateOptions(opts);
  ASSERT_EQ(0, *threads->ptr);
  ASSERT_EQ(0, *threadsLimit->ptr);
  ASSERT_EQ(expectedConsolidationThreads, *consolidationThreads->ptr);
  ASSERT_EQ(expectedConsolidationThreads, *consolidationThreadsIdle->ptr);
  ASSERT_EQ(expectedCommitThreads, *commitThreads->ptr);
  ASSERT_EQ(expectedCommitThreads, *commitThreadsIdle->ptr);

  feature.prepare();
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_1));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_1));

  feature.start();
  ASSERT_EQ(std::make_pair(size_t(*commitThreads->ptr), size_t(*commitThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(*consolidationThreads->ptr), size_t(*consolidationThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_1));
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_0);
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_1);
  feature.stop();
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_1));
}

TEST_F(IResearchFeatureTest, test_options_commit_threads) {
  using namespace arangodb::options;
  using namespace arangodb::iresearch;

  IResearchFeature feature(server.server());
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_1));

  auto waitForStats = [&](std::tuple<size_t, size_t, size_t> expectedStats,
                          arangodb::iresearch::ThreadGroup group,
                          std::chrono::steady_clock::duration timeout = 10s) {
    auto const end = std::chrono::steady_clock::now() + timeout;
    while (expectedStats != feature.stats(group)) {
      std::this_thread::sleep_for(10ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  };

  auto opts = std::make_shared<ProgramOptions>("", "", "", "");
  feature.collectOptions(opts);
  auto* threads = opts->get<UInt32Parameter>("--arangosearch.threads");
  ASSERT_NE(nullptr, threads);
  ASSERT_EQ(0, *threads->ptr);
  auto* threadsLimit = opts->get<UInt32Parameter>("--arangosearch.threads-limit");
  ASSERT_NE(nullptr, threadsLimit);
  ASSERT_EQ(0, *threadsLimit->ptr);
  auto* consolidationThreads = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads");
  ASSERT_NE(nullptr, consolidationThreads);
  ASSERT_EQ(0, *consolidationThreads->ptr);
  auto* consolidationThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads-idle");
  ASSERT_NE(nullptr, consolidationThreadsIdle);
  ASSERT_EQ(0, *consolidationThreadsIdle->ptr);
  auto* commitThreads = opts->get<UInt32Parameter>("--arangosearch.commit-threads");
  ASSERT_NE(nullptr, commitThreads);
  ASSERT_EQ(0, *commitThreads->ptr);
  auto* commitThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.commit-threads-idle");
  ASSERT_NE(nullptr, commitThreadsIdle);
  ASSERT_EQ(0, *commitThreadsIdle->ptr);

  uint32_t const expectedConsolidationThreads
    = std::max(1U, uint32_t(arangodb::NumberOfCores::getValue()) / 6);
  uint32_t const expectedCommitThreads = 6;

  opts->processingResult().touch("arangosearch.commit-threads");
  *commitThreads->ptr = 6;

  feature.validateOptions(opts);
  ASSERT_EQ(0, *threads->ptr);
  ASSERT_EQ(0, *threadsLimit->ptr);
  ASSERT_EQ(expectedConsolidationThreads, *consolidationThreads->ptr);
  ASSERT_EQ(expectedConsolidationThreads, *consolidationThreadsIdle->ptr);
  ASSERT_EQ(expectedCommitThreads, *commitThreads->ptr);
  ASSERT_EQ(expectedCommitThreads, *commitThreadsIdle->ptr);

  feature.prepare();
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_1));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_1));

  feature.start();
  ASSERT_EQ(std::make_pair(size_t(*commitThreads->ptr), size_t(*commitThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(*consolidationThreads->ptr), size_t(*consolidationThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_1));
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_0);
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_1);
  feature.stop();
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_1));
}

TEST_F(IResearchFeatureTest, test_options_consolidation_threads) {
  using namespace arangodb::options;
  using namespace arangodb::iresearch;

  IResearchFeature feature(server.server());
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_1));

  auto waitForStats = [&](std::tuple<size_t, size_t, size_t> expectedStats,
                          arangodb::iresearch::ThreadGroup group,
                          std::chrono::steady_clock::duration timeout = 10s) {
    auto const end = std::chrono::steady_clock::now() + timeout;
    while (expectedStats != feature.stats(group)) {
      std::this_thread::sleep_for(10ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  };

  auto opts = std::make_shared<ProgramOptions>("", "", "", "");
  feature.collectOptions(opts);
  auto* threads = opts->get<UInt32Parameter>("--arangosearch.threads");
  ASSERT_NE(nullptr, threads);
  ASSERT_EQ(0, *threads->ptr);
  auto* threadsLimit = opts->get<UInt32Parameter>("--arangosearch.threads-limit");
  ASSERT_NE(nullptr, threadsLimit);
  ASSERT_EQ(0, *threadsLimit->ptr);
  auto* consolidationThreads = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads");
  ASSERT_NE(nullptr, consolidationThreads);
  ASSERT_EQ(0, *consolidationThreads->ptr);
  auto* consolidationThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads-idle");
  ASSERT_NE(nullptr, consolidationThreadsIdle);
  ASSERT_EQ(0, *consolidationThreadsIdle->ptr);
  auto* commitThreads = opts->get<UInt32Parameter>("--arangosearch.commit-threads");
  ASSERT_NE(nullptr, commitThreads);
  ASSERT_EQ(0, *commitThreads->ptr);
  auto* commitThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.commit-threads-idle");
  ASSERT_NE(nullptr, commitThreadsIdle);
  ASSERT_EQ(0, *commitThreadsIdle->ptr);

  uint32_t const expectedCommitThreads
    = std::max(1U, uint32_t(arangodb::NumberOfCores::getValue()) / 6);
  uint32_t const expectedConsolidationThreads = 6;

  opts->processingResult().touch("arangosearch.consolidation-threads");
  *consolidationThreads->ptr = 6;

  feature.validateOptions(opts);
  ASSERT_EQ(0, *threads->ptr);
  ASSERT_EQ(0, *threadsLimit->ptr);
  ASSERT_EQ(expectedConsolidationThreads, *consolidationThreads->ptr);
  ASSERT_EQ(expectedConsolidationThreads, *consolidationThreadsIdle->ptr);
  ASSERT_EQ(expectedCommitThreads, *commitThreads->ptr);
  ASSERT_EQ(expectedCommitThreads, *commitThreadsIdle->ptr);

  feature.prepare();
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_1));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_1));

  feature.start();
  ASSERT_EQ(std::make_pair(size_t(*commitThreads->ptr), size_t(*commitThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(*consolidationThreads->ptr), size_t(*consolidationThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_1));
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_0);
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_1);
  feature.stop();
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_1));
}

TEST_F(IResearchFeatureTest, test_options_consolidation_threads_idle_auto) {
  using namespace arangodb::options;
  using namespace arangodb::iresearch;

  IResearchFeature feature(server.server());
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_1));

  auto waitForStats = [&](std::tuple<size_t, size_t, size_t> expectedStats,
                          arangodb::iresearch::ThreadGroup group,
                          std::chrono::steady_clock::duration timeout = 10s) {
    auto const end = std::chrono::steady_clock::now() + timeout;
    while (expectedStats != feature.stats(group)) {
      std::this_thread::sleep_for(10ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  };

  auto opts = std::make_shared<ProgramOptions>("", "", "", "");
  feature.collectOptions(opts);
  auto* threads = opts->get<UInt32Parameter>("--arangosearch.threads");
  ASSERT_NE(nullptr, threads);
  ASSERT_EQ(0, *threads->ptr);
  auto* threadsLimit = opts->get<UInt32Parameter>("--arangosearch.threads-limit");
  ASSERT_NE(nullptr, threadsLimit);
  ASSERT_EQ(0, *threadsLimit->ptr);
  auto* consolidationThreads = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads");
  ASSERT_NE(nullptr, consolidationThreads);
  ASSERT_EQ(0, *consolidationThreads->ptr);
  auto* consolidationThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads-idle");
  ASSERT_NE(nullptr, consolidationThreadsIdle);
  ASSERT_EQ(0, *consolidationThreadsIdle->ptr);
  auto* commitThreads = opts->get<UInt32Parameter>("--arangosearch.commit-threads");
  ASSERT_NE(nullptr, commitThreads);
  ASSERT_EQ(0, *commitThreads->ptr);
  auto* commitThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.commit-threads-idle");
  ASSERT_NE(nullptr, commitThreadsIdle);
  ASSERT_EQ(0, *commitThreadsIdle->ptr);

  uint32_t const expectedCommitThreads
    = std::max(1U, uint32_t(arangodb::NumberOfCores::getValue()) / 6);
  uint32_t const expectedConsolidationThreads = 6;

  opts->processingResult().touch("arangosearch.consolidation-threads");
  *consolidationThreads->ptr = 6;
  opts->processingResult().touch("arangosearch.consolidation-threads-idle");
  *consolidationThreadsIdle->ptr = 0;

  feature.validateOptions(opts);
  ASSERT_EQ(0, *threads->ptr);
  ASSERT_EQ(0, *threadsLimit->ptr);
  ASSERT_EQ(expectedConsolidationThreads, *consolidationThreads->ptr);
  ASSERT_EQ(expectedConsolidationThreads/2, *consolidationThreadsIdle->ptr);
  ASSERT_EQ(expectedCommitThreads, *commitThreads->ptr);
  ASSERT_EQ(expectedCommitThreads, *commitThreadsIdle->ptr);

  feature.prepare();
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_1));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_1));

  feature.start();
  ASSERT_EQ(std::make_pair(size_t(*commitThreads->ptr), size_t(*commitThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(*consolidationThreads->ptr), size_t(*consolidationThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_1));
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_0);
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_1);
  feature.stop();
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_1));
}

TEST_F(IResearchFeatureTest, test_options_consolidation_threads_idle_set) {
  using namespace arangodb::options;
  using namespace arangodb::iresearch;

  IResearchFeature feature(server.server());
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_1));

  auto waitForStats = [&](std::tuple<size_t, size_t, size_t> expectedStats,
                          arangodb::iresearch::ThreadGroup group,
                          std::chrono::steady_clock::duration timeout = 10s) {
    auto const end = std::chrono::steady_clock::now() + timeout;
    while (expectedStats != feature.stats(group)) {
      std::this_thread::sleep_for(10ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  };

  auto opts = std::make_shared<ProgramOptions>("", "", "", "");
  feature.collectOptions(opts);
  auto* threads = opts->get<UInt32Parameter>("--arangosearch.threads");
  ASSERT_NE(nullptr, threads);
  ASSERT_EQ(0, *threads->ptr);
  auto* threadsLimit = opts->get<UInt32Parameter>("--arangosearch.threads-limit");
  ASSERT_NE(nullptr, threadsLimit);
  ASSERT_EQ(0, *threadsLimit->ptr);
  auto* consolidationThreads = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads");
  ASSERT_NE(nullptr, consolidationThreads);
  ASSERT_EQ(0, *consolidationThreads->ptr);
  auto* consolidationThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads-idle");
  ASSERT_NE(nullptr, consolidationThreadsIdle);
  ASSERT_EQ(0, *consolidationThreadsIdle->ptr);
  auto* commitThreads = opts->get<UInt32Parameter>("--arangosearch.commit-threads");
  ASSERT_NE(nullptr, commitThreads);
  ASSERT_EQ(0, *commitThreads->ptr);
  auto* commitThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.commit-threads-idle");
  ASSERT_NE(nullptr, commitThreadsIdle);
  ASSERT_EQ(0, *commitThreadsIdle->ptr);

  uint32_t const expectedCommitThreads
    = std::max(1U, uint32_t(arangodb::NumberOfCores::getValue()) / 6);
  uint32_t const expectedConsolidationThreads = 6;
  uint32_t const expectedConsolidationThreadsIdle = 4;

  opts->processingResult().touch("arangosearch.consolidation-threads");
  *consolidationThreads->ptr = 6;
  opts->processingResult().touch("arangosearch.consolidation-threads-idle");
  *consolidationThreadsIdle->ptr = 4;

  feature.validateOptions(opts);
  ASSERT_EQ(0, *threads->ptr);
  ASSERT_EQ(0, *threadsLimit->ptr);
  ASSERT_EQ(expectedConsolidationThreads, *consolidationThreads->ptr);
  ASSERT_EQ(expectedConsolidationThreadsIdle, *consolidationThreadsIdle->ptr);
  ASSERT_EQ(expectedCommitThreads, *commitThreads->ptr);
  ASSERT_EQ(expectedCommitThreads, *commitThreadsIdle->ptr);

  feature.prepare();
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_1));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_1));

  feature.start();
  ASSERT_EQ(std::make_pair(size_t(*commitThreads->ptr), size_t(*commitThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(*consolidationThreads->ptr), size_t(*consolidationThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_1));
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_0);
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_1);
  feature.stop();
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_1));
}

TEST_F(IResearchFeatureTest, test_options_consolidation_threads_idle_set_to_zero) {
  using namespace arangodb::options;
  using namespace arangodb::iresearch;

  IResearchFeature feature(server.server());
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_1));

  auto waitForStats = [&](std::tuple<size_t, size_t, size_t> expectedStats,
                          arangodb::iresearch::ThreadGroup group,
                          std::chrono::steady_clock::duration timeout = 10s) {
    auto const end = std::chrono::steady_clock::now() + timeout;
    while (expectedStats != feature.stats(group)) {
      std::this_thread::sleep_for(10ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  };

  auto opts = std::make_shared<ProgramOptions>("", "", "", "");
  feature.collectOptions(opts);
  auto* threads = opts->get<UInt32Parameter>("--arangosearch.threads");
  ASSERT_NE(nullptr, threads);
  ASSERT_EQ(0, *threads->ptr);
  auto* threadsLimit = opts->get<UInt32Parameter>("--arangosearch.threads-limit");
  ASSERT_NE(nullptr, threadsLimit);
  ASSERT_EQ(0, *threadsLimit->ptr);
  auto* consolidationThreads = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads");
  ASSERT_NE(nullptr, consolidationThreads);
  ASSERT_EQ(0, *consolidationThreads->ptr);
  auto* consolidationThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads-idle");
  ASSERT_NE(nullptr, consolidationThreadsIdle);
  ASSERT_EQ(0, *consolidationThreadsIdle->ptr);
  auto* commitThreads = opts->get<UInt32Parameter>("--arangosearch.commit-threads");
  ASSERT_NE(nullptr, commitThreads);
  ASSERT_EQ(0, *commitThreads->ptr);
  auto* commitThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.commit-threads-idle");
  ASSERT_NE(nullptr, commitThreadsIdle);
  ASSERT_EQ(0, *commitThreadsIdle->ptr);

  uint32_t const expectedCommitThreads
    = std::max(1U, uint32_t(arangodb::NumberOfCores::getValue()) / 6);
  uint32_t const expectedConsolidationThreads = 6;
  uint32_t const expectedConsolidationThreadsIdle = expectedConsolidationThreads/2;

  opts->processingResult().touch("arangosearch.consolidation-threads");
  *consolidationThreads->ptr = 6;
  opts->processingResult().touch("arangosearch.consolidation-threads-idle");
  *consolidationThreadsIdle->ptr = 0;

  feature.validateOptions(opts);
  ASSERT_EQ(0, *threads->ptr);
  ASSERT_EQ(0, *threadsLimit->ptr);
  ASSERT_EQ(expectedConsolidationThreads, *consolidationThreads->ptr);
  ASSERT_EQ(expectedConsolidationThreadsIdle, *consolidationThreadsIdle->ptr);
  ASSERT_EQ(expectedCommitThreads, *commitThreads->ptr);
  ASSERT_EQ(expectedCommitThreads, *commitThreadsIdle->ptr);

  feature.prepare();
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_1));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_1));

  feature.start();
  ASSERT_EQ(std::make_pair(size_t(*commitThreads->ptr), size_t(*commitThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(*consolidationThreads->ptr), size_t(*consolidationThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_1));
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_0);
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_1);
  feature.stop();
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_1));
}

TEST_F(IResearchFeatureTest, test_options_consolidation_threads_idle_greater_than_consolidation_threads) {
  using namespace arangodb::options;
  using namespace arangodb::iresearch;

  IResearchFeature feature(server.server());
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_1));

  auto waitForStats = [&](std::tuple<size_t, size_t, size_t> expectedStats,
                          arangodb::iresearch::ThreadGroup group,
                          std::chrono::steady_clock::duration timeout = 10s) {
    auto const end = std::chrono::steady_clock::now() + timeout;
    while (expectedStats != feature.stats(group)) {
      std::this_thread::sleep_for(10ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  };

  auto opts = std::make_shared<ProgramOptions>("", "", "", "");
  feature.collectOptions(opts);
  auto* threads = opts->get<UInt32Parameter>("--arangosearch.threads");
  ASSERT_NE(nullptr, threads);
  ASSERT_EQ(0, *threads->ptr);
  auto* threadsLimit = opts->get<UInt32Parameter>("--arangosearch.threads-limit");
  ASSERT_NE(nullptr, threadsLimit);
  ASSERT_EQ(0, *threadsLimit->ptr);
  auto* consolidationThreads = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads");
  ASSERT_NE(nullptr, consolidationThreads);
  ASSERT_EQ(0, *consolidationThreads->ptr);
  auto* consolidationThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads-idle");
  ASSERT_NE(nullptr, consolidationThreadsIdle);
  ASSERT_EQ(0, *consolidationThreadsIdle->ptr);
  auto* commitThreads = opts->get<UInt32Parameter>("--arangosearch.commit-threads");
  ASSERT_NE(nullptr, commitThreads);
  ASSERT_EQ(0, *commitThreads->ptr);
  auto* commitThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.commit-threads-idle");
  ASSERT_NE(nullptr, commitThreadsIdle);
  ASSERT_EQ(0, *commitThreadsIdle->ptr);

  uint32_t const expectedCommitThreads
    = std::max(1U, uint32_t(arangodb::NumberOfCores::getValue()) / 6);
  uint32_t const expectedConsolidationThreads = 6;
  uint32_t const expectedConsolidationThreadsIdle = 6;

  opts->processingResult().touch("arangosearch.consolidation-threads");
  *consolidationThreads->ptr = 6;
  opts->processingResult().touch("arangosearch.consolidation-threads-idle");
  *consolidationThreadsIdle->ptr = 1 + *consolidationThreads->ptr;

  feature.validateOptions(opts);
  ASSERT_EQ(0, *threads->ptr);
  ASSERT_EQ(0, *threadsLimit->ptr);
  ASSERT_EQ(expectedConsolidationThreads, *consolidationThreads->ptr);
  ASSERT_EQ(expectedConsolidationThreadsIdle, *consolidationThreadsIdle->ptr);
  ASSERT_EQ(expectedCommitThreads, *commitThreads->ptr);
  ASSERT_EQ(expectedCommitThreads, *commitThreadsIdle->ptr);

  feature.prepare();
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_1));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_1));

  feature.start();
  ASSERT_EQ(std::make_pair(size_t(*commitThreads->ptr), size_t(*commitThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(*consolidationThreads->ptr), size_t(*consolidationThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_1));
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_0);
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_1);
  feature.stop();
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_1));
}

TEST_F(IResearchFeatureTest, test_options_commit_threads_idle_auto) {
  using namespace arangodb::options;
  using namespace arangodb::iresearch;

  IResearchFeature feature(server.server());
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_1));

  auto waitForStats = [&](std::tuple<size_t, size_t, size_t> expectedStats,
                          arangodb::iresearch::ThreadGroup group,
                          std::chrono::steady_clock::duration timeout = 10s) {
    auto const end = std::chrono::steady_clock::now() + timeout;
    while (expectedStats != feature.stats(group)) {
      std::this_thread::sleep_for(10ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  };

  auto opts = std::make_shared<ProgramOptions>("", "", "", "");
  feature.collectOptions(opts);
  auto* threads = opts->get<UInt32Parameter>("--arangosearch.threads");
  ASSERT_NE(nullptr, threads);
  ASSERT_EQ(0, *threads->ptr);
  auto* threadsLimit = opts->get<UInt32Parameter>("--arangosearch.threads-limit");
  ASSERT_NE(nullptr, threadsLimit);
  ASSERT_EQ(0, *threadsLimit->ptr);
  auto* consolidationThreads = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads");
  ASSERT_NE(nullptr, consolidationThreads);
  ASSERT_EQ(0, *consolidationThreads->ptr);
  auto* consolidationThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads-idle");
  ASSERT_NE(nullptr, consolidationThreadsIdle);
  ASSERT_EQ(0, *consolidationThreadsIdle->ptr);
  auto* commitThreads = opts->get<UInt32Parameter>("--arangosearch.commit-threads");
  ASSERT_NE(nullptr, commitThreads);
  ASSERT_EQ(0, *commitThreads->ptr);
  auto* commitThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.commit-threads-idle");
  ASSERT_NE(nullptr, commitThreadsIdle);
  ASSERT_EQ(0, *commitThreadsIdle->ptr);

  uint32_t const expectedConsolidationThreads
    = std::max(1U, uint32_t(arangodb::NumberOfCores::getValue()) / 6);
  uint32_t const expectedCommitThreads = 6;

  opts->processingResult().touch("arangosearch.commit-threads");
  *commitThreads->ptr = 6;
  opts->processingResult().touch("arangosearch.commit-threads-idle");
  *commitThreadsIdle->ptr = 0;

  feature.validateOptions(opts);
  ASSERT_EQ(0, *threads->ptr);
  ASSERT_EQ(0, *threadsLimit->ptr);
  ASSERT_EQ(expectedConsolidationThreads, *consolidationThreads->ptr);
  ASSERT_EQ(expectedConsolidationThreads, *consolidationThreadsIdle->ptr);
  ASSERT_EQ(expectedCommitThreads, *commitThreads->ptr);
  ASSERT_EQ(expectedCommitThreads/2, *commitThreadsIdle->ptr);

  feature.prepare();
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_1));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_1));

  feature.start();
  ASSERT_EQ(std::make_pair(size_t(*commitThreads->ptr), size_t(*commitThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(*consolidationThreads->ptr), size_t(*consolidationThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_1));
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_0);
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_1);
  feature.stop();
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_1));
}

TEST_F(IResearchFeatureTest, test_options_commit_threads_idle_set) {
  using namespace arangodb::options;
  using namespace arangodb::iresearch;

  IResearchFeature feature(server.server());
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_1));

  auto waitForStats = [&](std::tuple<size_t, size_t, size_t> expectedStats,
                          arangodb::iresearch::ThreadGroup group,
                          std::chrono::steady_clock::duration timeout = 10s) {
    auto const end = std::chrono::steady_clock::now() + timeout;
    while (expectedStats != feature.stats(group)) {
      std::this_thread::sleep_for(10ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  };

  auto opts = std::make_shared<ProgramOptions>("", "", "", "");
  feature.collectOptions(opts);
  auto* threads = opts->get<UInt32Parameter>("--arangosearch.threads");
  ASSERT_NE(nullptr, threads);
  ASSERT_EQ(0, *threads->ptr);
  auto* threadsLimit = opts->get<UInt32Parameter>("--arangosearch.threads-limit");
  ASSERT_NE(nullptr, threadsLimit);
  ASSERT_EQ(0, *threadsLimit->ptr);
  auto* consolidationThreads = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads");
  ASSERT_NE(nullptr, consolidationThreads);
  ASSERT_EQ(0, *consolidationThreads->ptr);
  auto* consolidationThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads-idle");
  ASSERT_NE(nullptr, consolidationThreadsIdle);
  ASSERT_EQ(0, *consolidationThreadsIdle->ptr);
  auto* commitThreads = opts->get<UInt32Parameter>("--arangosearch.commit-threads");
  ASSERT_NE(nullptr, commitThreads);
  ASSERT_EQ(0, *commitThreads->ptr);
  auto* commitThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.commit-threads-idle");
  ASSERT_NE(nullptr, commitThreadsIdle);
  ASSERT_EQ(0, *commitThreadsIdle->ptr);


  uint32_t const expectedConsolidationThreads
    = std::max(1U, uint32_t(arangodb::NumberOfCores::getValue()) / 6);
  uint32_t const expectedCommitThreads = 6;
  uint32_t const expectedCommitThreadsIdle = 4;

  opts->processingResult().touch("arangosearch.commit-threads");
  *commitThreads->ptr = 6;
  opts->processingResult().touch("arangosearch.commit-threads-idle");
  *commitThreadsIdle->ptr = 4;

  feature.validateOptions(opts);
  ASSERT_EQ(0, *threads->ptr);
  ASSERT_EQ(0, *threadsLimit->ptr);
  ASSERT_EQ(expectedConsolidationThreads, *consolidationThreads->ptr);
  ASSERT_EQ(expectedConsolidationThreads, *consolidationThreadsIdle->ptr);
  ASSERT_EQ(expectedCommitThreads, *commitThreads->ptr);
  ASSERT_EQ(expectedCommitThreadsIdle, *commitThreadsIdle->ptr);

  feature.prepare();
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_1));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_1));

  feature.start();
  ASSERT_EQ(std::make_pair(size_t(*commitThreads->ptr), size_t(*commitThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(*consolidationThreads->ptr), size_t(*consolidationThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_1));
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_0);
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_1);
  feature.stop();
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_1));
}

TEST_F(IResearchFeatureTest, test_options_commit_threads_idle_greater_than_commit_threads) {
  using namespace arangodb::options;
  using namespace arangodb::iresearch;

  IResearchFeature feature(server.server());
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_1));

  auto waitForStats = [&](std::tuple<size_t, size_t, size_t> expectedStats,
                          arangodb::iresearch::ThreadGroup group,
                          std::chrono::steady_clock::duration timeout = 10s) {
    auto const end = std::chrono::steady_clock::now() + timeout;
    while (expectedStats != feature.stats(group)) {
      std::this_thread::sleep_for(10ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  };

  auto opts = std::make_shared<ProgramOptions>("", "", "", "");
  feature.collectOptions(opts);
  auto* threads = opts->get<UInt32Parameter>("--arangosearch.threads");
  ASSERT_NE(nullptr, threads);
  ASSERT_EQ(0, *threads->ptr);
  auto* threadsLimit = opts->get<UInt32Parameter>("--arangosearch.threads-limit");
  ASSERT_NE(nullptr, threadsLimit);
  ASSERT_EQ(0, *threadsLimit->ptr);
  auto* consolidationThreads = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads");
  ASSERT_NE(nullptr, consolidationThreads);
  ASSERT_EQ(0, *consolidationThreads->ptr);
  auto* consolidationThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads-idle");
  ASSERT_NE(nullptr, consolidationThreadsIdle);
  ASSERT_EQ(0, *consolidationThreadsIdle->ptr);
  auto* commitThreads = opts->get<UInt32Parameter>("--arangosearch.commit-threads");
  ASSERT_NE(nullptr, commitThreads);
  ASSERT_EQ(0, *commitThreads->ptr);
  auto* commitThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.commit-threads-idle");
  ASSERT_NE(nullptr, commitThreadsIdle);
  ASSERT_EQ(0, *commitThreadsIdle->ptr);

  uint32_t const expectedConsolidationThreads
    = std::max(1U, uint32_t(arangodb::NumberOfCores::getValue()) / 6);
  uint32_t const expectedCommitThreads = 6;
  uint32_t const expectedCommitThreadsIdle = 6;

  opts->processingResult().touch("arangosearch.commit-threads");
  *commitThreads->ptr = 6;
  opts->processingResult().touch("arangosearch.commit-threads-idle");
  *commitThreadsIdle->ptr = 1 + *commitThreads->ptr;

  feature.validateOptions(opts);
  ASSERT_EQ(0, *threads->ptr);
  ASSERT_EQ(0, *threadsLimit->ptr);
  ASSERT_EQ(expectedConsolidationThreads, *consolidationThreads->ptr);
  ASSERT_EQ(expectedConsolidationThreads, *consolidationThreadsIdle->ptr);
  ASSERT_EQ(expectedCommitThreads, *commitThreads->ptr);
  ASSERT_EQ(expectedCommitThreadsIdle, *commitThreadsIdle->ptr);

  feature.prepare();
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_1));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_1));

  feature.start();
  ASSERT_EQ(std::make_pair(size_t(*commitThreads->ptr), size_t(*commitThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(*consolidationThreads->ptr), size_t(*consolidationThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_1));
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_0);
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_1);
  feature.stop();
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_1));
}

TEST_F(IResearchFeatureTest, test_options_custom_thread_count) {
  using namespace arangodb::options;
  using namespace arangodb::iresearch;

  IResearchFeature feature(server.server());
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_1));

  auto waitForStats = [&](std::tuple<size_t, size_t, size_t> expectedStats,
                          arangodb::iresearch::ThreadGroup group,
                          std::chrono::steady_clock::duration timeout = 10s) {
    auto const end = std::chrono::steady_clock::now() + timeout;
    while (expectedStats != feature.stats(group)) {
      std::this_thread::sleep_for(10ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  };

  auto opts = std::make_shared<ProgramOptions>("", "", "", "");
  feature.collectOptions(opts);
  auto* threads = opts->get<UInt32Parameter>("--arangosearch.threads");
  ASSERT_NE(nullptr, threads);
  ASSERT_EQ(0, *threads->ptr);
  auto* threadsLimit = opts->get<UInt32Parameter>("--arangosearch.threads-limit");
  ASSERT_NE(nullptr, threadsLimit);
  ASSERT_EQ(0, *threadsLimit->ptr);
  auto* consolidationThreads = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads");
  ASSERT_NE(nullptr, consolidationThreads);
  ASSERT_EQ(0, *consolidationThreads->ptr);
  auto* consolidationThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads-idle");
  ASSERT_NE(nullptr, consolidationThreadsIdle);
  ASSERT_EQ(0, *consolidationThreadsIdle->ptr);
  auto* commitThreads = opts->get<UInt32Parameter>("--arangosearch.commit-threads");
  ASSERT_NE(nullptr, commitThreads);
  ASSERT_EQ(0, *commitThreads->ptr);
  auto* commitThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.commit-threads-idle");
  ASSERT_NE(nullptr, commitThreadsIdle);
  ASSERT_EQ(0, *commitThreadsIdle->ptr);

  uint32_t const expectedConsolidationThreads = 4;
  uint32_t const expectedConsolidationThreadsIdle = 4;
  uint32_t const expectedCommitThreads = 6;
  uint32_t const expectedCommitThreadsIdle = 4;

  opts->processingResult().touch("arangosearch.commit-threads");
  *commitThreads->ptr = expectedCommitThreads;
  opts->processingResult().touch("arangosearch.commit-threads-idle");
  *commitThreadsIdle->ptr = expectedCommitThreadsIdle;
  opts->processingResult().touch("arangosearch.consolidation-threads");
  *consolidationThreads->ptr = expectedConsolidationThreads;
  opts->processingResult().touch("arangosearch.consolidation-threads-idle");
  *consolidationThreadsIdle->ptr = expectedConsolidationThreadsIdle;

  feature.validateOptions(opts);
  ASSERT_EQ(0, *threads->ptr);
  ASSERT_EQ(0, *threadsLimit->ptr);
  ASSERT_EQ(expectedConsolidationThreads, *consolidationThreads->ptr);
  ASSERT_EQ(expectedConsolidationThreadsIdle, *consolidationThreadsIdle->ptr);
  ASSERT_EQ(expectedCommitThreads, *commitThreads->ptr);
  ASSERT_EQ(expectedCommitThreadsIdle, *commitThreadsIdle->ptr);

  feature.prepare();
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_1));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_1));

  feature.start();
  ASSERT_EQ(std::make_pair(size_t(*commitThreads->ptr), size_t(*commitThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(*consolidationThreads->ptr), size_t(*consolidationThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_1));
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_0);
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_1);
  feature.stop();
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_1));
}

TEST_F(IResearchFeatureTest, test_options_commit_threads_max) {
  using namespace arangodb::options;
  using namespace arangodb::iresearch;

  IResearchFeature feature(server.server());
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_1));

  auto waitForStats = [&](std::tuple<size_t, size_t, size_t> expectedStats,
                          arangodb::iresearch::ThreadGroup group,
                          std::chrono::steady_clock::duration timeout = 10s) {
    auto const end = std::chrono::steady_clock::now() + timeout;
    while (expectedStats != feature.stats(group)) {
      std::this_thread::sleep_for(10ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  };

  auto opts = std::make_shared<ProgramOptions>("", "", "", "");
  feature.collectOptions(opts);
  auto* threads = opts->get<UInt32Parameter>("--arangosearch.threads");
  ASSERT_NE(nullptr, threads);
  ASSERT_EQ(0, *threads->ptr);
  auto* threadsLimit = opts->get<UInt32Parameter>("--arangosearch.threads-limit");
  ASSERT_NE(nullptr, threadsLimit);
  ASSERT_EQ(0, *threadsLimit->ptr);
  auto* consolidationThreads = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads");
  ASSERT_NE(nullptr, consolidationThreads);
  ASSERT_EQ(0, *consolidationThreads->ptr);
  auto* consolidationThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads-idle");
  ASSERT_NE(nullptr, consolidationThreadsIdle);
  ASSERT_EQ(0, *consolidationThreadsIdle->ptr);
  auto* commitThreads = opts->get<UInt32Parameter>("--arangosearch.commit-threads");
  ASSERT_NE(nullptr, commitThreads);
  ASSERT_EQ(0, *commitThreads->ptr);
  auto* commitThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.commit-threads-idle");
  ASSERT_NE(nullptr, commitThreadsIdle);
  ASSERT_EQ(0, *commitThreadsIdle->ptr);

  uint32_t const expectedConsolidationThreads
    = std::max(1U, uint32_t(arangodb::NumberOfCores::getValue()) / 6);
  uint32_t const expectedCommitThreads = 4*uint32_t(arangodb::NumberOfCores::getValue());

  opts->processingResult().touch("arangosearch.commit-threads");
  *commitThreads->ptr = std::numeric_limits<uint32_t>::max();

  feature.validateOptions(opts);
  ASSERT_EQ(0, *threads->ptr);
  ASSERT_EQ(0, *threadsLimit->ptr);
  ASSERT_EQ(expectedConsolidationThreads, *consolidationThreads->ptr);
  ASSERT_EQ(expectedConsolidationThreads, *consolidationThreadsIdle->ptr);
  ASSERT_EQ(expectedCommitThreads, *commitThreads->ptr);
  ASSERT_EQ(expectedCommitThreads, *commitThreadsIdle->ptr);

  feature.prepare();
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_1));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_1));

  feature.start();
  ASSERT_EQ(std::make_pair(size_t(*commitThreads->ptr), size_t(*commitThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(*consolidationThreads->ptr), size_t(*consolidationThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_1));
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_0);
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_1);
  feature.stop();
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_1));
}

TEST_F(IResearchFeatureTest, test_options_threads_set_zero) {
  using namespace arangodb::options;
  using namespace arangodb::iresearch;

  IResearchFeature feature(server.server());
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_1));

  auto waitForStats = [&](std::tuple<size_t, size_t, size_t> expectedStats,
                          arangodb::iresearch::ThreadGroup group,
                          std::chrono::steady_clock::duration timeout = 10s) {
    auto const end = std::chrono::steady_clock::now() + timeout;
    while (expectedStats != feature.stats(group)) {
      std::this_thread::sleep_for(10ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  };

  auto opts = std::make_shared<ProgramOptions>("", "", "", "");
  feature.collectOptions(opts);
  auto* threads = opts->get<UInt32Parameter>("--arangosearch.threads");
  ASSERT_NE(nullptr, threads);
  ASSERT_EQ(0, *threads->ptr);
  auto* threadsLimit = opts->get<UInt32Parameter>("--arangosearch.threads-limit");
  ASSERT_NE(nullptr, threadsLimit);
  ASSERT_EQ(0, *threadsLimit->ptr);
  auto* consolidationThreads = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads");
  ASSERT_NE(nullptr, consolidationThreads);
  ASSERT_EQ(0, *consolidationThreads->ptr);
  auto* consolidationThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads-idle");
  ASSERT_NE(nullptr, consolidationThreadsIdle);
  ASSERT_EQ(0, *consolidationThreadsIdle->ptr);
  auto* commitThreads = opts->get<UInt32Parameter>("--arangosearch.commit-threads");
  ASSERT_NE(nullptr, commitThreads);
  ASSERT_EQ(0, *commitThreads->ptr);
  auto* commitThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.commit-threads-idle");
  ASSERT_NE(nullptr, commitThreadsIdle);
  ASSERT_EQ(0, *commitThreadsIdle->ptr);

  opts->processingResult().touch("arangosearch.threads");

  uint32_t const expectedNumThreads = std::max(1U, uint32_t(arangodb::NumberOfCores::getValue()) / 8);
  feature.validateOptions(opts);
  ASSERT_EQ(0, *threads->ptr);
  ASSERT_EQ(0, *threadsLimit->ptr);
  ASSERT_EQ(expectedNumThreads, *consolidationThreads->ptr);
  ASSERT_EQ(expectedNumThreads, *consolidationThreadsIdle->ptr);
  ASSERT_EQ(expectedNumThreads, *commitThreads->ptr);
  ASSERT_EQ(expectedNumThreads, *commitThreadsIdle->ptr);

  feature.prepare();
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_1));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_1));

  feature.start();
  ASSERT_EQ(std::make_pair(size_t(*commitThreads->ptr), size_t(*commitThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(*consolidationThreads->ptr), size_t(*consolidationThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_1));
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_0);
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_1);
  feature.stop();
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_1));
}

TEST_F(IResearchFeatureTest, test_options_threads) {
  using namespace arangodb::options;
  using namespace arangodb::iresearch;

  IResearchFeature feature(server.server());
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_1));

  auto waitForStats = [&](std::tuple<size_t, size_t, size_t> expectedStats,
                          arangodb::iresearch::ThreadGroup group,
                          std::chrono::steady_clock::duration timeout = 10s) {
    auto const end = std::chrono::steady_clock::now() + timeout;
    while (expectedStats != feature.stats(group)) {
      std::this_thread::sleep_for(10ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  };

  auto opts = std::make_shared<ProgramOptions>("", "", "", "");
  feature.collectOptions(opts);
  auto* threads = opts->get<UInt32Parameter>("--arangosearch.threads");
  ASSERT_NE(nullptr, threads);
  ASSERT_EQ(0, *threads->ptr);
  auto* threadsLimit = opts->get<UInt32Parameter>("--arangosearch.threads-limit");
  ASSERT_NE(nullptr, threadsLimit);
  ASSERT_EQ(0, *threadsLimit->ptr);
  auto* consolidationThreads = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads");
  ASSERT_NE(nullptr, consolidationThreads);
  ASSERT_EQ(0, *consolidationThreads->ptr);
  auto* consolidationThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads-idle");
  ASSERT_NE(nullptr, consolidationThreadsIdle);
  ASSERT_EQ(0, *consolidationThreadsIdle->ptr);
  auto* commitThreads = opts->get<UInt32Parameter>("--arangosearch.commit-threads");
  ASSERT_NE(nullptr, commitThreads);
  ASSERT_EQ(0, *commitThreads->ptr);
  auto* commitThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.commit-threads-idle");
  ASSERT_NE(nullptr, commitThreadsIdle);
  ASSERT_EQ(0, *commitThreadsIdle->ptr);

  opts->processingResult().touch("arangosearch.threads");
  *threads->ptr = 3;

  uint32_t const expectedNumThreads = *threads->ptr/2;
  feature.validateOptions(opts);
  ASSERT_EQ(3, *threads->ptr);
  ASSERT_EQ(0, *threadsLimit->ptr);
  ASSERT_EQ(expectedNumThreads, *consolidationThreads->ptr);
  ASSERT_EQ(expectedNumThreads, *consolidationThreadsIdle->ptr);
  ASSERT_EQ(expectedNumThreads, *commitThreads->ptr);
  ASSERT_EQ(expectedNumThreads, *commitThreadsIdle->ptr);

  feature.prepare();
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_1));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_1));

  feature.start();
  ASSERT_EQ(std::make_pair(size_t(*commitThreads->ptr), size_t(*commitThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(*consolidationThreads->ptr), size_t(*consolidationThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_1));
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_0);
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_1);
  feature.stop();
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_1));
}

TEST_F(IResearchFeatureTest, test_options_threads_max) {
  using namespace arangodb::options;
  using namespace arangodb::iresearch;

  IResearchFeature feature(server.server());
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_1));

  auto waitForStats = [&](std::tuple<size_t, size_t, size_t> expectedStats,
                          arangodb::iresearch::ThreadGroup group,
                          std::chrono::steady_clock::duration timeout = 10s) {
    auto const end = std::chrono::steady_clock::now() + timeout;
    while (expectedStats != feature.stats(group)) {
      std::this_thread::sleep_for(10ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  };

  auto opts = std::make_shared<ProgramOptions>("", "", "", "");
  feature.collectOptions(opts);
  auto* threads = opts->get<UInt32Parameter>("--arangosearch.threads");
  ASSERT_NE(nullptr, threads);
  ASSERT_EQ(0, *threads->ptr);
  auto* threadsLimit = opts->get<UInt32Parameter>("--arangosearch.threads-limit");
  ASSERT_NE(nullptr, threadsLimit);
  ASSERT_EQ(0, *threadsLimit->ptr);
  auto* consolidationThreads = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads");
  ASSERT_NE(nullptr, consolidationThreads);
  ASSERT_EQ(0, *consolidationThreads->ptr);
  auto* consolidationThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads-idle");
  ASSERT_NE(nullptr, consolidationThreadsIdle);
  ASSERT_EQ(0, *consolidationThreadsIdle->ptr);
  auto* commitThreads = opts->get<UInt32Parameter>("--arangosearch.commit-threads");
  ASSERT_NE(nullptr, commitThreads);
  ASSERT_EQ(0, *commitThreads->ptr);
  auto* commitThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.commit-threads-idle");
  ASSERT_NE(nullptr, commitThreadsIdle);
  ASSERT_EQ(0, *commitThreadsIdle->ptr);

  opts->processingResult().touch("arangosearch.threads");
  *threads->ptr = std::numeric_limits<uint32_t>::max();

  uint32_t const expectedNumThreads = 8/2;
  feature.validateOptions(opts);
  ASSERT_EQ(std::numeric_limits<uint32_t>::max(), *threads->ptr);
  ASSERT_EQ(0, *threadsLimit->ptr);
  ASSERT_EQ(expectedNumThreads, *consolidationThreads->ptr);
  ASSERT_EQ(expectedNumThreads, *consolidationThreadsIdle->ptr);
  ASSERT_EQ(expectedNumThreads, *commitThreads->ptr);
  ASSERT_EQ(expectedNumThreads, *commitThreadsIdle->ptr);

  feature.prepare();
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_1));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_1));

  feature.start();
  ASSERT_EQ(std::make_pair(size_t(*commitThreads->ptr), size_t(*commitThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(*consolidationThreads->ptr), size_t(*consolidationThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_1));
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_0);
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_1);
  feature.stop();
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_1));
}

TEST_F(IResearchFeatureTest, test_options_threads_limit_max) {
  using namespace arangodb::options;
  using namespace arangodb::iresearch;

  IResearchFeature feature(server.server());
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)),
            feature.limits(ThreadGroup::_1));

  auto waitForStats = [&](std::tuple<size_t, size_t, size_t> expectedStats,
                          arangodb::iresearch::ThreadGroup group,
                          std::chrono::steady_clock::duration timeout = 10s) {
    auto const end = std::chrono::steady_clock::now() + timeout;
    while (expectedStats != feature.stats(group)) {
      std::this_thread::sleep_for(10ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  };

  auto opts = std::make_shared<ProgramOptions>("", "", "", "");
  feature.collectOptions(opts);
  auto* threads = opts->get<UInt32Parameter>("--arangosearch.threads");
  ASSERT_NE(nullptr, threads);
  ASSERT_EQ(0, *threads->ptr);
  auto* threadsLimit = opts->get<UInt32Parameter>("--arangosearch.threads-limit");
  ASSERT_NE(nullptr, threadsLimit);
  ASSERT_EQ(0, *threadsLimit->ptr);
  auto* consolidationThreads = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads");
  ASSERT_NE(nullptr, consolidationThreads);
  ASSERT_EQ(0, *consolidationThreads->ptr);
  auto* consolidationThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.consolidation-threads-idle");
  ASSERT_NE(nullptr, consolidationThreadsIdle);
  ASSERT_EQ(0, *consolidationThreadsIdle->ptr);
  auto* commitThreads = opts->get<UInt32Parameter>("--arangosearch.commit-threads");
  ASSERT_NE(nullptr, commitThreads);
  ASSERT_EQ(0, *commitThreads->ptr);
  auto* commitThreadsIdle = opts->get<UInt32Parameter>("--arangosearch.commit-threads-idle");
  ASSERT_NE(nullptr, commitThreadsIdle);
  ASSERT_EQ(0, *commitThreadsIdle->ptr);

  opts->processingResult().touch("arangosearch.threads-limit");
  *threadsLimit->ptr = 1;

  uint32_t const expectedNumThreads = 1;
  feature.validateOptions(opts);
  ASSERT_EQ(0, *threads->ptr);
  ASSERT_EQ(1, *threadsLimit->ptr);
  ASSERT_EQ(expectedNumThreads, *consolidationThreads->ptr);
  ASSERT_EQ(expectedNumThreads, *consolidationThreadsIdle->ptr);
  ASSERT_EQ(expectedNumThreads, *commitThreads->ptr);
  ASSERT_EQ(expectedNumThreads, *commitThreadsIdle->ptr);

  feature.prepare();
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(0), size_t(0)), feature.limits(ThreadGroup::_1));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            feature.stats(ThreadGroup::_1));

  feature.start();
  ASSERT_EQ(std::make_pair(size_t(*commitThreads->ptr), size_t(*commitThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_0));
  ASSERT_EQ(std::make_pair(size_t(*consolidationThreads->ptr), size_t(*consolidationThreadsIdle->ptr)),
            feature.limits(ThreadGroup::_1));
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_0);
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_1);
  feature.stop();
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            feature.stats(ThreadGroup::_1));
}

TEST_F(IResearchFeatureTest, test_start) {
  using namespace arangodb;
  using namespace arangodb::iresearch;
  using namespace arangodb::options;

  auto& functions = server.addFeatureUntracked<aql::AqlFunctionFeature>();
  auto& iresearch = server.addFeatureUntracked<IResearchFeature>();
  auto cleanup = irs::make_finally([&functions]() { functions.unprepare(); });

  auto waitForStats = [&](std::tuple<size_t, size_t, size_t> expectedStats,
                          arangodb::iresearch::ThreadGroup group,
                          std::chrono::steady_clock::duration timeout = 10s) {
    auto const end = std::chrono::steady_clock::now() + timeout;
    while (expectedStats != iresearch.stats(group)) {
      std::this_thread::sleep_for(10ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  };

  enum class FunctionType { FILTER = 0, SCORER };

  std::map<irs::string_ref, std::pair<irs::string_ref, FunctionType>> expected = {
    // filter functions
    { "EXISTS", { ".|.,.", FunctionType::FILTER } },
    { "PHRASE", { ".,.|.+", FunctionType::FILTER } },
    { "STARTS_WITH", { ".,.|.,.", FunctionType::FILTER } },
    { "MIN_MATCH", { ".,.|.+", FunctionType::FILTER } },
    { "LIKE", { ".,.|.", FunctionType::FILTER } },
    { "NGRAM_MATCH", { ".,.|.,.", FunctionType::FILTER } },
    { "LEVENSHTEIN_MATCH", { ".,.,.|.,.", FunctionType::FILTER } },
    { "IN_RANGE", { ".,.,.,.,.", FunctionType::FILTER } },
    { "GEO_IN_RANGE", { ".,.,.,.|.,.,.", FunctionType::FILTER } },
    { "GEO_CONTAINS", { ".,.", FunctionType::FILTER } },
    { "GEO_INTERSECTS", { ".,.", FunctionType::FILTER } },

    // context functions
    { "ANALYZER", { ".,.", FunctionType::FILTER } },
    { "BOOST", { ".,.", FunctionType::FILTER } },

    // scorer functions
    { "BM25", { ".|+", FunctionType::SCORER } },
    { "TFIDF", { ".|+", FunctionType::SCORER } },
  };

  auto opts = std::make_shared<ProgramOptions>("", "", "", "");
  iresearch.collectOptions(opts);
  iresearch.validateOptions(opts);

  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            iresearch.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            iresearch.stats(ThreadGroup::_1));

  for (auto& entry : expected) {
    auto* function = arangodb::iresearch::getFunction(functions, entry.first);
    EXPECT_EQ(nullptr, function);
  };

  functions.prepare();
  iresearch.prepare();

  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            iresearch.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(1), size_t(0)),
            iresearch.stats(ThreadGroup::_1));

  iresearch.start();
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_0);
  waitForStats(std::make_tuple(size_t(0), size_t(0), size_t(1)), ThreadGroup::_1);

  for (auto& entry : expected) {
    auto* function = arangodb::iresearch::getFunction(functions, entry.first);
    EXPECT_NE(nullptr, function);
    EXPECT_EQ(entry.second.first, function->arguments);
    EXPECT_TRUE((entry.second.second == FunctionType::FILTER &&
                  arangodb::iresearch::isFilter(*function)) ||
                 (entry.second.second == FunctionType::SCORER &&
                  arangodb::iresearch::isScorer(*function)));
  };

  iresearch.stop();

  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            iresearch.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(0)),
            iresearch.stats(ThreadGroup::_1));

  functions.unprepare();
}

TEST_F(IResearchFeatureTest, test_upgrade0_1_no_directory) {
  // test single-server (no directory)
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection\" }");
  auto linkJson = arangodb::velocypack::Parser::fromJson(
      "{ \"view\": \"testView\", \"type\": \"arangosearch\", "
      "\"includeAllFields\": true }");
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\", \"version\": 0 "
      "}");
  auto versionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"version\": 0, \"tasks\": {} }");

  // add the UpgradeFeature, but make sure it is not prepared
  server.addFeatureUntracked<arangodb::UpgradeFeature>(nullptr, std::vector<std::type_index>{});

  auto& feature = server.addFeatureUntracked<arangodb::iresearch::IResearchFeature>();
  feature.collectOptions(server.server().options());
  feature.validateOptions(server.server().options());
  feature.prepare();  // register iresearch view type
  feature.start();    // register upgrade tasks

  server.getFeature<arangodb::DatabaseFeature>().enableUpgrade();  // skip IResearchView validation

  auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
  arangodb::tests::setDatabasePath(dbPathFeature);  // ensure test data is stored in a unique directory
  auto versionFilename = StorageEngineMock::versionFilenameResult;
  auto versionFilenameRestore = irs::make_finally([&versionFilename]() -> void {
    StorageEngineMock::versionFilenameResult = versionFilename;
  });
  StorageEngineMock::versionFilenameResult =
      (irs::utf8_path(dbPathFeature.directory()) /= "version").utf8();
  ASSERT_TRUE(irs::utf8_path(dbPathFeature.directory()).mkdir());
  ASSERT_TRUE((arangodb::basics::VelocyPackHelper::velocyPackToFile(
      StorageEngineMock::versionFilenameResult, versionJson->slice(), false)));

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                        testDBInfo(server.server()));
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_NE(logicalCollection, nullptr);
  auto logicalView0 = vocbase.createView(viewJson->slice());
  ASSERT_NE(logicalView0, nullptr);
  bool created = false;
  auto index = logicalCollection->createIndex(linkJson->slice(), created);
  ASSERT_TRUE(created);
  ASSERT_NE(index, nullptr);
  auto link0 = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
  ASSERT_NE(link0, nullptr);

  index->unload();  // release file handles
  bool result;
  auto linkDataPath = getPersistedPath1(*link0);
  EXPECT_TRUE(linkDataPath.remove());  // remove link directory
  auto viewDataPath = getPersistedPath0(*logicalView0);
  EXPECT_TRUE(viewDataPath.exists(result) && !result);  // ensure no view directory
  arangodb::velocypack::Builder builder;
  builder.openObject();
  EXPECT_TRUE(logicalView0
                  ->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence)
                  .ok());
  builder.close();
  EXPECT_EQ(0, builder.slice().get("version").getNumber<uint32_t>());  // ensure 'version == 0 before upgrade

  EXPECT_TRUE(arangodb::methods::Upgrade::startup(vocbase, true, false).ok());  // run upgrade
  auto logicalView1 = vocbase.lookupView(logicalView0->name());
  EXPECT_FALSE(!logicalView1);  // ensure view present after upgrade
  EXPECT_EQ(logicalView0->id(), logicalView1->id());  // ensure same id for view
  auto link1 = arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection, *logicalView1);
  EXPECT_FALSE(!link1);                 // ensure link present after upgrade
  EXPECT_NE(link0->id(), link1->id());  // ensure new link
  linkDataPath = getPersistedPath1(*link1);
  EXPECT_TRUE(linkDataPath.exists(result) && result);  // ensure link directory created after upgrade
  EXPECT_TRUE(viewDataPath.exists(result) && !result);  // ensure view directory not present
  viewDataPath = getPersistedPath0(*logicalView1);
  EXPECT_TRUE(viewDataPath.exists(result) && !result);  // ensure view directory not created
  builder.clear();
  builder.openObject();
  EXPECT_TRUE(logicalView1
                  ->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence)
                  .ok());
  builder.close();
  EXPECT_EQ(1, builder.slice().get("version").getNumber<uint32_t>());  // ensure 'version == 1 after upgrade
}

TEST_F(IResearchFeatureTest, test_upgrade0_1_with_directory) {
  // test single-server (with directory)
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection\" }");
  auto linkJson = arangodb::velocypack::Parser::fromJson(
      "{ \"view\": \"testView\", \"type\": \"arangosearch\", "
      "\"includeAllFields\": true }");
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\", \"version\": 0 "
      "}");
  auto versionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"version\": 0, \"tasks\": {} }");

  // add the UpgradeFeature, but make sure it is not prepared
  server.addFeatureUntracked<arangodb::UpgradeFeature>(nullptr, std::vector<std::type_index>{});

  auto& feature = server.addFeatureUntracked<arangodb::iresearch::IResearchFeature>();
  feature.collectOptions(server.server().options());
  feature.validateOptions(server.server().options());
  feature.prepare();  // register iresearch view type
  feature.start();    // register upgrade tasks

  server.getFeature<arangodb::DatabaseFeature>().enableUpgrade();  // skip IResearchView validation

  auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
  arangodb::tests::setDatabasePath(dbPathFeature);  // ensure test data is stored in a unique directory
  auto versionFilename = StorageEngineMock::versionFilenameResult;
  auto versionFilenameRestore = irs::make_finally([&versionFilename]() -> void {
    StorageEngineMock::versionFilenameResult = versionFilename;
  });
  StorageEngineMock::versionFilenameResult =
      (irs::utf8_path(dbPathFeature.directory()) /= "version").utf8();
  ASSERT_TRUE(irs::utf8_path(dbPathFeature.directory()).mkdir());
  ASSERT_TRUE((arangodb::basics::VelocyPackHelper::velocyPackToFile(
      StorageEngineMock::versionFilenameResult, versionJson->slice(), false)));

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                        testDBInfo(server.server()));
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_FALSE(!logicalCollection);
  auto logicalView0 = vocbase.createView(viewJson->slice());
  ASSERT_FALSE(!logicalView0);
  bool created;
  auto index = logicalCollection->createIndex(linkJson->slice(), created);
  ASSERT_TRUE(created);
  ASSERT_FALSE(!index);
  auto link0 = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
  ASSERT_FALSE(!link0);

  index->unload();  // release file handles
  bool result;
  auto linkDataPath = getPersistedPath1(*link0);
  EXPECT_TRUE(linkDataPath.remove());  // remove link directory
  auto viewDataPath = getPersistedPath0(*logicalView0);
  EXPECT_TRUE(viewDataPath.exists(result) && !result);
  EXPECT_TRUE(viewDataPath.mkdir());  // create view directory
  EXPECT_TRUE(viewDataPath.exists(result) && result);
  arangodb::velocypack::Builder builder;
  builder.openObject();
  EXPECT_TRUE(logicalView0
                  ->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence)
                  .ok());
  builder.close();
  EXPECT_EQ(0, builder.slice().get("version").getNumber<uint32_t>());  // ensure 'version == 0 before upgrade

  EXPECT_TRUE(arangodb::methods::Upgrade::startup(vocbase, true, false).ok());  // run upgrade
  auto logicalView1 = vocbase.lookupView(logicalView0->name());
  EXPECT_FALSE(!logicalView1);  // ensure view present after upgrade
  EXPECT_EQ(logicalView0->id(), logicalView1->id());  // ensure same id for view
  auto link1 = arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection, *logicalView1);
  EXPECT_FALSE(!link1);                 // ensure link present after upgrade
  EXPECT_NE(link0->id(), link1->id());  // ensure new link
  linkDataPath = getPersistedPath1(*link1);
  EXPECT_TRUE(linkDataPath.exists(result) && result);  // ensure link directory created after upgrade
  EXPECT_TRUE(viewDataPath.exists(result) && !result);  // ensure view directory removed after upgrade
  viewDataPath = getPersistedPath0(*logicalView1);
  EXPECT_TRUE(viewDataPath.exists(result) && !result);  // ensure view directory not created
  builder.clear();
  builder.openObject();
  EXPECT_TRUE(logicalView1
                  ->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence)
                  .ok());
  builder.close();
  EXPECT_EQ(1, builder.slice().get("version").getNumber<uint32_t>());  // ensure 'version == 1 after upgrade
}

TEST_F(IResearchFeatureTest, IResearch_version_test) {
  EXPECT_EQ(IResearch_version, arangodb::rest::Version::getIResearchVersion());
  EXPECT_TRUE(IResearch_version ==
              arangodb::rest::Version::Values["iresearch-version"]);
}

// Temporarily surpress for MSVC
TEST_F(IResearchFeatureTest, test_async_schedule) {
  bool deallocated = false;  // declare above 'feature' to ensure proper destruction order
  arangodb::iresearch::IResearchFeature feature(server.server());
  feature.collectOptions(server.server().options());
  feature.validateOptions(server.server().options());
  feature.prepare();
  feature.start(); // start thread pool
  std::condition_variable cond;
  std::mutex mutex;
  auto lock = irs::make_unique_lock(mutex);

  {
    std::shared_ptr<bool> flag(&deallocated,
                               [](bool* ptr) -> void { *ptr = true; });
    feature.queue(
      arangodb::iresearch::ThreadGroup::_0, 0ms,
      [&cond, &mutex, flag]()  {
        auto scopedLock = irs::make_lock_guard(mutex);
        cond.notify_all();
    });
  }
  EXPECT_NE(std::cv_status::timeout, cond.wait_for(lock, 100ms));
  std::this_thread::sleep_for(100ms);
  EXPECT_TRUE(deallocated);
}

TEST_F(IResearchFeatureTest, test_async_schedule_wait_indefinite) {
  struct Task {
    Task(bool& deallocated, std::mutex& mutex,
         std::condition_variable& cond, std::atomic<size_t>& count,
         arangodb::iresearch::IResearchFeature& feature)
      : flag(&deallocated, [](bool* ptr) -> void { *ptr = true; }),
        mutex(&mutex), cond(&cond),
        count(&count), feature(&feature) {
    }

    void operator()() {
      ++*count;

      {
        auto scopedLock = irs::make_lock_guard(*mutex);
        feature->queue(arangodb::iresearch::ThreadGroup::_1, 10000ms, *this);
      }

      cond->notify_all();
    }

    std::shared_ptr<bool> flag;
    std::mutex* mutex;
    std::condition_variable* cond;
    std::atomic<size_t>* count;
    arangodb::iresearch::IResearchFeature* feature;
  };

  bool deallocated = false;  // declare above 'feature' to ensure proper destruction order
  arangodb::iresearch::IResearchFeature feature(server.server());
  feature.collectOptions(server.server().options());
  server.server().options()
    ->get<arangodb::options::UInt32Parameter>("arangosearch.consolidation-threads")->set("1");
  feature.validateOptions(server.server().options());
  feature.prepare();
  feature.start(); // start thread pool
  std::condition_variable cond;
  std::mutex mutex;
  std::atomic<size_t> count = 0;

  auto lock = irs::make_unique_lock(mutex);
  feature.queue(arangodb::iresearch::ThreadGroup::_1, 0ms,
                Task(deallocated, mutex, cond, count, feature));

  {
    auto const end = std::chrono::steady_clock::now() + 10s;
    while (!count) {
      std::this_thread::sleep_for(10ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  }

  EXPECT_EQ(1, count);
  EXPECT_NE(std::cv_status::timeout, cond.wait_for(lock, 1000ms));  // first run invoked immediately
  EXPECT_FALSE(deallocated);

  {
    auto const end = std::chrono::steady_clock::now() + 10s;
    while (!std::get<1>(feature.stats(arangodb::iresearch::ThreadGroup::_1))) {
      std::this_thread::sleep_for(10ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  }

  std::cv_status wait_status;
  do {
    wait_status = cond.wait_for(lock, 100ms);
    if (std::cv_status::timeout == wait_status) {
      break;
    }
    ASSERT_EQ(1, count); // spurious wakeup?
  } while(1);
  EXPECT_FALSE(deallocated); // still scheduled
  EXPECT_EQ(1, count);
}

TEST_F(IResearchFeatureTest, test_async_single_run_task) {
  bool deallocated = false;  // declare above 'feature' to ensure proper destruction order
  arangodb::iresearch::IResearchFeature feature(server.server());
  feature.collectOptions(server.server().options());
  feature.validateOptions(server.server().options());
  feature.prepare();
  feature.start(); // start thread pool
  std::condition_variable cond;
  std::mutex mutex;
  auto lock = irs::make_unique_lock(mutex);

  {
    std::shared_ptr<bool> flag(&deallocated,
                               [](bool* ptr) -> void { *ptr = true; });
    feature.queue(
      arangodb::iresearch::ThreadGroup::_0, 0ms,
      [&cond, &mutex, flag]() {
        auto scopedLock = irs::make_lock_guard(mutex);
        cond.notify_all();
    });
  }
  EXPECT_NE(std::cv_status::timeout, cond.wait_for(lock, 100ms));
  std::this_thread::sleep_for(100ms);
  EXPECT_TRUE(deallocated);
}

TEST_F(IResearchFeatureTest, test_async_multi_run_task) {
  bool deallocated = false;  // declare above 'feature' to ensure proper destruction order
  arangodb::iresearch::IResearchFeature feature(server.server());
  feature.collectOptions(server.server().options());
  feature.validateOptions(server.server().options());
  feature.prepare();
  feature.start(); // start thread pool
  std::mutex mutex;
  std::condition_variable cond;
  size_t count = 0;
  std::chrono::steady_clock::duration diff;
  auto lock = irs::make_unique_lock(mutex);

  {
    std::shared_ptr<bool> flag(&deallocated,
                               [](bool* ptr) -> void { *ptr = true; });

    struct Task {
      std::shared_ptr<bool> flag;
      size_t* count;
      std::chrono::steady_clock::duration* diff;
      std::mutex* mutex;
      std::condition_variable* cond;
      arangodb::iresearch::IResearchFeature* feature;
      std::chrono::steady_clock::time_point last = std::chrono::steady_clock::now();

      void operator()() {
        *diff = std::chrono::steady_clock::now() - last;
        last = std::chrono::steady_clock::now();
        if (++(*count) <= 1) {
          feature->queue(arangodb::iresearch::ThreadGroup::_0, 100ms, *this);
          return;
        }
        auto scopedLock = irs::make_lock_guard(*mutex);
        cond->notify_all();
      }
    };

    Task task;
    task.mutex = &mutex;
    task.cond = &cond;
    task.feature = &feature;
    task.count = &count;
    task.diff = &diff;
    task.flag = flag;

    feature.queue(arangodb::iresearch::ThreadGroup::_0, 0ms, task);
  }

  EXPECT_NE(std::cv_status::timeout, cond.wait_for(lock, 1000ms));
  std::this_thread::sleep_for(100ms);
  EXPECT_TRUE(deallocated);
  EXPECT_EQ(2, count);
  EXPECT_TRUE(100ms < diff);
}

TEST_F(IResearchFeatureTest, test_async_deallocate_with_running_tasks) {
  bool deallocated = false;
  std::condition_variable cond;
  std::mutex mutex;
  auto lock = irs::make_unique_lock(mutex);

  {
    arangodb::iresearch::IResearchFeature feature(server.server());
    feature.collectOptions(server.server().options());
    feature.validateOptions(server.server().options());
    feature.prepare();
    feature.start();  // start thread pool
    std::shared_ptr<bool> flag(&deallocated,
                               [](bool* ptr) -> void { *ptr = true; });

    struct Task {
      std::shared_ptr<bool> flag;
      std::mutex* mutex;
      std::condition_variable* cond;
      arangodb::iresearch::IResearchFeature* feature;

      void operator()() {
        auto scopedLock = irs::make_lock_guard(*mutex);
        cond->notify_all();

        feature->queue(arangodb::iresearch::ThreadGroup::_0, 100ms, *this);
      }
    };

    Task task;
    task.mutex = &mutex;
    task.cond = &cond;
    task.feature = &feature;
    task.flag = flag;

    feature.queue(arangodb::iresearch::ThreadGroup::_0, 0ms, task);

    EXPECT_NE(std::cv_status::timeout, cond.wait_for(lock, 100ms));
  }

  EXPECT_TRUE(deallocated);
}

TEST_F(IResearchFeatureTest, test_async_schedule_task_resize_pool) {
  bool deallocated = false;  // declare above 'feature' to ensure proper destruction order
  arangodb::iresearch::IResearchFeature feature(server.server());
  feature.collectOptions(server.server().options());
  server.server().options()->get<arangodb::options::UInt32Parameter>("arangosearch.threads")->set("8");
  feature.validateOptions(server.server().options());
  feature.prepare();
  std::condition_variable cond;
  std::mutex mutex;
  size_t count = 0;
  std::chrono::steady_clock::duration diff;
  auto lock = irs::make_unique_lock(mutex);
  {
    std::shared_ptr<bool> flag(&deallocated,
                               [](bool* ptr) -> void { *ptr = true; });

    struct Task {
      std::shared_ptr<bool> flag;
      size_t* count;
      std::chrono::steady_clock::duration* diff;
      std::mutex* mutex;
      std::condition_variable* cond;
      arangodb::iresearch::IResearchFeature* feature;
      std::chrono::steady_clock::time_point last = std::chrono::steady_clock::now();

      void operator()() {
        *diff = std::chrono::steady_clock::now() - last;
        last = std::chrono::steady_clock::now();
        if (++(*count) <= 1) {
          feature->queue(arangodb::iresearch::ThreadGroup::_0, 100ms, *this);
          return;
        }
        auto scopedLock = irs::make_lock_guard(*mutex);
        cond->notify_all();
      }
    };

    Task task;
    task.mutex = &mutex;
    task.cond = &cond;
    task.feature = &feature;
    task.count = &count;
    task.diff = &diff;
    task.flag = flag;

    feature.queue(arangodb::iresearch::ThreadGroup::_0, 0ms, task);
  }
  feature.start(); // start thread pool after a task has been scheduled, to trigger resize with a task
  EXPECT_NE(std::cv_status::timeout, cond.wait_for(lock, 1000ms));
  std::this_thread::sleep_for(100ms);
  EXPECT_TRUE(deallocated);
  EXPECT_EQ(2, count);
  EXPECT_TRUE(100ms < diff);
}

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
TEST_F(IResearchFeatureTest, test_fail_to_submit_task) {
  {
    auto cleanup = arangodb::scopeGuard(TRI_ClearFailurePointsDebugging);
    TRI_AddFailurePointDebugging("IResearchFeature::testGroupAccess");
    arangodb::iresearch::IResearchFeature feature(server.server());
    feature.collectOptions(server.server().options());
    feature.validateOptions(server.server().options());
    ASSERT_THROW(feature.prepare(), arangodb::basics::Exception);
  }

  {
    auto cleanup = arangodb::scopeGuard(TRI_ClearFailurePointsDebugging);
    TRI_AddFailurePointDebugging("IResearchFeature::queue");
    arangodb::iresearch::IResearchFeature feature(server.server());
    feature.collectOptions(server.server().options());
    feature.validateOptions(server.server().options());
    ASSERT_THROW(feature.prepare(), arangodb::basics::Exception);
  }

  {
    auto cleanup = arangodb::scopeGuard(TRI_ClearFailurePointsDebugging);
    TRI_AddFailurePointDebugging("IResearchFeature::queueGroup0");
    arangodb::iresearch::IResearchFeature feature(server.server());
    feature.collectOptions(server.server().options());
    feature.validateOptions(server.server().options());
    ASSERT_THROW(feature.prepare(), arangodb::basics::Exception);
  }

  {
    auto cleanup = arangodb::scopeGuard(TRI_ClearFailurePointsDebugging);
    TRI_AddFailurePointDebugging("IResearchFeature::queueGroup1");
    arangodb::iresearch::IResearchFeature feature(server.server());
    feature.collectOptions(server.server().options());
    feature.validateOptions(server.server().options());
    ASSERT_THROW(feature.prepare(), arangodb::basics::Exception);
  }
}

TEST_F(IResearchFeatureTest, test_fail_to_start) {
  auto cleanup = arangodb::scopeGuard(TRI_ClearFailurePointsDebugging);

  arangodb::iresearch::IResearchFeature feature(server.server());
  feature.collectOptions(server.server().options());
  feature.validateOptions(server.server().options());
  feature.prepare();
  TRI_AddFailurePointDebugging("IResearchFeature::testGroupAccess");
  ASSERT_THROW(feature.start(), arangodb::basics::Exception);
}
#endif

class IResearchFeatureTestCoordinator
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AGENCY, arangodb::LogLevel::FATAL>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::CLUSTER, arangodb::LogLevel::FATAL> {
 protected:
  arangodb::tests::mocks::MockCoordinator server;

 private:

 protected:
  IResearchFeatureTestCoordinator()
      : server(false) {

    arangodb::tests::init();

    arangodb::ServerState::instance()->setRebootId(arangodb::RebootId{1});  // Hack.
    // we will start Upgrade feature under our control
    server.untrackFeature<arangodb::UpgradeFeature>();
    server.startFeatures();
  }

  arangodb::consensus::index_t agencyTrx(std::string const& key, std::string const& value) {
    // Build an agency transaction:
    auto b2 = VPackParser::fromJson(value);
    auto b = std::make_shared<VPackBuilder>();
    { VPackArrayBuilder trxs(b.get());
      { VPackArrayBuilder trx(b.get());
        { VPackObjectBuilder op(b.get());
          b->add(key, b2->slice()); }}}
    return std::get<1>(
      server.getFeature<arangodb::ClusterFeature>().agencyCache().applyTestTransaction(b));
  }

  void agencyCreateDatabase(std::string const& name) {
    TemplateSpecializer ts(name);
    std::string st = ts.specialize(plan_dbs_string);
    agencyTrx("/arango/Plan/Databases/" + name, st);
    st = ts.specialize(plan_colls_string);
    agencyTrx("/arango/Plan/Collections/" + name, st);
    st = ts.specialize(current_dbs_string);
    agencyTrx("/arango/Current/Databases/" + name, st);
    st = ts.specialize(current_colls_string);
    agencyTrx("/arango/Current/Collections/" + name, st);
    server.getFeature<arangodb::ClusterFeature>().clusterInfo().waitForPlan(
      agencyTrx("/arango/Plan/Version", R"=({"op":"increment"})=")).wait();
    server.getFeature<arangodb::ClusterFeature>().clusterInfo().waitForCurrent(
      agencyTrx("/arango/Current/Version", R"=({"op":"increment"})=")).wait();
  }

  void agencyDropDatabase(std::string const& name) {
    std::string st = R"=({"op":"delete"}))=";
    agencyTrx("/arango/Plan/Databases/" + name, st);
    agencyTrx("/arango/Plan/Collections/" + name, st);
    agencyTrx("/arango/Current/Databases/" + name, st);
    agencyTrx("/arango/Current/Collections/" + name, st);
    server.getFeature<arangodb::ClusterFeature>().clusterInfo().waitForPlan(
      agencyTrx("/arango/Plan/Version", R"=({"op":"increment"})=")).wait();
    server.getFeature<arangodb::ClusterFeature>().clusterInfo().waitForCurrent(
      agencyTrx("/arango/Current/Version", R"=({"op":"increment"})=")).wait();
  }

  VPackBuilder agencyCreateIndex(
    std::string const& db, std::string const& cid, std::set<std::string> const& fields,
    bool deduplicate, uint64_t id, std::string const& name, bool sparse,
    std::string const& type, bool unique) {
    VPackBuilder b;
    { VPackObjectBuilder o(&b);
      b.add(
        VPackValue(std::string("/arango/Plan/Collections/") + db + "/" + cid + "/indexes"));
      { VPackObjectBuilder oo(&b);
        b.add("op", VPackValue("push"));
        b.add(VPackValue("new"));
        { VPackObjectBuilder ooo(&b);
          b.add(VPackValue("fields"));
          { VPackArrayBuilder aa(&b);
            for (auto const& i : fields) {
              b.add(VPackValue(i));
            }}
          b.add("deduplicate", VPackValue(deduplicate));
          b.add("id", VPackValue(id));
          b.add("inBackground", VPackValue(false));
          b.add("name", VPackValue(name));
          b.add("sparse", VPackValue(sparse));
          b.add("type", VPackValue(type));
          b.add("unique", VPackValue(unique)); }
      }
    }
    return b;
  }
};

TEST_F(IResearchFeatureTestCoordinator, test_upgrade0_1) {
  // test coordinator
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": \"41\", \"name\": \"testCollection\", \"shards\":{} }");
  auto linkJson = arangodb::velocypack::Parser::fromJson(
      "{ \"view\": \"testView\", \"type\": \"arangosearch\", "
      "\"includeAllFields\": true }");
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": 42, \"name\": \"testView\", \"type\": \"arangosearch\", "
      "\"version\": 0 }");
  auto versionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"version\": 0, \"tasks\": {} }");
  auto collectionId = std::to_string(41);
  auto viewId = std::to_string(42);

  server.getFeature<arangodb::DatabaseFeature>().enableUpgrade();  // skip IResearchView validation

  auto& engine = server.getFeature<arangodb::EngineSelectorFeature>().engine();
  auto& factory =
      server.getFeature<arangodb::iresearch::IResearchFeature>().factory<arangodb::ClusterEngine>();
  const_cast<arangodb::IndexFactory&>(engine.indexFactory())
      .emplace(  // required for Indexes::ensureIndex(...)
          arangodb::iresearch::DATA_SOURCE_TYPE.name(), factory);
  auto& ci = server.getFeature<arangodb::ClusterFeature>().clusterInfo();
  TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature

  auto& database = server.getFeature<arangodb::DatabaseFeature>();
  ASSERT_TRUE(database.createDatabase(testDBInfo(server.server()), vocbase).ok());

  agencyCreateDatabase(vocbase->name());

  ASSERT_TRUE(
    ci.createCollectionCoordinator(
      vocbase->name(), collectionId, 0, 1, 1, false, collectionJson->slice(), 0.0, false, nullptr)
    .ok());
  auto logicalCollection = ci.getCollection(vocbase->name(), collectionId);
  ASSERT_FALSE(!logicalCollection);
  EXPECT_TRUE(
      (ci.createViewCoordinator(vocbase->name(), viewId, viewJson->slice()).ok()));
  auto logicalView0 = ci.getView(vocbase->name(), viewId);
  ASSERT_FALSE(!logicalView0);


  arangodb::velocypack::Builder tmp;

  auto const currentCollectionPath = "/Current/Collections/" + vocbase->name() +
                                     "/" +
                                     std::to_string(logicalCollection->id().id());
  {
    ASSERT_TRUE(logicalView0);
    auto const viewId = std::to_string(logicalView0->planId().id());
    EXPECT_TRUE("42" == viewId);

    // simulate heartbeat thread (create index in current)
    {
      auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id\": { \"indexes\" : [ { \"id\": \"1\" } ] } }");
      EXPECT_TRUE(arangodb::AgencyComm(server.server())
                  .setValue(currentCollectionPath, value->slice(), 0.0)
                  .successful());
    }
  }

  auto [t,i] = server.getFeature<arangodb::ClusterFeature>().
    agencyCache().read(
      std::vector<std::string>{"/arango"});


  ASSERT_TRUE((arangodb::methods::Indexes::ensureIndex(logicalCollection.get(),
                                                         linkJson->slice(), true, tmp)
               .ok()));
  logicalCollection = ci.getCollection(vocbase->name(), collectionId);
  ASSERT_FALSE(!logicalCollection);
  auto link0 = arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection, *logicalView0);
  ASSERT_FALSE(!link0);

  arangodb::velocypack::Builder builder;
  builder.openObject();
  EXPECT_TRUE(logicalView0
                  ->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence)
                  .ok());
  builder.close();
  EXPECT_EQ(0, builder.slice().get("version").getNumber<uint32_t>());  // ensure 'version == 0 before upgrade

  // ensure no upgrade on coordinator
  // simulate heartbeat thread (create index in current)
  {
    auto const path = "/Current/Collections/" + vocbase->name() + "/" +
                      std::to_string(logicalCollection->id().id());
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"2\" } "
        "] } }");
    EXPECT_TRUE(arangodb::AgencyComm(server.server())
                    .setValue(path, value->slice(), 0.0)
                    .successful());

    auto b = std::make_shared<VPackBuilder>();
    { VPackArrayBuilder trxs(b.get());
      { VPackArrayBuilder trx(b.get());
        { VPackObjectBuilder op(b.get());
          b->add(path, value->slice()); }}}
    server.getFeature<arangodb::ClusterFeature>().agencyCache().applyTestTransaction(b);

  }
  EXPECT_TRUE(arangodb::methods::Upgrade::clusterBootstrap(*vocbase).ok());  // run upgrade
  auto logicalCollection2 = ci.getCollection(vocbase->name(), collectionId);
  ASSERT_FALSE(!logicalCollection2);
  auto logicalView1 = ci.getView(vocbase->name(), viewId);
  EXPECT_FALSE(!logicalView1);  // ensure view present after upgrade
  EXPECT_EQ(logicalView0->id(), logicalView1->id());  // ensure same id for view
  auto link1 = arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection2, *logicalView1);
  EXPECT_FALSE(!link1);                 // ensure link present after upgrade
  EXPECT_EQ(link0->id(), link1->id());  // ensure new link
  builder.clear();
  builder.openObject();
  EXPECT_TRUE(logicalView1
                  ->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence)
                  .ok());
  builder.close();
  EXPECT_EQ(0, builder.slice().get("version").getNumber<uint32_t>());  // ensure 'version == 0 after upgrade
}

class IResearchFeatureTestDBServer
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AGENCY, arangodb::LogLevel::FATAL>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::CLUSTER, arangodb::LogLevel::FATAL> {
 protected:
  arangodb::tests::mocks::MockDBServer server;

 private:

 protected:
  IResearchFeatureTestDBServer()
      : server(false) {


    arangodb::tests::init();

    arangodb::ServerState::instance()->setRebootId(arangodb::RebootId{1});  // Hack.

    //server.addFeature<arangodb::SchedulerFeature>(true);
    // we will control UgradeFeature start!
    server.untrackFeature<arangodb::UpgradeFeature>();
    server.startFeatures();
  }

  // version 0 data-source path
  irs::utf8_path getPersistedPath0(arangodb::LogicalView const& view) {
    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
    irs::utf8_path dataPath(dbPathFeature.directory());
    dataPath /= "databases";
    dataPath /= "database-";
    dataPath += std::to_string(view.vocbase().id());
    dataPath /= arangodb::iresearch::DATA_SOURCE_TYPE.name();
    dataPath += "-";
    dataPath += std::to_string(view.id().id());
    return dataPath;
  };

  // version 1 data-source path
  irs::utf8_path getPersistedPath1(arangodb::iresearch::IResearchLink const& link) {
    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
    irs::utf8_path dataPath(dbPathFeature.directory());
    dataPath /= "databases";
    dataPath /= "database-";
    dataPath += std::to_string(link.collection().vocbase().id());
    dataPath /= arangodb::iresearch::DATA_SOURCE_TYPE.name();
    dataPath += "-";
    dataPath += std::to_string(link.collection().id().id());
    dataPath += "_";
    dataPath += std::to_string(link.id().id());
    return dataPath;
  }


  void createTestDatabase(TRI_vocbase_t*& vocbase, std::string const name = "testDatabase") {
    vocbase = server.createDatabase(name);
    ASSERT_NE(nullptr, vocbase);
    ASSERT_EQ(name, vocbase->name());
    ASSERT_EQ(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, vocbase->type());
  }

};

TEST_F(IResearchFeatureTestDBServer, test_upgrade0_1_no_directory) {
  // test db-server (no directory)
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection\" }");
  auto linkJson = arangodb::velocypack::Parser::fromJson(
      "{ \"view\": \"testView\", \"type\": \"arangosearch\", "
      "\"includeAllFields\": true }");
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\", \"version\": 0 "
      "}");
  auto versionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"version\": 0, \"tasks\": {} }");

  server.getFeature<arangodb::DatabaseFeature>().enableUpgrade();  // skip IResearchView validation

  auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
  arangodb::tests::setDatabasePath(dbPathFeature);  // ensure test data is stored in a unique directory
  auto versionFilename = StorageEngineMock::versionFilenameResult;
  auto versionFilenameRestore = irs::make_finally([&versionFilename]() -> void {
    StorageEngineMock::versionFilenameResult = versionFilename;
  });
  StorageEngineMock::versionFilenameResult =
      (irs::utf8_path(dbPathFeature.directory()) /= "version").utf8();
  ASSERT_TRUE(irs::utf8_path(dbPathFeature.directory()).mkdir());
  ASSERT_TRUE((arangodb::basics::VelocyPackHelper::velocyPackToFile(
      StorageEngineMock::versionFilenameResult, versionJson->slice(), false)));

  auto bogus = std::make_shared<VPackBuilder>();
  { VPackArrayBuilder trxs(bogus.get());
    { VPackArrayBuilder trx(bogus.get());
      { VPackObjectBuilder op(bogus.get());
        bogus->add("a", VPackValue(12)); }}}
  server.server().getFeature<arangodb::ClusterFeature>().agencyCache().applyTestTransaction(
    bogus);

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                        testDBInfo(server.server()));
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_FALSE(!logicalCollection);
  auto logicalView = vocbase.createView(viewJson->slice());
  ASSERT_FALSE(!logicalView);
  auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
  ASSERT_FALSE(!view);
  bool created = false;
  auto index = logicalCollection->createIndex(linkJson->slice(), created);
  ASSERT_TRUE(created);
  ASSERT_FALSE(!index);
  auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
  ASSERT_FALSE(!link);
  ASSERT_TRUE(view->link(link->self()).ok());  // link will not notify view in 'vocbase', hence notify manually

  index->unload();  // release file handles
  bool result;
  auto linkDataPath = getPersistedPath1(*link);
  EXPECT_TRUE(linkDataPath.remove());  // remove link directory
  auto viewDataPath = getPersistedPath0(*logicalView);
  EXPECT_TRUE(viewDataPath.exists(result) && !result);  // ensure no view directory
  arangodb::velocypack::Builder builder;
  builder.openObject();
  EXPECT_TRUE(logicalView
                  ->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence)
                  .ok());
  builder.close();
  EXPECT_EQ(0, builder.slice().get("version").getNumber<uint32_t>());  // ensure 'version == 0 before upgrade

  EXPECT_TRUE(arangodb::methods::Upgrade::startup(vocbase, true, false).ok());  // run upgrade
  logicalView = vocbase.lookupView(logicalView->name());
  EXPECT_FALSE(logicalView);  // ensure view removed after upgrade
  EXPECT_TRUE(viewDataPath.exists(result) && !result);  // ensure view directory not present
}

TEST_F(IResearchFeatureTestDBServer, test_upgrade0_1_with_directory) {
  // test db-server (with directory)
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection\" }");
  auto linkJson = arangodb::velocypack::Parser::fromJson(
      "{ \"view\": \"testView\", \"type\": \"arangosearch\", "
      "\"includeAllFields\": true }");
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\", \"version\": 0 "
      "}");
  auto versionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"version\": 0, \"tasks\": {} }");

  server.getFeature<arangodb::DatabaseFeature>().enableUpgrade();  // skip IResearchView validation

  auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
  arangodb::tests::setDatabasePath(dbPathFeature);  // ensure test data is stored in a unique directory
  auto versionFilename = StorageEngineMock::versionFilenameResult;
  auto versionFilenameRestore = irs::make_finally([&versionFilename]() -> void {
    StorageEngineMock::versionFilenameResult = versionFilename;
  });
  StorageEngineMock::versionFilenameResult =
      (irs::utf8_path(dbPathFeature.directory()) /= "version").utf8();
  ASSERT_TRUE(irs::utf8_path(dbPathFeature.directory()).mkdir());
  ASSERT_TRUE((arangodb::basics::VelocyPackHelper::velocyPackToFile(
      StorageEngineMock::versionFilenameResult, versionJson->slice(), false)));

  auto& engine = *static_cast<StorageEngineMock*>(
      &server.getFeature<arangodb::EngineSelectorFeature>().engine());
  engine.views.clear();

  auto bogus = std::make_shared<VPackBuilder>();
  { VPackArrayBuilder trxs(bogus.get());
    { VPackArrayBuilder trx(bogus.get());
      { VPackObjectBuilder op(bogus.get());
        bogus->add("a", VPackValue(12)); }}}
  server.server().getFeature<arangodb::ClusterFeature>().agencyCache().applyTestTransaction(
    bogus);

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                        testDBInfo(server.server()));
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_FALSE(!logicalCollection);
  auto logicalView = vocbase.createView(viewJson->slice());
  ASSERT_FALSE(!logicalView);
  auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
  ASSERT_FALSE(!view);
  bool created;
  auto index = logicalCollection->createIndex(linkJson->slice(), created);
  ASSERT_TRUE(created);
  ASSERT_FALSE(!index);
  auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
  ASSERT_FALSE(!link);
  ASSERT_TRUE(view->link(link->self()).ok());  // link will not notify view in 'vocbase', hence notify manually

  index->unload();  // release file handles
  bool result;
  auto linkDataPath = getPersistedPath1(*link);
  EXPECT_TRUE(linkDataPath.remove());  // remove link directory
  auto viewDataPath = getPersistedPath0(*logicalView);
  EXPECT_TRUE(viewDataPath.exists(result) && !result);
  EXPECT_TRUE(viewDataPath.mkdir());  // create view directory
  EXPECT_TRUE(viewDataPath.exists(result) && result);
  arangodb::velocypack::Builder builder;
  builder.openObject();
  EXPECT_TRUE(logicalView
                  ->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence)
                  .ok());
  builder.close();
  EXPECT_EQ(0, builder.slice().get("version").getNumber<uint32_t>());  // ensure 'version == 0 before upgrade

  EXPECT_TRUE(arangodb::methods::Upgrade::startup(vocbase, true, false).ok());  // run upgrade
  //    EXPECT_TRUE(arangodb::methods::Upgrade::clusterBootstrap(vocbase).ok()); // run upgrade
  logicalView = vocbase.lookupView(logicalView->name());
  EXPECT_FALSE(logicalView);  // ensure view removed after upgrade
  EXPECT_TRUE(viewDataPath.exists(result) && !result);  // ensure view directory removed after upgrade
}

TEST_F(IResearchFeatureTestDBServer, test_upgrade1_link_collectionName) {
  // test db-server (with directory)
  //auto collectionJson = arangodb::velocypack::Parser::fromJson(
  //    "{ \"name\": \"testCollection\", \"id\":999 }");
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\", \"version\": 1 "
      "}");

  auto linkJson = arangodb::velocypack::Parser::fromJson(
      "{ \"view\": \"testView\", \"type\": \"arangosearch\", "
      "\"includeAllFields\": true }");
  // assume step 1 already finished
  auto versionJson = arangodb::velocypack::Parser::fromJson(
      std::string("{ \"version\": ") + std::to_string(arangodb::methods::Version::current()) + ", \"tasks\": {\"upgradeArangoSearch0_1\":true} }");

  server.getFeature<arangodb::DatabaseFeature>().enableUpgrade();  // skip IResearchView validation

  auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
  arangodb::tests::setDatabasePath(dbPathFeature);  // ensure test data is stored in a unique directory
  auto versionFilename = StorageEngineMock::versionFilenameResult;
  auto versionFilenameRestore = irs::make_finally([&versionFilename]() -> void {
    StorageEngineMock::versionFilenameResult = versionFilename;
  });
  StorageEngineMock::versionFilenameResult =
      (irs::utf8_path(dbPathFeature.directory()) /= "version").utf8();
  ASSERT_TRUE(irs::utf8_path(dbPathFeature.directory()).mkdir());

  auto& engine = *static_cast<StorageEngineMock*>(
      &server.getFeature<arangodb::EngineSelectorFeature>().engine());
  engine.views.clear();

  TRI_vocbase_t* vocbase;
  createTestDatabase(vocbase);

  // rewrite file so upgrade task was not executed
  ASSERT_TRUE((arangodb::basics::VelocyPackHelper::velocyPackToFile(
      StorageEngineMock::versionFilenameResult, versionJson->slice(), false)));

  auto& clusterInfo =
      vocbase->server().getFeature<arangodb::ClusterFeature>().clusterInfo();

  //server.createCollection("testDatabase", "999", "testCollection");
  auto logicalCollectionCluster = clusterInfo.getCollection("testDatabase", "_analyzers");//createCollection(collectionJson->slice());
  ASSERT_FALSE(!logicalCollectionCluster);

  // now we have standart collections in ClusterInfo
  // we need corresponding collection in vocbase with the same id!
  // FIXME: remove this as soon as proper DBServer mock will be ready
  // and  createTestDatabase will actually fill collections in vocbase
  std::string collectionJson = "{ \"isSystem\":true, \"name\": \"_analyzers\", \"id\":";
  collectionJson.append(std::to_string(logicalCollectionCluster->id().id())).append("}");
  auto logicalCollection = vocbase->createCollection(VPackParser::fromJson(collectionJson)->slice());

  auto logicalView = vocbase->createView(viewJson->slice());
  ASSERT_FALSE(!logicalView);
  auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
  bool created;
  auto index = logicalCollection->createIndex(linkJson->slice(), created);
  ASSERT_TRUE(created);
  ASSERT_FALSE(!index);
  auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
  ASSERT_FALSE(!link);
  ASSERT_TRUE(view->link(link->self()).ok());  // link will not notify view in 'vocbase', hence notify manually

  {
    auto indexes = logicalCollection->getIndexes();
    for (auto& index : indexes) {
      if (index->type() == arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK) {
        VPackBuilder builder;
        index->toVelocyPack(builder, arangodb::Index::makeFlags(arangodb::Index::Serialize::Internals));
        ASSERT_FALSE(builder.slice().hasKey("collectionName"));
      }
    }
  }

  EXPECT_TRUE(arangodb::methods::Upgrade::startup(*vocbase, false, false).ok());  // run upgrade

  {
    auto indexes = logicalCollection->getIndexes();
    for (auto& index : indexes) {
      if (index->type() == arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK) {
        VPackBuilder builder;
        index->toVelocyPack(builder, arangodb::Index::makeFlags(arangodb::Index::Serialize::Internals));
        auto slice = builder.slice();
        ASSERT_TRUE(slice.hasKey("collectionName"));
        ASSERT_EQ("_analyzers", slice.get("collectionName").copyString());
      }
    }
  }
}
