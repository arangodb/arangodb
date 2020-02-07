// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/node-origin-table.h"
#include "src/compiler/graph.h"
#include "src/compiler/node-aux-data.h"

namespace v8 {
namespace internal {
namespace compiler {

void NodeOrigin::PrintJson(std::ostream& out) const {
  out << "{ ";
  switch (origin_kind_) {
    case kGraphNode:
      out << "\"nodeId\" : ";
      break;
    case kWasmBytecode:
      out << "\"bytecodePosition\" : ";
      break;
  }
  out << created_from();
  out << ", \"reducer\" : \"" << reducer_name() << "\"";
  out << ", \"phase\" : \"" << phase_name() << "\"";
  out << "}";
}

class NodeOriginTable::Decorator final : public GraphDecorator {
 public:
  explicit Decorator(NodeOriginTable* origins) : origins_(origins) {}

  void Decorate(Node* node) final {
    origins_->SetNodeOrigin(node, origins_->current_origin_);
  }

 private:
  NodeOriginTable* origins_;
};

NodeOriginTable::NodeOriginTable(Graph* graph)
    : graph_(graph),
      decorator_(nullptr),
      current_origin_(NodeOrigin::Unknown()),
      current_phase_name_("unknown"),
      table_(graph->zone()) {}

void NodeOriginTable::AddDecorator() {
  DCHECK_NULL(decorator_);
  decorator_ = new (graph_->zone()) Decorator(this);
  graph_->AddDecorator(decorator_);
}

void NodeOriginTable::RemoveDecorator() {
  DCHECK_NOT_NULL(decorator_);
  graph_->RemoveDecorator(decorator_);
  decorator_ = nullptr;
}

NodeOrigin NodeOriginTable::GetNodeOrigin(Node* node) const {
  return table_.Get(node);
}

void NodeOriginTable::SetNodeOrigin(Node* node, const NodeOrigin& no) {
  table_.Set(node, no);
}

void NodeOriginTable::PrintJson(std::ostream& os) const {
  os << "{";
  bool needs_comma = false;
  for (auto i : table_) {
    NodeOrigin no = i.second;
    if (no.IsKnown()) {
      if (needs_comma) {
        os << ",";
      }
      os << "\"" << i.first << "\""
         << ": ";
      no.PrintJson(os);
      needs_comma = true;
    }
  }
  os << "}";
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
