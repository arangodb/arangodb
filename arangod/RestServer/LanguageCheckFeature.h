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
/// @author Daniel Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_REST_SERVER_LANGUAGE_CHECK_FEATURE_H
#define ARANGODB_REST_SERVER_LANGUAGE_CHECK_FEATURE_H 1

#include <unicode/locid.h>
#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {

class LanguageCheckFeature final : public application_features::ApplicationFeature {
 public:
  explicit LanguageCheckFeature(application_features::ApplicationServer& server);
  ~LanguageCheckFeature();

  static std::string const& name() noexcept;
  void start() override final;
};

}  // namespace arangodb

#endif