// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This file contains functions that remove a defined part from the string,
// i.e., strip the string.

#include "s2/third_party/absl/strings/strip.h"

#include "s2/third_party/absl/strings/string_view.h"

// ----------------------------------------------------------------------
// ReplaceCharacters
//    Replaces any occurrence of any of the 'remove' *bytes*
//    with the 'replace_with' *byte*.
// ----------------------------------------------------------------------
void ReplaceCharacters(char* str, size_t len, absl::string_view remove,
                       char replace_with) {
  for (char* end = str + len; str != end; ++str) {
    if (remove.find(*str) != absl::string_view::npos) {
      *str = replace_with;
    }
  }
}

void ReplaceCharacters(string* s, absl::string_view remove, char replace_with) {
  for (char& ch : *s) {
    if (remove.find(ch) != absl::string_view::npos) {
      ch = replace_with;
    }
  }
}
