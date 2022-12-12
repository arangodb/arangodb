#include "PregelOptions.h"
#include "Graph/GraphManager.h"
#include "VocBase/vocbase.h"

namespace {
template<class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
}  // namespace

using namespace arangodb::pregel;

auto GraphDataSource::collectionNames(TRI_vocbase_t& vocbase)
    -> ResultT<GraphCollectionNames> {
  return std::visit(
      overloaded{
          [&](pregel::GraphName const& x) -> ResultT<GraphCollectionNames> {
            graph::GraphManager gmngr{vocbase};
            auto graphRes = gmngr.lookupGraphByName(x.graph);
            if (graphRes.fail()) {
              return graphRes.result();
            }
            std::unique_ptr<graph::Graph> graph = std::move(graphRes.get());

            std::vector<std::string> vertexCollections;
            std::vector<std::string> edgeCollections;
            auto const& gv = graph->vertexCollections();
            for (auto const& v : gv) {
              vertexCollections.push_back(v);
            }
            auto const& ge = graph->edgeCollections();
            for (auto const& e : ge) {
              edgeCollections.push_back(e);
            }

            return {GraphCollectionNames{.vertexCollections = vertexCollections,
                                         .edgeCollections = edgeCollections}};
          },
          [&](pregel::GraphCollectionNames const& x)
              -> ResultT<GraphCollectionNames> { return x; }},
      *this);
}

auto GraphDataSource::graphRestrictions(TRI_vocbase_t& vocbase)
    -> ResultT<EdgeCollectionRestrictions> {
  return std::visit(
      overloaded{[&](pregel::GraphName const& x)
                     -> ResultT<EdgeCollectionRestrictions> {
                   graph::GraphManager gmngr{vocbase};
                   auto graphRes = gmngr.lookupGraphByName(x.graph);
                   if (graphRes.fail()) {
                     return graphRes.result();
                   }
                   std::unique_ptr<graph::Graph> graph =
                       std::move(graphRes.get());

                   auto restrictions =
                       std::unordered_map<VertexCollectionID,
                                          std::vector<EdgeCollectionID>>{};
                   auto const& ed = graph->edgeDefinitions();
                   for (auto const& [_, edgeDefinition] : ed) {
                     auto const& from = edgeDefinition.getFrom();
                     for (auto const& f : from) {
                       restrictions[f].push_back(edgeDefinition.getName());
                     }
                   }
                   return EdgeCollectionRestrictions{restrictions};
                 },
                 [&](pregel::GraphCollectionNames const& x)
                     -> ResultT<EdgeCollectionRestrictions> {
                   return EdgeCollectionRestrictions{};
                 }},
      *this);
}

auto EdgeCollectionRestrictions::add(EdgeCollectionRestrictions others) const
    -> EdgeCollectionRestrictions {
  auto newItems = items;
  for (auto const& [vertexCollection, edgeCollections] : others.items) {
    for (auto const& edgeCollection : edgeCollections) {
      newItems[vertexCollection].emplace_back(edgeCollection);
    }
  }
  return {newItems};
}
