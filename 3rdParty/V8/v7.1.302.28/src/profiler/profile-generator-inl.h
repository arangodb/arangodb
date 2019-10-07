// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILER_PROFILE_GENERATOR_INL_H_
#define V8_PROFILER_PROFILE_GENERATOR_INL_H_

#include "src/profiler/profile-generator.h"

namespace v8 {
namespace internal {

CodeEntry::CodeEntry(CodeEventListener::LogEventsAndTags tag, const char* name,
                     const char* resource_name, int line_number,
                     int column_number,
                     std::unique_ptr<SourcePositionTable> line_info,
                     Address instruction_start)
    : bit_field_(TagField::encode(tag) |
                 BuiltinIdField::encode(Builtins::builtin_count)),
      name_(name),
      resource_name_(resource_name),
      line_number_(line_number),
      column_number_(column_number),
      script_id_(v8::UnboundScript::kNoScriptId),
      position_(0),
      line_info_(std::move(line_info)),
      instruction_start_(instruction_start) {}

inline CodeEntry* ProfileGenerator::FindEntry(Address address) {
  CodeEntry* entry = code_map_.FindEntry(address);
  if (entry) entry->mark_used();
  return entry;
}

ProfileNode::ProfileNode(ProfileTree* tree, CodeEntry* entry,
                         ProfileNode* parent, int line_number)
    : tree_(tree),
      entry_(entry),
      self_ticks_(0),
      line_number_(line_number),
      parent_(parent),
      id_(tree->next_node_id()) {
  tree_->EnqueueNode(this);
}

inline unsigned ProfileNode::function_id() const {
  return tree_->GetFunctionId(this);
}

inline Isolate* ProfileNode::isolate() const { return tree_->isolate(); }

}  // namespace internal
}  // namespace v8

#endif  // V8_PROFILER_PROFILE_GENERATOR_INL_H_
