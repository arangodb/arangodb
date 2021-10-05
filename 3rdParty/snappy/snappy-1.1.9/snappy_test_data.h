// Copyright 2020 Google Inc. All Rights Reserved.
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
//
// List of test case files.

#ifndef THIRD_PARTY_SNAPPY_SNAPPY_TEST_DATA_H__
#define THIRD_PARTY_SNAPPY_SNAPPY_TEST_DATA_H__

#include <cstddef>
#include <string>

namespace snappy {

std::string ReadTestDataFile(const char* base, size_t size_limit);

// TODO: Replace anonymous namespace with inline variable when we can
//               rely on C++17.
namespace {

constexpr struct {
  const char* label;
  const char* filename;
  size_t size_limit;
} kTestDataFiles[] = {
  { "html", "html", 0 },
  { "urls", "urls.10K", 0 },
  { "jpg", "fireworks.jpeg", 0 },
  { "jpg_200", "fireworks.jpeg", 200 },
  { "pdf", "paper-100k.pdf", 0 },
  { "html4", "html_x_4", 0 },
  { "txt1", "alice29.txt", 0 },
  { "txt2", "asyoulik.txt", 0 },
  { "txt3", "lcet10.txt", 0 },
  { "txt4", "plrabn12.txt", 0 },
  { "pb", "geo.protodata", 0 },
  { "gaviota", "kppkn.gtb", 0 },
};

}  // namespace

}  // namespace snappy

#endif  // THIRD_PARTY_SNAPPY_SNAPPY_TEST_DATA_H__
