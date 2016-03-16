// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace blink {

// Simple global constants.
const char kHelloWorldConstant[] = "Hello world!";
// Make sure a one-character constant doesn't get mangled.
const float kE = 2.718281828;
// Some constants start with a capital letter already.
const int kSpeedOfLightInMetresPerSecond = 299792458;

// Already Chrome style, so shouldn't change.
const float kPi = 3.141592654;

class C {
 public:
  // Static class constants.
  static const int kUsefulConstant = 8;
  // Note: s_ prefix should not be retained.
  static const int kStaticConstant = 9;
  // Note: m_ prefix should not be retained even though the proper prefix is s_.
  static const int kSuperNumber = 42;

  // Not a constant even though it has static storage duration.
  static const char* current_event_;

  static int Function();

  static void FunctionWithConstant() {
    const int kFunctionConstant = 4;
    const int kFunctionConstantFromExpression = 4 + 6;
    const int kFunctionConstantFromOtherConsts =
        kFunctionConstant + kFunctionConstantFromExpression;
    // These don't do the right thing right now, but names like this don't
    // exist in blink (hopefully).
    const int kShould_be_renamed_to_a_const = 9 - 2;
    const int kShould_also_be_renamed_to_a_const =
        kFunctionConstant + kFunctionConstantFromOtherConsts;
    const int not_compile_time_const = kFunctionConstant + Function();
  }
};

void F() {
  // Constant in function body.
  static const char kStaticString[] = "abc";
  // Constant-style naming, since it's initialized with a literal.
  const char* const kNonStaticStringConstant = "def";
  // Not constant-style naming, since it's not initialized with a literal.
  const char* const non_static_string_unconstant = kNonStaticStringConstant;
}

}  // namespace blink
