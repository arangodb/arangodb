// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace blink {

// Note: do not add any copy or move constructors to this class: doing so will
// break test coverage that we don't clobber the class name by trying to emit
// replacements for synthesized functions.
class C {
 public:
  // Make sure initializers are updated to use the new names.
  C() : m_flagField(~0), m_fieldMentioningHTTPAndHTTPS(1), m_shouldRename(0) {}

  int method() {
    // Test that references to fields are updated correctly.
    return instanceCount + m_flagField + m_fieldMentioningHTTPAndHTTPS;
  }

  // Test that a field without a m_ prefix is correctly renamed.
  static int instanceCount;

 protected:
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

  union {
    // Anonymous union members should be renamed, as should contructor
    // initializers of them.
    char* m_shouldRename;
    int* m_doesRename;
  };
};

struct Derived : public C {
  using C::m_flagField;
  using C::m_fieldMentioningHTTPAndHTTPS;
};

int C::instanceCount = 0;

// Structs are like classes.
struct S {
  int m_integerField;
  int wantsRename;
  int google_style_already;
};

// Unions also use struct-style naming.
union U {
  char fourChars[4];
  short twoShorts[2];
  int one_hopefully_four_byte_int;
  int m_hasPrefix;
};

}  // namespace blink

namespace WTF {

struct TypeTrait {
  // WTF has structs for things like type traits, which we don't want to
  // capitalize.
  static const bool value = true;
};

};  // namespace WTF

void F() {
  // Test that references to a static field are correctly rewritten.
  blink::C::instanceCount++;
  // Force instantiation of a copy constructor for blink::C to make sure field
  // initializers for synthesized functions don't cause weird rewrites.
  blink::C c;
  blink::C c2 = c;

  bool b = WTF::TypeTrait::value;
}
