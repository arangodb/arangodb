// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace blink {

// Global variables
int frame_count = 0;
// Make sure that underscore-insertion doesn't get too confused by acronyms.
static int variable_mentioning_http_and_https = 1;
// Already Google style, should not change.
int already_google_style_;

// Function parameters
int Function(int interesting_number) {
  // Local variables.
  int a_local_variable = 1;
  // Static locals.
  static int a_static_local_variable = 2;
  // Make sure references to variables are also rewritten.
  return frame_count +
         variable_mentioning_http_and_https * interesting_number /
             a_local_variable % a_static_local_variable;
}

}  // namespace blink

using blink::frame_count;

int F() {
  // Make sure variables qualified with a namespace name are still rewritten
  // correctly.
  return frame_count + blink::frame_count;
}
