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
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterTypes.h"
#include "Cluster/AgencyCache.h"
#include "Metrics/CollectMode.h"
#include "Metrics/Gauge.h"
#include "Metrics/MetricKey.h"
#include "Metrics/MetricsFeature.h"
#include "Metrics/MetricsParts.h"
#include "Mocks/Servers.h"

#include <absl/strings/str_cat.h>

#include <string>
#include <string_view>

using namespace arangodb;

struct ClusterInfoTest : public ::testing::Test {
  ClusterInfoTest() : server("CRDN_0001") {}

  arangodb::tests::mocks::MockCoordinator server;

  ClusterInfo& clusterInfo() {
    return server.getFeature<ClusterFeature>().clusterInfo();
  }

  metrics::MetricsFeature& metricsFeature() {
    return server.getFeature<metrics::MetricsFeature>();
  }

  /// Metric-specific labels used by arangodb_server_health. Exporter identity
  /// (global shortname/role) is not part of these labels.
  static std::string healthLabels(std::string_view targetServer,
                                  std::string_view targetShortName,
                                  std::string_view targetRole) {
    return absl::StrCat("target_server=\"", targetServer,
                        "\",target_shortname=\"", targetShortName,
                        "\",target_role=\"", targetRole, "\"");
  }

  metrics::Gauge<uint64_t>* loadHealthGauge(
      std::string_view targetServer, std::string_view targetShortName,
      std::string_view targetRole) {
    auto* metric = metricsFeature().get(metrics::MetricKeyView{
        "arangodb_server_health",
        healthLabels(targetServer, targetShortName, targetRole)});
    return static_cast<metrics::Gauge<uint64_t>*>(metric);
  }

  /// Serialize a single gauge. Do not call MetricsFeature::toPrometheus()
  /// under MockCoordinator: MetricsFeature is wired with null
  /// QueryRegistry/Statistics/Database feature refs and toPrometheus()
  /// immediately dereferences them.
  static std::string serializeGauge(metrics::Gauge<uint64_t> const& gauge) {
    std::string prometheus;
    gauge.toPrometheus(prometheus, /*globals*/ "", /*ensureWhitespace*/ false);
    return prometheus;
  }

  static void expectPrometheusLine(std::string const& prometheus,
                                   std::string_view targetServer,
                                   std::string_view targetShortName,
                                   std::string_view targetRole,
                                   std::uint64_t value) {
    auto const line =
        absl::StrCat("arangodb_server_health{",
                     healthLabels(targetServer, targetShortName, targetRole),
                     "}", value, "\n");
    EXPECT_NE(std::string::npos, prometheus.find(line))
        << "expected Prometheus line missing:\n"
        << line << "\nin:\n"
        << prometheus;
  }

  void setAliases(std::initializer_list<std::pair<std::string, std::string>>
                      shortNameToServerId) {
    containers::FlatHashMap<ServerID, std::string> aliases;
    for (auto const& [shortName, serverId] : shortNameToServerId) {
      aliases.emplace(shortName, serverId);
    }
    clusterInfo().setServerAliases(std::move(aliases));
  }
};

TEST_F(ClusterInfoTest, testServerExists) {
  auto& ci = server.getFeature<arangodb::ClusterFeature>().clusterInfo();

  // no servers present
  ASSERT_FALSE(ci.serverExists(""));
  ASSERT_FALSE(ci.serverExists("foo"));
  ASSERT_FALSE(ci.serverExists("bar"));
  ASSERT_FALSE(ci.serverExists("PRMR-abcdef-1090595"));

  // populate some servers
  {
    containers::FlatHashMap<ServerID, std::string> servers;
    servers.emplace("PRMR-012345-678", "testi");
    servers.emplace("PRMR-012345-123", "testmann");
    ci.setServers(std::move(servers));

    ASSERT_TRUE(ci.serverExists("PRMR-012345-678"));
    ASSERT_TRUE(ci.serverExists("PRMR-012345-123"));
    ASSERT_FALSE(ci.serverExists("PRMR-012345-1234"));
    ASSERT_FALSE(ci.serverExists("PRMR-12345-123"));

    ASSERT_FALSE(ci.serverExists("testi"));
    ASSERT_FALSE(ci.serverExists("testmann"));
    ASSERT_FALSE(ci.serverExists(""));
    ASSERT_FALSE(ci.serverExists("foo"));
    ASSERT_FALSE(ci.serverExists("bar"));
    ASSERT_FALSE(ci.serverExists("PRMR-abcdef-1090595"));
  }

  {
    // flush servers map once more
    ci.setServers({});

    ASSERT_FALSE(ci.serverExists("PRMR-012345-678"));
    ASSERT_FALSE(ci.serverExists("PRMR-012345-123"));
    ASSERT_FALSE(ci.serverExists("testi"));
    ASSERT_FALSE(ci.serverExists("testmann"));
    ASSERT_FALSE(ci.serverExists(""));
    ASSERT_FALSE(ci.serverExists("foo"));
    ASSERT_FALSE(ci.serverExists("bar"));
    ASSERT_FALSE(ci.serverExists("PRMR-abcdef-1090595"));
  }
}

TEST_F(ClusterInfoTest, testServerAliasExists) {
  auto& ci = server.getFeature<arangodb::ClusterFeature>().clusterInfo();

  // no aliases present
  ASSERT_FALSE(ci.serverAliasExists(""));
  ASSERT_FALSE(ci.serverAliasExists("foo"));
  ASSERT_FALSE(ci.serverAliasExists("bar"));
  ASSERT_FALSE(ci.serverAliasExists("PRMR-abcdef-1090595"));

  // populate some aliases
  {
    containers::FlatHashMap<ServerID, std::string> aliases;
    aliases.emplace("DBServer0001", "PRMR-012345-678");
    aliases.emplace("DBServer0002", "PRMR-9999-666");
    ci.setServerAliases(std::move(aliases));

    ASSERT_TRUE(ci.serverAliasExists("DBServer0001"));
    ASSERT_TRUE(ci.serverAliasExists("DBServer0002"));
    ASSERT_FALSE(ci.serverAliasExists("DBServer0003"));
    ASSERT_FALSE(ci.serverAliasExists("DBServer0000"));
    ASSERT_FALSE(ci.serverAliasExists("DBServer00001"));
    ASSERT_FALSE(ci.serverAliasExists("PRMR-012345-678"));
    ASSERT_FALSE(ci.serverAliasExists("PRMR-9999-666"));
  }

  {
    // flush aliases map once more
    ci.setServerAliases({});

    ASSERT_FALSE(ci.serverAliasExists("DBServer0001"));
    ASSERT_FALSE(ci.serverAliasExists("DBServer0002"));
    ASSERT_FALSE(ci.serverAliasExists("DBServer0003"));
    ASSERT_FALSE(ci.serverAliasExists("DBServer0000"));
    ASSERT_FALSE(ci.serverAliasExists("DBServer00001"));
    ASSERT_FALSE(ci.serverAliasExists("PRMR-012345-678"));
    ASSERT_FALSE(ci.serverAliasExists("PRMR-9999-666"));
  }
}

TEST_F(ClusterInfoTest, plan_will_provide_latest_id) {
  auto& cache{server.getFeature<ClusterFeature>().agencyCache()};
  auto [acb, index]{cache.read({AgencyCommHelper::path("Sync/LatestID")})};
  auto expectedLatestId{
      acb->slice().at(0).get("arango").get("Sync").get("LatestID").getInt()};
  auto& ci{server.getFeature<arangodb::ClusterFeature>().clusterInfo()};
  auto builder{std::make_shared<VPackBuilder>()};
  auto result{ci.agencyPlan(builder)};
  ASSERT_TRUE(result.ok());
  ASSERT_EQ(
      builder->slice().at(0).get("arango").get("Sync").get("LatestID").getInt(),
      expectedLatestId);
}

TEST(ServerHealthMetricValueTest, mapsHealthStatuses) {
  EXPECT_EQ(2u, serverHealthMetricValue(ServerHealth::kGood));
  EXPECT_EQ(1u, serverHealthMetricValue(ServerHealth::kBad));
  EXPECT_EQ(0u, serverHealthMetricValue(ServerHealth::kFailed));
  EXPECT_EQ(0u, serverHealthMetricValue(ServerHealth::kUnclear));
}

TEST_F(ClusterInfoTest, testServerHealthMetrics) {
  auto& ci = clusterInfo();

  setAliases({{"Coordinator0001", "CRDN-1"},
              {"DBServer0001", "PRMR-1"},
              {"DBServer0002", "PRMR-2"}});

  // healthy Coordinator + healthy/bad/failed DBServers in one update
  {
    ServersKnown known;
    known.emplace("CRDN-1", ServerHealthState{.rebootId = RebootId{1},
                                              .status = ServerHealth::kGood});
    known.emplace("PRMR-1", ServerHealthState{.rebootId = RebootId{1},
                                              .status = ServerHealth::kBad});
    known.emplace("PRMR-2", ServerHealthState{.rebootId = RebootId{1},
                                              .status = ServerHealth::kFailed});
    ci.setServersKnown(std::move(known));

    auto* good = loadHealthGauge("CRDN-1", "Coordinator0001", "COORDINATOR");
    auto* bad = loadHealthGauge("PRMR-1", "DBServer0001", "PRIMARY");
    auto* failed = loadHealthGauge("PRMR-2", "DBServer0002", "PRIMARY");
    ASSERT_NE(nullptr, good);
    ASSERT_NE(nullptr, bad);
    ASSERT_NE(nullptr, failed);
    EXPECT_EQ(2u, good->load());
    EXPECT_EQ(1u, bad->load());
    EXPECT_EQ(0u, failed->load());

    std::string prometheus = serializeGauge(*good);
    prometheus += serializeGauge(*bad);
    prometheus += serializeGauge(*failed);

    EXPECT_NE(std::string::npos,
              prometheus.find("arangodb_server_health{target_server=\"CRDN-1\","
                              "target_shortname=\"Coordinator0001\","
                              "target_role=\"COORDINATOR\"}"));
    EXPECT_NE(std::string::npos,
              prometheus.find("arangodb_server_health{target_server=\"PRMR-1\","
                              "target_shortname=\"DBServer0001\","
                              "target_role=\"PRIMARY\"}"));
    EXPECT_NE(std::string::npos,
              prometheus.find("arangodb_server_health{target_server=\"PRMR-2\","
                              "target_shortname=\"DBServer0002\","
                              "target_role=\"PRIMARY\"}"));

    // Exporter identity must not be baked into metric-specific labels.
    // Avoid matching the substring inside target_shortname="...".
    EXPECT_EQ(std::string::npos, prometheus.find("exporter_shortname="));
    EXPECT_EQ(std::string::npos, prometheus.find("{shortname="));
    EXPECT_EQ(std::string::npos, prometheus.find(",shortname="));
  }

  // update values and remove a disappeared DBServer
  {
    setAliases(
        {{"Coordinator0001", "CRDN-1"}, {"DBServer0001", "PRMR-1"}});

    ServersKnown known;
    known.emplace("CRDN-1", ServerHealthState{.rebootId = RebootId{2},
                                              .status = ServerHealth::kBad});
    known.emplace("PRMR-1", ServerHealthState{.rebootId = RebootId{2},
                                              .status = ServerHealth::kGood});
    ci.setServersKnown(std::move(known));

    auto* crdn = loadHealthGauge("CRDN-1", "Coordinator0001", "COORDINATOR");
    auto* prmr1 = loadHealthGauge("PRMR-1", "DBServer0001", "PRIMARY");
    auto* prmr2 = loadHealthGauge("PRMR-2", "DBServer0002", "PRIMARY");
    ASSERT_NE(nullptr, crdn);
    ASSERT_NE(nullptr, prmr1);
    EXPECT_EQ(nullptr, prmr2);
    EXPECT_EQ(1u, crdn->load());
    EXPECT_EQ(2u, prmr1->load());
  }

  // ShortName becoming available recreates the series with the new label
  {
    ci.setServerAliases({});
    ServersKnown known;
    known.emplace("PRMR-9", ServerHealthState{.rebootId = RebootId{1},
                                              .status = ServerHealth::kGood});
    ci.setServersKnown(std::move(known));

    auto* withoutAlias = loadHealthGauge("PRMR-9", "", "PRIMARY");
    ASSERT_NE(nullptr, withoutAlias);
    EXPECT_EQ(2u, withoutAlias->load());
    EXPECT_EQ(nullptr, loadHealthGauge("PRMR-9", "DBServer0009", "PRIMARY"));

    setAliases({{"DBServer0009", "PRMR-9"}});
    ci.setServersKnown(
        ServersKnown{{"PRMR-9", ServerHealthState{.rebootId = RebootId{1},
                                                  .status = ServerHealth::kGood}}});

    EXPECT_EQ(nullptr, loadHealthGauge("PRMR-9", "", "PRIMARY"));
    auto* withAlias = loadHealthGauge("PRMR-9", "DBServer0009", "PRIMARY");
    ASSERT_NE(nullptr, withAlias);
    EXPECT_EQ(2u, withAlias->load());
  }

  // clearing ServersKnown removes all series
  {
    ci.setServersKnown({});
    EXPECT_EQ(nullptr,
              loadHealthGauge("CRDN-1", "Coordinator0001", "COORDINATOR"));
    EXPECT_EQ(nullptr, loadHealthGauge("PRMR-1", "DBServer0001", "PRIMARY"));
    EXPECT_EQ(nullptr, loadHealthGauge("PRMR-2", "DBServer0002", "PRIMARY"));
    EXPECT_EQ(nullptr, loadHealthGauge("PRMR-9", "DBServer0009", "PRIMARY"));
  }
}

TEST_F(ClusterInfoTest, testServerHealthMetricsPrometheusValues) {
  // Verifies labels + numeric values for every ServerHealth mapping.
  // Labels currently implemented: target_server, target_shortname, target_role.
  // (Prompt names like server_name / server_id are not used by this metric.)
  setAliases({{"Coordinator0001", "CRDN-1"},
              {"DBServer0001", "PRMR-1"},
              {"DBServer0002", "PRMR-2"},
              {"DBServer0003", "PRMR-3"}});

  ServersKnown known;
  known.emplace("CRDN-1", ServerHealthState{.rebootId = RebootId{1},
                                            .status = ServerHealth::kGood});
  known.emplace("PRMR-1", ServerHealthState{.rebootId = RebootId{1},
                                            .status = ServerHealth::kBad});
  known.emplace("PRMR-2", ServerHealthState{.rebootId = RebootId{1},
                                            .status = ServerHealth::kFailed});
  known.emplace("PRMR-3", ServerHealthState{.rebootId = RebootId{1},
                                            .status = ServerHealth::kUnclear});
  clusterInfo().setServersKnown(std::move(known));

  auto* good = loadHealthGauge("CRDN-1", "Coordinator0001", "COORDINATOR");
  auto* bad = loadHealthGauge("PRMR-1", "DBServer0001", "PRIMARY");
  auto* failed = loadHealthGauge("PRMR-2", "DBServer0002", "PRIMARY");
  auto* unclear = loadHealthGauge("PRMR-3", "DBServer0003", "PRIMARY");
  ASSERT_NE(nullptr, good);
  ASSERT_NE(nullptr, bad);
  ASSERT_NE(nullptr, failed);
  ASSERT_NE(nullptr, unclear);

  std::string prometheus;
  prometheus += serializeGauge(*good);
  prometheus += serializeGauge(*bad);
  prometheus += serializeGauge(*failed);
  prometheus += serializeGauge(*unclear);

  expectPrometheusLine(prometheus, "CRDN-1", "Coordinator0001", "COORDINATOR",
                       /*GOOD*/ 2);
  expectPrometheusLine(prometheus, "PRMR-1", "DBServer0001", "PRIMARY",
                       /*BAD*/ 1);
  expectPrometheusLine(prometheus, "PRMR-2", "DBServer0002", "PRIMARY",
                       /*FAILED*/ 0);
  expectPrometheusLine(prometheus, "PRMR-3", "DBServer0003", "PRIMARY",
                       /*UNCLEAR*/ 0);
}

TEST_F(ClusterInfoTest, testServerHealthMetricsAliasRefresh) {
  // Alias map rebuild must recreate series so stale shortname labels vanish
  // and the registry does not accumulate duplicates.
  setAliases({{"Coordinator0001", "CRDN-1"}});

  clusterInfo().setServersKnown(ServersKnown{
      {"CRDN-1", ServerHealthState{.rebootId = RebootId{1},
                                   .status = ServerHealth::kGood}}});

  auto* before = loadHealthGauge("CRDN-1", "Coordinator0001", "COORDINATOR");
  ASSERT_NE(nullptr, before);
  EXPECT_EQ(2u, before->load());
  expectPrometheusLine(serializeGauge(*before), "CRDN-1", "Coordinator0001",
                       "COORDINATOR", 2);

  // Rebuild aliases with a different short name for the same unique id.
  // setServerAliases alone does not touch metrics; setServersKnown triggers
  // updateServerHealthMetrics() again.
  setAliases({{"Coordinator9999", "CRDN-1"}});
  clusterInfo().setServersKnown(ServersKnown{
      {"CRDN-1", ServerHealthState{.rebootId = RebootId{2},
                                   .status = ServerHealth::kGood}}});

  EXPECT_EQ(nullptr,
            loadHealthGauge("CRDN-1", "Coordinator0001", "COORDINATOR"))
      << "stale shortname series must be removed";
  auto* after = loadHealthGauge("CRDN-1", "Coordinator9999", "COORDINATOR");
  ASSERT_NE(nullptr, after);
  EXPECT_EQ(2u, after->load());
  expectPrometheusLine(serializeGauge(*after), "CRDN-1", "Coordinator9999",
                       "COORDINATOR", 2);

  // Re-applying the same alias/health must not create a second series.
  auto* afterPtr = after;
  clusterInfo().setServersKnown(ServersKnown{
      {"CRDN-1", ServerHealthState{.rebootId = RebootId{3},
                                   .status = ServerHealth::kBad}}});
  auto* again = loadHealthGauge("CRDN-1", "Coordinator9999", "COORDINATOR");
  ASSERT_NE(nullptr, again);
  EXPECT_EQ(afterPtr, again) << "unchanged labels must reuse the same gauge";
  EXPECT_EQ(1u, again->load());
  EXPECT_EQ(nullptr,
            loadHealthGauge("CRDN-1", "Coordinator0001", "COORDINATOR"));
}

TEST_F(ClusterInfoTest, testServerHealthMetricsUnknownServerType) {
  // serverHealthTargetRole() returns "UNDEFINED" for unrecognized ids.
  // The metric is still exported (no role-based filtering); shortname is
  // empty unless an alias exists.
  constexpr std::string_view kUnknownId = "SomeRandomServer";

  clusterInfo().setServersKnown(ServersKnown{
      {std::string{kUnknownId},
       ServerHealthState{.rebootId = RebootId{1},
                         .status = ServerHealth::kGood}}});

  auto* gauge = loadHealthGauge(kUnknownId, /*targetShortName*/ "",
                                /*targetRole*/ "UNDEFINED");
  ASSERT_NE(nullptr, gauge)
      << "unknown server types must still produce a series";
  EXPECT_EQ(2u, gauge->load());
  expectPrometheusLine(serializeGauge(*gauge), kUnknownId, "", "UNDEFINED", 2);

  // With an alias, shortname is filled but role stays UNDEFINED.
  setAliases({{"WeirdAlias0001", std::string{kUnknownId}}});
  clusterInfo().setServersKnown(ServersKnown{
      {std::string{kUnknownId},
       ServerHealthState{.rebootId = RebootId{1},
                         .status = ServerHealth::kBad}}});

  EXPECT_EQ(nullptr, loadHealthGauge(kUnknownId, "", "UNDEFINED"));
  auto* withAlias =
      loadHealthGauge(kUnknownId, "WeirdAlias0001", "UNDEFINED");
  ASSERT_NE(nullptr, withAlias);
  EXPECT_EQ(1u, withAlias->load());
  expectPrometheusLine(serializeGauge(*withAlias), kUnknownId, "WeirdAlias0001",
                       "UNDEFINED", 1);
}

TEST_F(ClusterInfoTest, testServerHealthMetricsAliasRename) {
  // Distinct from "alias missing -> present": same unique id, short name
  // renamed (Coordinator0001 -> Coordinator0002).
  setAliases({{"Coordinator0001", "CRDN-1"}});
  clusterInfo().setServersKnown(ServersKnown{
      {"CRDN-1", ServerHealthState{.rebootId = RebootId{1},
                                   .status = ServerHealth::kGood}}});

  auto* oldSeries =
      loadHealthGauge("CRDN-1", "Coordinator0001", "COORDINATOR");
  ASSERT_NE(nullptr, oldSeries);
  EXPECT_EQ(2u, oldSeries->load());

  setAliases({{"Coordinator0002", "CRDN-1"}});
  clusterInfo().setServersKnown(ServersKnown{
      {"CRDN-1", ServerHealthState{.rebootId = RebootId{1},
                                   .status = ServerHealth::kGood}}});

  EXPECT_EQ(nullptr,
            loadHealthGauge("CRDN-1", "Coordinator0001", "COORDINATOR"))
      << "old shortname label must disappear after rename";
  auto* newSeries =
      loadHealthGauge("CRDN-1", "Coordinator0002", "COORDINATOR");
  ASSERT_NE(nullptr, newSeries);
  // Do not compare raw pointers: MetricsFeature may recycle the same address
  // after remove()+ensureMetric(). Lifecycle is validated by label identity.
  EXPECT_EQ(healthLabels("CRDN-1", "Coordinator0002", "COORDINATOR"),
            newSeries->labels());
  EXPECT_EQ(2u, newSeries->load());
  // Unique server id is unchanged across the rename.
  expectPrometheusLine(serializeGauge(*newSeries), "CRDN-1", "Coordinator0002",
                       "COORDINATOR", 2);
}
