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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef APPLICATION_FEATURES_DATABASE_PATH_FEATURE_H
#define APPLICATION_FEATURES_DATABASE_PATH_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {

class DatabasePathFeature final : public application_features::ApplicationFeature {
 public:
  explicit DatabasePathFeature(application_features::ApplicationServer& server);

  static constexpr const char* name() { return "DatabasePath"; }

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;

  std::string const& directory() const { return _directory; }
  std::string subdirectoryName(std::string const& subDirectory) const;
  void setDirectory(std::string const& path) {
    // This is only needed in the catch tests, where we initialize the
    // feature but do not have options or run `validateOptions`. Please
    // do not use it from other code.
    _directory = path;
  }

 private:
  std::string _directory;
  std::string _requiredDirectoryState;
};

}  // namespace arangodb

#endif
