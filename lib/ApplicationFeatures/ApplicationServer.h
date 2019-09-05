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

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"

#include <velocypack/Builder.h>

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

   static ApplicationServer& server();

   static std::atomic<bool> CTRL_C;

  public:
   ApplicationServer(std::shared_ptr<options::ProgramOptions>, char const* binaryPath);

   TEST_VIRTUAL ~ApplicationServer();

   std::string helpSection() const { return _helpSection; }
   bool helpShown() const { return !_helpSection.empty(); }

   /// @brief stringify the internal state
   char const* stringifyState() const;

   // return whether or not a feature is enabled
   // will throw when called for a non-existing feature
   template <typename T>
   bool isEnabled() const {
     return getFeature<T>().isEnabled();
   }

   // return whether or not a feature is optional
   // will throw when called for a non-existing feature
   template <typename T>
   bool isOptional() const {
     return getFeature<T>().isOptional();
   }

   // return whether or not a feature is required
   // will throw when called for a non-existing feature
   template <typename T>
   bool isRequired() const {
     return getFeature<T>().isRequired();
   }

   /// @brief whether or not the server has made it as least as far as the IN_START state
   bool isPrepared();

   /// @brief whether or not the server has made it as least as far as the IN_SHUTDOWN state
   bool isStopping();

   /// @brief whether or not state is the shutting down state or further (i.e. stopped, aborted etc.)
   bool isStoppingState(State state);

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
   velocypack::Builder options(std::function<bool(std::string const&)> const& filter) const;

   // return the program options object
   std::shared_ptr<options::ProgramOptions> options() const { return _options; }

   // return the server state
   TEST_VIRTUAL State state() const { return _state; }

   void addReporter(ProgressHandler reporter) {
     _progressReports.emplace_back(reporter);
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

   std::vector<std::reference_wrapper<ApplicationFeature>> const& getOrderedFeatures() {
     return _orderedFeatures;
   }

#ifdef TEST_VIRTUAL
   void setStateUnsafe(State ss) { server()._state = ss; }
#endif

   // adds a feature to the application server. the application server
   // will take ownership of the feature object and destroy it in its
   // destructor
   template <typename T, typename std::enable_if<std::is_base_of<ApplicationFeature, T>::value, int>::type = 0>
   void addFeature(std::unique_ptr<T>&& feature) {
     // TODO TRI_ASSERT(feature->state() == ApplicationFeature::State::UNINITIALIZED);
     _features.emplace(typeid(T), std::move(feature));
   }

   // checks for the existence of a feature by type. will not throw when used
   // for a non-existing feature
   bool hasFeature(std::type_index const& type) const {
     return (_features.find(type) != _features.end());
   }

   // checks for the existence of a feature. will not throw when used for
   // a non-existing feature
   template <typename T, typename std::enable_if<std::is_base_of<ApplicationFeature, T>::value, int>::type = 0>
   bool hasFeature() const {
     return hasFeature(typeid(T));
   }

   // returns a reference to a feature given the type. will throw when used for
   // a non-existing feature
   template <typename T, typename std::enable_if<std::is_base_of<ApplicationFeature, T>::value, int>::type = 0>
   T& getFeature(std::type_index const& type) {
     auto it = _features.find(type);
     if (it == _features.end()) {
       throwFeatureNotFoundException(type.name());
     }
     return *dynamic_cast<T*>(it->second.get());
   }

   // returns a const reference to a feature. will throw when used for
   // a non-existing feature
   template <typename T, typename std::enable_if<std::is_base_of<ApplicationFeature, T>::value, int>::type = 0>
   T const& getFeature() const {
     return getFeature<T>();
   }

   // returns a reference to a feature. will throw when used for
   // a non-existing feature
   template <typename T, typename std::enable_if<std::is_base_of<ApplicationFeature, T>::value, int>::type = 0>
   T& getFeature() {
     auto it = _features.find(typeid(T));
     if (it == _features.end()) {
       throwFeatureNotFoundException(typeid(T).name());
     }
     return *dynamic_cast<T*>(it->second.get());
   }

   // returns the feature with the given name if known and enabled
   // throws otherwise
   template <typename T, typename std::enable_if<std::is_base_of<ApplicationFeature, T>::value, int>::type = 0>
   T& getEnabledFeature() {
     T& feature = getFeature<T>();
     if (!feature.isEnabled()) {
       throwFeatureNotEnabledException(typeid(T).name());
     }
     return feature;
   }

   void disableFeatures(std::vector<std::type_index> const&);
   void forceDisableFeatures(std::vector<std::type_index> const&);

  private:
   // throws an exception that a requested feature was not found
   [[noreturn]] static void throwFeatureNotFoundException(std::string const&);

   // throws an exception that a requested feature is not enabled
   [[noreturn]] static void throwFeatureNotEnabledException(std::string const&);

   void disableFeatures(std::vector<std::type_index> const& types, bool force);

   // walks over all features and runs a callback function for them
   void apply(std::function<void(ApplicationFeature&)>, bool enabledOnly);

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
   static ApplicationServer* INSTANCE;

   // the current state
   std::atomic<State> _state;

   // the shared program options
   std::shared_ptr<options::ProgramOptions> _options;

   // map of feature names to features
   std::unordered_map<std::type_index, std::unique_ptr<ApplicationFeature>> _features;

   // features order for prepare/start
   std::vector<std::reference_wrapper<ApplicationFeature>> _orderedFeatures;

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
