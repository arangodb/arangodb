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
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <mutex>

namespace arangodb::metrics {

template<typename T>
class Guard {
 public:
  using Data = T;

  T load() const {
    std::unique_lock lock{_m};
    return _data;
  }

  void store(T&& data) {
    std::unique_lock lock{_m};
    _data = std::move(data);
  }
  void store(T const& data) {
    std::unique_lock lock{_m};
    _data = data;
  }

 private:
  mutable std::mutex _m;
  T _data;
};

}  // namespace arangodb::metrics
