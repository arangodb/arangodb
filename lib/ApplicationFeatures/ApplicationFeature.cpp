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

#include "ApplicationFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Logger/Logger.h"

using namespace arangodb::application_features;
using namespace arangodb::options;

ApplicationFeature::ApplicationFeature(ApplicationServer* server,
                                       std::string const& name)
    : _server(server),
      _name(name),
      _enabled(true),
      _optional(false),
      _requiresElevatedPrivileges(false) {}

ApplicationFeature::~ApplicationFeature() {}

// add the feature's options to the global list of options. this method will be
// called regardless of whether to feature is enabled or disabled
void ApplicationFeature::collectOptions(std::shared_ptr<ProgramOptions>) {}

// load options from somewhere. this method will only be called for enabled
// features
void ApplicationFeature::loadOptions(std::shared_ptr<ProgramOptions>) {}

// validate the feature's options. this method will only be called for active
// features, after the ApplicationServer has determined which features should be
// turned off globally. in order to abort further processing in case of invalid
// parameter values, the feature should bail out by calling
// `abortInvalidParameters()`
void ApplicationFeature::validateOptions(std::shared_ptr<ProgramOptions>) {}

// preparation phase for feature
// in the preparation phase, the features must not start any threads
// furthermore, they must not write any files under elevated privileges
// if they want other features to access them, or if they want to access
// these files with dropped privileges
void ApplicationFeature::prepare() {}

// start the feature
void ApplicationFeature::start() {}

// notify the feature about a shutdown request
void ApplicationFeature::beginShutdown() {}

// stop and shut down the feature
void ApplicationFeature::stop() {}

// determine all direct and indirect ancestors of a feature
std::unordered_set<std::string> ApplicationFeature::ancestors() const {
  std::function<void(std::unordered_set<std::string>&, std::string const&)>
      build = [this, &build](std::unordered_set<std::string>& found,
                             std::string const& name) {
        for (auto& ancestor : this->server()->feature(name)->startsAfter()) {
          found.emplace(ancestor);
          build(found, ancestor);
        }
      };

  std::unordered_set<std::string> found;
  build(found, name());
  if (found.find(name()) != found.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "dependencies for feature '" + name() + "' are cyclic");
  }
  return found;
}

// whether the feature starts before another
bool ApplicationFeature::doesStartBefore(std::string const& other) const {
  auto otherAncestors = _server->feature(other)->ancestors();
  if (otherAncestors.find(name()) != otherAncestors.end()) {
    // we are an ancestor of the other feature
    return true;
  }

  auto ourAncestors = ancestors();
  if (ourAncestors.find(other) != ourAncestors.end()) {
    // the other feature is an ancestor of us
    return false;
  }

  // no direct or indirect relationship between features
  return false;
}
