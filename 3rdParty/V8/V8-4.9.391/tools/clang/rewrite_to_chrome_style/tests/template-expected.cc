// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace blink {

template <typename T, int number>
void F() {
  // We don't assert on this, and we don't end up considering it a const for
  // now.
  const int maybe_a_const = sizeof(T);
  const int is_a_const = number;
}

template <int number, typename... T>
void F() {
  // We don't assert on this, and we don't end up considering it a const for
  // now.
  const int maybe_a_const = sizeof...(T);
  const int is_a_const = number;
}

}  // namespace blink
