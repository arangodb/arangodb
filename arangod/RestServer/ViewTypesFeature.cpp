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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ViewTypesFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/BootstrapFeature.h"
#include "Utils/Events.h"
#include "VocBase/vocbase.h"

namespace {

using namespace arangodb;

struct InvalidViewFactory final : arangodb::ViewFactory {
  Result create(LogicalView::ptr&, TRI_vocbase_t& vocbase,
                VPackSlice definition, bool /*isUserRequest*/) const final {
    std::string name;
    if (definition.isObject()) {
      name = basics::VelocyPackHelper::getStringValue(
          definition, StaticStrings::DataSourceName, "");
    }
    events::CreateView(vocbase.name(), name, TRI_ERROR_INTERNAL);
    return {
        TRI_ERROR_BAD_PARAMETER,
        std::string("invalid type provided to create view with definition: ") +
            definition.toString()};
  }

  Result instantiate(LogicalView::ptr&, TRI_vocbase_t&, VPackSlice definition,
                     bool /*isUserRequest*/) const final {
    return {TRI_ERROR_BAD_PARAMETER,
            std::string(
                "invalid type provided to instantiate view with definition: ") +
                definition.toString()};
  }
};

InvalidViewFactory const kInvalid;

}  // namespace

namespace arangodb {

ViewTypesFeature::ViewTypesFeature(Server& server)
    : ArangodFeature{server, *this} {
  setOptional(false);
  startsAfter<application_features::BasicFeaturePhaseServer>();
}

Result ViewTypesFeature::emplace(std::string_view type,
                                 ViewFactory const& factory) {
  // ensure new factories are not added at runtime since that would require
  // additional locks
  if (server().hasFeature<BootstrapFeature>()) {
    auto& bootstrapFeature = server().getFeature<BootstrapFeature>();
    if (bootstrapFeature.isReady()) {
      return {TRI_ERROR_INTERNAL,
              std::string("view factory registration is only "
                          "allowed during server startup")};
    }
  }

  if (!isEnabled()) {  // should not be called
    TRI_ASSERT(false);
    return {};
  }

  if (!_factories.try_emplace(type, &factory).second) {
    return {
        TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER,
        std::string("view factory previously registered during view factory "
                    "registration for view type '")
            .append(type)
            .append("'")};
  }

  return {};
}

ViewFactory const& ViewTypesFeature::factory(
    std::string_view type) const noexcept {
  auto const itr = _factories.find(type);

  // ViewTypesFeature::emplace(...) inserts non-nullptr
  TRI_ASSERT(itr == _factories.end() || static_cast<bool>(itr->second));

  return itr == _factories.end() ? kInvalid : *(itr->second);
}

void ViewTypesFeature::unprepare() { _factories.clear(); }

}  // namespace arangodb
