////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#ifndef APPLICATION_FEATURES_BOOTSTRAP_FEATURE_H
#define APPLICATION_FEATURES_BOOTSTRAP_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {

class BootstrapFeature final : public application_features::ApplicationFeature {
 public:
  explicit BootstrapFeature(application_features::ApplicationServer& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void start() override final;
  void unprepare() override final;

  static std::string const& name() noexcept;

  bool isReady() const { return _isReady; }

 private:
  void waitForHealthEntry();
  /// @brief wait for databases to appear in Plan and Current
  void waitForDatabases() const;
 
 private:
  bool _isReady;
  bool _bark;
};

}  // namespace arangodb

#endif
