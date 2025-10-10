#pragma once

#include <variant>

namespace arangodb::graph {

struct NextBatch {
  std::size_t from;
};

template<typename Step>
struct QueueEntry : std::variant<Step, NextBatch> {};

}  // namespace arangodb::graph
