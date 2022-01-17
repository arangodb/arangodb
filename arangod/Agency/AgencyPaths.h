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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Agency/PathComponent.h"
#include "Basics/debugging.h"
#include "Cluster/ClusterTypes.h"

#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

/**
 * @brief Build agency paths in a compile-time safe manner.
 *
 * Usage:
 *
 *   using namespace arangodb::cluster::paths;
 *
 *   std::string path =
 * root()->arango()->plan()->databases()->database("_system")->str();
 *   // path == "/arango/Plan/Databases/_system"
 *   ...
 *   std::vector<std::string> path =
 * root()->arango()->plan()->databases()->database("_system")->vec();
 *   // path == {"arango", "Plan", "Databases", "_system"}
 *   ...
 *   std::stringstream stream;
 *   stream << *root()->arango()->plan()->databases()->database("_system");
 *   // stream.str() == "/arango/Plan/Databases/_system"
 *   ...
 *   std::stringstream stream;
 *   root()->arango()->plan()->databases()->database("_system")->toStream(stream);
 *   // stream.str() == "/arango/Plan/Databases/_system"
 *
 * Or use shorthands:
 *
 *   using namespace arangodb::cluster::paths::aliases;
 *
 *   arango()->initCollectionsDone();
 *   plan()->databases();
 *   current()->serversKnown();
 *   target()->pending();
 *   supervision()->health();
 *
 * @details
 * Note that no class here may be instantiated directly! You can only call
 * root() and work your way down from there.
 *
 * If you add anything, make sure to add tests in
 * tests/Cluster/AgencyPathsTest.cpp.
 *
 * An example for a static component looks like this:
 *   class SomeOuterClass {
 *   ...
 *     // Add your component
 *     class YourComponent : public StaticComponent<YourComponent,
 * SomeOuterClass> { public: constexpr char const* component() const noexcept {
 * return "YourComponent"; }
 *
 *       // Inherit constructors
 *       using BaseType::StaticComponent;
 *
 *       // Add possible inner classes here
 *     };
 *
 *     // Add an accessor to it in the outer class
 *     std::shared_ptr<YourComponent const> yourComponent() const {
 *       return YourComponent::make_shared(shared_from_this());
 *     }
 *   ...
 *   }
 *
 * An example for a dynamic component looks like this, here holding a value of
 * type SomeType: class SomeOuterClass {
 *   ...
 *     // Add your component
 *     class YourComponent : public DynamicComponent<YourComponent,
 * SomeOuterClass, SomeType> { public:
 *       // Access your SomeType value with value():
 *       char const* component() const noexcept { return value().c_str(); }
 *
 *       // Inherit constructors
 *       using BaseType::DynamicComponent;
 *     };
 *
 *     // Add an accessor to it in the outer class
 *     std::shared_ptr<YourComponent const> yourComponent(DatabaseID name) const
 * { return YourComponent::make_shared(shared_from_this(), std::move(name));
 *     }
 *   ...
 *   }
 *
 */

namespace arangodb::replication2 {
class LogId;
}

namespace arangodb::cluster::paths {

class Root;

auto root() -> std::shared_ptr<Root const>;

// The root is no StaticComponent, mainly because it has no parent and is the
// base case for recursions.
class Root : public std::enable_shared_from_this<Root>, public Path {
 public:
  void forEach(
      std::function<void(char const* component)> const&) const override final {}

 public:
#include "../test.h"
 private:
  // May only be constructed by root()
  friend auto root() -> std::shared_ptr<Root const>;
  Root() = default;
  static auto make_shared() -> std::shared_ptr<Root const> {
    struct ConstructibleRoot : public Root {
     public:
      explicit ConstructibleRoot() noexcept = default;
    };
    return std::make_shared<ConstructibleRoot const>();
  }
};

auto to_string(Path const& path) -> std::string;

namespace aliases {

auto arango() -> std::shared_ptr<Root::Arango const>;
auto plan() -> std::shared_ptr<Root::Arango::Plan const>;
auto current() -> std::shared_ptr<Root::Arango::Current const>;
auto target() -> std::shared_ptr<Root::Arango::Target const>;
auto supervision() -> std::shared_ptr<Root::Arango::Supervision const>;

}  // namespace aliases

}  // namespace arangodb::cluster::paths
