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
#include "Basics/ConditionVariable.h"

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
// This must stop all threads, but not destroy the features.
//
// `unprepare`
//
// This destroys the features.

class ApplicationServer {
  ApplicationServer(ApplicationServer const&) = delete;
  ApplicationServer& operator=(ApplicationServer const&) = delete;

 public:
  // handled i.e. in WindowsServiceFeature.cpp
  enum class State : int {
    UNINITIALIZED,
    IN_COLLECT_OPTIONS,
    IN_VALIDATE_OPTIONS,
    IN_PREPARE,
    IN_START,
    IN_WAIT,
    IN_SHUTDOWN,
    IN_STOP,
    IN_UNPREPARE,
    STOPPED,
    ABORTED
  };
  
  class ProgressHandler {
   public:
    std::function<void(State)> _state;
    std::function<void(State, std::string const& featureName)> _feature;
   };

  static ApplicationServer* server;

  
  /// @brief whether or not the server has made it as least as far as the IN_START state
  static bool isPrepared();
  
  /// @brief whether or not the server has made it as least as far as the IN_SHUTDOWN state
  static bool isStopping();

  // returns the feature with the given name if known
  // throws otherwise
  template <typename T>
  static T* getFeature(std::string const& name) {
    T* feature =
        dynamic_cast<T*>(application_features::ApplicationServer::lookupFeature(name));
    if (feature == nullptr) {
      throwFeatureNotFoundException(name);
    }
    return feature;
  }

  template <typename T>
  static T* getFeature() {
    return getFeature<T>(T::name());
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
  ApplicationServer(std::shared_ptr<options::ProgramOptions>, char const* binaryPath);

  ~ApplicationServer();

  std::string helpSection() const { return _helpSection; }
  bool helpShown() const { return !_helpSection.empty(); }

  /// @brief stringify the internal state
  char const* stringifyState() const;

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

  // report that we are going down by fatal error
  void shutdownFatalError();

  // return VPack options, with optional filters applied to filter
  // out specific options. the filter function is expected to return true
  // for any options that should become part of the result
  VPackBuilder options(std::function<bool(std::string const&)> const& filter) const;
  
  // return the program options object
  std::shared_ptr<options::ProgramOptions> options() const { return _options; }

  // return the server state
  State state() const { return _state; }

  void addReporter(ProgressHandler reporter) {
    _progressReports.emplace_back(reporter);
  }

  // look up a feature and return a pointer to it. may be nullptr
  static ApplicationFeature* lookupFeature(std::string const&);

  template <typename T>
  static T* lookupFeature(std::string const& name) {
    typedef typename std::enable_if<std::is_base_of<ApplicationFeature, T>::value, T>::type type;
    return dynamic_cast<type*>(lookupFeature(name));
  }

  template <typename T>
  static T* lookupFeature() {
    return lookupFeature<T>(T::name());
  }

  char const* getBinaryPath() const { return _binaryPath; }

  void registerStartupCallback(std::function<void()> const& callback) {
    _startupCallbacks.emplace_back(callback);
  }

  void registerFailCallback(std::function<void(std::string const&)> const& callback) {
    fail = callback;
  }

  // setup and validate all feature dependencies, determine feature order
  void setupDependencies(bool failOnMissing);

  std::vector<ApplicationFeature*> const& getOrderedFeatures() {
    return _orderedFeatures;
  }

 private:
  // throws an exception that a requested feature was not found
  [[noreturn]] static void throwFeatureNotFoundException(std::string const& name);

  // throws an exception that a requested feature is not enabled
  [[noreturn]] static void throwFeatureNotEnabledException(std::string const& name);

  static void disableFeatures(std::vector<std::string> const& names, bool force);

  // walks over all features and runs a callback function for them
  void apply(std::function<void(ApplicationFeature*)>, bool enabledOnly);

  // collects the program options from all features,
  // without validating them
  void collectOptions();

  // parse options
  void parseOptions(int argc, char* argv[]);

  // allows features to cross-validate their program options
  void validateOptions();

  // allows process control
  void daemonize();

  // disables all features that depend on other features, which, themselves
  // are disabled
  void disableDependentFeatures();

  // allows features to prepare themselves
  void prepare();

  // starts features
  void start();

  // stops features
  void stop();

  // destroys features
  void unprepare();

  // after start, the server will wait in this method until
  // beginShutdown is called
  void wait();

  void raisePrivilegesTemporarily();
  void dropPrivilegesTemporarily();
  void dropPrivilegesPermanently();

  void reportServerProgress(State);
  void reportFeatureProgress(State, std::string const&);

 private:
  // the current state
  std::atomic<State> _state;

  // the shared program options
  std::shared_ptr<options::ProgramOptions> _options;

  // map of feature names to features
  std::unordered_map<std::string, ApplicationFeature*> _features;

  // features order for prepare/start
  std::vector<ApplicationFeature*> _orderedFeatures;

  // will be signaled when the application server is asked to shut down
  basics::ConditionVariable _shutdownCondition;

  /// @brief the condition variable protects access to this flag
  /// the flag is set to true when beginShutdown finishes
  bool _abortWaiting = false;

  // whether or not privileges have been dropped permanently
  bool _privilegesDropped = false;

  // whether or not to dump dependencies
  bool _dumpDependencies = false;

  // whether or not to dump configuration options
  bool _dumpOptions = false;

  // reporter for progress
  std::vector<ProgressHandler> _progressReports;

  // callbacks that are called after start
  std::vector<std::function<void()>> _startupCallbacks;

  // help section displayed
  std::string _helpSection;

  // the install directory of this program:
  char const* _binaryPath;

  // fail callback
  std::function<void(std::string const&)> fail;
};

}  // namespace application_features
}  // namespace arangodb

#endif
