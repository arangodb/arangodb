////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>
#include <utility>

#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/iterator_range.hpp>

#include <velocypack/Options.h>
#include <velocypack/Slice.h>

#include "ApplicationServer.h"

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/Exceptions.h"
#include "Basics/Result.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StringUtils.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Basics/voc-errors.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ArgumentParser.h"
#include "ProgramOptions/Option.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace {
// fail and abort with the specified message
void failCallback(std::string const& message) {
  LOG_TOPIC("85b08", FATAL, arangodb::Logger::FIXME)
      << "error. cannot proceed. reason: " << message;
  FATAL_ERROR_EXIT();
}

auto nonEmptyFeatures(
    std::span<std::unique_ptr<ApplicationFeature>> features) noexcept {
  return boost::make_iterator_range(features) |
         boost::adaptors::filtered([](auto& p) { return nullptr != p; }) |
         boost::adaptors::transformed(
             [](auto& p) -> ApplicationFeature& { return *p; });
}

}  // namespace

std::atomic<bool> ApplicationServer::CTRL_C(false);

ApplicationServer::ApplicationServer(
    std::shared_ptr<ProgramOptions> options, char const* binaryPath,
    std::span<std::unique_ptr<ApplicationFeature>> features)
    : _state(State::UNINITIALIZED),
      _options(options),
      _features{features},
      _fail{failCallback},  // register callback function for failures
      _binaryPath{binaryPath} {
  addReporter(ProgressHandler{
      [server = this](ApplicationServer::State state) {
        server->progressInfo(server->stringifyState(state), "");
      },
      [server = this](ApplicationServer::State state, std::string_view name) {
        server->progressInfo(server->stringifyState(state), name);
      }});
}

std::pair<std::string, std::string> ApplicationServer::progressInfo() const {
  std::unique_lock mtx{_progressMutex};

  return {_progressPhase, _progressFeature};
}

void ApplicationServer::progressInfo(std::string_view phase,
                                     std::string_view feature) {
  std::unique_lock mtx{_progressMutex};
  _progressPhase = phase;
  _progressFeature = feature;
}

bool ApplicationServer::isPrepared() {
  auto tmp = state();
  return tmp == State::IN_START || tmp == State::IN_WAIT ||
         tmp == State::IN_SHUTDOWN || tmp == State::IN_STOP;
}

bool ApplicationServer::isStopping() const {
  auto tmp = state();
  return isStoppingState(tmp);
}

bool ApplicationServer::isStoppingState(State state) const {
  return state == State::IN_SHUTDOWN || state == State::IN_STOP ||
         state == State::IN_UNPREPARE || state == State::STOPPED ||
         state == State::ABORTED;
}

void ApplicationServer::disableFeatures(std::span<const size_t> types) {
  disableFeatures(types, false);
}

void ApplicationServer::forceDisableFeatures(std::span<const size_t> types) {
  disableFeatures(types, true);
}

void ApplicationServer::disableFeatures(std::span<const size_t> types,
                                        bool force) {
  for (size_t const type : types) {
    if (hasFeature(type)) {
      auto& feature = *_features[type];
      if (force) {
        feature.forceDisable();
      } else {
        feature.disable();
      }
    }
  }
}

// this method will initialize and validate options
// of all feature, start them and wait for a shutdown
// signal. after that, it will shutdown all features
void ApplicationServer::run(int argc, char* argv[]) {
  LOG_TOPIC("cc34f", TRACE, Logger::STARTUP) << "ApplicationServer::run";

  // collect options from all features
  // in this phase, all features are order-independent
  _state.store(State::IN_COLLECT_OPTIONS, std::memory_order_release);
  reportServerProgress(State::IN_COLLECT_OPTIONS);
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
  _state.store(State::IN_VALIDATE_OPTIONS, std::memory_order_release);
  reportServerProgress(State::IN_VALIDATE_OPTIONS);
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
  _state.store(State::IN_PREPARE, std::memory_order_release);
  reportServerProgress(State::IN_PREPARE);
  prepare();

  // turn off all features that depend on other features that have been
  // turned off. we repeat this to allow features to turn other features
  // off even in the prepare phase
  disableDependentFeatures();

  // start features. now features are allowed to start threads, write files
  // etc.
  _state.store(State::IN_START, std::memory_order_release);
  reportServerProgress(State::IN_START);
  start();

  // wait until we get signaled the shutdown request
  _state.store(State::IN_WAIT, std::memory_order_release);
  reportServerProgress(State::IN_WAIT);
  wait();

  // beginShutdown is called asynchronously ----------

  // stop all features
  _state.store(State::IN_STOP, std::memory_order_release);
  reportServerProgress(State::IN_STOP);
  stop();

  // unprepare all features
  _state.store(State::IN_UNPREPARE, std::memory_order_release);
  reportServerProgress(State::IN_UNPREPARE);
  unprepare();

  // stopped
  _state.store(State::STOPPED, std::memory_order_release);
  reportServerProgress(State::STOPPED);
}

// signal the server to initiate a soft shutdown
void ApplicationServer::initiateSoftShutdown() {
  LOG_TOPIC("aa452", TRACE, Logger::STARTUP)
      << "ApplicationServer::initiateSoftShutdown";

  // fowards the begin shutdown signal to all features
  for (auto it = _orderedFeatures.rbegin(); it != _orderedFeatures.rend();
       ++it) {
    ApplicationFeature& feature = it->get();
    if (feature.isEnabled()) {
      LOG_TOPIC("65421", TRACE, Logger::STARTUP)
          << (*it).get().name() << "::initiateSoftShutdown";
      try {
        feature.initiateSoftShutdown();
      } catch (std::exception const& ex) {
        LOG_TOPIC("eaf42", ERR, Logger::STARTUP)
            << "caught exception during initiateSoftShutdown of feature '"
            << feature.name() << "': " << ex.what();
      } catch (...) {
        LOG_TOPIC("53421", ERR, Logger::STARTUP)
            << "caught unknown exception during initiateSoftShutdown of "
               "feature '"
            << feature.name() << "'";
      }
    }
  }
}

// signal the server to shut down
void ApplicationServer::beginShutdown() {
  // fetch the old state, check if somebody already called shutdown, and only
  // proceed if not.
  State old = state();
  do {
    if (isStoppingState(old)) {
      // beginShutdown already called, nothing to do now
      return;
    }
    // try to enter the new state, but make sure nobody changed it in between
  } while (!_state.compare_exchange_weak(old, State::IN_SHUTDOWN,
                                         std::memory_order_release,
                                         std::memory_order_acquire));

  LOG_TOPIC("c7911", TRACE, Logger::STARTUP)
      << "ApplicationServer::beginShutdown";

  // make sure that we advance the state when we get out of here
  auto waitAborter = scopeGuard([this]() noexcept {
    std::lock_guard guard{_shutdownCondition.mutex};

    _abortWaiting = true;
    _shutdownCondition.cv.notify_one();
  });

  // now we can execute the actual shutdown sequence

  // fowards the begin shutdown signal to all features
  for (auto it = _orderedFeatures.rbegin(); it != _orderedFeatures.rend();
       ++it) {
    ApplicationFeature& feature = it->get();
    if (feature.isEnabled()) {
      LOG_TOPIC("e181f", TRACE, Logger::STARTUP)
          << (*it).get().name() << "::beginShutdown";
      try {
        feature.beginShutdown();
      } catch (std::exception const& ex) {
        LOG_TOPIC("b2cf4", ERR, Logger::STARTUP)
            << "caught exception during beginShutdown of feature '"
            << feature.name() << "': " << ex.what();
      } catch (...) {
        LOG_TOPIC("3f708", ERR, Logger::STARTUP)
            << "caught unknown exception during beginShutdown of feature '"
            << feature.name() << "'";
      }
    }
  }
}

void ApplicationServer::shutdownFatalError() {
  reportServerProgress(State::ABORTED);
}

// return VPack options, with optional filters applied to filter
// out specific options. the filter function is expected to return true
// for any options that should become part of the result
VPackBuilder ApplicationServer::options(
    std::function<bool(std::string const&)> const& filter) const {
  return _options->toVelocyPack(false, false, filter);
}

// walks over all features and runs a callback function for them
// the order in which features are visited is unspecified
void ApplicationServer::apply(std::function<void(ApplicationFeature&)> callback,
                              bool enabledOnly) {
  for (auto& feature : nonEmptyFeatures(_features)) {
    if (!enabledOnly || feature.isEnabled()) {
      callback(feature);
    }
  }
}

void ApplicationServer::collectOptions() {
  LOG_TOPIC("0eac7", TRACE, Logger::STARTUP)
      << "ApplicationServer::collectOptions";

  _options->addSection(
      Section("", "general settings", "", "general options", false, false));

  _options->addOption(
      "--dump-dependencies",
      "Dump the dependency graph of the feature phases (internal) and exit.",
      new BooleanParameter(&_dumpDependencies),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon,
                                          arangodb::options::Flags::Command));

  _options->addOption(
      "--dump-options",
      "Dump all available startup options in JSON format and exit.",
      new BooleanParameter(&_dumpOptions),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon,
                                          arangodb::options::Flags::Command));

  apply(
      [this](ApplicationFeature& feature) {
        LOG_TOPIC("b2731", TRACE, Logger::STARTUP)
            << feature.name() << "::loadOptions";
        reportFeatureProgress(_state.load(std::memory_order_relaxed),
                              feature.name());
        feature.collectOptions(_options);
      },
      true);
}

void ApplicationServer::parseOptions(int argc, char* argv[]) {
  ArgumentParser parser(_options.get());

  _helpSection = parser.helpSection(argc, argv);

  if (!_helpSection.empty()) {
    // user asked for "--help"

    // translate "all" to ".", because section "all" does not exist
    if (_helpSection == "all" || _helpSection == "hidden") {
      _helpSection = ".";
    }
    _options->printHelp(_helpSection);
    return;
  }

  if (!parser.parse(argc, argv)) {
    // command-line option parsing failed. an error was already printed
    // by now, so we can exit
    FATAL_ERROR_EXIT_CODE(_options->processingResult().exitCodeOrFailure());
  }

  if (_dumpDependencies) {
    std::cout << "digraph dependencies\n"
              << "{\n"
              << "  overlap = false;\n";
    for (auto& feature : ::nonEmptyFeatures(_features)) {
      for (size_t const before : feature.startsAfter()) {
        std::string_view depName;
        if (hasFeature(before)) {
          depName = getFeature(before).name();
        }
        std::cout << "  " << feature.name() << " -> " << depName << ";\n";
      }
    }
    std::cout << "}\n";
    exit(EXIT_SUCCESS);
  }

  for (auto it = _orderedFeatures.begin(); it != _orderedFeatures.end(); ++it) {
    ApplicationFeature& feature = (*it).get();
    if (feature.isEnabled()) {
      LOG_TOPIC("5c642", TRACE, Logger::STARTUP)
          << feature.name() << "::loadOptions";
      feature.loadOptions(_options, _binaryPath);
    }
  }

  if (_dumpOptions) {
    auto builder = _options->toVelocyPack(
        false, true, [](std::string const&) { return true; });
    arangodb::velocypack::Options options;
    options.prettyPrint = true;
    std::cout << builder.slice().toJson(&options) << std::endl;
    exit(EXIT_SUCCESS);
  }
}

void ApplicationServer::validateOptions() {
  LOG_TOPIC("1ed27", TRACE, Logger::STARTUP)
      << "ApplicationServer::validateOptions";

  for (ApplicationFeature& feature : _orderedFeatures) {
    if (feature.isEnabled()) {
      LOG_TOPIC("fa73c", TRACE, Logger::STARTUP)
          << feature.name() << "::validateOptions";
      reportFeatureProgress(_state.load(std::memory_order_relaxed),
                            feature.name());
      feature.validateOptions(_options);
      feature.state(ApplicationFeature::State::VALIDATED);
    }
  }
}

// setup and validate all feature dependencies, determine feature order
void ApplicationServer::setupDependencies(bool failOnMissing) {
  LOG_TOPIC("15559", TRACE, Logger::STARTUP)
      << "ApplicationServer::validateDependencies";

  // apply all "startsBefore" values
  for (auto& feature : nonEmptyFeatures(_features)) {
    for (auto const& other : feature.startsBefore()) {
      if (!hasFeature(other)) {
        if (failOnMissing) {
          _fail(std::string{"feature '"}
                    .append(feature.name())
                    .append("' depends on unknown feature id '")
                    .append(std::to_string(other))
                    .append("'"));
        }
        continue;
      }
      getFeature(other).startsAfter(feature.registration());
    }
  }

  // calculate ancestors for all features
  for (auto& feature : nonEmptyFeatures(_features)) {
    feature.determineAncestors(feature.registration());
  }

  // first check if a feature references an unknown other feature
  if (failOnMissing) {
    apply(
        [this](ApplicationFeature& feature) {
          for (auto& other : feature.dependsOn()) {
            if (!hasFeature(other)) {
              _fail(std::string{"feature '"}
                        .append(feature.name())
                        .append("' depends on unknown feature with id '")
                        .append(std::to_string(other))
                        .append("'"));
            }
            if (!getFeature(other).isEnabled()) {
              _fail(std::string{"enabled feature '"}
                        .append(feature.name())
                        .append("' depends on other feature '")
                        .append(getFeature(other).name())
                        .append("', which is disabled"));
            }
          }
        },
        true);
  }

  // first insert all features, even the inactive ones
  std::vector<std::reference_wrapper<ApplicationFeature>> features;
  for (auto& us : nonEmptyFeatures(_features)) {
    auto insertPosition = features.end();

    for (size_t i = features.size(); i > 0; --i) {
      auto const& other = features[i - 1].get();
      if (us.doesStartBefore(other.registration())) {
        // we start before the other feature. so move ourselves up
        insertPosition = features.begin() + (i - 1);
      } else if (other.doesStartBefore(us.registration())) {
        // the other feature starts before us. so stop moving up
        break;
      } else {
        // no dependencies between the two features
        if (us.name() < other.name()) {
          insertPosition = features.begin() + (i - 1);
        }
      }
    }
    features.insert(insertPosition, us);
  }

  if (Logger::isEnabled(LogLevel::TRACE, Logger::STARTUP)) {
    LOG_TOPIC("0fafb", TRACE, Logger::STARTUP) << "ordered features:";

    int position = 0;
    for (ApplicationFeature& feature : features) {
      auto const& startsAfter = feature.startsAfter();

      std::string dependencies;
      if (!startsAfter.empty()) {
        std::function<std::string(size_t)> cb =
            [this](size_t type) -> std::string {
          return hasFeature(type) ? std::string{getFeature(type).name()}
                                  : "unknown";
        };
        dependencies =
            " - depends on: " + StringUtils::join(startsAfter, ", ", cb);
      }
      LOG_TOPIC("b2ad5", TRACE, Logger::STARTUP)
          << "feature #" << ++position << ": " << feature.name()
          << (feature.isEnabled() ? "" : " (disabled)") << dependencies;
    }
  }

  // remove all inactive features
  for (auto it = features.begin(); it != features.end(); /* no hoisting */) {
    ApplicationFeature& feature = (*it).get();
    if (feature.isEnabled()) {
      // keep feature
      feature.state(ApplicationFeature::State::INITIALIZED);
      ++it;
    } else {
      // remove feature
      it = features.erase(it);
    }
  }

  _orderedFeatures = features;
}

void ApplicationServer::daemonize() {
  LOG_TOPIC("ca0b1", TRACE, Logger::STARTUP) << "ApplicationServer::daemonize";

  for (auto it = _orderedFeatures.begin(); it != _orderedFeatures.end(); ++it) {
    ApplicationFeature& feature = (*it).get();
    if (feature.isEnabled()) {
      feature.daemonize();
    }
  }
}

void ApplicationServer::disableDependentFeatures() {
  LOG_TOPIC("3e03b", TRACE, Logger::STARTUP)
      << "ApplicationServer::disableDependentFeatures";

  for (ApplicationFeature& feature : _orderedFeatures) {
    auto const& onlyEnabledWith = feature.onlyEnabledWith();

    if (!feature.isEnabled() || onlyEnabledWith.empty()) {
      continue;
    }

    for (auto const& other : onlyEnabledWith) {
      if (!hasFeature(other)) {
        LOG_TOPIC("f70cc", TRACE, Logger::STARTUP)
            << "turning off feature '" << feature.name()
            << "' because it is enabled only in conjunction with "
               "non-existing "
               "feature with id '"
            << other << "'";
        feature.disable();
        break;
      }
      ApplicationFeature& f = getFeature(other);
      if (!f.isEnabled()) {
        LOG_TOPIC("58e0e", TRACE, Logger::STARTUP)
            << "turning off feature '" << feature.name()
            << "' because it is enabled only in conjunction with disabled "
               "feature '"
            << f.name() << "'";
        feature.disable();
        break;
      }
    }
  }
}

void ApplicationServer::prepare() {
  LOG_TOPIC("04e8f", TRACE, Logger::STARTUP) << "ApplicationServer::prepare";

  for (ApplicationFeature& feature : _orderedFeatures) {
    reportFeatureProgress(_state.load(std::memory_order_relaxed),
                          feature.name());
    if (feature.isEnabled()) {
      try {
        LOG_TOPIC("d4e57", TRACE, Logger::STARTUP)
            << feature.name() << "::prepare";
        feature.prepare();
        feature.state(ApplicationFeature::State::PREPARED);
      } catch (std::exception const& ex) {
        LOG_TOPIC("37921", ERR, Logger::STARTUP)
            << "caught exception during prepare of feature '" << feature.name()
            << "': " << ex.what();
        throw;
      } catch (...) {
        LOG_TOPIC("a1b9f", ERR, Logger::STARTUP)
            << "caught unknown exception during preparation of feature '"
            << feature.name() << "'";
        throw;
      }
    }
  }
}

void ApplicationServer::start() {
  LOG_TOPIC("8ef64", TRACE, Logger::STARTUP) << "ApplicationServer::start";

  Result res;

  for (ApplicationFeature& feature : _orderedFeatures) {
    if (!feature.isEnabled()) {
      continue;
    }

    LOG_TOPIC("27b63", TRACE, Logger::STARTUP) << feature.name() << "::start";

    try {
      reportFeatureProgress(_state.load(std::memory_order_relaxed),
                            feature.name());
      feature.start();
      feature.state(ApplicationFeature::State::STARTED);
    } catch (basics::Exception const& ex) {
      res.reset(
          ex.code(),
          std::string(
              "startup aborted: caught exception during start of feature '")
                  .append(feature.name()) +
              "': " + ex.what());
    } catch (std::bad_alloc const& ex) {
      res.reset(
          TRI_ERROR_OUT_OF_MEMORY,
          std::string(
              "startup aborted: caught exception during start of feature '")
                  .append(feature.name()) +
              "': " + ex.what());
    } catch (std::exception const& ex) {
      res.reset(
          TRI_ERROR_INTERNAL,
          std::string(
              "startup aborted: caught exception during start of feature '")
                  .append(feature.name()) +
              "': " + ex.what());
    } catch (...) {
      res.reset(TRI_ERROR_INTERNAL,
                std::string("startup aborted: caught unknown exception during "
                            "start of feature '")
                        .append(feature.name()) +
                    "'");
    }

    if (res.fail()) {
      LOG_TOPIC("4ec19", ERR, Logger::STARTUP)
          << res.errorMessage() << ". shutting down";
      LOG_TOPIC("51732", TRACE, Logger::STARTUP)
          << "aborting startup, now stopping and unpreparing all features";

      // try to stop all feature that we just started
      for (auto it = _orderedFeatures.rbegin(); it != _orderedFeatures.rend();
           ++it) {
        ApplicationFeature& feature = *it;
        if (!feature.isEnabled()) {
          continue;
        }
        if (feature.state() == ApplicationFeature::State::STARTED) {
          LOG_TOPIC("e5cfe", TRACE, Logger::STARTUP)
              << "forcefully beginning stop of feature '" << feature.name()
              << "'";
          try {
            feature.beginShutdown();
          } catch (...) {
            // ignore errors on shutdown
            LOG_TOPIC("13224", TRACE, Logger::STARTUP)
                << "caught exception while stopping feature '" << feature.name()
                << "'";
          }
        }
      }

      // try to stop all feature that we just started
      for (auto it = _orderedFeatures.rbegin(); it != _orderedFeatures.rend();
           ++it) {
        ApplicationFeature& feature = *it;
        if (!feature.isEnabled()) {
          continue;
        }
        if (feature.state() == ApplicationFeature::State::STARTED) {
          LOG_TOPIC("e5cfd", TRACE, Logger::STARTUP)
              << "forcefully stopping feature '" << feature.name() << "'";
          try {
            feature.stop();
            feature.state(ApplicationFeature::State::STOPPED);
          } catch (...) {
            // if something goes wrong, we simply rethrow to abort!
            LOG_TOPIC("13223", FATAL, Logger::STARTUP)
                << "caught exception while stopping feature '" << feature.name()
                << "'";
            throw;
          }
        }
      }

      // try to unprepare all feature that we just started
      for (auto it = _orderedFeatures.rbegin(); it != _orderedFeatures.rend();
           ++it) {
        ApplicationFeature& feature = *it;
        ADB_PROD_ASSERT(feature.state() == ApplicationFeature::State::STOPPED ||
                        feature.state() == ApplicationFeature::State::PREPARED)
            << "feature " << feature.name() << " is in state "
            << (int)feature.state();

        LOG_TOPIC("6ba4f", TRACE, Logger::STARTUP)
            << "forcefully unpreparing feature '" << feature.name() << "'";
        try {
          feature.unprepare();
          feature.state(ApplicationFeature::State::UNPREPARED);
        } catch (...) {
          // if something goes wrong, we simply rethrow to abort!
          LOG_TOPIC("7d68f", FATAL, Logger::STARTUP)
              << "caught exception while unpreparing feature '"
              << feature.name() << "'";
          throw;
        }
      }
      shutdownFatalError();
      // throw exception so the startup aborts
      THROW_ARANGO_EXCEPTION(res);
    }
  }

  for (auto const& callback : _startupCallbacks) {
    callback();
  }
}

void ApplicationServer::stop() {
  LOG_TOPIC("3e53e", TRACE, Logger::STARTUP) << "ApplicationServer::stop";

  for (auto it = _orderedFeatures.rbegin(); it != _orderedFeatures.rend();
       ++it) {
    ApplicationFeature& feature = (*it).get();
    if (!feature.isEnabled()) {
      continue;
    }

    reportFeatureProgress(_state.load(std::memory_order_relaxed),
                          feature.name());
    LOG_TOPIC("4cd18", TRACE, Logger::STARTUP) << feature.name() << "::stop";
    try {
      feature.stop();
    } catch (std::exception const& ex) {
      LOG_TOPIC("f07eb", ERR, Logger::STARTUP)
          << "caught exception during stop of feature '" << feature.name()
          << "': " << ex.what();
    } catch (...) {
      LOG_TOPIC("6d496", ERR, Logger::STARTUP)
          << "caught unknown exception during stop of feature '"
          << feature.name() << "'";
    }
    feature.state(ApplicationFeature::State::STOPPED);
  }
}

void ApplicationServer::unprepare() {
  LOG_TOPIC("d6764", TRACE, Logger::STARTUP) << "ApplicationServer::unprepare";

  for (auto it = _orderedFeatures.rbegin(); it != _orderedFeatures.rend();
       ++it) {
    ApplicationFeature& feature = (*it).get();
    if (!feature.isEnabled()) {
      continue;
    }

    LOG_TOPIC("98be4", TRACE, Logger::STARTUP)
        << feature.name() << "::unprepare";
    reportFeatureProgress(_state.load(std::memory_order_relaxed),
                          feature.name());
    try {
      feature.unprepare();
    } catch (std::exception const& ex) {
      LOG_TOPIC("dc019", ERR, Logger::STARTUP)
          << "caught exception during unprepare of feature '" << feature.name()
          << "': " << ex.what();
    } catch (...) {
      LOG_TOPIC("859a3", ERR, Logger::STARTUP)
          << "caught unknown exception during unprepare of feature '"
          << feature.name() << "'";
    }
    feature.state(ApplicationFeature::State::UNPREPARED);
  }
}

void ApplicationServer::wait() {
  LOG_TOPIC("f86df", TRACE, Logger::STARTUP) << "ApplicationServer::wait";

  // wait here until beginShutdown has been called and finished
  while (true) {
    if (CTRL_C.load()) {
      beginShutdown();
    }

    // wait until somebody calls beginShutdown and it finishes
    std::unique_lock guard{_shutdownCondition.mutex};

    if (_abortWaiting) {
      // yippieh!
      break;
    }

    using namespace std::chrono_literals;
    _shutdownCondition.cv.wait_for(guard, 100ms);
  }
}

void ApplicationServer::reportServerProgress(State state) {
  for (auto& reporter : _progressReports) {
    if (reporter._state) {
      reporter._state(state);
    }
  }
}

void ApplicationServer::reportFeatureProgress(State state,
                                              std::string_view name) {
  for (auto& reporter : _progressReports) {
    if (reporter._feature) {
      reporter._feature(state, name);
    }
  }
}

std::string_view ApplicationServer::stringifyState() const {
  return stringifyState(_state.load(std::memory_order_acquire));
}

std::string_view ApplicationServer::stringifyState(State state) {
  switch (state) {
    case State::UNINITIALIZED:
      return "uninitialized";
    case State::IN_COLLECT_OPTIONS:
      return "in collect options";
    case State::IN_VALIDATE_OPTIONS:
      return "in validate options";
    case State::IN_PREPARE:
      return "in prepare";
    case State::IN_START:
      return "in start";
    case State::IN_WAIT:
      return "in wait";
    case State::IN_SHUTDOWN:
      return "in beginShutdown";
    case State::IN_STOP:
      return "in stop";
    case State::IN_UNPREPARE:
      return "in unprepare";
    case State::STOPPED:
      return "in stopped";
    case State::ABORTED:
      return "in aborted";
  }
  // we should never get here
  TRI_ASSERT(false);
  return "unknown";
}
