////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2024 ArangoDB GmbH, Cologne, Germany
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

#include "ApplicationFeatures/ApplicationFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "RestServer/arangod.h"

#include <function2.hpp>

namespace arangodb {

// This is a workaround so we can work with explicit dependencies (in order to
// avoid passing around ArangodServer and using getFeature calls) in face of
// circular dependencies.
// We should try to remove circular deps as well whenever possible, but we can't
// solve all problems at once. :)
// We also might want to change how this works (internally, and from the
// construction-site) later: Instead of having a callback that holds a reference
// to an ArangodServer - which we want to get rid of - we can have the
// construction site hold some kind of reference to this, and put in the
// feature-pointer when it's available.
template<typename FeatureT>
// removed, so we can work with incomplete types
// requires std::derived_from<FeatureT,
// application_features::ApplicationFeature>
struct LazyApplicationFeatureReference {
  auto get() noexcept -> FeatureT&;

  using Factory = fu2::unique_function<FeatureT*() noexcept>;

  explicit LazyApplicationFeatureReference(Factory factory);
  // convenience constructor, should probably be removed/replaced later (see
  // comment above).
  template<typename Server>
  requires requires(Server& server) {
    { server.template getFeature<FeatureT>() } -> std::same_as<FeatureT&>;
  }
  static auto fromServer(Server& server) noexcept
      -> LazyApplicationFeatureReference {
    return LazyApplicationFeatureReference<FeatureT>([&server]() noexcept {
      return &server.template getFeature<FeatureT>();
    });
  }

 private:
  void ensureInitialization() noexcept;

 private:
  Factory _factory;
  FeatureT* _feature = nullptr;
};

// TODO move these into the .cpp file, and instantiate the class for all
//      ArangodFeatures. Then move the includes that are no longer needed in the
//      header into the .cpp file.
template<typename FeatureT>
LazyApplicationFeatureReference<FeatureT>::LazyApplicationFeatureReference(
    LazyApplicationFeatureReference::Factory factory)
    : _factory(std::move(factory)) {}

template<typename FeatureT>
auto LazyApplicationFeatureReference<FeatureT>::get() noexcept -> FeatureT& {
  ensureInitialization();
  return *_feature;
}

template<typename FeatureT>
void LazyApplicationFeatureReference<
    FeatureT>::ensureInitialization() noexcept {
  if (_feature == nullptr) {
    _feature = std::move(_factory)();
    _factory = nullptr;
  }
  ADB_PROD_ASSERT(_feature != nullptr)
      << "Feature reference initialization failed: " << typeid(FeatureT).name();
}

}  // namespace arangodb
