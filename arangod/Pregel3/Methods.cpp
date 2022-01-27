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

#include "Pregel3/Methods.h"

#include <Basics/Exceptions.h>
#include <Basics/Result.h>
#include <VocBase/vocbase.h>
#include <Basics/voc-errors.h>
#include <ApplicationFeatures/ApplicationServer.h>
#include <Pregel3/Pregel3Feature.h>
#include "Cluster/ServerState.h"

using namespace arangodb;
using namespace arangodb::pregel3;

struct Pregel3MethodsSingleServer final
    : Pregel3Methods,
      std::enable_shared_from_this<Pregel3MethodsSingleServer> {
  explicit Pregel3MethodsSingleServer(TRI_vocbase_t& vocbase)
      : vocbase(vocbase) {}

  auto createQuery(GraphSpecification const& graph) const
      -> arangodb::futures::Future<Result> override {
    vocbase.server().getFeature<Pregel3Feature>().createQuery(graph);
    return Result{};
  }

  TRI_vocbase_t& vocbase;
};

auto arangodb::pregel3::Pregel3Methods::createInstance(TRI_vocbase_t& vocbase)
    -> std::shared_ptr<Pregel3Methods> {
  switch (ServerState::instance()->getRole()) {
    case ServerState::ROLE_SINGLE:
      return std::make_shared<Pregel3MethodsSingleServer>(vocbase);
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_NOT_IMPLEMENTED,
          "api only on available coordinators or dbservers");
  }
}
