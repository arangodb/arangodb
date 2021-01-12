////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ViewTypesFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "FeaturePhases/BasicFeaturePhaseServer.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/BootstrapFeature.h"
#include "Utils/Events.h"

namespace {

struct InvalidViewFactory : public arangodb::ViewFactory {
  virtual arangodb::Result create(arangodb::LogicalView::ptr&, TRI_vocbase_t& vocbase,
                                  arangodb::velocypack::Slice const& definition) const override {
    std::string name;
    if (definition.isObject()) {
      name = arangodb::basics::VelocyPackHelper::getStringValue(
          definition, arangodb::StaticStrings::DataSourceName, "");
    }
    arangodb::events::CreateView(vocbase.name(), name, TRI_ERROR_INTERNAL);
    return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        std::string(
            "invalid type provided to create view with definition: ") +
            definition.toString());
  }

  virtual arangodb::Result instantiate(arangodb::LogicalView::ptr&, TRI_vocbase_t&,
                                       arangodb::velocypack::Slice const& definition) const override {
    return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        std::string(
            "invalid type provided to instantiate view with definition: ") +
            definition.toString());
  }
};

std::string const FEATURE_NAME("ViewTypes");
InvalidViewFactory const INVALID;

}  // namespace

namespace arangodb {

ViewTypesFeature::ViewTypesFeature(application_features::ApplicationServer& server)
    : application_features::ApplicationFeature(server, ViewTypesFeature::name()) {
  setOptional(false);
  startsAfter<application_features::BasicFeaturePhaseServer>();
}

Result ViewTypesFeature::emplace(LogicalDataSource::Type const& type,
                                 ViewFactory const& factory) {

  // ensure new factories are not added at runtime since that would require
  // additional locks
  if (server().hasFeature<BootstrapFeature>()) {
    auto& bootstrapFeature = server().getFeature<BootstrapFeature>();
    if (bootstrapFeature.isReady()) {
      return arangodb::Result(TRI_ERROR_INTERNAL,
                              std::string("view factory registration is only "
                                          "allowed during server startup"));
    }
  }

  if (!isEnabled()) { // should not be called
    TRI_ASSERT(false);
    return arangodb::Result();
  }
  
  if (!_factories.try_emplace(&type, &factory).second) {
    return arangodb::Result(TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER, std::string("view factory previously registered during view factory "
                                                                               "registration for view type '") +
                                                                       type.name() +
                                                                       "'");
  }

  return arangodb::Result();
}

ViewFactory const& ViewTypesFeature::factory(LogicalDataSource::Type const& type) const noexcept {
  auto itr = _factories.find(&type);
  TRI_ASSERT(itr == _factories.end() || false == !(itr->second));  // ViewTypesFeature::emplace(...)
                                                                   // inserts non-nullptr

  return itr == _factories.end() ? INVALID : *(itr->second);
}

/*static*/ std::string const& ViewTypesFeature::name() { return FEATURE_NAME; }

void ViewTypesFeature::prepare() {}

void ViewTypesFeature::unprepare() { _factories.clear(); }

}  // namespace arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
