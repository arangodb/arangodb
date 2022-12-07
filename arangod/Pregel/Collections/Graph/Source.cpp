#include "Source.h"

#include "Graph/GraphManager.h"

namespace {
template<class... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
}  // namespace

using namespace arangodb::pregel::collections::graph;

auto GraphSource::collectionNames(TRI_vocbase_t& vocbase)
    -> ResultT<GraphCollectionNames> {
  return std::visit(
      overloaded{
          [&](GraphName const& x) -> ResultT<GraphCollectionNames> {
            arangodb::graph::GraphManager gmngr{vocbase};
            auto graphRes = gmngr.lookupGraphByName(x.graph);
            if (graphRes.fail()) {
              return graphRes.result();
            }
            std::unique_ptr<arangodb::graph::Graph> graph =
                std::move(graphRes.get());

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
          [&](GraphCollectionNames const& x) -> ResultT<GraphCollectionNames> {
            return x;
          }},
      graphOrCollections);
}

auto GraphSource::restrictions(TRI_vocbase_t& vocbase)
    -> ResultT<EdgeCollectionRestrictions> {
  auto graphSpecificRestrictions = graphRestrictions(vocbase);
  if (graphSpecificRestrictions.fail()) {
    return graphSpecificRestrictions.result();
  }
  auto allRestrictions = edgeCollectionRestrictions;
  return allRestrictions.add(std::move(graphSpecificRestrictions).get());
}

auto GraphSource::graphRestrictions(TRI_vocbase_t& vocbase)
    -> ResultT<EdgeCollectionRestrictions> {
  return std::visit(
      overloaded{
          [&](GraphName const& x) -> ResultT<EdgeCollectionRestrictions> {
            arangodb::graph::GraphManager gmngr{vocbase};
            auto graphRes = gmngr.lookupGraphByName(x.graph);
            if (graphRes.fail()) {
              return graphRes.result();
            }
            std::unique_ptr<arangodb::graph::Graph> graph =
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
          [&](GraphCollectionNames const& x)
              -> ResultT<EdgeCollectionRestrictions> {
            return EdgeCollectionRestrictions{};
          }},
      graphOrCollections);
}
