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
#include "VocBase/LogicalView.h"

namespace arangodb {

class ViewTypesFeature final: public application_features::ApplicationFeature {
 public:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief typedef for a LogicalView factory function
  /// This typedef is used when registering the factory function for any view
  /// type. the creator function is called when a view is first created or
  /// re-opened after a server restart. the VelocyPack Slice will contain all
  /// information about the view's general and implementation-specific properties.
  /// @param preCommit called before completing view creation (IFF returns true)
  ///                  e.g. before persisting definition to filesystem
  ///                  IFF preCommit == false then skip invocation
  //////////////////////////////////////////////////////////////////////////////
  typedef std::function<std::shared_ptr<LogicalView>(
    TRI_vocbase_t& vocbase, // database
    velocypack::Slice const& definition, // view definition
    bool isNew, // new view mark
    uint64_t planVersion, // cluster plan version ('0' by default for non-cluster)
    LogicalView::PreCommitCallback const& preCommit
  )> ViewFactory;

  explicit ViewTypesFeature(application_features::ApplicationServer& server);

  /// @return 'factory' for 'type' was added successfully
  arangodb::Result emplace(
    LogicalDataSource::Type const& type,
    ViewFactory const& factory
  );

  /// @return factory for the specified type or false if no such type
  ViewFactory const& factory(
    LogicalDataSource::Type const& type
  ) const noexcept;

  static std::string const& name();
  void prepare() override final;
  void unprepare() override final;

 private:
  std::unordered_map<LogicalDataSource::Type const*, ViewFactory> _factories;
};

} // arangodb

#endif