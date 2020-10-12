////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_PATHCOMPONENT_H
#define ARANGOD_CLUSTER_PATHCOMPONENT_H

#include <functional>
#include <iosfwd>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace arangodb {
namespace cluster {
namespace paths {

class Path {
 public:
  // Call for each component on the path, starting with the topmost component,
  // excluding Root.
  virtual void forEach(std::function<void(char const* component)> const&) const = 0;

  // Fold the path.
  template <class T>
  T fold(std::function<T(const char*, T)> const& callback, T init) const {
    forEach([&callback, &init](const char* component) { init = callback(init); });
    return std::move(init);
  }

  std::ostream& toStream(std::ostream& stream) const {
    forEach([&stream](const char* component) { stream << "/" << component; });
    return stream;
  }

  std::vector<std::string> vec(size_t skip = 0) const {
    std::vector<std::string> res;
    forEach([&res, &skip](const char* component) {
      if (skip == 0) {
        res.emplace_back(std::string{component});
      } else {
        skip--;
      }
    });
    return res;
  }

  std::string str() const {
    auto stream = std::stringstream{};
    toStream(stream);
    return stream.str();
  }

  virtual ~Path() = default;
};

template <class T, class P>
class StaticComponent : public std::enable_shared_from_this<T> /* (sic) */, public Path {
 public:
  using ParentType = P;
  using BaseType = StaticComponent<T, P>;

  StaticComponent() = delete;

  void forEach(std::function<void(char const* component)> const& callback) const final {
    parent().forEach(callback);
    callback(child().component());
  }

  // Only the parent type P may instantiate a component, so make this protected
  // and P a friend. MSVC ignores the friend declaration, though.
#if defined(_WIN32) || defined(_WIN64)
 public:
#else
 protected:
  friend P;
#endif
  explicit constexpr StaticComponent(std::shared_ptr<P const> parent) noexcept
      : _parent(std::move(parent)) {}

  // shared ptr constructor
  static std::shared_ptr<T const> make_shared(std::shared_ptr<P const> parent) {
    struct ConstructibleT : public T {
     public:
      explicit ConstructibleT(std::shared_ptr<P const> parent) noexcept
          : T(std::move(parent)) {}
    };
    return std::make_shared<ConstructibleT const>(std::move(parent));
  }

 private:
  // Accessor to our subclass
  T const& child() const { return static_cast<T const&>(*this); }

  // Accessor to our parent. Could be made public, but should then probably return the shared_ptr.
  P const& parent() const noexcept { return *_parent; }

  std::shared_ptr<P const> const _parent;
};

template <class T, class P, class V>
class DynamicComponent : public std::enable_shared_from_this<T> /* (sic) */, public Path {
 public:
  using ParentType = P;
  using BaseType = DynamicComponent<T, P, V>;

  DynamicComponent() = delete;

  void forEach(std::function<void(char const* component)> const& callback) const final {
    parent().forEach(callback);
    callback(child().component());
  }

  // Only the parent type P may instantiate a component, so make this protected
  // and P a friend. MSVC ignores the friend declaration, though.
#if defined(_WIN32) || defined(_WIN64)
 public:
#else
 protected:
  friend P;
#endif
  explicit constexpr DynamicComponent(std::shared_ptr<P const> parent, V value) noexcept
      : _parent(std::move(parent)), _value(std::move(value)) {
    // cppcheck-suppress *
    static_assert(noexcept(V(std::move(value))),
                  "Move constructor of V is expected to be noexcept");
  }

  // shared ptr constructor
  static std::shared_ptr<T const> make_shared(std::shared_ptr<P const> parent, V value) {
    struct ConstructibleT : public T {
     public:
      explicit ConstructibleT(std::shared_ptr<P const> parent, V value) noexcept
          : T(std::move(parent), std::move(value)) {}
    };
    return std::make_shared<ConstructibleT const>(std::move(parent), std::move(value));
  }

  V const& value() const noexcept { return _value; }

 private:
  // Accessor to our subclass
  T const& child() const { return static_cast<T const&>(*this); }

  // Accessor to our parent. Could be made public, but should then probably return the shared_ptr.
  P const& parent() const noexcept { return *_parent; }

  std::shared_ptr<P const> const _parent;

  V const _value;
};

std::ostream& operator<<(std::ostream& stream, Path const& path);

}  // namespace paths
}  // namespace cluster
}  // namespace arangodb

#endif  // ARANGOD_CLUSTER_PATHCOMPONENT_H
