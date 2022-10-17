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
  auto ok() const -> bool { return status.ok(); }
  auto error() -> std::string const& { return status.error(); }
  auto path() -> std::string const& { return status.path(); }

  auto get() const -> T const& { return _val.value(); }
  auto get() -> T& { return _val.value(); }

  T* operator->() { return &get(); }

  T const* operator->() const { return &get(); }

  T& operator*() & { return get(); }

  T const& operator*() const& { return get(); }

  T&& operator*() && { return std::move(get()); }

  T const&& operator*() const&& { return get(); }

  template<typename U = T,
           typename = std::enable_if_t<!std::is_same<U, bool>::value>>
  explicit operator bool() const {
    return ok();
  }

  StatusT(StatusT const& other) = delete;
  StatusT(StatusT&& other) = default;

  StatusT(Status const& status) = delete;
  StatusT(Status&& status) : status(std::move(status)), _val(std::nullopt) {}

  StatusT(T const& val) : status(), _val(val) {}
  StatusT(T&& val) : status(), _val(std::move(val)) {}

  StatusT() : status(), _val{T{}} {}

  StatusT& operator=(T const& val_) {
    _val = val_;
    return *this;
  }
  StatusT& operator=(T&& val_) {
    _val = std::move(val_);
    return *this;
  }

 private:
  Status status;
  std::optional<T> _val;
};

}  // namespace arangodb::inspection
