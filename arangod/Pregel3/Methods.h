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
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Pregel3/Query.h"
#include "VocBase/VocbaseInfo.h"
#include "Futures/Future.h"
#include "Pregel3Feature.h"
#include <VocBase/vocbase.h>
#include <ApplicationFeatures/ApplicationServer.h>

#include <memory>

namespace arangodb::pregel3 {

struct Pregel3Methods {
  virtual ~Pregel3Methods() = default;
  static auto createInstance(TRI_vocbase_t& vocbase)
      -> std::shared_ptr<Pregel3Methods>;

  virtual auto createQuery(std::string queryId,
                           GraphSpecification const& graph) const
      -> futures::Future<Result> = 0;

  virtual Pregel3Feature* getPregel3Feature() const = 0;
};

struct Pregel3MethodsSingleServer final
    : Pregel3Methods,
      std::enable_shared_from_this<Pregel3MethodsSingleServer> {
  explicit Pregel3MethodsSingleServer(TRI_vocbase_t& vocbase)
      : vocbase(vocbase) {}

  auto createQuery(std::string queryId, GraphSpecification const& graph) const
      -> arangodb::futures::Future<Result> override {
    vocbase.server().getFeature<Pregel3Feature>().createQuery(queryId, graph);
    return Result{};
  }

  Pregel3Feature* getPregel3Feature() const final {
    return &(vocbase.server().getFeature<Pregel3Feature>());
  }

  TRI_vocbase_t& vocbase;
};

}  // namespace arangodb::pregel3
