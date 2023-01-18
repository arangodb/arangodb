#include "RestOptions.h"
#include <variant>

using namespace arangodb::pregel;

auto RestOptions::options() -> PregelOptions {
  if (std::holds_alternative<pregel::RestGraphSettings>(*this)) {
    auto x = std::get<pregel::RestGraphSettings>(*this);
    return PregelOptions{
        .algorithm = x.options.algorithm,
        .userParameters = x.options.userParameters,
        .graphSource = {{GraphName{.graph = x.graph}},
                        {x.options.edgeCollectionRestrictions}}};
  }
  auto x = std::get<pregel::RestCollectionSettings>(*this);
  return PregelOptions{
      .algorithm = x.options.algorithm,
      .userParameters = x.options.userParameters,
      .graphSource = {
          {GraphCollectionNames{.vertexCollections = x.vertexCollections,
                                .edgeCollections = x.edgeCollections}},
          {x.options.edgeCollectionRestrictions}}};
}
