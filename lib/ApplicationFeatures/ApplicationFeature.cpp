////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include <boost/core/demangle.hpp>

#include <algorithm>
#include <chrono>
#include <thread>

#include "ApplicationFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StringUtils.h"
#include "Basics/debugging.h"
#include "Logger/LogMacros.h"
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

ApplicationFeature::~ApplicationFeature() = default;

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
std::unordered_set<std::type_index> ApplicationFeature::ancestors() const {
  TRI_ASSERT(_ancestorsDetermined);
  return _ancestors;
}

void ApplicationFeature::startsAfter(std::type_index type) {
  _startsAfter.emplace(type);
}

void ApplicationFeature::startsBefore(std::type_index type) {
  _startsBefore.emplace(type);
}

bool ApplicationFeature::doesStartBefore(std::type_index type) const {
  if (!_server.hasFeature(type)) {
    // no relationship if the feature doesn't exist
    return false;
  }

  auto otherAncestors = _server.getFeature<ApplicationFeature>(type).ancestors();
  if (otherAncestors.find(std::type_index(typeid(*this))) != otherAncestors.end()) {
    // we are an ancestor of the other feature
    return true;
  }

  auto ourAncestors = ancestors();
  if (ourAncestors.find(type) != ourAncestors.end()) {
    // the other feature is an ancestor of us
    return false;
  }

  // no direct or indirect relationship between features
  return false;
}

void ApplicationFeature::addAncestorToAllInPath(
    std::vector<std::pair<size_t, std::reference_wrapper<ApplicationFeature>>>& path,
    std::type_index ancestorType) {
  std::function<bool(std::pair<size_t, std::reference_wrapper<ApplicationFeature>>&)> typeMatch =
      [ancestorType](std::pair<size_t, std::reference_wrapper<ApplicationFeature>>& pair) -> bool {
    auto& feature = pair.second.get();
    return std::type_index(typeid(feature)) == ancestorType;
  };

  if (std::find_if(path.begin(), path.end(), typeMatch) != path.end()) {
    // dependencies are cyclic

    // build type list to print out error
    std::vector<std::type_index> pathTypes;
    for (std::pair<size_t, std::reference_wrapper<ApplicationFeature>>& pair : path) {
      auto& feature = pair.second.get();
      pathTypes.emplace_back(std::type_index(typeid(feature)));
    }
    pathTypes.emplace_back(ancestorType);  // make sure we show the duplicate

    // helper for string join
    std::function<std::string(std::type_index)> cb = [](std::type_index type) -> std::string {
      return boost::core::demangle(type.name());
    };

    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "dependencies for feature '" + boost::core::demangle(pathTypes.begin()->name()) +
            "' are cyclic: " + basics::StringUtils::join(pathTypes, " <= ", cb));
  }

  // not cyclic, go ahead and add
  for (std::pair<size_t, std::reference_wrapper<ApplicationFeature>>& pair : path) {
    ApplicationFeature& descendant = pair.second;
    descendant._ancestors.emplace(ancestorType);
  }
}

// determine all direct and indirect ancestors of a feature
void ApplicationFeature::determineAncestors(std::type_index rootAsType) {
  if (_ancestorsDetermined) {
    return;
  }

  std::vector<std::pair<size_t, std::reference_wrapper<ApplicationFeature>>> path;
  std::vector<std::pair<size_t, std::type_index>> toProcess{{0, rootAsType}};
  while (!toProcess.empty()) {
    size_t depth = toProcess.back().first;
    std::type_index type = toProcess.back().second;
    toProcess.pop_back();

    if (server().hasFeature(type)) {
      ApplicationFeature& feature = server().getFeature<ApplicationFeature>(type);
      path.emplace_back(depth, feature);
      if (feature._ancestorsDetermined) {
        // short cut, just get the ancestors list and append add it everything
        // on the path
        std::unordered_set<std::type_index> ancestors = feature._ancestors;
        for (std::type_index ancestorType : ancestors) {
          addAncestorToAllInPath(path, ancestorType);
        }
      } else {
        for (std::type_index ancestorType : feature.startsAfter()) {
          addAncestorToAllInPath(path, ancestorType);
          toProcess.emplace_back(depth + 1, ancestorType);
        }
      }
    }

    // pop any elements off path that have had all their ancestors processed
    while (!path.empty() &&
           (toProcess.empty() || toProcess.back().first <= path.back().first)) {
      ApplicationFeature& finished = path.back().second;
      finished._ancestorsDetermined = true;
      path.pop_back();
    }
  }

  TRI_ASSERT(_ancestorsDetermined);
}

}  // namespace application_features
}  // namespace arangodb
