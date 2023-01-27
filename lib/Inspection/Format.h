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

#include <fmt/core.h>
#include <fmt/format.h>

#include "Inspection/VPackSaveInspector.h"
#include "Inspection/VPackWithErrorT.h"
#include "Inspection/detail/traits.h"

template<>
struct fmt::formatter<VPackSlice> {
  void set_debug_format() = delete;

  enum class Presentation { NotPretty, Pretty };
  // Presentation format: 'u' - use toJson, 'p' - use toString.
  Presentation presentation = Presentation::NotPretty;

  constexpr auto parse(fmt::format_parse_context& ctx)
      -> decltype(ctx.begin()) {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end) {
      if (*it == 'u') {
        presentation = Presentation::NotPretty;
        it++;
      } else if (*it == 'p') {
        presentation = Presentation::Pretty;
        it++;
      }
    }
    if (it != end && *it != '}') throw fmt::format_error("invalid format");
    return it;
  }

  template<typename FormatContext>
  auto format(VPackSlice const& slice, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    arangodb::velocypack::Options options =
        arangodb::velocypack::Options::Defaults;
    options.dumpAttributesInIndexOrder = false;
    switch (presentation) {
      case Presentation::Pretty:
        return fmt::format_to(ctx.out(), "{}", slice.toString(&options));
      case Presentation::NotPretty:
      default:
        return fmt::format_to(ctx.out(), "{}", slice.toJson(&options));
    }
  }
};

namespace arangodb::inspection {
// Formats an object of type T that has an overloaded inspector.
struct inspection_formatter : fmt::formatter<VPackSlice> {
  template<typename T, typename FormatContext,
           typename Inspector = VPackSaveInspector<NoContext>>
  requires detail::HasInspectOverload<T, Inspector>::value auto format(
      const T& value, FormatContext& ctx) const -> decltype(ctx.out()) {
    auto sharedSlice = inspection::serializeWithErrorT(value);
    if (not sharedSlice.ok()) {
      VPackBuilder error;
      {
        VPackObjectBuilder ob(&error);
        error.add("error", VPackValue(sharedSlice.error().error()));
      }
      return fmt::formatter<VPackSlice>::format(error.slice(), ctx);
    }
    return fmt::formatter<VPackSlice>::format(sharedSlice.get().slice(), ctx);
  }
};
}  // namespace arangodb::inspection
