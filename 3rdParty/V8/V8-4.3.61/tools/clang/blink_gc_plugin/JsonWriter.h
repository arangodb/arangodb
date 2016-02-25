// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_BLINK_GC_PLUGIN_JSON_WRITER_H_
#define TOOLS_BLINK_GC_PLUGIN_JSON_WRITER_H_

#include "llvm/Support/raw_ostream.h"

// TODO(hans): Remove this #ifdef after Clang is rolled past r234897.
#ifdef LLVM_FORCE_HEAD_REVISION
#define JSON_WRITER_STREAM std::unique_ptr<llvm::raw_ostream>
#else
#define JSON_WRITER_STREAM llvm::raw_fd_ostream*
#endif

// Helper to write information for the points-to graph.
class JsonWriter {
 public:
  static JsonWriter* from(JSON_WRITER_STREAM os) {
    return os ? new JsonWriter(std::move(os)) : 0;
  }
#ifndef LLVM_FORCE_HEAD_REVISION
  ~JsonWriter() {
    delete os_;
  }
#endif
  void OpenList() {
    Separator();
    *os_ << "[";
    state_.push(false);
  }
  void OpenList(const std::string key) {
    Write(key);
    *os_ << ":";
    OpenList();
  }
  void CloseList() {
    *os_ << "]";
    state_.pop();
  }
  void OpenObject() {
    Separator();
    *os_ << "{";
    state_.push(false);
  }
  void CloseObject() {
    *os_ << "}\n";
    state_.pop();
  }
  void Write(const size_t val) {
    Separator();
    *os_ << val;
  }
  void Write(const std::string val) {
    Separator();
    *os_ << "\"" << val << "\"";
  }
  void Write(const std::string key, const size_t val) {
    Separator();
    *os_ << "\"" << key << "\":" << val;
  }
  void Write(const std::string key, const std::string val) {
    Separator();
    *os_ << "\"" << key << "\":\"" << val << "\"";
  }
 private:
  JsonWriter(JSON_WRITER_STREAM os) : os_(std::move(os)) {}
  void Separator() {
    if (state_.empty())
      return;
    if (state_.top()) {
      *os_ << ",";
      return;
    }
    state_.top() = true;
  }
  JSON_WRITER_STREAM os_;
  std::stack<bool> state_;
};

#endif // TOOLS_BLINK_GC_PLUGIN_JSON_WRITER_H_
