#include <variant>
#include "gtest/gtest.h"
#include "Inspection/VPackWithErrorT.h"
#include "Pregel/REST/RestOptions.h"
#include "VelocypackUtils/VelocyPackStringLiteral.h"
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"

using namespace arangodb::velocypack;
using namespace arangodb::inspection;
using namespace arangodb::pregel;

TEST(pregel_rest_options, requires_an_algorithm) {
  auto slice = R"({})"_vpack;
  auto result = deserializeWithErrorT<RestOptions>(slice);
  ASSERT_TRUE(!result.ok());
}

TEST(pregel_rest_options, initializes_graph_settings) {
  auto slice =
      R"({"algorithm": "wcc", "graphName": "some_graphname", "edgeCollectionRestrictions": {}})"_vpack;
  auto result = deserializeWithErrorT<RestOptions>(slice);
  ASSERT_TRUE(result.ok()) << result.error().error();
  ASSERT_TRUE(std::holds_alternative<RestGraphSettings>(result.get()));
  auto alternative = std::get<RestGraphSettings>(result.get());
  ASSERT_EQ(alternative.options.algorithm, "wcc");
  ASSERT_EQ(
      serializeWithErrorT(alternative.options.userParameters).get().toJson(),
      "{}");
  ASSERT_EQ(alternative.graph, "some_graphname");
  ASSERT_TRUE(alternative.options.edgeCollectionRestrictions.empty());
}

TEST(pregel_rest_options,
     edge_collection_restrictions_in_graph_settings_is_optional) {
  auto slice = R"({"algorithm": "wcc", "graphName": "some_graphname"})"_vpack;
  auto result = deserializeWithErrorT<RestOptions>(slice);
  ASSERT_TRUE(result.ok()) << result.error().error();
  ASSERT_TRUE(std::holds_alternative<RestGraphSettings>(result.get()));
  auto alternative = std::get<RestGraphSettings>(result.get());
  ASSERT_TRUE(alternative.options.edgeCollectionRestrictions.empty());
}

TEST(pregel_rest_options, initializes_graph_settings_with_different_order) {
  auto slice =
      R"({"algorithm": "wcc", "graphName": "some_graphname", "params": {"resultField":"result", "store":"true"}})"_vpack;
  auto result = deserializeWithErrorT<RestOptions>(slice);
  ASSERT_TRUE(result.ok()) << result.error().error();
  ASSERT_TRUE(std::holds_alternative<RestGraphSettings>(result.get()));
  auto alternative = std::get<RestGraphSettings>(result.get());
  ASSERT_EQ(alternative.options.algorithm, "wcc");
  ASSERT_EQ(
      (serializeWithErrorT(alternative.options.userParameters).get().toJson()),
      ("{\"resultField\":\"result\",\"store\":\"true\"}"));
  ASSERT_EQ(alternative.graph, "some_graphname");
  ASSERT_TRUE(alternative.options.edgeCollectionRestrictions.empty());
}

TEST(pregel_rest_options, deserialize_graph_settings) {
  auto params = R"({"resultField":"result", "store":"true"})"_vpack;
  VPackBuilder builder;
  {
    VPackObjectBuilder ob(&builder);
    builder.add(VPackObjectIterator(params.slice()));
  }
  auto options = RestOptions{
      RestGraphSettings{.options = {.algorithm = "wcc",
                                    .userParameters = builder,
                                    .edgeCollectionRestrictions = {}},
                        .graph = "some_graphname"}};
  auto result = serializeWithErrorT(options);
  ASSERT_TRUE(result.ok()) << result.error().error();
  ASSERT_EQ(result.get().toJson(),
            "{\"algorithm\":\"wcc\",\"edgeCollectionRestrictions\":{},"
            "\"graphName\":\"some_graphname\",\"params\":{"
            "\"resultField\":\"result\",\"store\":\"true\"}}");
}

TEST(pregel_rest_options, initializes_collection_settings) {
  auto slice =
      R"({"algorithm": "wcc", "vertexCollections": ["some_collection_name"], "edgeCollections": ["some_collection_name", "another_collection_name"]})"_vpack;
  auto result = deserializeWithErrorT<RestOptions>(slice);
  ASSERT_TRUE(result.ok()) << result.error().error();
  ASSERT_TRUE(std::holds_alternative<RestCollectionSettings>(result.get()));
  auto alternative = std::get<RestCollectionSettings>(result.get());
  ASSERT_EQ(alternative.options.algorithm, "wcc");
  ASSERT_EQ(
      serializeWithErrorT(alternative.options.userParameters).get().toJson(),
      "{}");
  ASSERT_EQ(alternative.vertexCollections,
            std::vector<std::string>{"some_collection_name"});
  ASSERT_EQ(alternative.edgeCollections,
            (std::vector<std::string>{"some_collection_name",
                                      "another_collection_name"}));
}
