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

#ifndef ARANGODB_APPLICATION_FEATURES_APPLICATION_FEATURE_H
#define ARANGODB_APPLICATION_FEATURES_APPLICATION_FEATURE_H 1

#include "Basics/Common.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"

namespace arangodb {
namespace application_features {

class ApplicationFeature {
  friend class ApplicationServer;

 public:
  ApplicationFeature() = delete;
  ApplicationFeature(ApplicationFeature const&) = delete;
  ApplicationFeature& operator=(ApplicationFeature const&) = delete;

  ApplicationFeature(ApplicationServer*, std::string const&);

  virtual ~ApplicationFeature();

 public:
  // return the feature's name
  std::string const& name() const { return _name; }

  bool isOptional() const { return _optional; }
  bool isRequired() const { return !_optional; }

  ApplicationServer::FeatureState state() const { return _state; }

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
          "cannot disable non-optional feature '" + name() + "'");
    }
    _enabled = value;
  }

  // names of features required to be enabled for this feature to be enabled
  std::vector<std::string> const& requires() const { return _requires; }

  // register whether the feature requires elevated privileges
  void requiresElevatedPrivileges(bool value) {
    _requiresElevatedPrivileges = value;
  }

  // test whether the feature requires elevated privileges
  bool requiresElevatedPrivileges() const {
    return _requiresElevatedPrivileges;
  }

  // whether the feature starts before another
  bool doesStartBefore(std::string const& other) const;
  
  // whether the feature starts after another
  bool doesStartAfter(std::string const& other) const {
    return !doesStartBefore(other);
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

  // notify the feature about a shutdown request
  virtual void beginShutdown();

  // stop the feature
  virtual void stop();

  // shut down the feature
  virtual void unprepare();
  
  // return startup dependencies for feature
  std::unordered_set<std::string> const& startsAfter() const {
    return _startsAfter;
  }

  // return startup dependencies for feature
  std::unordered_set<std::string> const& startsBefore() const {
    return _startsBefore;
  }

 protected:
  // return the ApplicationServer instance
  ApplicationServer* server() const { return _server; }

  void setOptional() { setOptional(true); }

  // make the feature optional (or not)
  void setOptional(bool value) { _optional = value; }

  // note that this feature requires another to be present
  void requires(std::string const& other) { _requires.emplace_back(other); }

  // register a start dependency upon another feature
  void startsAfter(std::string const& other) { _startsAfter.emplace(other); }

  // register a start dependency upon another feature
  void startsBefore(std::string const& other) { _startsBefore.emplace(other); }
  
  // determine all direct and indirect ancestors of a feature
  std::unordered_set<std::string> ancestors() const;

  void onlyEnabledWith(std::string const& other) { _onlyEnabledWith.emplace(other); }
  
  // return the list of other features that this feature depends on
  std::unordered_set<std::string> const& onlyEnabledWith() const {
    return _onlyEnabledWith;
  }

 private:
  // set a feature's state. this method should be called by the
  // application server only
  void state(ApplicationServer::FeatureState state) {
    _state = state;
  }

  // determine all direct and indirect ancestors of a feature
  void determineAncestors();

 private:
  // pointer to application server
  ApplicationServer* _server;

  // name of feature
  std::string const _name;

  // names of other features required to be enabled if this feature
  // is enabled
  std::vector<std::string> _requires;

  // a list of start dependencies for the feature
  std::unordered_set<std::string> _startsAfter;

  // a list of start dependencies for the feature
  std::unordered_set<std::string> _startsBefore;

  // list of direct and indirect ancestors of the feature
  std::unordered_set<std::string> _ancestors;
  
  // enable this feature only if the following other features are enabled
  std::unordered_set<std::string> _onlyEnabledWith;

  // state of feature
  ApplicationServer::FeatureState _state;

  // whether or not the feature is enabled
  bool _enabled;

  // whether or not the feature is optional
  bool _optional;

  // whether or not the feature requires elevated privileges
  bool _requiresElevatedPrivileges;

  bool _ancestorsDetermined;
};
}
}

#endif
