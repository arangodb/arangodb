////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Alexey Bakharew
////////////////////////////////////////////////////////////////////////////////

#ifndef ICU_LOCALE_UTILS_HPP
#define ICU_LOCALE_UTILS_HPP

#include <unicode/locid.h>

#include "string.hpp"
#include "velocypack/Slice.h"
#include "velocypack/Builder.h"
#include "velocypack/velocypack-aliases.h"

namespace iresearch {
namespace icu_locale_utils {

enum class Unicode { UTF8, NON_UTF8 };

bool get_locale_from_vpack(const VPackSlice slice,
                           icu::Locale& locale,
                           Unicode* unicode = nullptr);

bool get_locale_from_str(irs::string_ref locale_str,
                         icu::Locale& locale,
                         bool is_new_format,
                         Unicode* unicode = nullptr,
                         std::string* encoding = nullptr);

bool locale_to_vpack(const icu::Locale& locale,
                     VPackBuilder* const builder,
                     const Unicode* unicode = nullptr);

} // icu_locale_utils
} // iresearch


#endif // ICU_LOCALE_UTILS_HPP
