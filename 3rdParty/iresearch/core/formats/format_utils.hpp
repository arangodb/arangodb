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

#ifndef IRESEARCH_FORMATS_UTILS_H
#define IRESEARCH_FORMATS_UTILS_H

#include "store/store_utils.hpp"
#include "utils/string_utils.hpp"
#include "index/field_meta.hpp"

namespace iresearch {

IRESEARCH_API void validate_footer(index_input& in);

namespace format_utils {

const int32_t FORMAT_MAGIC = 0x3fd76c17;

const int32_t FOOTER_MAGIC = -FORMAT_MAGIC;

const uint32_t FOOTER_LEN = 2 * sizeof(int32_t) + sizeof(int64_t);

IRESEARCH_API void write_header(index_output& out, const string_ref& format, int32_t ver);

IRESEARCH_API void write_footer(index_output& out);

IRESEARCH_API int32_t check_header(
  index_input& in,
  const string_ref& format,
  int32_t min_ver,
  int32_t max_ver
);

inline int64_t read_checksum(index_input& in) {
  in.seek(in.length() - FOOTER_LEN);
  validate_footer(in);
  return in.read_long();
}

inline int64_t check_footer(index_input& in, int64_t checksum) {
  validate_footer(in);

  if (checksum != in.read_long()) {
    throw index_error(string_utils::to_string(
      "while checking footer, error: invalid checksum '" IR_UINT64_T_SPECIFIER "'",
      checksum
    ));
  }

  return checksum;
}

IRESEARCH_API int64_t checksum(const index_input& in);

}
}

#endif
