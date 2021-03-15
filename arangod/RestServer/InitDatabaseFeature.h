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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef APPLICATION_FEATURES_INIT_DATABASE_FEATURE_H
#define APPLICATION_FEATURES_INIT_DATABASE_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {

class InitDatabaseFeature final : public application_features::ApplicationFeature {
 public:
  InitDatabaseFeature(application_features::ApplicationServer& server,
                      std::vector<std::type_index> const& nonServerFeatures);

  std::string const& defaultPassword() const { return _password; }
  bool isInitDatabase() const { return _initDatabase; }
  bool restoreAdmin() const { return _restoreAdmin; }

 private:
  bool _initDatabase = false;
  bool _restoreAdmin = false;
  std::string _password;

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;

 private:
  void checkEmptyDatabase();
  std::string readPassword(std::string const&);

  bool _seenPassword = false;
  std::vector<std::type_index> _nonServerFeatures;
};

}  // namespace arangodb

#endif