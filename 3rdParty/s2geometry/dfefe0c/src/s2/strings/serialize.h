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

#ifndef S2_STRINGS_SERIALIZE_H_
#define S2_STRINGS_SERIALIZE_H_

#include <string>
#include <vector>

#include "s2/third_party/absl/strings/string_view.h"

namespace strings {

// -------------------------------------------------------------------------
// DictionaryParse
//   This routine parses a common dictionary format (key and value separated
//   by ':', entries separated by commas). This format is used for many
//   complex commandline flags. It is also used to encode dictionaries for
//   exporting them or writing them to a checkpoint. Returns a vector of
//   <key, value> pairs. Returns true if there if no error in parsing, false
//   otherwise.
// -------------------------------------------------------------------------
bool DictionaryParse(absl::string_view encoded_str,
                     std::vector<std::pair<std::string, std::string>>* items);

}  // namespace strings

#endif  // S2_STRINGS_SERIALIZE_H_
