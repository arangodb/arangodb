////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Aql/Query.h"
#include "Aql/QueryOptions.h"
#include "Aql/QueryString.h"
#include "RestServer/arangod.h"
#include "Transaction/Context.h"

namespace arangodb {

class ApiRecordingFeature;

class AqlFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Aql"; }

  explicit AqlFeature(ArangodServer& server);
  ~AqlFeature();

  static bool lease() noexcept;
  static void unlease() noexcept;
  void start() override final;
  void stop() override final;

  ApiRecordingFeature* apiRecordingFeature();

  std::shared_ptr<arangodb::aql::Query> createQuery(
      std::shared_ptr<arangodb::transaction::Context> ctx,
      arangodb::aql::QueryString queryString,
      std::shared_ptr<velocypack::Builder> bindParameters,
      arangodb::aql::QueryOptions options, Scheduler* scheduler);

 private:
  ApiRecordingFeature* _apiRecordingFeature = nullptr;
};

}  // namespace arangodb
