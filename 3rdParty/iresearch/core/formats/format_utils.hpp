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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_FORMATS_UTILS_H
#define IRESEARCH_FORMATS_UTILS_H

#include "store/store_utils.hpp"
#include "utils/string_utils.hpp"
#include "index/field_meta.hpp"

NS_ROOT

void IRESEARCH_API validate_footer(iresearch::index_input& in);

template<typename Type, typename Reader, typename Visitor>
size_t read_all(const Visitor& visitor, Reader& reader, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    Type value;

    reader.read(value);

    if (!visitor(value)) {
      return count - i + 1; // +1 for current iteration
    }
  }

  return count;
}

template<typename Iterator, typename Writer>
Iterator write_all(Writer& writer, Iterator begin, Iterator end) {
  for(; begin != end; ++begin) {
    writer.write(*begin);
  }

  return begin;
}

NS_BEGIN(format_utils)

const int32_t FORMAT_MAGIC = 0x3fd76c17;

const int32_t FOOTER_MAGIC = -FORMAT_MAGIC;

const uint32_t FOOTER_LEN = 2 * sizeof( int32_t ) + sizeof( int64_t );

void IRESEARCH_API write_header(index_output& out, const string_ref& format, int32_t ver);

void IRESEARCH_API write_footer(index_output& out);

int32_t IRESEARCH_API check_header(
  index_input& in, const string_ref& format, int32_t min_ver, int32_t max_ver
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

inline int64_t checksum(const index_input& in) {
  auto* stream = &in;

  index_input::ptr dup;
  if (0 != in.file_pointer()) {
    dup = in.dup();

    if (!dup) {
      IR_FRMT_ERROR("Failed to duplicate input in: %s", __FUNCTION__);

      throw io_error("failed to duplicate input");
    }

    dup->seek(0);
    stream = dup.get();
  }

  assert(0 == stream->file_pointer());
  return stream->checksum(stream->length() - sizeof(uint64_t));
}

NS_END
NS_END

#endif
