// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/adapters.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/liveness-analyzer.h"
#include "src/compiler/node.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/state-values-utils.h"

namespace v8 {
namespace internal {
namespace compiler {

LivenessAnalyzer::LivenessAnalyzer(size_t local_count, bool has_accumulator,
                                   Zone* zone)
    : zone_(zone),
      blocks_(zone),
      local_count_(local_count),
      has_accumulator_(has_accumulator),
      queue_(zone) {}

void LivenessAnalyzer::Print(std::ostream& os) {
  for (auto block : blocks_) {
    block->Print(os);
    os << std::endl;
  }
}


LivenessAnalyzerBlock* LivenessAnalyzer::NewBlock() {
  LivenessAnalyzerBlock* result =
      new (zone()->New(sizeof(LivenessAnalyzerBlock))) LivenessAnalyzerBlock(
          blocks_.size(), local_count_, has_accumulator_, zone());
  blocks_.push_back(result);
  return result;
}


LivenessAnalyzerBlock* LivenessAnalyzer::NewBlock(
    LivenessAnalyzerBlock* predecessor) {
  LivenessAnalyzerBlock* result = NewBlock();
  result->AddPredecessor(predecessor);
  return result;
}


void LivenessAnalyzer::Queue(LivenessAnalyzerBlock* block) {
  if (!block->IsQueued()) {
    block->SetQueued();
    queue_.push(block);
  }
}


void LivenessAnalyzer::Run(NonLiveFrameStateSlotReplacer* replacer) {
  if (local_count_ == 0 && !has_accumulator_) {
    // No variables => nothing to do.
    return;
  }

  // Put all blocks into the queue.
  DCHECK(queue_.empty());
  for (auto block : blocks_) {
    Queue(block);
  }

  // Compute the fix-point.
  BitVector working_area(
      static_cast<int>(local_count_) + (has_accumulator_ ? 1 : 0), zone_);
  while (!queue_.empty()) {
    LivenessAnalyzerBlock* block = queue_.front();
    queue_.pop();
    block->Process(&working_area, nullptr);

    for (auto i = block->pred_begin(); i != block->pred_end(); i++) {
      if ((*i)->UpdateLive(&working_area)) {
        Queue(*i);
      }
    }
  }

  // Update the frame states according to the liveness.
  for (auto block : blocks_) {
    block->Process(&working_area, replacer);
  }
}

LivenessAnalyzerBlock::LivenessAnalyzerBlock(size_t id, size_t local_count,
                                             bool has_accumulator, Zone* zone)
    : entries_(zone),
      predecessors_(zone),
      live_(static_cast<int>(local_count) + (has_accumulator ? 1 : 0), zone),
      queued_(false),
      has_accumulator_(has_accumulator),
      id_(id) {}

void LivenessAnalyzerBlock::Process(BitVector* result,
                                    NonLiveFrameStateSlotReplacer* replacer) {
  queued_ = false;

  // Copy the bitvector to the target bit vector.
  result->CopyFrom(live_);

  for (auto entry : base::Reversed(entries_)) {
    switch (entry.kind()) {
      case Entry::kLookup:
        result->Add(entry.var());
        break;
      case Entry::kBind:
        result->Remove(entry.var());
        break;
      case Entry::kCheckpoint:
        if (replacer != nullptr) {
          replacer->ClearNonLiveFrameStateSlots(entry.node(), result);
        }
        break;
    }
  }
}


bool LivenessAnalyzerBlock::UpdateLive(BitVector* working_area) {
  return live_.UnionIsChanged(*working_area);
}


void NonLiveFrameStateSlotReplacer::ClearNonLiveFrameStateSlots(
    Node* frame_state, BitVector* liveness) {
  DCHECK_EQ(liveness->length(), permanently_live_.length());

  DCHECK_EQ(frame_state->opcode(), IrOpcode::kFrameState);
  Node* locals_state = frame_state->InputAt(1);
  DCHECK_EQ(locals_state->opcode(), IrOpcode::kStateValues);
  int count = liveness->length() - (has_accumulator_ ? 1 : 0);
  DCHECK_EQ(count, static_cast<int>(StateValuesAccess(locals_state).size()));
  for (int i = 0; i < count; i++) {
    if (!liveness->Contains(i) && !permanently_live_.Contains(i)) {
      Node* new_values = ClearNonLiveStateValues(locals_state, liveness);
      frame_state->ReplaceInput(1, new_values);
      break;
    }
  }

  if (has_accumulator_) {
    DCHECK_EQ(frame_state->InputAt(2)->opcode(), IrOpcode::kStateValues);
    DCHECK_EQ(
        static_cast<int>(StateValuesAccess(frame_state->InputAt(2)).size()), 1);
    int index = liveness->length() - 1;
    if (!liveness->Contains(index) && !permanently_live_.Contains(index)) {
      Node* new_value =
          state_values_cache()->GetNodeForValues(&replacement_node_, 1);
      frame_state->ReplaceInput(2, new_value);
    }
  }
}


Node* NonLiveFrameStateSlotReplacer::ClearNonLiveStateValues(
    Node* values, BitVector* liveness) {
  DCHECK(inputs_buffer_.empty());

  int var = 0;
  for (Node* value_node : values->inputs()) {
    // Make sure this isn't a state value tree
    DCHECK(value_node->opcode() != IrOpcode::kStateValues);

    // Index of the next variable is its furure index in the inputs buffer,
    // i.e., the buffer's size.
    bool live = liveness->Contains(var) || permanently_live_.Contains(var);
    inputs_buffer_.push_back(live ? value_node : replacement_node_);

    var++;
  }

  Node* result = state_values_cache()->GetNodeForValues(
      inputs_buffer_.empty() ? nullptr : &(inputs_buffer_.front()),
      inputs_buffer_.size());
  inputs_buffer_.clear();
  return result;
}


void LivenessAnalyzerBlock::Print(std::ostream& os) {
  os << "Block " << id();
  bool first = true;
  for (LivenessAnalyzerBlock* pred : predecessors_) {
    if (!first) {
      os << ", ";
    } else {
      os << "; predecessors: ";
      first = false;
    }
    os << pred->id();
  }
  os << std::endl;

  for (auto entry : entries_) {
    os << "    ";
    switch (entry.kind()) {
      case Entry::kLookup:
        if (has_accumulator_ && entry.var() == live_.length() - 1) {
          os << "- Lookup accumulator" << std::endl;
        } else {
          os << "- Lookup " << entry.var() << std::endl;
        }
        break;
      case Entry::kBind:
        if (has_accumulator_ && entry.var() == live_.length() - 1) {
          os << "- Bind accumulator" << std::endl;
        } else {
          os << "- Bind " << entry.var() << std::endl;
        }
        break;
      case Entry::kCheckpoint:
        os << "- Checkpoint " << entry.node()->id() << std::endl;
        break;
    }
  }

  if (live_.length() > 0) {
    os << "    Live set: ";
    for (int i = 0; i < live_.length(); i++) {
      os << (live_.Contains(i) ? "L" : ".");
    }
    os << std::endl;
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
