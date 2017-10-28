// Copyright 2010-2014, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef MOZC_TESTING_BASE_PUBLIC_TESTING_UTIL_H_
#define MOZC_TESTING_BASE_PUBLIC_TESTING_UTIL_H_

#include <string>

#include "base/port.h"
#include "base/protobuf/message.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace testing {

// The following code is the implementation of EXPECT_PROTO_EQ and
// EXPECT_PROTO_PEQ (meaning EXPECT_PROTO_PARTIALLY_EQ),
// which compare given proto buffers with output appropriate message.
//
// How to use:
// You can pass the same type of proto buffer to EXPECT_PROTO_P?EQ, in
// "expect, actual" order. For convenience, the first argument of the macros,
// (i.e. expect), can take string literal. It'll be parsed appropriately.
// Assuming the proto P has members a and b;
//
// 1)
// P actual;
// ... (testing code) ...
// P expected;
// expected.set_a(expected_a);
// expected.set_b(expected_b);
// EXPECT_PROTO_EQ(expected, actual);
//
// 2)
// P actual;
// ... (testing code) ...
// EXPECT_PROTO_EQ("a: expected_a b: expected_b", actual);
//
// and so on. We can use EXPECT_PROTO_PEQ instead of EXPECT_PROTO_EQ.
// The difference between them is that EXPECT_PROTO_EQ compares all fields,
// but EXPECT_PROTO_PEQ checks only fields which is set in "expected".

namespace internal {
::testing::AssertionResult EqualsProtoFormat(
    const char *expect_string,
    const char *actual_string,
    const mozc::protobuf::Message &expect,
    const mozc::protobuf::Message &actual,
    bool is_partial);
}  // namespace internal

// Thin wrapper of EqualsProto to check if expect and actual has same type
// on compile time.
template<typename T>
::testing::AssertionResult EqualsProto(
    const char *expect_string, const char *actual_string,
    const T &expect, const T &actual) {
  return ::mozc::testing::internal::EqualsProtoFormat(
      expect_string, actual_string, expect, actual, false);
}

// To accept string constant, we also define a function takeing const char *.
::testing::AssertionResult EqualsProto(
    const char *expect_string, const char *actual_string,
    const char *expect, const mozc::protobuf::Message &actual);

#define EXPECT_PROTO_EQ(expect, actual) \
  EXPECT_PRED_FORMAT2(::mozc::testing::EqualsProto, expect, actual)

// Thin wrapper of PartiallyEqualsProto to if check expect and actual has same
// type on compile time.
template<typename T>
::testing::AssertionResult PartiallyEqualsProtoInternal(
    const char *expect_string, const char *actual_string,
    const T &expect, const T &actual) {
  return ::mozc::testing::internal::EqualsProtoFormat(
      expect_string, actual_string, expect, actual, true);
}

// To accept string constant, we also define a function takeing const char *.
::testing::AssertionResult PartiallyEqualsProto(
    const char *expect_string, const char *actual_string,
    const char *expect, const mozc::protobuf::Message &actual);

#define EXPECT_PROTO_PEQ(expect, actual) \
  EXPECT_PRED_FORMAT2(::mozc::testing::PartiallyEqualsProto, expect, actual)

// Returns serialized string of the given unknown_field_set.
// The main purpose of this method is to make a test which interact with
// binary-serialized protocol buffer directly, such as protobuf format
// change migration.
string SerializeUnknownFieldSetAsString(
    const mozc::protobuf::UnknownFieldSet &unknown_field_set);

}  // namespace testing
}  // namespace mozc

#endif  // MOZC_TESTING_BASE_PUBLIC_TESTING_UTIL_H_
