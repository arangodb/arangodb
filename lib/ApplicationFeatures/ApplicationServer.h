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

#ifndef ARANGODB_APPLICATION_FEATURES_APPLICATION_SERVER_H
#define ARANGODB_APPLICATION_FEATURES_APPLICATION_SERVER_H 1

#include "Basics/Common.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace options {
class ProgramOptions;
}

namespace application_features {
class ApplicationFeature;

// the following phases exists:
//
// `collectOptions`
//
// Creates the prgramm options for a feature. Features are not
// allowed to open files or sockets, create threads or allocate
// other resources. This method will be called regardless of whether
// to feature is enabled or disabled. There is no defined order in
// which the features are traversed.
//
// `loadOptions`
//
// Allows a feature to load more options from somewhere. This method
// will only be called for enabled features. There is no defined
// order in which the features are traversed.
//
// `validateOptions`
//
// Validates the feature's options. This method will only be called for enabled
// features. Help is handled before any `validateOptions` of a feature is
// called. The `validateOptions` methods are called in a order that obeys the
// `startsAfter `conditions.
//
// `daemonize`
//
// In this phase process control (like putting the process into the background
// will be handled). This method will only be called for enabled features.
// The `daemonize` methods are called in a order that obeys the `startsAfter`
// conditions.
//
// `prepare`
//
// Now the features will actually do some preparation work
// in the preparation phase, the features must not start any threads
// furthermore, they must not write any files under elevated privileges
// if they want other features to access them, or if they want to access
// these files with dropped privileges. The `prepare` methods are called in a
// order that obeys the `startsAfter` conditions.
//
// `start`
//
// Start the features. Features are now allowed to created threads.
//
// The `start` methods are called in a order that obeys the `startsAfter`
// conditions.
//
// `stop`
//
// Stops the features. The `stop` methods are called in reversed `start` order.

class ApplicationServer {
  ApplicationServer(ApplicationServer const&) = delete;
  ApplicationServer& operator=(ApplicationServer const&) = delete;

 public:
  static ApplicationServer* server;
  static ApplicationFeature* lookupFeature(std::string const&);
  static bool isStopping() {
    return server != nullptr && server->_stopping.load();
  }

  enum class FeatureState {
    UNINITIALIZED,
    INITIALIZED,
    VALIDATED,
    PREPARED,
    STARTED,
    STOPPED
  };

  // returns the feature with the given name if known
  // throws otherwise
  template <typename T>
  static T* getFeature(std::string const& name) {
    T* feature = dynamic_cast<T*>(
        application_features::ApplicationServer::lookupFeature(name));
    if (feature == nullptr) {
      throwFeatureNotFoundException(name);
    }
    return feature;
  }

  // returns the feature with the given name if known and enabled
  // throws otherwise
  template <typename T>
  static T* getEnabledFeature(std::string const& name) {
    T* feature = getFeature<T>(name);
    if (!feature->isEnabled()) {
      throwFeatureNotEnabledException(name);
    }
    return feature;
  }

  static void disableFeatures(std::vector<std::string> const&);
  static void forceDisableFeatures(std::vector<std::string> const&);

 public:
  explicit ApplicationServer(std::shared_ptr<options::ProgramOptions>);

  ~ApplicationServer();

  // adds a feature to the application server. the application server
  // will take ownership of the feature object and destroy it in its
  // destructor
  void addFeature(ApplicationFeature*);

  // checks for the existence of a named feature. will not throw when used for
  // a non-existing feature
  bool exists(std::string const&) const;

  // returns a pointer to a named feature. will throw when used for
  // a non-existing feature
  ApplicationFeature* feature(std::string const&) const;

  // return whether or not a feature is enabled
  // will throw when called for a non-existing feature
  bool isEnabled(std::string const&) const;

  // return whether or not a feature is optional
  // will throw when called for a non-existing feature
  bool isOptional(std::string const&) const;

  // return whether or not a feature is required
  // will throw when called for a non-existing feature
  bool isRequired(std::string const&) const;

  // this method will initialize and validate options
  // of all feature, start them and wait for a shutdown
  // signal. after that, it will shutdown all features
  void run(int argc, char* argv[]);

  // signal the server to shut down
  void beginShutdown();

  // return VPack options
  VPackBuilder options(std::unordered_set<std::string> const& excludes) const;

 private:
  // throws an exception if a requested feature was not found
  static void throwFeatureNotFoundException(std::string const& name);

  // throws an exception if a requested feature is not enabled
  static void throwFeatureNotEnabledException(std::string const& name);

  static void disableFeatures(std::vector<std::string> const& names,
                              bool force);

  // fail and abort with the specified message
  void fail(std::string const& message);

  // walks over all features and runs a callback function for them
  void apply(std::function<void(ApplicationFeature*)>, bool enabledOnly);

  // collects the program options from all features,
  // without validating them
  void collectOptions();

  // parse options
  void parseOptions(int argc, char* argv[]);

  // allows features to cross-validate their program options
  void validateOptions();

  // enable automatic features
  void enableAutomaticFeatures();

  // setup and validate all feature dependencies, determine feature order
  void setupDependencies(bool failOnMissing);

  // allows process control
  void daemonize();

  // allows features to prepare themselves
  void prepare();

  // starts features
  void start();

  // stops features
  void stop();

  // after start, the server will wait in this method until
  // beginShutdown is called
  void wait();

  void raisePrivilegesTemporarily();
  void dropPrivilegesTemporarily();
  void dropPrivilegesPermanently();

 private:
  // the shared program options
  std::shared_ptr<options::ProgramOptions> _options;

  // map of feature names to features
  std::unordered_map<std::string, ApplicationFeature*> _features;

  // features order for prepare/start
  std::vector<ApplicationFeature*> _orderedFeatures;

  // stop flag. this is being changed by calling beginShutdown
  std::atomic<bool> _stopping;

  // whether or not privileges have been dropped permanently
  bool _privilegesDropped;

  // whether or not to dump dependencies
  bool _dumpDependencies;
};
}
}

#endif
