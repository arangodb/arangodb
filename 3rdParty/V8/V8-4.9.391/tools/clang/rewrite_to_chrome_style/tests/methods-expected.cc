// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gen/thing.h"

namespace v8 {

class InterfaceOutsideOfBlink {
 public:
  virtual void nonBlinkVirtual() = 0;
};

}  // namespace v8

namespace blink {

class InsideOfBlink : public v8::InterfaceOutsideOfBlink {
 public:
  // This function overrides something outside of blink so don't rename it.
  void nonBlinkVirtual() override {}
  // This function is in blink so rename it.
  virtual void BlinkVirtual() {}
};

class MyIterator {};
using my_iterator = char*;

class Task {
 public:
  // Already style-compliant methods shouldn't change.
  void OutputDebugString() {}

  // Tests that the declarations for methods are updated.
  void DoTheWork();
  // Overload to test using declarations that introduce multiple shadow
  // declarations.
  void DoTheWork(int);
  virtual void ReallyDoTheWork() = 0;

  // Note: this is purposely copyable and assignable, to make sure the Clang
  // tool doesn't try to emit replacements for things that aren't explicitly
  // written.

  // Overloaded operators should not be rewritten.
  Task& operator++() { return *this; }

  // Conversion functions should not be rewritten.
  explicit operator int() const { return 42; }

  // These are special functions that we don't rename so that range-based
  // for loops and STL things work.
  MyIterator begin() {}
  my_iterator end() {}
  my_iterator rbegin() {}
  MyIterator rend() {}
  // The trace() method is used by Oilpan, we shouldn't rename it.
  void trace() {}
  // These are used by std::unique_lock and std::lock_guard.
  void lock() {}
  void unlock() {}
  void try_lock() {}
};

class Other {
  // Static begin/end/trace don't count, and should be renamed.
  static MyIterator Begin() {}
  static my_iterator End() {}
  static void Trace() {}
  static void Lock() {}
};

class NonIterators {
  // begin()/end() and friends are renamed if they don't return an iterator.
  void Begin() {}
  int End() { return 0; }
  void Rbegin() {}
  int Rend() { return 0; }
};

// Test that the actual method definition is also updated.
void Task::DoTheWork() {
  ReallyDoTheWork();
}

template <typename T>
class Testable {
 public:
  typedef T Testable::*UnspecifiedBoolType;
  // This method has a reference to a member in a "member context" and a
  // "non-member context" to verify both are rewritten.
  operator UnspecifiedBoolType() { return ptr_ ? &Testable::ptr_ : 0; }

 private:
  int ptr_;
};

namespace subname {

class SubnameParent {
  virtual void SubnameMethod() {}
};

}  // namespace subname

class SubnameChild : public subname::SubnameParent {
  // This subclasses from blink::subname::SubnameParent and should be renamed.
  void SubnameMethod() override {}
};

class GenChild : public blink::GenClass {
  // This subclasses from the blink namespace but in the gen directory so it
  // should not be renamed.
  void genMethod() override {}
};

}  // namespace blink

// Test that overrides from outside the Blink namespace are also updated.
class BovineTask : public blink::Task {
 public:
  using Task::DoTheWork;
  void ReallyDoTheWork() override;
};

class SuperBovineTask : public BovineTask {
 public:
  using BovineTask::ReallyDoTheWork;
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

bool G() {
  // Use the Testable class to rewrite the method.
  blink::Testable<int> tt;
  return tt;
}

class SubclassOfInsideOfBlink : public blink::InsideOfBlink {
 public:
  // This function overrides something outside of blink so don't rename it.
  void nonBlinkVirtual() override {}
  // This function overrides something in blink so rename it.
  void BlinkVirtual() override {}
};

class TestSubclassInsideOfBlink : public SubclassOfInsideOfBlink {
 public:
 public:
  // This function overrides something outside of blink so don't rename it.
  void nonBlinkVirtual() override {}
  // This function overrides something in blink so rename it.
  void BlinkVirtual() override {}
};

namespace blink {

struct StructInBlink {
  // Structs in blink should rename their methods to capitals.
  bool Function() { return true; }
};

}  // namespace blink

namespace WTF {

struct StructInWTF {
  // Structs in WTF should rename their methods to capitals.
  bool Function() { return true; }
};

}  // namespace WTF

void F2() {
  blink::StructInBlink b;
  b.Function();
  WTF::StructInWTF w;
  w.Function();
}
