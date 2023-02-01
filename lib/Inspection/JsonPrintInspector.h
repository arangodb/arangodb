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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cassert>
#include <ostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include "Inspection/SaveInspectorBase.h"

namespace arangodb::inspection {

enum class JsonPrintFormat { kPretty, kCompact, kMinimal };

template<class Context = NoContext>
struct JsonPrintInspector
    : SaveInspectorBase<JsonPrintInspector<Context>, Context> {
 protected:
  using Base = SaveInspectorBase<JsonPrintInspector, Context>;
  static constexpr unsigned IndentationPerLevel = 2;

 public:
  JsonPrintInspector(std::ostream& stream, JsonPrintFormat format,
                     bool quoteFieldNames = true) requires(!Base::hasContext)
      : Base(),
        _stream(stream),
        _indentation(),
        _linebreak(format == JsonPrintFormat::kPretty    ? "\n"
                   : format == JsonPrintFormat::kCompact ? " "
                                                         : ""),
        _separator(format == JsonPrintFormat::kMinimal ? "" : " "),
        _format(format),
        _quoteFieldNames(quoteFieldNames) {}

  JsonPrintInspector(std::ostream& stream, JsonPrintFormat format,
                     bool quoteFieldNames, Context const& context)
      : Base(context),
        _stream(stream),
        _indentation(),
        _linebreak(format == JsonPrintFormat::kPretty    ? "\n"
                   : format == JsonPrintFormat::kCompact ? " "
                                                         : ""),
        _separator(format == JsonPrintFormat::kMinimal ? "" : " "),
        _format(format),
        _quoteFieldNames(quoteFieldNames) {}

  template<class T>
  [[nodiscard]] Status apply(T const& x) {
    return process(*this, x);
  }

  template<class T>
  [[nodiscard]] Status::Success value(T& v) {
    _stream << v;
    return {};
  }

  [[nodiscard]] Status::Success value(Null v) {
    _stream << "null";
    return {};
  }

  [[nodiscard]] Status::Success value(bool v) {
    _stream << (v ? "true" : "false");
    return {};
  }

  [[nodiscard]] Status::Success value(std::string& v) {
    _stream << '"' << v << '"';
    return {};
  }

  [[nodiscard]] Status::Success value(std::string_view v) {
    _stream << '"' << v << '"';
    return {};
  }

  [[nodiscard]] Status::Success value(velocypack::Slice s) {
    _stream << s.toJson();
    return {};
  }

  [[nodiscard]] Status::Success value(velocypack::SharedSlice s) {
    _stream << s.toJson();
    return {};
  }

  [[nodiscard]] Status::Success value(velocypack::HashedStringRef& s) {
    return value(s.stringView());
  }

  [[nodiscard]] Status::Success beginObject() {
    _stream << '{';
    incrementIndentationLevel();
    _firstField = true;
    return {};
  }

  [[nodiscard]] Status::Success endObject() {
    decrementIndentationLevel();
    _stream << _linebreak << _indentation << '}';
    return {};
  }

  [[nodiscard]] Status::Success beginField(std::string_view name) {
    if (_firstField) {
      _firstField = false;
    } else {
      _stream << ',';
    }
    _stream << _linebreak << _indentation << (_quoteFieldNames ? "\"" : "")
            << name << (_quoteFieldNames ? "\":" : ":") << _separator;
    return {};
  }

  [[nodiscard]] Status::Success endField() { return {}; }

  [[nodiscard]] Status::Success beginArray() {
    _stream << '[' << _linebreak;
    incrementIndentationLevel();
    return {};
  }

  [[nodiscard]] Status::Success endArray() {
    decrementIndentationLevel();
    _stream << _linebreak << _indentation << ']';
    return {};
  }

  template<class U>
  struct FallbackContainer {
    explicit FallbackContainer(U&&) {}
  };
  template<class Fn>
  struct FallbackFactoryContainer {
    explicit FallbackFactoryContainer(Fn&&) {}
  };

  template<class Invariant>
  struct InvariantContainer {
    explicit InvariantContainer(Invariant&&) {}
  };

 private:
  JsonPrintInspector(std::ostream& stream, std::string indentation,
                     Context const& context)
      : Base(context), _stream(stream), _indentation(std::move(indentation)) {}

  template<class>
  friend struct detail::EmbeddedFields;
  template<class, class...>
  friend struct detail::EmbeddedFieldsImpl;
  template<class, class, class>
  friend struct detail::EmbeddedFieldsWithObjectInvariant;
  template<class, class>
  friend struct detail::EmbeddedFieldInspector;

  template<class, class>
  friend struct SaveInspectorBase;

  using EmbeddedParam = std::monostate;

  template<std::size_t Idx, std::size_t End, class T>
  [[nodiscard]] auto processTuple(T const& data) {
    if constexpr (Idx < End) {
      _stream << _indentation;
      return process(*this, std::get<Idx>(data))  //
             | [&]() {
                 if constexpr (Idx + 1 < End) {
                   _stream << ',' << _linebreak;
                 }
                 return processTuple<Idx + 1, End>(data);
               };
    } else {
      return Status::Success{};
    }
  }

  template<class Iterator>
  [[nodiscard]] auto processList(Iterator begin, Iterator end)
      -> decltype(process(std::declval<JsonPrintInspector&>(), *begin)) {
    for (auto it = begin; it != end;) {
      _stream << _indentation;
      if (auto res = process(*this, *it); !res.ok()) {
        return res;
      }
      if (++it != end) {
        _stream << ',' << _linebreak;
      }
    }
    return {};
  }

  template<class T>
  [[nodiscard]] auto processMap(T const& map)
      -> decltype(process(std::declval<JsonPrintInspector&>(),
                          map.begin()->second)) {
    auto end = map.end();
    _stream << _linebreak;
    for (auto it = map.begin(); it != end;) {
      auto&& [k, v] = *it;
      _stream << _indentation << '"' << k << "\":" << _separator;
      if (auto res = process(*this, v); !res.ok()) {
        return res;
      }
      if (++it != end) {
        _stream << ',' << _linebreak;
      }
    }
    return {};
  }

  void incrementIndentationLevel() {
    if (_format == JsonPrintFormat::kPretty) {
      _indentation.append(IndentationPerLevel, ' ');
    }
  }

  void decrementIndentationLevel() {
    if (_format == JsonPrintFormat::kPretty) {
      assert(_indentation.size() >= IndentationPerLevel &&
             _indentation.size() % IndentationPerLevel == 0);
      _indentation.resize(_indentation.size() - IndentationPerLevel);
    }
  }

  std::ostream& _stream;
  std::string _indentation;
  std::string_view _linebreak;
  std::string_view _separator;
  JsonPrintFormat _format;
  bool _firstField = false;
  bool _quoteFieldNames = true;
};
}  // namespace arangodb::inspection
