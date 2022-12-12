#include "RestOptions.h"

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
  return std::visit(
      overloaded{[&](pregel::RestGraphSettings const& x) -> PregelOptions {
                   return PregelOptions{
                       .algorithm = x.options.algorithm,
                       .userParameters = x.options.userParameters,
                       .graphDataSource = {GraphName{.graph = x.graph}},
                       .edgeCollectionRestrictions = {
                           x.options.edgeCollectionRestrictions}};
                 },
                 [&](pregel::RestCollectionSettings const& x) -> PregelOptions {
                   return PregelOptions{
                       .algorithm = x.options.algorithm,
                       .userParameters = x.options.userParameters,
                       .graphDataSource = {GraphCollectionNames{
                           .vertexCollections = x.vertexCollections,
                           .edgeCollections = x.edgeCollections}},
                       .edgeCollectionRestrictions = {
                           x.options.edgeCollectionRestrictions}};
                 }},
      *this);
}
