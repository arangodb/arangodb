////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Containers/FlatHashSet.h"

namespace arangodb {
namespace options {
class ProgramOptions;
}
namespace application_features {
class ApplicationServer;

class ApplicationFeature {
  friend class ApplicationServer;

 public:
  ApplicationFeature() = delete;
  ApplicationFeature(ApplicationFeature const&) = delete;
  ApplicationFeature& operator=(ApplicationFeature const&) = delete;

  virtual ~ApplicationFeature() = default;

  enum class State {
    UNINITIALIZED,
    INITIALIZED,
    VALIDATED,
    PREPARED,
    STARTED,
    STOPPED,
    UNPREPARED
  };

  // return the ApplicationServer instance
  ApplicationServer& server() const { return _server; }

  // return the feature's name
  std::string_view name() const noexcept { return _name; }

  bool isOptional() const { return _optional; }
  bool isRequired() const { return !_optional; }

  State state() const { return _state; }

  // whether or not the feature is enabled
  bool isEnabled() const { return _enabled; }

  // enable the feature
  void enable() { setEnabled(true); }

  // disable the feature entirely. if disabled, the feature's options will be
  // ignored and no methods apart from `collectOptions` will be called for the
  // feature
  void disable() { setEnabled(false); }

  // disable the feature, and perform no checks if it's optional
  void forceDisable() { _enabled = false; }

  // enable or disable a feature
  void setEnabled(bool value) {
    if (!value && !isOptional()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          std::string{"cannot disable non-optional feature '"}
              .append(name())
              .append("'"));
    }
    _enabled = value;
  }

  // names of features required to be enabled for this feature to be enabled
  std::vector<size_t> const& dependsOn() const { return _requires; }

  // whether the feature starts before another
  template<typename T, typename Server>
  bool doesStartBefore() const {
    static_assert(std::is_base_of_v<ApplicationFeature, T>);
    static_assert(std::is_base_of_v<ApplicationServer, Server>);

    return doesStartBefore(Server::template id<T>());
  }

  // whether the feature starts after another
  template<typename T, typename Server>
  bool doesStartAfter() const {
    return !doesStartBefore<T, Server>();
  }

  // add the feature's options to the global list of options. this method will
  // be called regardless of whether to feature is enabled or disabled
  virtual void collectOptions(std::shared_ptr<options::ProgramOptions>);

  // load options from somewhere. this method will only be called for enabled
  // features
  virtual void loadOptions(std::shared_ptr<options::ProgramOptions>,
                           char const* binaryPath);

  // validate the feature's options. this method will only be called for active
  // features, after the ApplicationServer has determined which features should
  // be turned off globally. in order to abort further processing in case of
  // invalid parameter values, the feature should bail out by calling
  // FATAL_ERROR_EXIT.
  virtual void validateOptions(std::shared_ptr<options::ProgramOptions>);

  // allows process control
  virtual void daemonize();

  // preparation phase for feature in the preparation phase, the features must
  // not start any threads. furthermore, they must not write any files under
  // elevated privileges if they want other features to access them, or if they
  // want to access these files with dropped privileges
  virtual void prepare();

  // start the feature
  virtual void start();

  // notify the feature about a soft shutdown request
  virtual void initiateSoftShutdown();

  // notify the feature about a shutdown request
  virtual void beginShutdown();

  // stop the feature
  virtual void stop();

  // shut down the feature
  virtual void unprepare();

  // return startup dependencies for feature
  auto const& startsAfter() const { return _startsAfter; }

  // return startup dependencies for feature
  auto const& startsBefore() const { return _startsBefore; }

  size_t registration() const { return _registration; }

 protected:
  ApplicationFeature(ApplicationServer& server, size_t registration,
                     std::string_view name);

  template<typename Server, typename Impl>
  ApplicationFeature(Server& server, const Impl&)
      : ApplicationFeature{server, Server::template id<Impl>(), Impl::name()} {}

  void setOptional() { setOptional(true); }

  // make the feature optional (or not)
  void setOptional(bool value) { _optional = value; }

  // note that this feature requires another to be present
  void dependsOn(size_t other) { _requires.emplace_back(other); }

  // register a start dependency upon another feature
  template<typename T, typename Server>
  void startsAfter() {
    startsAfter(Server::template id<T>());
  }

  void startsAfter(size_t type);

  // register a start dependency upon another feature
  template<typename T, typename Server>
  void startsBefore() {
    startsBefore(Server::template id<T>());
  }

  void startsBefore(size_t type);

  // determine all direct and indirect ancestors of a feature
  auto const& ancestors() const {
    TRI_ASSERT(_ancestorsDetermined);
    return _ancestors;
  }

  template<typename T, typename Server>
  void onlyEnabledWith() {
    _onlyEnabledWith.emplace(Server::template id<T>());
  }

  // return the list of other features that this feature depends on
  auto const& onlyEnabledWith() const { return _onlyEnabledWith; }

 private:
  // whether the feature starts before another
  bool doesStartBefore(size_t type) const;

  void addAncestorToAllInPath(
      std::vector<
          std::pair<size_t, std::reference_wrapper<ApplicationFeature>>>& path,
      size_t ancestorType);

  // set a feature's state. this method should be called by the
  // application server only
  void state(State state) { _state = state; }

  // determine all direct and indirect ancestors of a feature
  void determineAncestors(size_t as);

  // pointer to application server
  ApplicationServer& _server;

  // type registration for lookup within the ApplicationServer
  size_t _registration;

  // name of feature
  std::string_view _name;

  // names of other features required to be enabled if this feature
  // is enabled
  std::vector<size_t> _requires;

  // a list of start dependencies for the feature
  containers::FlatHashSet<size_t> _startsAfter;

  // a list of start dependencies for the feature
  containers::FlatHashSet<size_t> _startsBefore;

  // list of direct and indirect ancestors of the feature
  containers::FlatHashSet<size_t> _ancestors;

  // enable this feature only if the following other features are enabled
  containers::FlatHashSet<size_t> _onlyEnabledWith;

  // state of feature
  State _state;

  // whether or not the feature is enabled
  bool _enabled;

  // whether or not the feature is optional
  bool _optional;

  bool _ancestorsDetermined;
};

template<typename ServerT>
class ApplicationFeatureT : public ApplicationFeature {
 public:
  using Server = ServerT;

  Server& server() const noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto* p = dynamic_cast<Server*>(&ApplicationFeature::server());
    TRI_ASSERT(p);
    return *p;
#else
    return static_cast<Server&>(ApplicationFeature::server());
#endif
  }

  // register a start dependency upon another feature
  template<typename T>
  void startsAfter() {
    ApplicationFeature::startsAfter<T, Server>();
  }

  // register a start dependency upon another feature
  template<typename T>
  void startsBefore() {
    ApplicationFeature::startsBefore<T, Server>();
  }

  template<typename T>
  void onlyEnabledWith() {
    ApplicationFeature::onlyEnabledWith<T, Server>();
  }

  template<typename T>
  bool doesStartBefore() const {
    return ApplicationFeature::doesStartBefore<T, Server>();
  }

  template<typename T>
  bool doesStartAfter() const {
    return ApplicationFeature::doesStartAfter<T, Server>();
  }

 protected:
  template<typename Impl>
  ApplicationFeatureT(Server& server, const Impl&)
      : ApplicationFeatureT(server, Server::template id<Impl>(), Impl::name()) {
  }

  ApplicationFeatureT(Server& server, size_t registration,
                      std::string_view name)
      : ApplicationFeature{server, registration, name} {}
};

}  // namespace application_features
}  // namespace arangodb
