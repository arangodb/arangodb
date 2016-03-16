// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace blink {

// Tests that the prototype for a function is updated.
int testFunctionThatTakesTwoInts(int x, int y);
// Overload to test using declarations that introduce multiple shadow
// declarations.
int testFunctionThatTakesTwoInts(int x, int y, int z);

// Test that the actual function definition is also updated.
int testFunctionThatTakesTwoInts(int x, int y) {
  if (x == 0)
    return y;
  // Calls to the function also need to be updated.
  return testFunctionThatTakesTwoInts(x - 1, y + 1);
}

// This is named like the begin() method which isn't renamed, but
// here it's not a method so it should be.
void begin() {}
// Same for trace() and friends.
void trace() {}
void lock() {}

class SwapType {};

// swap() functions are not renamed.
void swap(SwapType& a, SwapType& b) {}

// Note: F is already Google style and should not change.
void F() {
  // Test referencing a function without calling it.
  int (*functionPointer)(int, int) = &testFunctionThatTakesTwoInts;
}

}  // namespace blink

using blink::testFunctionThatTakesTwoInts;

void G() {
  testFunctionThatTakesTwoInts(1, 2);

  blink::SwapType a, b;
  swap(a, b);
}
