// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_SOURCE_POSITION_TABLE_H_
#define V8_CODEGEN_SOURCE_POSITION_TABLE_H_

#include "src/codegen/source-position.h"
#include "src/common/assert-scope.h"
#include "src/common/checks.h"
#include "src/common/globals.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

class ByteArray;
template <typename T>
class Handle;
class Isolate;
class Zone;

struct PositionTableEntry {
  PositionTableEntry()
      : code_offset(0), source_position(0), is_statement(false) {}
  PositionTableEntry(int offset, int64_t source, bool statement)
      : code_offset(offset), source_position(source), is_statement(statement) {}

  int code_offset;
  int64_t source_position;
  bool is_statement;
};

class V8_EXPORT_PRIVATE SourcePositionTableBuilder {
 public:
  enum RecordingMode {
    // Indicates that source positions are never to be generated. (Resulting in
    // an empty table).
    OMIT_SOURCE_POSITIONS,
    // Indicates that source positions are not currently required, but may be
    // generated later.
    LAZY_SOURCE_POSITIONS,
    // Indicates that source positions should be immediately generated.
    RECORD_SOURCE_POSITIONS
  };

  explicit SourcePositionTableBuilder(
      RecordingMode mode = RECORD_SOURCE_POSITIONS);

  void AddPosition(size_t code_offset, SourcePosition source_position,
                   bool is_statement);

  Handle<ByteArray> ToSourcePositionTable(Isolate* isolate);
  OwnedVector<byte> ToSourcePositionTableVector();

  inline bool Omit() const { return mode_ != RECORD_SOURCE_POSITIONS; }
  inline bool Lazy() const { return mode_ == LAZY_SOURCE_POSITIONS; }

 private:
  void AddEntry(const PositionTableEntry& entry);

  RecordingMode mode_;
  std::vector<byte> bytes_;
#ifdef ENABLE_SLOW_DCHECKS
  std::vector<PositionTableEntry> raw_entries_;
#endif
  PositionTableEntry previous_;  // Previously written entry, to compute delta.
};

class V8_EXPORT_PRIVATE SourcePositionTableIterator {
 public:
  enum IterationFilter { kJavaScriptOnly = 0, kExternalOnly = 1, kAll = 2 };

  // Used for saving/restoring the iterator.
  struct IndexAndPositionState {
    int index_;
    PositionTableEntry position_;
    IterationFilter filter_;
  };

  // We expose three flavours of the iterator, depending on the argument passed
  // to the constructor:

  // Handlified iterator allows allocation, but it needs a handle (and thus
  // a handle scope). This is the preferred version.
  explicit SourcePositionTableIterator(
      Handle<ByteArray> byte_array, IterationFilter filter = kJavaScriptOnly);

  // Non-handlified iterator does not need a handle scope, but it disallows
  // allocation during its lifetime. This is useful if there is no handle
  // scope around.
  explicit SourcePositionTableIterator(
      ByteArray byte_array, IterationFilter filter = kJavaScriptOnly);

  // Handle-safe iterator based on an a vector located outside the garbage
  // collected heap, allows allocation during its lifetime.
  explicit SourcePositionTableIterator(
      Vector<const byte> bytes, IterationFilter filter = kJavaScriptOnly);

  void Advance();

  int code_offset() const {
    DCHECK(!done());
    return current_.code_offset;
  }
  SourcePosition source_position() const {
    DCHECK(!done());
    return SourcePosition::FromRaw(current_.source_position);
  }
  bool is_statement() const {
    DCHECK(!done());
    return current_.is_statement;
  }
  bool done() const { return index_ == kDone; }

  IndexAndPositionState GetState() const { return {index_, current_, filter_}; }

  void RestoreState(const IndexAndPositionState& saved_state) {
    index_ = saved_state.index_;
    current_ = saved_state.position_;
    filter_ = saved_state.filter_;
  }

 private:
  static const int kDone = -1;

  Vector<const byte> raw_table_;
  Handle<ByteArray> table_;
  int index_ = 0;
  PositionTableEntry current_;
  IterationFilter filter_;
  DISALLOW_HEAP_ALLOCATION(no_gc)
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_SOURCE_POSITION_TABLE_H_
