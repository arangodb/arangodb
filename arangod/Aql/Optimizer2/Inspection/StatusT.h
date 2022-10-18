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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <type_traits>

#include <optional>

#include "Inspection/Status.h"

namespace arangodb::inspection {

template<typename T>
struct StatusT {
  using Contained = std::variant<Status, T>;

  auto static error(Status&& status) -> StatusT<T> {
    return StatusT(
        std::move(Contained(std::in_place_index<0>, std::move(status))));
  }
  auto static ok(T const& val) -> StatusT<T> {
    return StatusT(std::move(Contained(std::in_place_index<1>, val)));
  }
  auto static ok(T&& val) -> StatusT<T> {
    return StatusT(
        std::move(Contained(std::in_place_index<1>, std::move(val))));
  }

  StatusT(StatusT const& other) = delete;
  StatusT(StatusT&& other) = default;

  // T is default constructible, otherwise inspection wouldn't work
  StatusT() requires(!std::is_nothrow_default_constructible_v<T>)
      : _contained{T{}} {}

  StatusT& operator=(T const& val_) {
    _contained = val_;
    return *this;
  }
  StatusT& operator=(T&& val_) {
    _contained = std::move(val_);
    return *this;
  }

  auto ok() const -> bool { return std::holds_alternative<T>(_contained); }
  auto error() -> std::string const& { return std::get<0>(_contained).error(); }
  auto path() -> std::string const& { return std::get<0>(_contained).path(); }

  auto get() const -> T const& { return std::get<1>(_contained); }
  auto get() -> T& { return std::get<T>(_contained); }

  T* operator->() { return &get(); }

  T const* operator->() const { return &get(); }

  T& operator*() & { return get(); }

  T const& operator*() const& { return get(); }

  T&& operator*() && { return std::move(get()); }

  T const&& operator*() const&& { return get(); }

  explicit operator bool() const requires(!std::is_same_v<T, bool>) {
    return ok();
  }

 private : StatusT(std::variant<Status, T>&& val)
     : _contained(std::move(val)) {}
  std::variant<Status, T> _contained;
};

}  // namespace arangodb::inspection
