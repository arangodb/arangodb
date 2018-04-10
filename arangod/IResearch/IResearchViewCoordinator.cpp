////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchViewCoordinator.h"
#include "IResearchView.h" // FIXME remove dependency
#include "Basics/StringUtils.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "IResearch/IResearchFeature.h"
#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Parser.h>

namespace arangodb {
namespace iresearch {

/*static*/ std::shared_ptr<LogicalView> IResearchViewCoordinator::make(
    TRI_vocbase_t& vocbase,
    velocypack::Slice const& info,
    uint64_t planVersion,
    LogicalView::PreCommitCallback const& preCommit
) {
  auto view = std::unique_ptr<IResearchViewCoordinator>(
    new IResearchViewCoordinator(vocbase, info, planVersion)
  );

  std::string error;

  auto properties = info.get("properties");

  if (!properties.isObject()) {
    // set to defaults
    properties = VPackSlice::emptyObjectSlice();
  }

  if (view->_meta.init(properties, error)) {
    return view;
  }

  LOG_TOPIC(WARN, IResearchFeature::IRESEARCH)
    << "failed to initialize IResearch view from definition, error: " << error;

  return nullptr;
}

/*static*/ arangodb::LogicalDataSource::Type const& IResearchViewCoordinator::type() noexcept {
  // retrieve type from IResearchFeature
  return IResearchView::type();
}

IResearchViewCoordinator::IResearchViewCoordinator(
    TRI_vocbase_t& vocbase,
    velocypack::Slice info,
    uint64_t planVersion
) : LogicalView(vocbase, info, planVersion) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
}

bool IResearchViewCoordinator::visitCollections(
    CollectionVisitor const& visitor
) const {
  for (auto& cid: _meta._collections) {
    if (!visitor(cid)) {
      return false;
    }
  }

  return true;
}

void IResearchViewCoordinator::toVelocyPack(
    velocypack::Builder& result,
    bool includeProperties,
    bool includeSystem
) const {
  // We write into an open object
  TRI_ASSERT(result.isOpenObject());

  // Meta Information
  result.add("id", VPackValue(std::to_string(id())));
  result.add("name", VPackValue(name()));
  result.add("type", VPackValue(type().name()));

  if (includeSystem) {
    result.add("deleted", VPackValue(deleted()));
    result.add("planId", VPackValue(std::to_string(planId())));
  }

  if (includeProperties) {
    result.add("properties", VPackValue(VPackValueType::Object));
    _meta.json(result);
    result.close();
  }

  TRI_ASSERT(result.isOpenObject()); // We leave the object open
}

arangodb::Result IResearchViewCoordinator::updateProperties(
    velocypack::Slice const& properties,
    bool /*partialUpdate*/,
    bool /*doSync*/
) {
  return ClusterInfo::instance()->setViewPropertiesCoordinator(
    vocbase().name(), // database name,
    basics::StringUtils::itoa(id()), // view id
    properties
  );
}

Result IResearchViewCoordinator::drop() {
  std::string errorMsg;

  int const res = ClusterInfo::instance()->dropViewCoordinator(
    vocbase().name(), // database name
    basics::StringUtils::itoa(id()), // view id
    errorMsg
  );

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(ERR, arangodb::Logger::CLUSTER)
      << "Could not drop view in agency, error: " << errorMsg
      << ", errorCode: " << res;

    return { res, errorMsg };
  }

  return {};
}
} // iresearch
} // arangodb
