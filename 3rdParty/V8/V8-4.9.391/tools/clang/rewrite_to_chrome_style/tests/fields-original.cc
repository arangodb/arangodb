// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace blink {

class C {
 public:
  // Make sure initializers are updated to use the new names.
  C() : m_flagField(~0), m_fieldMentioningHTTPAndHTTPS(1) {}

  int method() {
    // Test that references to fields are updated correctly.
    return instanceCount + m_flagField + m_fieldMentioningHTTPAndHTTPS;
  }

  // Test that a field without a m_ prefix is correctly renamed.
  static int instanceCount;

 private:
  // Test that a field with a m_ prefix is correctly renamed.
  const int m_flagField;
  // Statics should be named with s_, but make sure s_ and m_ are both correctly
  // stripped.
  static int s_staticCount;
  static int m_staticCountWithBadName;
  // Make sure that acronyms don't confuse the underscore inserter.
  int m_fieldMentioningHTTPAndHTTPS;
  // Already Google style, should not change.
  int already_google_style_;
};

int C::instanceCount = 0;

// Structs are like classes, but don't use a `_` suffix for members.
struct S {
  int m_integerField;
  int google_style_already;
};

// Unions also use struct-style naming.
union U {
  char fourChars[4];
  short twoShorts[2];
  int one_hopefully_four_byte_int;
};

}  // namespace blink

void F() {
  // Test that references to a static field are correctly rewritten.
  blink::C::instanceCount++;
}
