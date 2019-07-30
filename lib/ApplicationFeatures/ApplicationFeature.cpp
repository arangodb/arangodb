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
#include "Basics/StringUtils.h"
#include "Logger/Logger.h"

using namespace arangodb::options;

namespace arangodb {
namespace application_features {

ApplicationFeature::ApplicationFeature(ApplicationServer& server, std::string const& name)
    : _server(server),
      _name(name),
      _state(State::UNINITIALIZED),
      _enabled(true),
      _optional(false),
      _requiresElevatedPrivileges(false),
      _ancestorsDetermined(false) {}

ApplicationFeature::~ApplicationFeature() {}

// add the feature's options to the global list of options. this method will be
// called regardless of whether to feature is enabled or disabled
void ApplicationFeature::collectOptions(std::shared_ptr<ProgramOptions>) {}

// load options from somewhere. this method will only be called for enabled
// features
void ApplicationFeature::loadOptions(std::shared_ptr<ProgramOptions>,
                                     char const* /*binaryPath*/) {}

// validate the feature's options. this method will only be called for active
// features, after the ApplicationServer has determined which features should be
// turned off globally. in order to abort further processing in case of invalid
// parameter values, the feature should bail out by calling
// `abortInvalidParameters()`
void ApplicationFeature::validateOptions(std::shared_ptr<ProgramOptions>) {}

// allows process control
void ApplicationFeature::daemonize() {}

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

// stop the feature
void ApplicationFeature::stop() {}

// shut down the feature
void ApplicationFeature::unprepare() {}

// determine all direct and indirect ancestors of a feature
std::unordered_set<std::string> ApplicationFeature::ancestors() const {
  TRI_ASSERT(_ancestorsDetermined);
  return _ancestors;
}

// whether the feature starts before another
bool ApplicationFeature::doesStartBefore(std::string const& other) const {
  auto otherAncestors = _server.feature(other)->ancestors();

  if (otherAncestors.find(_name) != otherAncestors.end()) {
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

// determine all direct and indirect ancestors of a feature
void ApplicationFeature::determineAncestors() {
  if (_ancestorsDetermined) {
    return;
  }

  std::vector<std::string> path;

  std::function<void(std::string const&)> build = [this, &build,
                                                   &path](std::string const& name) {
    // lookup the feature first. it may not exist
    if (!this->server()->exists(name)) {
      // feature not found. no worries
      return;
    }

    ApplicationFeature* other = this->server()->lookupFeature(name);

    if (other != nullptr) {
      path.emplace_back(name);
      for (auto& ancestor : other->startsAfter()) {
        if (_ancestors.emplace(ancestor).second) {
          if (ancestor == _name) {
            path.emplace_back(ancestor);
            THROW_ARANGO_EXCEPTION_MESSAGE(
                TRI_ERROR_INTERNAL,
                "dependencies for feature '" + _name + "' are cyclic: " +
                    arangodb::basics::StringUtils::join(path, " <= "));
          }
          build(ancestor);
        }
      }
      path.pop_back();
    }
  };

  build(_name);
  _ancestorsDetermined = true;
}

}  // namespace application_features
}  // namespace arangodb
