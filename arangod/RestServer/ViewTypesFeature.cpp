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

#include "BootstrapFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

namespace {

std::string const FEATURE_NAME("ViewTypes");
arangodb::ViewTypesFeature::ViewFactory const INVALID{};

} // namespace

namespace arangodb {

ViewTypesFeature::ViewTypesFeature(
  application_features::ApplicationServer& server
): application_features::ApplicationFeature(server, ViewTypesFeature::name()) {
  setOptional(false);
  startsAfter("BasicsPhase");
}

arangodb::Result ViewTypesFeature::emplace(
    LogicalDataSource::Type const& type,
    ViewFactory const& factory
) {
  if (!factory) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("view factory undefined during view factory registration for view type '") + type.name() + "'"
    );
  }

  auto* feature =
    arangodb::application_features::ApplicationServer::lookupFeature("Bootstrap");
  auto* bootstrapFeature = dynamic_cast<BootstrapFeature*>(feature);

  // ensure new factories are not added at runtime since that would require additional locks
  if (bootstrapFeature && bootstrapFeature->isReady()) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("view factory registration is only allowed during server startup")
    );
  }

  if (!_factories.emplace(&type, factory).second) {
    return arangodb::Result(
      TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER,
      std::string("view factory previously registered during view factory registration for view type '") + type.name() + "'"
    );
  }

  return arangodb::Result();
}

ViewTypesFeature::ViewFactory const& ViewTypesFeature::factory(
    LogicalDataSource::Type const& type
) const noexcept {
  auto itr = _factories.find(&type);

  return itr == _factories.end() ? INVALID : itr->second;
}

/*static*/ std::string const& ViewTypesFeature::name() {
  return FEATURE_NAME;
}

void ViewTypesFeature::prepare() { }

void ViewTypesFeature::unprepare() {
  _factories.clear();
}

} // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------