////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include <format>
#include <sstream>

#include "Inspection/JsonPrintInspector.h"
#include "Inspection/VPackSaveInspector.h"
#include "Inspection/VPackWithErrorT.h"
#include "Inspection/detail/traits.h"

template<>
struct std::formatter<VPackSlice> {
  void set_debug_format() = delete;

  enum class Presentation { NotPretty, Pretty };
  // Presentation format: 'u' - use toJson, 'p' - use toString.
  Presentation presentation = Presentation::NotPretty;

  constexpr auto parse(std::format_parse_context& ctx)
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
    if (it != end && *it != '}') throw std::format_error("invalid format");
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
        return std::format_to(ctx.out(), "{}", slice.toString(&options));
      case Presentation::NotPretty:
      default:
        return std::format_to(ctx.out(), "{}", slice.toJson(&options));
    }
  }
};

namespace arangodb::inspection {
// Formats an object of type T that has an overloaded inspector.
struct inspection_formatter : std::formatter<VPackSlice> {
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
      return std::formatter<VPackSlice>::format(error.slice(), ctx);
    }
    return std::formatter<VPackSlice>::format(sharedSlice.get().slice(), ctx);
  }
};

template<class T>
struct JsonPrintable {
  T const& value;
  JsonPrintFormat format;
  bool quoteFieldNames;
};

template<class T>
auto json(T const& value, JsonPrintFormat format = JsonPrintFormat::kCompact,
          bool quoteFieldNames = true) {
  static_assert(detail::IsInspectable<T, JsonPrintInspector<>>());
  return JsonPrintable<T>{
      .value = value, .format = format, .quoteFieldNames = quoteFieldNames};
}

}  // namespace arangodb::inspection

template<class T, class Char>
struct std::formatter<arangodb::inspection::JsonPrintable<T>, Char>
    : std::formatter<std::basic_string_view<Char>, Char> {
  void set_debug_format() = delete;

  constexpr auto parse(std::format_parse_context& ctx)
      -> decltype(ctx.begin()) {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end) {
      if (*it == 'm') {
        _format = arangodb::inspection::JsonPrintFormat::kMinimal;
        it++;
      } else if (*it == 'c') {
        _format = arangodb::inspection::JsonPrintFormat::kCompact;
        it++;
      } else if (*it == 'p') {
        _format = arangodb::inspection::JsonPrintFormat::kPretty;
        it++;
      }
    }
    if (it != end && *it == 'u') {
      _quoteFieldNames = false;
      ++it;
    }

    if (it != end && *it != '}') throw std::format_error("invalid format");
    return it;
  }

  template<typename OutputIt>
  auto format(arangodb::inspection::JsonPrintable<T> const& v,
              std::basic_format_context<OutputIt, Char>& ctx) const
      -> decltype(ctx.out()) {
    auto format = _format.value_or(v.format);

    // Use stringstream to capture the JSON output
    std::basic_ostringstream<Char> oss;
    arangodb::inspection::JsonPrintInspector<> insp(
        oss, format, _quoteFieldNames && v.quoteFieldNames);
    auto res = insp.apply(v.value);
    TRI_ASSERT(res.ok());  // TODO - print error if failed?

    auto str = oss.str();
    return std::formatter<std::basic_string_view<Char>, Char>::format(
        std::basic_string_view<Char>(str.data(), str.size()), ctx);
  }

 private:
  // format: 'm' - Minimal, 'c' - Compact, 'p' - Pretty.
  std::optional<arangodb::inspection::JsonPrintFormat> _format;
  bool _quoteFieldNames = true;
};

namespace std {
template<class T>
ostream& operator<<(ostream& stream,
                    arangodb::inspection::JsonPrintable<T> const& v) {
  arangodb::inspection::JsonPrintInspector<> inspector(stream, v.format);
  auto result = inspector.apply(v.value);
  assert(result.ok());  // TODO - print error if failed?
  return stream;
}
}  // namespace std
