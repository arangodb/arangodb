////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace arangodb {
namespace cluster {
namespace paths {

class Path {
 public:
  virtual std::ostream& pathToStream(std::ostream& stream) const = 0;

  std::vector<std::string> pathVec() const { return _pathVec(0); }

  virtual std::vector<std::string> _pathVec(size_t size) const = 0;

  virtual std::string pathStr() const = 0;

  virtual ~Path() = default;
};

template <class T, class P>
class PathComponent : public std::enable_shared_from_this<T> /* (sic) */, public Path {
 public:
  using ParentType = P;
  using BaseType = PathComponent<T, P>;

  PathComponent() = delete;

  std::ostream& pathToStream(std::ostream& stream) const override {
    return parent().pathToStream(stream) << "/" << child().component();
  }

  std::vector<std::string> _pathVec(size_t size) const override {
    auto path = parent()._pathVec(size + 1);
    path.emplace_back(child().component());
    return path;
  }

  std::string pathStr() const override {
    auto stream = std::stringstream{};
    pathToStream(stream);
    return stream.str();
  }

  // Only the parent type P may instantiate a component, so make this protected
  // and P a friend. MSVC ignores the friend declaration, though.
#if defined(_WIN32) || defined(_WIN64)
 public:
#else
 protected:
  friend P;
#endif
  // constexpr PathComponent(P const& parent) noexcept : _parent(parent) {}
  explicit constexpr PathComponent(std::shared_ptr<P const> parent) noexcept : _parent(std::move(parent)) {}

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

  std::shared_ptr<P const> _parent;
};

template <class T, class P>
std::ostream& operator<<(std::ostream& stream, PathComponent<T, P> const& pc) {
  return pc.pathToStream(stream);
}

}  // namespace paths
}  // namespace cluster
}  // namespace arangodb

#endif  // ARANGOD_CLUSTER_PATHCOMPONENT_H
