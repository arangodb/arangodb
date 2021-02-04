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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_REST_SERVER_VIEW_TYPES_FEATURE_H
#define ARANGODB_REST_SERVER_VIEW_TYPES_FEATURE_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "VocBase/LogicalView.h"

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief LogicalView factory for both end-user and internal instantiation
////////////////////////////////////////////////////////////////////////////////
struct ViewFactory {
  virtual ~ViewFactory() = default;  // define to silence warning

  //////////////////////////////////////////////////////////////////////////////
  /// @brief LogicalView factory for end-user validation instantiation and
  ///        persistence
  /// @return if success then 'view' is set, else 'view' state is undefined
  //////////////////////////////////////////////////////////////////////////////
  virtual Result create(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                        velocypack::Slice const& definition) const = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief LogicalView factory for internal instantiation only
  //////////////////////////////////////////////////////////////////////////////
  virtual Result instantiate(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                             velocypack::Slice const& definition) const = 0;
};

class ViewTypesFeature final : public application_features::ApplicationFeature {
 public:
  explicit ViewTypesFeature(application_features::ApplicationServer& server);

  /// @return 'factory' for 'type' was added successfully
  Result emplace(LogicalDataSource::Type const& type, ViewFactory const& factory);

  /// @return factory for the specified type or a failing placeholder if no such
  /// type
  ViewFactory const& factory(LogicalDataSource::Type const& type) const noexcept;

  static std::string const& name();
  void prepare() override final;
  void unprepare() override final;

 private:
  std::unordered_map<LogicalDataSource::Type const*, ViewFactory const*> _factories;
};

}  // namespace arangodb

#endif
