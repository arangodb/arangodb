////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ViewTypesFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Views/LoggerView.h"

namespace {

std::string const FEATURE_NAME("ViewTypes");
arangodb::ViewCreator const INVALID{};

} // namespace

namespace arangodb {

ViewTypesFeature::ViewTypesFeature(
  application_features::ApplicationServer* server
): application_features::ApplicationFeature(server, ViewTypesFeature::name()) {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("WorkMonitor");
}

bool ViewTypesFeature::emplace(
    LogicalDataSource::Type const& type,
    ViewCreator creator
) {
  return _factories.emplace(&type, creator).second;
}

ViewCreator const& ViewTypesFeature::factory(
    LogicalDataSource::Type const& type
) const noexcept {
  auto itr = _factories.find(&type);

  return itr == _factories.end() ? INVALID : itr->second;
}

/*static*/ std::string const& ViewTypesFeature::name() {
  return FEATURE_NAME;
}

void ViewTypesFeature::prepare() {
  // register the "logger" example view type
  emplace(LoggerView::type(), LoggerView::creator);
}

void ViewTypesFeature::unprepare() { _factories.clear(); }

} // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------