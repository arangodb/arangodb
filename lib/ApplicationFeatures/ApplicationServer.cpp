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
#include "ApplicationFeatures/PrivilegeFeature.h"
#include "Basics/ConditionLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/process-utils.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ArgumentParser.h"

#include <iostream>

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace {
// fail and abort with the specified message
static void failCallback(std::string const& message) {
  LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "error. cannot proceed. reason: " << message;
  FATAL_ERROR_EXIT();
}
}

ApplicationServer* ApplicationServer::server = nullptr;

ApplicationServer::ApplicationServer(std::shared_ptr<ProgramOptions> options,
    const char *binaryPath)
    : _options(options), _stopping(false), _binaryPath(binaryPath) {
  // register callback function for failures
  fail = failCallback;
  
  if (ApplicationServer::server != nullptr) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "ApplicationServer initialized twice";
  }

  ApplicationServer::server = this;
}

ApplicationServer::~ApplicationServer() {
  for (auto& it : _features) {
    try {
      delete it.second;
    } catch (...) {
      // we must skip over errors here as we're in the destructor.
      // we cannot rely on the LoggerFeature being present either, so
      // we have to suppress errors here
    }
  }

  ApplicationServer::server = nullptr;
}

void ApplicationServer::throwFeatureNotFoundException(std::string const& name) {
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "unknown feature '" + name + "'");
}

void ApplicationServer::throwFeatureNotEnabledException(
    std::string const& name) {
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "feature '" + name + "' is not enabled");
}

ApplicationFeature* ApplicationServer::lookupFeature(std::string const& name) {
  if (ApplicationServer::server == nullptr) {
    return nullptr;
  }

  try {
    return ApplicationServer::server->feature(name);
  } catch (...) {
    return nullptr;
  }
}

void ApplicationServer::disableFeatures(std::vector<std::string> const& names) {
  disableFeatures(names, false);
}

void ApplicationServer::forceDisableFeatures(
    std::vector<std::string> const& names) {
  disableFeatures(names, true);
}

void ApplicationServer::disableFeatures(std::vector<std::string> const& names,
                                        bool force) {
  for (auto const& name : names) {
    auto feature = ApplicationServer::lookupFeature(name);

    if (feature != nullptr) {
      if (force) {
        feature->forceDisable();
      } else {
        feature->disable();
      }
    }
  }
}

// adds a feature to the application server. the application server
// will take ownership of the feature object and destroy it in its
// destructor
void ApplicationServer::addFeature(ApplicationFeature* feature) {
  TRI_ASSERT(feature->state() == FeatureState::UNINITIALIZED);
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
    throwFeatureNotFoundException(name);
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
  _state = ServerState::IN_COLLECT_OPTIONS;
  reportServerProgress(_state);
  collectOptions();

  // setup dependency, but ignore any failure for now
  setupDependencies(false);

  // parse the command line parameters and load any configuration
  // file(s)
  parseOptions(argc, argv);

  if (!_helpSection.empty()) {
    // help shown. we can exit early
    return;
  }

  // seal the options
  _options->seal();

  // validate options of all features
  _state = ServerState::IN_VALIDATE_OPTIONS;
  reportServerProgress(_state);
  validateOptions();

  // setup and validate all feature dependencies
  setupDependencies(true);

  // turn off all features that depend on other features that have been
  // turned off
  disableDependentFeatures();

  // allows process control
  daemonize();

  // now the features will actually do some preparation work
  // in the preparation phase, the features must not start any threads
  // furthermore, they must not write any files under elevated privileges
  // if they want other features to access them, or if they want to access
  // these files with dropped privileges
  _state = ServerState::IN_PREPARE;
  reportServerProgress(_state);
  prepare();
  
  // turn off all features that depend on other features that have been
  // turned off. we repeat this to allow features to turn other features
  // off even in the prepare phase
  disableDependentFeatures();

  // permanently drop the privileges
  dropPrivilegesPermanently();

  // start features. now features are allowed to start threads, write files etc.
  _state = ServerState::IN_START;
  reportServerProgress(_state);
  start();

  // wait until we get signaled the shutdown request
  _state = ServerState::IN_WAIT;
  reportServerProgress(_state);
  wait();

  // stop all features
  _state = ServerState::IN_STOP;
  reportServerProgress(_state);
  stop();

  // unprepare all features
  _state = ServerState::IN_UNPREPARE;
  reportServerProgress(_state);
  unprepare();

  // stopped
  _state = ServerState::STOPPED;
  reportServerProgress(_state);
}

// signal the server to shut down
void ApplicationServer::beginShutdown() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << "ApplicationServer::beginShutdown";

  bool old = _stopping.exchange(true);

  // fowards the begin shutdown signal to all features
  if (!old) {
    for (auto it = _orderedFeatures.rbegin(); it != _orderedFeatures.rend();
         ++it) {
      if ((*it)->isEnabled()) {
        LOG_TOPIC(TRACE, Logger::STARTUP) << (*it)->name() << "::beginShutdown";
        try {
          (*it)->beginShutdown();
        } catch (std::exception const& ex) {
          LOG_TOPIC(ERR, Logger::STARTUP)
              << "caught exception during beginShutdown of feature '"
              << (*it)->name() << "': " << ex.what();
        } catch (...) {
          LOG_TOPIC(ERR, Logger::STARTUP)
              << "caught unknown exception during beginShutdown of feature '"
              << (*it)->name() << "'";
        }
      }
    }
  }

  CONDITION_LOCKER(guard, _shutdownCondition);
  guard.signal();
}

void ApplicationServer::shutdownFatalError() {
  reportServerProgress(ServerState::ABORT);
}

VPackBuilder ApplicationServer::options(
    std::unordered_set<std::string> const& excludes) const {
  return _options->toVPack(false, excludes);
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

  _options->addSection(
      Section("", "Global configuration", "global options", false, false));

  _options->addHiddenOption("--dump-dependencies", "dump dependency graph",
                            new BooleanParameter(&_dumpDependencies));

  apply(
      [this](ApplicationFeature* feature) {
        LOG_TOPIC(TRACE, Logger::STARTUP) << feature->name() << "::loadOptions";
        feature->collectOptions(_options);
        reportFeatureProgress(_state, feature->name());
      },
      true);
}

void ApplicationServer::parseOptions(int argc, char* argv[]) {
  ArgumentParser parser(_options.get());

  _helpSection = parser.helpSection(argc, argv);

  if (!_helpSection.empty()) {
    // user asked for "--help"

    // translate "all" to "*"
    if (_helpSection == "all") {
      _helpSection = "*";
    }
    _options->printHelp(_helpSection);
    return;
  }

  if (!parser.parse(argc, argv)) {
    // command-line option parsing failed. an error was already printed
    // by now, so we can exit
    exit(EXIT_FAILURE);
  }

  if (_dumpDependencies) {
    std::cout << "digraph dependencies\n"
              << "{\n"
              << "  overlap = false;\n";
    for (auto feature : _features) {
      for (auto before : feature.second->startsAfter()) {
        std::cout << "  " << feature.first << " -> " << before << ";\n";
      }
    }
    std::cout << "}\n";
    exit(EXIT_SUCCESS);
  }

  for (auto it = _orderedFeatures.begin(); it != _orderedFeatures.end(); ++it) {
    if ((*it)->isEnabled()) {
      LOG_TOPIC(TRACE, Logger::STARTUP) << (*it)->name() << "::loadOptions";
      (*it)->loadOptions(_options, _binaryPath);
    }
  }
}

void ApplicationServer::validateOptions() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << "ApplicationServer::validateOptions";

  for (auto feature : _orderedFeatures) {
    if (feature->isEnabled()) {
      LOG_TOPIC(TRACE, Logger::STARTUP) << feature->name()
                                        << "::validateOptions";
      feature->validateOptions(_options);
      feature->state(FeatureState::VALIDATED);
      reportFeatureProgress(_state, feature->name());
    }
  }
}

// setup and validate all feature dependencies, determine feature order
void ApplicationServer::setupDependencies(bool failOnMissing) {
  LOG_TOPIC(TRACE, Logger::STARTUP)
      << "ApplicationServer::validateDependencies";

  // apply all "startsBefore" values
  for (auto& it : _features) {
    for (auto const& other : it.second->startsBefore()) {
      if (!this->exists(other)) {
        if (failOnMissing) {
          fail("feature '" + it.second->name() +
               "' depends on unknown feature '" + other + "'");
        }
        continue;
      }
/*
      if (failOnMissing &&
          it.second->isEnabled() && 
          !this->feature(other)->isEnabled()) {
        fail("enabled feature '" + it.second->name() +
             "' depends on other feature '" + other +
             "', which is disabled");
      }
*/
      this->feature(other)->startsAfter(it.second->name());
    }
  }

  // calculate ancestors for all features
  for (auto& it : _features) {
    it.second->determineAncestors();
  }

  // first check if a feature references an unknown other feature
  if (failOnMissing) {
    apply(
        [this](ApplicationFeature* feature) {
          for (auto& other : feature->requires()) {
            if (!this->exists(other)) {
              fail("feature '" + feature->name() +
                   "' depends on unknown feature '" + other + "'");
            }
            if (!this->feature(other)->isEnabled()) {
              fail("enabled feature '" + feature->name() +
                   "' depends on other feature '" + other +
                   "', which is disabled");
            }
          }
        },
        true);
  }

  // first insert all features, even the inactive ones
  std::vector<ApplicationFeature*> features;
  for (auto& it : _features) {
    auto const& us = it.second;
    auto insertPosition = features.end();

    for (size_t i = features.size(); i > 0; --i) {
      auto const& other = features[i - 1];
      if (us->doesStartBefore(other->name())) {
        // we start before the other feature. so move ourselves up
        insertPosition = features.begin() + (i - 1);
      } else if (other->doesStartBefore(us->name())) {
        // the other feature starts before us. so stop moving up
        break;
      } else {
        // no dependencies between the two features
        if (us->name() < other->name()) {
          insertPosition = features.begin() + (i - 1);
        }
      }
    }
    features.insert(insertPosition, it.second);
  }
  
  LOG_TOPIC(TRACE, Logger::STARTUP) << "ordered features:";

  int position = 0;
  for (auto feature : features) {
    auto const& startsAfter = feature->startsAfter();

    std::string dependencies;
    if (!startsAfter.empty()) {
      dependencies = " - depends on: " + StringUtils::join(startsAfter, ", ");
    }
    LOG_TOPIC(TRACE, Logger::STARTUP)
        << "feature #" << ++position << ": " << feature->name()
        << (feature->isEnabled() ? "" : " (disabled)") << dependencies;
  }

  // remove all inactive features
  for (auto it = features.begin(); it != features.end(); /* no hoisting */) {
    if ((*it)->isEnabled()) {
      // keep feature
      (*it)->state(FeatureState::INITIALIZED);
      ++it;
    } else {
      // remove feature
      it = features.erase(it);
    }
  }

  _orderedFeatures = features;
}

void ApplicationServer::daemonize() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << "ApplicationServer::daemonize";

  for (auto it = _orderedFeatures.begin(); it != _orderedFeatures.end(); ++it) {
    if ((*it)->isEnabled()) {
      (*it)->daemonize();
    }
  }
}

void ApplicationServer::disableDependentFeatures() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << "ApplicationServer::disableDependentFeatures";

  for (auto feature : _orderedFeatures) {
    auto const& onlyEnabledWith = feature->onlyEnabledWith();

    if (!feature->isEnabled() || onlyEnabledWith.empty()) {
      continue;
    }

    for (auto const& other : onlyEnabledWith) {
      ApplicationFeature* f = lookupFeature(other);
      if (f == nullptr) {
        LOG_TOPIC(TRACE, Logger::STARTUP) << "turning off feature '" << feature->name() 
                                          << "' because it is enabled only in conjunction with non-existing feature '" 
                                          << other << "'";
        feature->disable();
        break;
      } else if (!f->isEnabled()) {
        LOG_TOPIC(TRACE, Logger::STARTUP) << "turning off feature '" << feature->name() 
                                          << "' because it is enabled only in conjunction with disabled feature '" 
                                          << f->name() << "'";
        feature->disable();
        break;
      }
    }
  }
}

void ApplicationServer::prepare() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << "ApplicationServer::prepare";

  // we start with elevated privileges
  bool privilegesElevated = true;

  for (auto feature : _orderedFeatures) {
    if (feature->isEnabled()) {
      bool const requiresElevated = feature->requiresElevatedPrivileges();

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
        LOG_TOPIC(TRACE, Logger::STARTUP) << feature->name() << "::prepare";
        feature->prepare();
        feature->state(FeatureState::PREPARED);
      } catch (std::exception const& ex) {
        LOG_TOPIC(ERR, Logger::STARTUP) << "caught exception during prepare of feature '"
                 << feature->name() << "': " << ex.what();
        // restore original privileges
        if (!privilegesElevated) {
          raisePrivilegesTemporarily();
        }
        throw;
      } catch (...) {
        LOG_TOPIC(ERR, Logger::STARTUP) << "caught unknown exception during preparation of feature '"
                 << feature->name() << "'";
        // restore original privileges
        if (!privilegesElevated) {
          raisePrivilegesTemporarily();
        }
        throw;
      }

      reportFeatureProgress(_state, feature->name());
    }
  }
}

void ApplicationServer::start() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << "ApplicationServer::start";

  int res = TRI_ERROR_NO_ERROR;

  for (auto feature : _orderedFeatures) {
    if (!feature->isEnabled()) {
      continue;
    }

    LOG_TOPIC(TRACE, Logger::STARTUP) << feature->name() << "::start";

    try {
      feature->start();
      feature->state(FeatureState::STARTED);
      reportFeatureProgress(_state, feature->name());
    } catch (basics::Exception const& ex) {
      LOG_TOPIC(ERR, Logger::STARTUP) << "caught exception during start of feature '" << feature->name()
               << "': " << ex.what() << ". shutting down";
      res = ex.code();
    } catch (std::bad_alloc const& ex) {
      LOG_TOPIC(ERR, Logger::STARTUP) << "caught exception during start of feature '" << feature->name()
               << "': " << ex.what() << ". shutting down";
      res = TRI_ERROR_OUT_OF_MEMORY;
    } catch (std::exception const& ex) {
      LOG_TOPIC(ERR, Logger::STARTUP) << "caught exception during start of feature '" << feature->name()
               << "': " << ex.what() << ". shutting down";
      res = TRI_ERROR_INTERNAL;
    } catch (...) {
      LOG_TOPIC(ERR, Logger::STARTUP) << "caught unknown exception during start of feature '"
               << feature->name() << "'. shutting down";
      res = TRI_ERROR_INTERNAL;
    }

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(TRACE, Logger::STARTUP) << "aborting startup, now stopping and unpreparing all features";
      // try to stop all feature that we just started
      for (auto it = _orderedFeatures.rbegin(); it != _orderedFeatures.rend();
           ++it) {
        auto feature = *it;
        if (!feature->isEnabled()) {
          continue;
        }
        if (feature->state() == FeatureState::STARTED) {
          LOG_TOPIC(TRACE, Logger::STARTUP) << "forcefully stopping feature '" << feature->name() << "'";
          try {
            feature->beginShutdown();
            feature->stop();
            feature->state(FeatureState::STOPPED);
          } catch (...) {
            // ignore errors on shutdown
            LOG_TOPIC(TRACE, Logger::STARTUP) << "caught exception while stopping feature '" << feature->name() << "'";
          }
        }
      }

      // try to unprepare all feature that we just started
      for (auto it = _orderedFeatures.rbegin(); it != _orderedFeatures.rend();
           ++it) {
        auto feature = *it;
        if (feature->state() == FeatureState::STOPPED) {
          LOG_TOPIC(TRACE, Logger::STARTUP) << "forcefully unpreparing feature '" << feature->name() << "'";
          try {
            feature->unprepare();
            feature->state(FeatureState::UNPREPARED);
          } catch (...) {
            // ignore errors on shutdown
            LOG_TOPIC(TRACE, Logger::STARTUP) << "caught exception while unpreparing feature '" << feature->name() << "'";
          }
        }
      }
      shutdownFatalError();
      // throw exception so the startup aborts
      THROW_ARANGO_EXCEPTION_MESSAGE(res, std::string("startup aborted: ") + TRI_errno_string(res));
    }
  }

  for (auto const& callback : _startupCallbacks) { 
    callback();
  }
}

void ApplicationServer::stop() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << "ApplicationServer::stop";

  for (auto it = _orderedFeatures.rbegin(); it != _orderedFeatures.rend();
       ++it) {
    auto feature = *it;
    if (!feature->isEnabled()) {
      continue;
    }

    LOG_TOPIC(TRACE, Logger::STARTUP) << feature->name() << "::stop";
    try {
      feature->stop();
    } catch (std::exception const& ex) {
      LOG_TOPIC(ERR, Logger::STARTUP) << "caught exception during stop of feature '" << feature->name() << "': " << ex.what();
    } catch (...) {
      LOG_TOPIC(ERR, Logger::STARTUP) << "caught unknown exception during stop of feature '" << feature->name() << "'";
    }
    feature->state(FeatureState::STOPPED);
    reportFeatureProgress(_state, feature->name());
  }
}

void ApplicationServer::unprepare() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << "ApplicationServer::unprepare";

  for (auto it = _orderedFeatures.rbegin(); it != _orderedFeatures.rend();
       ++it) {
    auto feature = *it;

    LOG_TOPIC(TRACE, Logger::STARTUP) << feature->name() << "::unprepare";
    try {
      feature->unprepare();
    } catch (std::exception const& ex) {
      LOG_TOPIC(ERR, Logger::STARTUP) << "caught exception during unprepare of feature '" << feature->name() << "': " << ex.what();
    } catch (...) {
      LOG_TOPIC(ERR, Logger::STARTUP) << "caught unknown exception during unprepare of feature '" << feature->name() << "'";
    }
    feature->state(FeatureState::UNPREPARED);
    reportFeatureProgress(_state, feature->name());
  }
}

void ApplicationServer::wait() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << "ApplicationServer::wait";

  while (!_stopping) {
    CONDITION_LOCKER(guard, _shutdownCondition);
    guard.wait(100000);
  }
}

// temporarily raise privileges
void ApplicationServer::raisePrivilegesTemporarily() {
  if (_privilegesDropped) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "must not raise privileges after dropping them");
  }

  LOG_TOPIC(TRACE, Logger::STARTUP) << "raising privileges";

  // TODO
}

// temporarily drop privileges
void ApplicationServer::dropPrivilegesTemporarily() {
  if (_privilegesDropped) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "must not try to drop privileges after dropping them");
  }

  LOG_TOPIC(TRACE, Logger::STARTUP) << "dropping privileges";

  // TODO
}

// permanently dropped privileges
void ApplicationServer::dropPrivilegesPermanently() {
  if (_privilegesDropped) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "must not try to drop privileges after dropping them");
  }

  auto privilege = dynamic_cast<PrivilegeFeature*>(lookupFeature("Privilege"));

  if (privilege != nullptr) {
    privilege->dropPrivilegesPermanently();
  }

  _privilegesDropped = true;
}

void ApplicationServer::reportServerProgress(ServerState state) {
  for (auto reporter : _progressReports) {
    reporter._state(state);
  }
}

void ApplicationServer::reportFeatureProgress(ServerState state,
                                              std::string const& name) {
  for (auto reporter : _progressReports) {
    reporter._feature(state, name);
  }
}

