//
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
//

#ifndef IRESEARCH_ABSL_STRINGS_INTERNAL_RESIZE_UNINITIALIZED_H_
#define IRESEARCH_ABSL_STRINGS_INTERNAL_RESIZE_UNINITIALIZED_H_

#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/port.h"
#include "absl/meta/type_traits.h"  //  for void_t

namespace iresearch_absl {
IRESEARCH_ABSL_NAMESPACE_BEGIN
namespace strings_internal {

// Is a subclass of true_type or false_type, depending on whether or not
// T has a __resize_default_init member.
template <typename string_type, typename = void>
struct ResizeUninitializedTraits {
  using HasMember = std::false_type;
  static void Resize(string_type* s, size_t new_size) { s->resize(new_size); }
};

// __resize_default_init is provided by libc++ >= 8.0
template <typename string_type>
struct ResizeUninitializedTraits<
    string_type, iresearch_absl::void_t<decltype(std::declval<string_type&>()
                                           .__resize_default_init(237))> > {
  using HasMember = std::true_type;
  static void Resize(string_type* s, size_t new_size) {
    s->__resize_default_init(new_size);
  }
};

// Returns true if the std::string implementation supports a resize where
// the new characters added to the std::string are left untouched.
//
// (A better name might be "STLStringSupportsUninitializedResize", alluding to
// the previous function.)
template <typename string_type>
inline constexpr bool STLStringSupportsNontrashingResize(string_type*) {
  return ResizeUninitializedTraits<string_type>::HasMember::value;
}

// Like str->resize(new_size), except any new characters added to "*str" as a
// result of resizing may be left uninitialized, rather than being filled with
// '0' bytes. Typically used when code is then going to overwrite the backing
// store of the std::string with known data.
template <typename string_type, typename = void>
inline void STLStringResizeUninitialized(string_type* s, size_t new_size) {
  ResizeUninitializedTraits<string_type>::Resize(s, new_size);
}

}  // namespace strings_internal
IRESEARCH_ABSL_NAMESPACE_END
}  // namespace absl

#endif  // IRESEARCH_ABSL_STRINGS_INTERNAL_RESIZE_UNINITIALIZED_H_
