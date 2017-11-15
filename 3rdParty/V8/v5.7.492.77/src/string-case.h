// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_STRING_CASE_H_
#define V8_STRING_CASE_H_

namespace v8 {
namespace internal {

template <bool is_lower>
int FastAsciiConvert(char* dst, const char* src, int length, bool* changed_out);

}  // namespace internal
}  // namespace v8

#endif  // V8_STRING_CASE_H__
