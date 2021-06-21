////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "result.hpp"
#include "memory.hpp"

namespace iresearch {

/*static*/ std::unique_ptr<char[]> result::copyState(const char* src) {
  const size_t len = std::strlen(src);
  auto copy = memory::make_unique<char[]>(len + 1); // +1 for null terminator
  memcpy(copy.get(), src, len);
  copy[len] = 0; // null terminator
  return copy;
}

result::result(Code code)
  : code_(code) {
  // FIXME use static error message
}

result::result(Code code, const string_ref& msg1, const string_ref& msg2)
  : code_(code) {
  assert(code_ != OK);
  const size_t len1 = msg1.size();
  const size_t len2 = msg2.size();
  const size_t size = len1 + (len2 ? (len2 + 2) : 0);
  auto state = memory::make_unique<char[]>(size + 1); // +1 for null terminator
  memcpy(state.get(), msg1.c_str(), len1);
  if (len2) {
    state[len1] = ':';
    state[len1 + 1] = ' ';
    memcpy(state.get() + len1 + 2, msg2.c_str(), len2);
  }
  state[size] = 0; // null terminator
  state_ = std::move(state);
}

}
