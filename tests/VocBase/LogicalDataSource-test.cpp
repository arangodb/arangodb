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

#include "gtest/gtest.h"

#include "../IResearch/common.h"
#include "../Mocks/StorageEngineMock.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "RestServer/MetricsFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "velocypack/Parser.h"

namespace {

class LogicalViewImpl : public arangodb::LogicalView {
 public:
  LogicalViewImpl(TRI_vocbase_t& vocbase, arangodb::velocypack::Slice const& definition)
      : LogicalView(vocbase, definition) {}
  virtual arangodb::Result appendVelocyPackImpl(
      arangodb::velocypack::Builder&, Serialization) const override {
    return arangodb::Result();
  }
  virtual arangodb::Result dropImpl() override {
    return arangodb::Result();
  }
  virtual void open() override {}
  virtual arangodb::Result renameImpl(std::string const&) override {
    return arangodb::Result();
  }
  virtual arangodb::Result properties(arangodb::velocypack::Slice const& properties,
                                      bool partialUpdate) override {
    return arangodb::Result();
  }
  virtual bool visitCollections(CollectionVisitor const& visitor) const override {
    return true;
  }
};

}

class LogicalDataSourceTest : public ::testing::Test {
 protected:
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature&, bool>> features;

  LogicalDataSourceTest() : engine(server), server(nullptr, nullptr) {
    // setup required application features
    auto& selector = server.addFeature<arangodb::EngineSelectorFeature>();
    features.emplace_back(selector, false);
    selector.setEngineTesting(&engine);
    features.emplace_back(server.addFeature<arangodb::MetricsFeature>(), false);  
    features.emplace_back(server.addFeature<arangodb::QueryRegistryFeature>(), false);  // required for TRI_vocbase_t
    features.emplace_back(server.addFeature<arangodb::ShardingFeature>(), false);

    for (auto& f : features) {
      f.first.prepare();
    }

    for (auto& f : features) {
      if (f.second) {
        f.first.start();
      }
    }
  }

  ~LogicalDataSourceTest() {
    server.getFeature<arangodb::EngineSelectorFeature>().setEngineTesting(nullptr);

    // destroy application features
    for (auto& f : features) {
      if (f.second) {
        f.first.stop();
      }
    }

    for (auto& f : features) {
      f.first.unprepare();
    }
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(LogicalDataSourceTest, test_category) {
  // LogicalCollection
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server));
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection\" }");
    arangodb::LogicalCollection instance(vocbase, json->slice(), true);

    EXPECT_EQ(arangodb::LogicalCollection::category(), instance.category());
  }

  // LogicalView
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server));
    auto json =
        arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
    LogicalViewImpl instance(vocbase, json->slice());

    EXPECT_EQ(arangodb::LogicalView::category(), instance.category());
  }
}

TEST_F(LogicalDataSourceTest, test_construct) {
  // LogicalCollection
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server));
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": 1, \"planId\": 2, \"globallyUniqueId\": \"abc\", \"name\": "
        "\"testCollection\" }");
    arangodb::LogicalCollection instance(vocbase, json->slice(), true);

    EXPECT_EQ(1, instance.id().id());
    EXPECT_EQ(2, instance.planId().id());
    EXPECT_EQ(std::string("abc"), instance.guid());
  }

  // LogicalView
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server));
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": 1, \"planId\": 2, \"globallyUniqueId\": \"abc\", \"name\": "
        "\"testView\" }");
    LogicalViewImpl instance(vocbase, json->slice());

    EXPECT_EQ(1, instance.id().id());
    EXPECT_EQ(2, instance.planId().id());
    EXPECT_EQ(std::string("abc"), instance.guid());
  }
}

TEST_F(LogicalDataSourceTest, test_defaults) {
  // LogicalCollection
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server));
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection\" }");
    arangodb::LogicalCollection instance(vocbase, json->slice(), true);

    EXPECT_TRUE(instance.id().isSet());
    EXPECT_TRUE(instance.planId().isSet());
    EXPECT_FALSE(instance.guid().empty());
  }

  // LogicalView
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server));
    auto json =
        arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
    LogicalViewImpl instance(vocbase, json->slice());

    EXPECT_TRUE(instance.id().isSet());
    EXPECT_TRUE(instance.planId().isSet());
    EXPECT_EQ(instance.id(), instance.planId());
    EXPECT_FALSE(instance.guid().empty());
  }
}
