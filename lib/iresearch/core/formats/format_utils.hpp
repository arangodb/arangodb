////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "index/field_meta.hpp"
#include "store/data_input.hpp"
#include "store/data_output.hpp"

#include <absl/strings/str_cat.h>

namespace irs {

void validate_footer(index_input& in);

namespace format_utils {

constexpr int32_t kFormatMagic = 0x3fd76c17;
constexpr int32_t kFooterMagic = -kFormatMagic;
constexpr uint32_t kFooterLen = 2 * sizeof(int32_t) + sizeof(int64_t);

void write_header(index_output& out, std::string_view format, int32_t ver);

void write_footer(index_output& out);

size_t header_length(std::string_view format) noexcept;

int32_t check_header(index_input& in, std::string_view format, int32_t min_ver,
                     int32_t max_ver);

inline int64_t read_checksum(index_input& in) {
  in.seek(in.length() - kFooterLen);
  validate_footer(in);
  return in.read_long();
}

inline int64_t check_footer(index_input& in, int64_t checksum) {
  validate_footer(in);

  if (checksum != in.read_long()) {
    throw index_error{absl::StrCat(
      "while checking footer, error: invalid checksum '", checksum, "'")};
  }

  return checksum;
}

int64_t checksum(const index_input& in);

}  // namespace format_utils
}  // namespace irs
