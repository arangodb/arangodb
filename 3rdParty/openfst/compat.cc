// Copyright 2005-2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the 'License');
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an 'AS IS' BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Google compatibility definitions.

#include <fst/compat.h>

#include <algorithm>
#include <iostream>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

void FailedNewHandler() {
  std::cerr << "Memory allocation failed" << std::endl;
  std::exit(1);
}

namespace fst {

CheckSummer::CheckSummer() : count_(0) {
  check_sum_.resize(kCheckSumLength, '\0');
}

void CheckSummer::Reset() {
  count_ = 0;
  for (int i = 0; i < kCheckSumLength; ++i) check_sum_[i] = '\0';
}

void CheckSummer::Update(std::string_view data) {
  for (int i = 0; i < data.size(); ++i) {
    check_sum_[(count_++) % kCheckSumLength] ^= data[i];
  }
}

namespace internal {

std::vector<std::string_view> StringSplitter::SplitToSv() {
  std::vector<std::string_view> vec;
  if (delim_.empty()) {
    if (string_.empty() && !skip_empty_) {
      vec.push_back("");
    } else {
      // If empty delimiter, then simply return every character separately as a
      // single-character string_view.
      vec.reserve(string_.size());
      for (int i = 0; i < string_.size(); ++i) {
        vec.push_back(string_.substr(i, 1));
      }
    }
    return vec;
  }
  size_t prev_pos = 0, pos = 0;
  while (pos <= string_.length()) {
    pos = string_.find_first_of(delim_, pos);
    if (pos == std::string_view::npos) {
      pos = string_.length();
    }
    if (!skip_empty_ || pos != prev_pos) {
      vec.push_back(string_.substr(prev_pos, pos - prev_pos));
    }
    prev_pos = ++pos;
  }
  return vec;
}

}  // namespace internal

internal::StringSplitter StrSplit(std::string_view full, ByAnyChar delim) {
  return internal::StringSplitter(full, std::move(delim).delimiters);
}

internal::StringSplitter StrSplit(std::string_view full, char delim) {
  return StrSplit(full, ByAnyChar(std::string_view(&delim, 1)));
}

internal::StringSplitter StrSplit(std::string_view full, ByAnyChar delim,
                                  SkipEmpty) {
  return internal::StringSplitter(full, std::move(delim).delimiters,
                                  /*skip_empty=*/true);
}

internal::StringSplitter StrSplit(std::string_view full, char delim,
                                  SkipEmpty) {
  return StrSplit(full, ByAnyChar(std::string_view(&delim, 1)), SkipEmpty());
}

namespace {
bool IsAsciiSpace(unsigned char ch) { return std::isspace(ch); }
}  // namespace

std::string_view StripTrailingAsciiWhitespace(std::string_view full) {
  auto it = std::find_if_not(full.rbegin(), full.rend(), IsAsciiSpace);
  return full.substr(0, full.rend() - it);
}

void StripTrailingAsciiWhitespace(std::string *full) {
  auto it = std::find_if_not(full->rbegin(), full->rend(), IsAsciiSpace);
  full->erase(full->rend() - it);
}

}  // namespace fst
