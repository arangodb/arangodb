// Copyright 2018 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "s2/strings/serialize.h"

#include <string>
#include <vector>

#include "s2/third_party/absl/strings/str_split.h"
#include "s2/third_party/absl/strings/string_view.h"

using absl::StrSplit;
using absl::string_view;
using std::pair;
using std::string;
using std::vector;

namespace strings {

bool DictionaryParse(string_view encoded_str,
                     vector<pair<string, string>>* items) {
  if (encoded_str.empty())
    return true;
  vector<string_view> const entries = StrSplit(encoded_str, ',');
  for (int i = 0; i < entries.size(); ++i) {
    vector<string_view> const fields = StrSplit(entries[i], ':');
    if (fields.size() != 2)  // parsing error
      return false;
    items->push_back(std::make_pair(string(fields[0]), string(fields[1])));
  }
  return true;
}

}  // namespace strings
