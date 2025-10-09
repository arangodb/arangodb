#pragma once

#include <variant>

namespace arangodb::graph {

struct NextBatch {};

template<typename Step>
struct QueueEntry : std::variant<Step, NextBatch> {};

}  // namespace arangodb::graph
