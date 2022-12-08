#include "RestOptions.h"
#include <variant>

#include "Pregel/Collections/Graph/Properties.h"

namespace {
template<class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
}  // namespace

using namespace arangodb::pregel;

auto RestOptions::options() -> PregelOptions {
  if (std::holds_alternative<pregel::RestGraphSettings>(*this)) {
    auto x = std::get<pregel::RestGraphSettings>(*this);
    return PregelOptions{
        .algorithm = x.options.algorithm,
        .userParameters = x.options.userParameters,
        .graphSource = {{collections::graph::GraphName{.graph = x.graph}},
                        {x.options.edgeCollectionRestrictions}}};
  }
  auto x = std::get<pregel::RestCollectionSettings>(*this);
  return PregelOptions{
      .algorithm = x.options.algorithm,
      .userParameters = x.options.userParameters,
      .graphSource = {{collections::graph::GraphCollectionNames{
                          .vertexCollections = x.vertexCollections,
                          .edgeCollections = x.edgeCollections}},
                      {x.options.edgeCollectionRestrictions}}};
}
