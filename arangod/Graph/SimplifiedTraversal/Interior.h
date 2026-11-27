#pragma once

#include <vector>
#include <variant>
#include "Graph/SimplifiedTraversal/IGraphView.h"

namespace arangodb::graph::experimental {
/**
   Memory-efficient datastructure for edge-centric traversal results
 */
struct Interior {
  struct Root {};
  using EdgeLocation = std::variant<Root, size_t>;
  // the location of the previous edge in _screamer and the new edge
  using Entry = std::pair<EdgeLocation, EdgeId>;

  Interior(VertexId start) : _screamer{}, _startVertex{start} {}

  /**
     Add an edge after EdgeLocation `entry` in the traversal
   */
  auto add(Entry const& entry) -> EdgeLocation {
    _screamer.emplace_back(entry);
    return _screamer.size();
  }

  [[nodiscard]] auto shouldEmplace(EdgeId edge) const -> bool { return true; }

  [[nodiscard]] auto size() const -> size_t { return _screamer.size(); }

  /**
     Gives the full path of a traversal, starting from `location` up to
     `_startVertex`
   */
  [[nodiscard]] auto trace(EdgeLocation location)
      -> std::optional<std::vector<EdgeId>> {
    auto result = std::vector<EdgeId>{};
    while (not std::holds_alternative<Root>(location)) {
      size_t index = std::get<size_t>(location);
      if (index > _screamer.size()) {
        return std::nullopt;
      }
      auto [prev, edge] = _screamer.at(index - 1);
      result.emplace_back(edge);
      location = prev;
    }
    return result;
  }

  std::vector<Entry> _screamer;
  VertexId _startVertex;
};

}  // namespace arangodb::graph::experimental
