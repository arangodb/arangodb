////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <Inspection/VPack.h>

#include <velocypack/vpack.h>

#include <fmt/core.h>

template<typename T>
concept HasInspectOverload = arangodb::inspection::detail::HasInspectOverload<
    T, arangodb::inspection::VPackSaveInspector>();

template<typename T>
requires HasInspectOverload<T>
struct fmt::formatter<T> {
  // Presentation format: 'u' - use toJson, 'f' - use toString.
  char presentation = 'u';

  constexpr auto parse(fmt::format_parse_context& ctx)
      -> decltype(ctx.begin()) {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end && (*it == 'u' || *it == 'f')) presentation = *it++;
    if (it != end && *it != '}') throw fmt::format_error("invalid format");
    return it;
  }

  template<typename FormatContext>
  auto format(const T& v, FormatContext& ctx) const -> decltype(ctx.out()) {
    VPackBuilder b;
    serialize(b, v);
    return presentation == 'u'
               ? fmt::format_to(ctx.out(), "{}", b.toJson())
               : fmt::format_to(ctx.out(), "{}\n", b.toString());
  }
};

template<>
struct fmt::formatter<VPackBuilder> {
  // Presentation format: 'u' - use toJson, 'f' - use toString.
  char presentation = 'u';

  constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end && (*it == 'u' || *it == 'f')) presentation = *it++;
    if (it != end && *it != '}') throw format_error("invalid format");
    return it;
  }

  template<typename FormatContext>
  auto format(const VPackBuilder& b, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    return presentation == 'u'
               ? fmt::format_to(ctx.out(), "{}", b.toJson())
               : fmt::format_to(ctx.out(), "{}\n", b.toString());
  }
};
