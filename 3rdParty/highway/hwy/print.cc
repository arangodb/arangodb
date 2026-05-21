// Copyright 2022 Google LLC
// SPDX-License-Identifier: Apache-2.0
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

#include "hwy/print.h"

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS  // before inttypes.h
#endif
#include <inttypes.h>  // IWYU pragma: keep
#include <stdio.h>

#include "hwy/base.h"

namespace hwy {
namespace detail {

HWY_DLLEXPORT void TypeName(const TypeInfo& info, size_t N, char* string100) {
  const char prefix = info.is_float ? 'f' : (info.is_signed ? 'i' : 'u');
  // Omit the xN suffix for scalars.
  if (N == 1) {
    // NOLINTNEXTLINE
    snprintf(string100, 64, "%c%d", prefix,
             static_cast<int>(info.sizeof_t * 8));
  } else {
    // NOLINTNEXTLINE
    snprintf(string100, 64, "%c%dx%d", prefix,
             static_cast<int>(info.sizeof_t * 8), static_cast<int>(N));
  }
}

HWY_DLLEXPORT void ToString(const TypeInfo& info, const void* ptr,
                            char* string100) {
  if (info.sizeof_t == 1) {
    uint8_t byte;
    CopyBytes<1>(ptr, &byte);  // endian-safe: we ensured sizeof(T)=1.
    snprintf(string100, 100, "0x%02X", byte);  // NOLINT
  } else if (info.sizeof_t == 2) {
    uint16_t bits;
    CopyBytes<2>(ptr, &bits);
    snprintf(string100, 100, "0x%04X", bits);  // NOLINT
  } else if (info.sizeof_t == 4) {
    if (info.is_float) {
      float value;
      CopyBytes<4>(ptr, &value);
      snprintf(string100, 100, "%g", static_cast<double>(value));  // NOLINT
    } else if (info.is_signed) {
      int32_t value;
      CopyBytes<4>(ptr, &value);
      snprintf(string100, 100, "%d", value);  // NOLINT
    } else {
      uint32_t value;
      CopyBytes<4>(ptr, &value);
      snprintf(string100, 100, "%u", value);  // NOLINT
    }
  } else {
    HWY_ASSERT(info.sizeof_t == 8);
    if (info.is_float) {
      double value;
      CopyBytes<8>(ptr, &value);
      snprintf(string100, 100, "%g", value);  // NOLINT
    } else if (info.is_signed) {
      int64_t value;
      CopyBytes<8>(ptr, &value);
      snprintf(string100, 100, "%" PRIi64 "", value);  // NOLINT
    } else {
      uint64_t value;
      CopyBytes<8>(ptr, &value);
      snprintf(string100, 100, "%" PRIu64 "", value);  // NOLINT
    }
  }
}

HWY_DLLEXPORT void PrintArray(const TypeInfo& info, const char* caption,
                              const void* array_void, size_t N, size_t lane_u,
                              size_t max_lanes) {
  const uint8_t* array_bytes = reinterpret_cast<const uint8_t*>(array_void);

  char type_name[100];
  TypeName(info, N, type_name);

  const intptr_t lane = intptr_t(lane_u);
  const size_t begin = static_cast<size_t>(HWY_MAX(0, lane - 2));
  const size_t end = HWY_MIN(begin + max_lanes, N);
  fprintf(stderr, "%s %s [%" PRIu64 "+ ->]:\n  ", type_name, caption,
          static_cast<uint64_t>(begin));
  for (size_t i = begin; i < end; ++i) {
    const void* ptr = array_bytes + i * info.sizeof_t;
    char str[100];
    ToString(info, ptr, str);
    fprintf(stderr, "%s,", str);
  }
  if (begin >= end) fprintf(stderr, "(out of bounds)");
  fprintf(stderr, "\n");
}

}  // namespace detail
}  // namespace hwy
