////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>

namespace arangodb::inspection {

// If your type T is not default constructible, you can specialize `Factory` for
// your type and implement make_value().
// If T has a copy or move constructor, you can derive from BaseFactory (CRTP)
// to get default make_unique() and make_shared() implementations.
// If it hasn't, implement them too.
template<typename T>
struct Factory {
  static auto make_value() -> T {
    // If you get a compile error here because your T is not default
    // constructible, see comment above.
    return T{};
  }
  static auto make_unique() -> std::unique_ptr<T> {
    // If you get a compile error here because your T is not default
    // constructible, see comment above.
    return std::make_unique<T>();
  }
  static auto make_shared() -> std::shared_ptr<T> {
    // If you get a compile error here because your T is not default
    // constructible, see comment above.
    return std::make_shared<T>();
  }
};

template<typename T, typename Derived = Factory<T>>
struct BaseFactory {
  static auto make_unique() -> std::unique_ptr<T> {
    // If you get a compile error here because T has deleted copy and move
    // constructors, see comment above Factory.
    return std::make_unique<T>(Derived::make_value());
  }
  static auto make_shared() -> std::shared_ptr<T> {
    // If you get a compile error here because T has deleted copy and move
    // constructors, see comment above Factory.
    return std::make_shared<T>(Derived::make_value());
  }
};

}  // namespace arangodb::inspection
