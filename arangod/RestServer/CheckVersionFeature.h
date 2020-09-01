////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef APPLICATION_FEATURES_CHECK_VERSION_FEATURE_H
#define APPLICATION_FEATURES_CHECK_VERSION_FEATURE_H 1

#include <cstdint>
#include "ApplicationFeatures/ApplicationFeature.h"

struct TRI_vocbase_t;

namespace arangodb {

class CheckVersionFeature final : public application_features::ApplicationFeature {
 public:
  explicit CheckVersionFeature(application_features::ApplicationServer& server, int* result,
                               std::vector<std::type_index> const& nonServerFeatures);

 private:
  bool _checkVersion;

 public:
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void start() override final;

 private:
  void checkVersion();

  int* _result;
  std::vector<std::type_index> _nonServerFeatures;
};

}  // namespace arangodb

#endif