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

#include "Views/LoggerView.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

std::unordered_map<std::string, arangodb::ViewCreator>
    ViewTypesFeature::_viewCreators;

ViewTypesFeature::ViewTypesFeature(ApplicationServer* server)
    : ApplicationFeature(server, "ViewTypes") {
  setOptional(false);
  requiresElevatedPrivileges(false);
}

void ViewTypesFeature::prepare() {
  // register the "logger" example view type
  registerViewImplementation(LoggerView::type, LoggerView::creator);
}

void ViewTypesFeature::unprepare() { _viewCreators.clear(); }

void ViewTypesFeature::registerViewImplementation(std::string const& type,
                                                  ViewCreator creator) {
  _viewCreators.emplace(type, creator);
}

bool ViewTypesFeature::isValidType(std::string const& type) const {
  return (_viewCreators.find(type) != _viewCreators.end());
}

ViewCreator& ViewTypesFeature::creator(std::string const& type) const {
  auto it = _viewCreators.find(type);

  if (it == _viewCreators.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "no handler found for view type");
  }
  return (*it).second;
}
