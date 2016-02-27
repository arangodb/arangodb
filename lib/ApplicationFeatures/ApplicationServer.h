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

#ifndef APPLICATION_FEATURES_APPLICATION_SERVER_H
#define APPLICATION_FEATURES_APPLICATION_SERVER_H 1

#include "Basics/Common.h"
#include <functional>

namespace arangodb {
namespace options {
class ProgramOptions;
}

namespace application_features {
class ApplicationFeature;

class ApplicationServer {
  ApplicationServer(ApplicationServer const&) = delete;
  ApplicationServer& operator=(ApplicationServer const&) = delete;

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
  void run();

  // signal the server to shut down
  void beginShutdown();

 private:
  // fail and abort with the specified message
  void fail(std::string const& message);

  // walks over all features and runs a callback function for them
  void apply(std::function<void(ApplicationFeature*)>, bool enabledOnly);

  // collects the program options from all features,
  // without validating them
  void collectOptions();
  // allows features to cross-validate their program options
  void validateOptions();
  // enable automatic features
  void enableAutomaticFeatures();
  // setup and validate all feature dependencies, determine feature order
  void setupDependencies();
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
};
}
}

#endif
