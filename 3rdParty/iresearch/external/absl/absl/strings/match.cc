// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/strings/match.h"

#include "absl/strings/internal/memutil.h"

namespace iresearch_absl {
IRESEARCH_ABSL_NAMESPACE_BEGIN

bool EqualsIgnoreCase(iresearch_absl::string_view piece1, iresearch_absl::string_view piece2) {
  return (piece1.size() == piece2.size() &&
          0 == iresearch_absl::strings_internal::memcasecmp(piece1.data(), piece2.data(),
                                                  piece1.size()));
  // memcasecmp uses iresearch_absl::ascii_tolower().
}

bool StartsWithIgnoreCase(iresearch_absl::string_view text, iresearch_absl::string_view prefix) {
  return (text.size() >= prefix.size()) &&
         EqualsIgnoreCase(text.substr(0, prefix.size()), prefix);
}

bool EndsWithIgnoreCase(iresearch_absl::string_view text, iresearch_absl::string_view suffix) {
  return (text.size() >= suffix.size()) &&
         EqualsIgnoreCase(text.substr(text.size() - suffix.size()), suffix);
}

IRESEARCH_ABSL_NAMESPACE_END
}  // namespace absl
