// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace blink {

// Tests that the prototype for a function is updated.
int TestFunctionThatTakesTwoInts(int x, int y);

// Test that the actual function definition is also updated.
int TestFunctionThatTakesTwoInts(int x, int y) {
  if (x == 0)
    return y;
  // Calls to the function also need to be updated.
  return TestFunctionThatTakesTwoInts(x - 1, y + 1);
}

// Note: F is already Google style and should not change.
void F() {
  // Test referencing a function without calling it.
  int (*function_pointer)(int, int) = &TestFunctionThatTakesTwoInts;
}

}  // namespace blink

void G() {
  blink::TestFunctionThatTakesTwoInts(1, 2);
}
