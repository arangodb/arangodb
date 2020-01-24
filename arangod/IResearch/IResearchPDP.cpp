////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_IRESEARCH__IRESEARCH_PDP_H
#define ARANGOD_IRESEARCH__IRESEARCH_PDP_H 1

#include "IResearchPDP.h"

#include "lz4.h"

#include "store/store_utils.hpp"
#include "utils/levenshtein_utils.hpp"
#include "utils/misc.hpp"
#include "utils/std.hpp"

#include "Basics/voc-errors.h"

namespace {

irs::parametric_description readParametricDescription(
    std::pair<std::initializer_list<irs::byte_type>, size_t> args) {
  auto const rawSize = args.second;
  const auto& data = args.first;

  if (data.size() >= LZ4_MAX_INPUT_SIZE
      || rawSize >= irs::integer_traits<int>::const_max) {
    return {};
  }

  irs::bstring dst(rawSize, 0);
  const auto lz4_size = ::LZ4_decompress_safe(
    reinterpret_cast<char const*>(data.begin()),
    reinterpret_cast<char*>(&dst[0]),
    static_cast<int>(data.size()),
    static_cast<int>(rawSize));

  if (lz4_size < 0 || static_cast<size_t>(lz4_size) != rawSize) {
    return {};
  }

  irs::bytes_ref_input in({dst.c_str(), rawSize});
  return irs::read(in);
}

irs::parametric_description const DESCRIPTIONS[] = {
  // distance 0
  irs::make_parametric_description(0, false),
  irs::make_parametric_description(0, true),

  // distance 1
  irs::make_parametric_description(1, false),
  irs::make_parametric_description(1, true),

  // distance 2
  irs::make_parametric_description(2, false),
  irs::make_parametric_description(2, true),

#if defined(_WIN32) && defined(_MSC_VER)
  // distance 3
  irs::make_parametric_description(3, false),
  irs::make_parametric_description(3, true),

  // distance 4
  irs::make_parametric_description(4, false),
#else
  // distance 3
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  readParametricDescription(
    #include "PD30"
  ),
  readParametricDescription(
    #include "PD31"
  ),
#else
  irs::make_parametric_description(3, false),
  irs::make_parametric_description(3, true),
#endif

  // distance 4
  readParametricDescription(
    #include "PD40"
  ),
#endif
};

size_t args2index(irs::byte_type distance,
                  bool with_transpositions) noexcept {
  return 2*size_t(distance) + size_t(with_transpositions);
}

}

namespace arangodb {
namespace iresearch {

const irs::parametric_description& getParametricDescription(
    irs::byte_type distance,
    bool with_transpositions) {
  const size_t idx = args2index(distance, with_transpositions);

  if (idx < IRESEARCH_COUNTOF(DESCRIPTIONS)) {
    return DESCRIPTIONS[idx];
  }

  static const irs::parametric_description INVALID;
  return INVALID;
}

} // iresearch
} // arangodb

#endif // ARANGOD_IRESEARCH__IRESEARCH_PDP_H
