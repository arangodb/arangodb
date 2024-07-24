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

#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/ConditionVariable.h"

#include <velocypack/Builder.h>

namespace arangodb {

template<typename T>
struct TypeTag;

namespace options {
class ProgramOptions;
}
namespace application_features {

// the following phases exist:
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
    std::function<void(State, std::string_view featureName)> _feature;
  };

  static std::atomic<bool> CTRL_C;

 public:
  ApplicationServer(std::shared_ptr<options::ProgramOptions>,
                    char const* binaryPath,
                    std::span<std::unique_ptr<ApplicationFeature>> features);

  virtual ~ApplicationServer() = default;

  std::string helpSection() const { return _helpSection; }
  bool helpShown() const { return !_helpSection.empty(); }

  /// @brief stringify the internal state
  std::string_view stringifyState() const;

  /// @brief stringify the given state
  static std::string_view stringifyState(State state);

  /// @brief whether or not the server has made it as least as far as the
  /// IN_START state
  bool isPrepared();

  /// @brief whether or not the server has made it as least as far as the
  /// IN_SHUTDOWN state
  bool isStopping() const;

  /// @brief whether or not state is the shutting down state or further (i.e.
  /// stopped, aborted etc.)
  bool isStoppingState(State state) const;

  // this method will initialize and validate options
  // of all feature, start them and wait for a shutdown
  // signal. after that, it will shutdown all features
  void run(int argc, char* argv[]);

  // signal a soft shutdown (only used for coordinators so far)
  void initiateSoftShutdown();

  // signal the server to shut down
  void beginShutdown();

  // report that we are going down by fatal error
  void shutdownFatalError();

  // return VPack options, with optional filters applied to filter
  // out specific options. the filter function is expected to return true
  // for any options that should become part of the result
  velocypack::Builder options(
      std::function<bool(std::string const&)> const& filter) const;

  // return the program options object
  std::shared_ptr<options::ProgramOptions> options() const { return _options; }

  // return the server state
  TEST_VIRTUAL State state() const {
    return _state.load(std::memory_order_acquire);
  }

  // return the current progress, as string values (phase, feature name)
  std::pair<std::string, std::string> progressInfo() const;

  void addReporter(ProgressHandler reporter) {
    _progressReports.emplace_back(std::move(reporter));
  }

#ifdef ARANGODB_USE_GOOGLE_TESTS
  void setBinaryPath(char const* path) { _binaryPath = path; }
#endif

  char const* getBinaryPath() const { return _binaryPath; }

  void registerStartupCallback(std::function<void()> const& callback) {
    _startupCallbacks.emplace_back(callback);
  }

  void registerFailCallback(
      std::function<void(std::string const&)> const& callback) {
    _fail = callback;
  }

  // setup and validate all feature dependencies, determine feature order
  void setupDependencies(bool failOnMissing);

  std::span<std::reference_wrapper<ApplicationFeature>> getOrderedFeatures() {
    return _orderedFeatures;
  }

#ifdef TEST_VIRTUAL
  void setStateUnsafe(State ss) { _state = ss; }
#endif

  void disableFeatures(std::span<const size_t>);
  void forceDisableFeatures(std::span<const size_t>);

 private:
  friend class ApplicationFeature;

  // set the current progress (string versions)
  void progressInfo(std::string_view phase, std::string_view feature);

  // checks for the existence of a feature by type. will not throw when used
  // for a non-existing feature
  bool hasFeature(size_t type) const noexcept {
    return type < _features.size() && nullptr != _features[type];
  }

  ApplicationFeature& getFeature(size_t type) const {
    if (ADB_LIKELY(hasFeature(type))) {
      return *_features[type];
    }

    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "unknown feature '" + std::to_string(type) + "'");
  }

  void disableFeatures(std::span<const size_t> types, bool force);

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

  void reportServerProgress(State);
  void reportFeatureProgress(State, std::string_view);

 private:
  // the current state
  std::atomic<State> _state;

  // the shared program options
  std::shared_ptr<options::ProgramOptions> _options;

  // application features
  std::span<std::unique_ptr<ApplicationFeature>> _features;

  // features order for prepare/start
  std::vector<std::reference_wrapper<ApplicationFeature>> _orderedFeatures;

  // will be signaled when the application server is asked to shut down
  basics::ConditionVariable _shutdownCondition;

  // reporter for progress
  std::vector<ProgressHandler> _progressReports;

  // mutex for protecting _progressPhase and _progressFeature
  mutable std::mutex _progressMutex;

  // stringified progress value (phase)
  std::string _progressPhase;
  // stringified progress value (feature name)
  std::string _progressFeature;

  // callbacks that are called after start
  std::vector<std::function<void()>> _startupCallbacks;

  // help section displayed
  std::string _helpSection;

  // fail callback
  std::function<void(std::string const&)> _fail;

  // the install directory of this program:
  char const* _binaryPath;

  /// @brief the condition variable protects access to this flag
  /// the flag is set to true when beginShutdown finishes
  bool _abortWaiting = false;

  // whether or not to dump dependencies
  bool _dumpDependencies = false;

  // whether or not to dump configuration options
  bool _dumpOptions = false;
};
/**
// ApplicationServerT is intended to provide statically checked access to
// application features. Whenever you need to create an application server
// consider the following usage pattern:
//
// Declare a list of all features in header file:

namespace arangodb {
class Feature1;
class Feature2;
using namespace arangodb::application_features;
using ServerFeaturesList = basics::TypeList<Feature1, Feature2>;
// struct ServerFeatures is needed to make stacktrace, compile error messages,
// etc more readable.
struct ServerFeatures : ServerFeaturesList {};
using Server = ApplicationServerT<ServerFeatures>;
using ServerFeature = ApplicationFeatureT<ServerFeatures>;
}

// Note that the order of features in basics::TypeList<Feature1, Feature2> is
// significant and defines creation order, i.e. Feature1 is constructed before
// Feature2.
//
// To instantiate server and its features consider the following snippet:

Server server;
server.addFeatures(Visitor{
  []<typename T>(Server& server, TypeTag<T>) {
    return std::make_unique<T>(server);
  },
  [](Server& server, TypeTag<Feature2>) {
    // Feature constructor requires extra argument
    return std::make_unique<Feature2>(server, "arg");
  }});
*/
template<typename Features>
class ApplicationServerT : public ApplicationServer {
 public:
  // Returns feature identifier.
  template<typename T>
  static constexpr size_t id() noexcept {
    return Features::template id<T>();
  }

  // Returns true if a feature denoted by `T` is registered with the server.
  template<typename T>
  static constexpr bool contains() noexcept {
    return Features::template contains<T>();
  }

  // Returns true if a feature denoted by `Feature` is created before other
  // feautures denoted by `OtherFeatures`.
  template<typename Feature, typename... OtherFeatures>
  static constexpr bool isCreatedAfter() noexcept {
    return (std::greater{}(id<Feature>(), id<OtherFeatures>()) && ...);
  }

  ApplicationServerT(std::shared_ptr<arangodb::options::ProgramOptions> opts,
                     char const* binaryPath)
      : ApplicationServer{opts, binaryPath, _features} {}

  // Adds all registered features to the application server.
  template<typename Initializer>
  void addFeatures(Initializer&& initializer) {
    Features::visit([&]<typename T>(TypeTag<T>) {
      static_assert(std::is_base_of_v<ApplicationFeature, T>);
      constexpr auto featureId = Features::template id<T>();

      TRI_ASSERT(!hasFeature<T>());
      _features[featureId] =
          std::forward<Initializer>(initializer)(*this, TypeTag<T>{});
      TRI_ASSERT(hasFeature<T>());
    });
  }

#ifdef ARANGODB_USE_GOOGLE_TESTS
  // Adds a feature to the application server. the application server
  // will take ownership of the feature object and destroy it in its
  // destructor.
  template<typename Type, typename Impl = Type, typename... Args>
  Impl& addFeature(Args&&... args) {
    static_assert(std::is_base_of_v<ApplicationFeature, Type>);
    static_assert(std::is_base_of_v<ApplicationFeature, Impl>);
    static_assert(std::is_base_of_v<Type, Impl>);
    constexpr auto featureId = Features::template id<Type>();

    TRI_ASSERT(!hasFeature<Type>());
    auto& slot = _features[featureId];
    slot = std::make_unique<Impl>(*this, std::forward<Args>(args)...);

    return static_cast<Impl&>(*slot);
  }
#endif

  // Return whether or not a feature is enabled
  // will throw when called for a non-existing feature.
  template<typename T>
  bool isEnabled() const {
    return getFeature<T>().isEnabled();
  }

  // Return whether or not a feature is optional
  // will throw when called for a non-existing feature.
  template<typename T>
  bool isOptional() const {
    return getFeature<T>().isOptional();
  }

  // Return whether or not a feature is required
  // will throw when called for a non-existing feature.
  template<typename T>
  bool isRequired() const {
    return getFeature<T>().isRequired();
  }

  // Checks for the existence of a feature. will not throw when used for
  // a non-existing feature.
  template<typename Type>
  bool hasFeature() const noexcept {
    static_assert(std::is_base_of_v<ApplicationFeature, Type>);

    constexpr auto featureId = Features::template id<Type>();
    return nullptr != _features[featureId];
  }

  // Returns a const reference to a feature. will throw when used for
  // a non-existing feature.
  template<typename Type, typename Impl = Type>
  Impl& getFeature() const {
    static_assert(std::is_base_of_v<ApplicationFeature, Type>);
    static_assert(std::is_base_of_v<Type, Impl> ||
                  std::is_base_of_v<Impl, Type>);
    constexpr auto featureId = Features::template id<Type>();

    TRI_ASSERT(hasFeature<Type>())
        << "Feature missing: " << typeid(Type).name();
    auto& feature = *_features[featureId];
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto obj = dynamic_cast<Impl*>(&feature);
    TRI_ASSERT(obj != nullptr);
    return *obj;
#else
    return static_cast<Impl&>(feature);
#endif
  }

  // Returns the feature with the given name if known and enabled
  // throws otherwise.
  template<typename Type, typename Impl = Type>
  Impl& getEnabledFeature() const {
    auto& feature = getFeature<Type, Impl>();
    if (!feature.isEnabled()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          "feature '" + std::string{feature.name()} + "' is not enabled");
    }
    return feature;
  }

 private:
  std::array<std::unique_ptr<ApplicationFeature>, Features::size()> _features;
};

}  // namespace application_features
}  // namespace arangodb
