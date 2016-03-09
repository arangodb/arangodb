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

#include "ApplicationServer.h"

#include "ApplicationFeatures/ApplicationFeature.h"
#include "ProgramOptions2/ArgumentParser.h"
#include "Logger/Logger.h"

using namespace arangodb::application_features;
using namespace arangodb::options;

ApplicationServer* ApplicationServer::server = nullptr;

ApplicationServer::ApplicationServer(std::shared_ptr<ProgramOptions> options)
    : _options(options), _stopping(false), _privilegesDropped(false) {
  if (ApplicationServer::server != nullptr) {
    LOG(ERR) << "ApplicationServer initialized twice";
  }

  ApplicationServer::server = this;
}

ApplicationServer::~ApplicationServer() {
  for (auto& it : _features) {
    delete it.second;
  }

  ApplicationServer::server = nullptr;
}

ApplicationFeature* ApplicationServer::lookupFeature(std::string const& name) {
  if (ApplicationServer::server == nullptr) {
    return nullptr;
  }

  try {
    return ApplicationServer::server->feature(name);
  } catch (...) {
  }

  return nullptr;
}

// adds a feature to the application server. the application server
// will take ownership of the feature object and destroy it in its
// destructor
void ApplicationServer::addFeature(ApplicationFeature* feature) {
  _features.emplace(feature->name(), feature);
}

// checks for the existence of a named feature. will not throw when used for
// a non-existing feature
bool ApplicationServer::exists(std::string const& name) const {
  return (_features.find(name) != _features.end());
}

// returns a pointer to a named feature. will throw when used for
// a non-existing feature
ApplicationFeature* ApplicationServer::feature(std::string const& name) const {
  auto it = _features.find(name);

  if (it == _features.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "unknown feature '" + name + "'");
  }
  return (*it).second;
}

// return whether or not a feature is enabled
// will throw when called for a non-existing feature
bool ApplicationServer::isEnabled(std::string const& name) const {
  return feature(name)->isEnabled();
}

// return whether or not a feature is optional
// will throw when called for a non-existing feature
bool ApplicationServer::isOptional(std::string const& name) const {
  return feature(name)->isOptional();
}

// return whether or not a feature is required
// will throw when called for a non-existing feature
bool ApplicationServer::isRequired(std::string const& name) const {
  return feature(name)->isRequired();
}

// this method will initialize and validate options
// of all feature, start them and wait for a shutdown
// signal. after that, it will shutdown all features
void ApplicationServer::run(int argc, char* argv[]) {
  LOG_TOPIC(TRACE, Logger::STARTUP) << "ApplicationServer::run";

  // collect options from all features
  // in this phase, all features are order-independent
  collectOptions();

  // setup dependency, but ignore any failure for now
  setupDependencies(false);

  // parse the command line parameters and load any configuration
  // file(s)
  parseOptions(argc, argv);

  // seal the options
  _options->seal();

  // validate options of all features
  // in this phase, all features are stil order-independent
  validateOptions();

  // enable automatic features
  enableAutomaticFeatures();

  // setup and validate all feature dependencies
  setupDependencies(true);

  // now the features will actually do some preparation work
  // in the preparation phase, the features must not start any threads
  // furthermore, they must not write any files under elevated privileges
  // if they want other features to access them, or if they want to access
  // these files with dropped privileges
  prepare();

  // permanently drop the privileges
  dropPrivilegesPermanently();

  // start features. now features are allowed to start threads, write files etc.
  start();

  // wait until we get signaled the shutdown request
  wait();

  // stop all features
  stop();
}

// signal the server to shut down
void ApplicationServer::beginShutdown() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << "";
  LOG_TOPIC(TRACE, Logger::STARTUP) << "ApplicationServer::beginShutdown";
  LOG_TOPIC(TRACE, Logger::STARTUP)
      << "------------------------------------------------";

  // fowards the begin shutdown signal to all features
  for (auto it = _orderedFeatures.rbegin(); it != _orderedFeatures.rend();
       ++it) {
    if ((*it)->isEnabled()) {
      (*it)->beginShutdown();
    }
  }

  _stopping = true;
  // TODO: use condition variable for signaling shutdown
  // to run method
}

// fail and abort with the specified message
void ApplicationServer::fail(std::string const& message) {
  LOG(FATAL) << "error. cannot proceed. reason: " << message;
  FATAL_ERROR_EXIT();
}

// walks over all features and runs a callback function for them
// the order in which features are visited is unspecified
void ApplicationServer::apply(std::function<void(ApplicationFeature*)> callback,
                              bool enabledOnly) {
  for (auto& it : _features) {
    if (!enabledOnly || it.second->isEnabled()) {
      callback(it.second);
    }
  }
}

void ApplicationServer::collectOptions() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << "ApplicationServer::collectOptions";

  apply([this](ApplicationFeature* feature) {
    feature->collectOptions(_options);
  }, true);
}

void ApplicationServer::parseOptions(int argc, char* argv[]) {
  ArgumentParser parser(_options.get());

  std::string helpSection = parser.helpSection(argc, argv);

  if (!helpSection.empty()) {
    // user asked for "--help"
    _options->printHelp(helpSection);
    exit(EXIT_SUCCESS);
  }

  if (!parser.parse(argc, argv)) {
    // command-line option parsing failed. an error was already printed
    // by now, so we can exit
    exit(EXIT_FAILURE);
  }

  for (auto it = _orderedFeatures.begin(); it != _orderedFeatures.end(); ++it) {
    if ((*it)->isEnabled()) {
      (*it)->loadOptions(_options);
    }
  }
}

void ApplicationServer::validateOptions() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << "";
  LOG_TOPIC(TRACE, Logger::STARTUP) << "ApplicationServer::validateOptions";
  LOG_TOPIC(TRACE, Logger::STARTUP)
      << "------------------------------------------------";

  apply([this](ApplicationFeature* feature) {
    feature->validateOptions(_options);
  }, true);
}

void ApplicationServer::enableAutomaticFeatures() {
  bool changed;
  do {
    changed = false;
    for (auto& it : _features) {
      auto other = it.second->enableWith();
      if (other.empty()) {
        continue;
      }
      if (!this->exists(other)) {
        fail("feature '" + it.second->name() +
             "' depends on unknown feature '" + other + "'");
      }
      bool otherIsEnabled = this->feature(other)->isEnabled();
      if (otherIsEnabled != it.second->isEnabled()) {
        it.second->setEnabled(otherIsEnabled);
        changed = true;
      }
    }
  } while (changed);
}

// setup and validate all feature dependencies, determine feature order
void ApplicationServer::setupDependencies(bool failOnMissing) {
  LOG_TOPIC(TRACE, Logger::STARTUP) << "";
  LOG_TOPIC(TRACE, Logger::STARTUP)
      << "ApplicationServer::validateDependencies";
  LOG_TOPIC(TRACE, Logger::STARTUP)
      << "------------------------------------------------";

  // first check if a feature references an unknown other feature
  if (failOnMissing) {
    apply([this](ApplicationFeature* feature) {
      for (auto& other : feature->requires()) {
        if (!this->exists(other)) {
          fail("feature '" + feature->name() +
               "' depends on unknown feature '" + other + "'");
        }
        if (!this->feature(other)->isEnabled()) {
          fail("enabled feature '" + feature->name() +
               "' depends on other feature '" + other + "', which is disabled");
        }
      }
    }, true);
  }

  // first insert all features, even the inactive ones
  std::vector<ApplicationFeature*> features;
  for (auto& it : _features) {
    auto insertPosition = features.end();
     
    if (!features.empty()) {
      for (size_t i = features.size(); i > 0; --i) {
        if (it.second->doesStartBefore(features[i - 1]->name())) {
          insertPosition = features.begin() + (i - 1);
        }
      }
    }
    features.insert(insertPosition, it.second);
  }
  
  // remove all inactive features
  for (auto it = features.begin(); it != features.end(); /* no hoisting */) {
    if ((*it)->isEnabled()) {
      // keep feature
      ++it;
    } else {
      // remove feature
      it = features.erase(it);
    }
  }

  _orderedFeatures = features;
}

void ApplicationServer::prepare() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << "";
  LOG_TOPIC(TRACE, Logger::STARTUP) << "ApplicationServer::prepare";
  LOG_TOPIC(TRACE, Logger::STARTUP)
      << "------------------------------------------------";

  // we start with elevated privileges
  bool privilegesElevated = true;

  for (auto it = _orderedFeatures.begin(); it != _orderedFeatures.end(); ++it) {
    if ((*it)->isEnabled()) {
      bool const requiresElevated = (*it)->requiresElevatedPrivileges();

      if (requiresElevated != privilegesElevated) {
        // must change privileges for the feature
        if (requiresElevated) {
          raisePrivilegesTemporarily();
          privilegesElevated = true;
        } else {
          dropPrivilegesTemporarily();
          privilegesElevated = false;
        }
      }

      try {
        (*it)->prepare();
      } catch (...) {
        // restore original privileges
        if (!privilegesElevated) {
          raisePrivilegesTemporarily();
        }
        throw;
      }
    }
  }
}

void ApplicationServer::start() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << "";
  LOG_TOPIC(TRACE, Logger::STARTUP) << "ApplicationServer::start";
  LOG_TOPIC(TRACE, Logger::STARTUP)
      << "------------------------------------------------";

  for (auto it = _orderedFeatures.begin(); it != _orderedFeatures.end(); ++it) {
    (*it)->start();
  }
}

void ApplicationServer::stop() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << "";
  LOG_TOPIC(TRACE, Logger::STARTUP) << "ApplicationServer::stop";
  LOG_TOPIC(TRACE, Logger::STARTUP)
      << "------------------------------------------------";

  for (auto it = _orderedFeatures.rbegin(); it != _orderedFeatures.rend();
       ++it) {
    (*it)->stop();
  }
}

void ApplicationServer::wait() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << "";
  LOG_TOPIC(TRACE, Logger::STARTUP) << "ApplicationServer::wait";
  LOG_TOPIC(TRACE, Logger::STARTUP)
      << "------------------------------------------------";

  while (!_stopping) {
    // TODO: use condition variable for waiting for shutdown
    ::usleep(100000);
  }
}

// temporarily raise privileges
void ApplicationServer::raisePrivilegesTemporarily() {
  if (_privilegesDropped) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "must not raise privileges after dropping them");
  }

  // TODO
}

// temporarily drop privileges
void ApplicationServer::dropPrivilegesTemporarily() {
  if (_privilegesDropped) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "must not try to drop privileges after dropping them");
  }

  // TODO
}

// permanently dropped privileges
void ApplicationServer::dropPrivilegesPermanently() {
  if (_privilegesDropped) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "must not try to drop privileges after dropping them");
  }
  _privilegesDropped = true;

  // TODO
}
