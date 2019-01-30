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

#ifndef ARANGODB_REST_SERVER_VIEW_TYPES_FEATURE_H
#define ARANGODB_REST_SERVER_VIEW_TYPES_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "VocBase/ViewImplementation.h"

namespace arangodb {
class ViewTypesFeature final : public application_features::ApplicationFeature {
 public:
  explicit ViewTypesFeature(application_features::ApplicationServer* server);

 public:
  void prepare() override final;
  void unprepare() override final;

  static void registerViewImplementation(std::string const& type, ViewCreator creator);

  bool isValidType(std::string const& type) const;

  ViewCreator& creator(std::string const& type) const;

 private:
  static std::unordered_map<std::string, arangodb::ViewCreator> _viewCreators;
};
}  // namespace arangodb

#endif
