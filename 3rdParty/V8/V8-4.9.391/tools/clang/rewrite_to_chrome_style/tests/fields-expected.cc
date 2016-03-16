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
  C()
      : flag_field_(~0),
        field_mentioning_http_and_https_(1),
        should_rename_(0) {}

  int Method() {
    // Test that references to fields are updated correctly.
    return instance_count_ + flag_field_ + field_mentioning_http_and_https_;
  }

  // Test that a field without a m_ prefix is correctly renamed.
  static int instance_count_;

 protected:
  // Test that a field with a m_ prefix is correctly renamed.
  const int flag_field_;
  // Statics should be named with s_, but make sure s_ and m_ are both correctly
  // stripped.
  static int static_count_;
  static int static_count_with_bad_name_;
  // Make sure that acronyms don't confuse the underscore inserter.
  int field_mentioning_http_and_https_;
  // Already Google style, should not change.
  int already_google_style_;

  union {
    // Anonymous union members should be renamed, as should contructor
    // initializers of them.
    char* should_rename_;
    int* does_rename_;
  };
};

struct Derived : public C {
  using C::flag_field_;
  using C::field_mentioning_http_and_https_;
};

int C::instance_count_ = 0;

// Structs are like classes.
struct S {
  int integer_field_;
  int wants_rename;
  int google_style_already;
};

// Unions also use struct-style naming.
union U {
  char four_chars[4];
  short two_shorts[2];
  int one_hopefully_four_byte_int;
  int has_prefix_;
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
  blink::C::instance_count_++;
  // Force instantiation of a copy constructor for blink::C to make sure field
  // initializers for synthesized functions don't cause weird rewrites.
  blink::C c;
  blink::C c2 = c;

  bool b = WTF::TypeTrait::value;
}
