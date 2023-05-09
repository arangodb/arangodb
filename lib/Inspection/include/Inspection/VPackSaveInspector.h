////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include <memory>
#include <optional>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <variant>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/Value.h>

#include "Basics/overload.h"
#include "Inspection/SaveInspectorBase.h"

namespace arangodb::inspection {

template<class Context = NoContext>
struct VPackSaveInspector
    : SaveInspectorBase<VPackSaveInspector<Context>, Context> {
 protected:
  using Base = SaveInspectorBase<VPackSaveInspector, Context>;

 public:
  explicit VPackSaveInspector(velocypack::Builder& builder) requires(
      !Base::hasContext)
      : _builder(builder) {}

  explicit VPackSaveInspector(velocypack::Builder& builder,
                              Context const& context)
      : Base(context), _builder(builder) {}

  [[nodiscard]] Status::Success beginObject() {
    _builder.openObject();
    return {};
  }

  [[nodiscard]] Status::Success endObject() {
    _builder.close();
    return {};
  }

  [[nodiscard]] Status::Success beginField(std::string_view name) {
    _builder.add(VPackValue(name));
    return {};
  }

  [[nodiscard]] Status::Success endField() { return {}; }

  [[nodiscard]] Status::Success value(Null v) {
    _builder.add(VPackValue(velocypack::ValueType::Null));
    return {};
  }

  [[nodiscard]] Status::Success value(velocypack::Slice s) {
    _builder.add(s);
    return {};
  }

  [[nodiscard]] Status::Success value(velocypack::SharedSlice s) {
    _builder.add(s.slice());
    return {};
  }

  [[nodiscard]] Status::Success value(velocypack::HashedStringRef const& s) {
    _builder.add(VPackValue(s.stringView()));
    return {};
  }

  template<class T, class = std::enable_if_t<detail::IsBuiltinType<T>()>>
  [[nodiscard]] Status::Success value(T const& v) {
    static_assert(detail::IsBuiltinType<T>());
    _builder.add(VPackValue(v));
    return {};
  }

  [[nodiscard]] Status::Success beginArray() {
    _builder.openArray();
    return {};
  }

  [[nodiscard]] Status::Success endArray() {
    _builder.close();
    return {};
  }

  velocypack::Builder& builder() noexcept { return _builder; }

  template<class U>
  struct FallbackContainer {
    explicit FallbackContainer(U&&) {}
  };
  template<class Fn>
  struct FallbackFactoryContainer {
    explicit FallbackFactoryContainer(Fn&&) {}
  };
  template<class Fn>
  struct InvariantContainer {
    explicit InvariantContainer(Fn&&) {}
  };

 private:
  template<class, class>
  friend struct SaveInspectorBase;

  template<std::size_t Idx, std::size_t End, class T>
  [[nodiscard]] auto processTuple(T const& data) {
    if constexpr (Idx < End) {
      return process(*this, std::get<Idx>(data))                    //
             | [&]() { return processTuple<Idx + 1, End>(data); };  //
    } else {
      return Status::Success{};
    }
  }

  template<class Iterator>
  [[nodiscard]] auto processList(Iterator begin, Iterator end)
      -> decltype(process(std::declval<VPackSaveInspector&>(), *begin)) {
    for (auto it = begin; it != end; ++it) {
      if (auto res = process(*this, *it); !res.ok()) {
        return res;
      }
    }
    return {};
  }
  template<class T>
  [[nodiscard]] auto processMap(T const& map)
      -> decltype(process(std::declval<VPackSaveInspector&>(),
                          map.begin()->second)) {
    for (auto&& [k, v] : map) {
      _builder.add(VPackValue(k));
      if (auto res = process(*this, v); !res.ok()) {
        return res;
      }
    }
    return {};
  }

  velocypack::Builder& _builder;
};

}  // namespace arangodb::inspection
