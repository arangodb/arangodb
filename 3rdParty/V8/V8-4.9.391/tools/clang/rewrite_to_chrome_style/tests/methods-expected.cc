// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace blink {

class Task {
 public:
  // Already style-compliant methods shouldn't change.
  void OutputDebugString() {}

  // Tests that the declarations for methods are updated.
  void DoTheWork();
  virtual void ReallyDoTheWork() = 0;

  // Note: this is purposely copyable and assignable, to make sure the Clang
  // tool doesn't try to emit replacements for things that aren't explicitly
  // written.
  // TODO(dcheng): Add an explicit test for something like operator+.
};

// Test that the actual method definition is also updated.
void Task::DoTheWork() {
  ReallyDoTheWork();
}

}  // namespace blink

namespace Moo {

// Test that overrides from outside the Blink namespace are also updated.
class BovineTask : public blink::Task {
 public:
  void ReallyDoTheWork() override;
};

void BovineTask::ReallyDoTheWork() {
  DoTheWork();
  // Calls via an overridden method should also be updated.
  ReallyDoTheWork();
}

// Finally, test that method pointers are also updated.
void F() {
  void (blink::Task::*p1)() = &blink::Task::DoTheWork;
  void (blink::Task::*p2)() = &BovineTask::DoTheWork;
  void (blink::Task::*p3)() = &blink::Task::ReallyDoTheWork;
  void (BovineTask::*p4)() = &BovineTask::ReallyDoTheWork;
}

}  // namespace Moo
