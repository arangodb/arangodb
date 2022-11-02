////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "Containers/FlatHashMap.h"
#include "RestServer/arangod.h"
#include "VocBase/LogicalView.h"

namespace arangodb {

// LogicalView factory for both end-user and internal instantiation
struct ViewFactory {
  virtual ~ViewFactory() = default;

  // LogicalView factory for end-user validation instantiation and
  // persistence.
  // Return if success then 'view' is set, else 'view' state is undefined
  virtual Result create(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                        velocypack::Slice definition,
                        bool isUserRequest) const = 0;

  // LogicalView factory for internal instantiation only
  virtual Result instantiate(LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                             velocypack::Slice definition,
                             bool isUserRequest) const = 0;
};

class ViewTypesFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "ViewTypes"; }

  explicit ViewTypesFeature(Server& server);

  // Return 'factory' for 'type' was added successfully
  Result emplace(std::string_view type, ViewFactory const& factory);

  // Return factory for the specified type or a failing placeholder if no such
  // type
  ViewFactory const& factory(std::string_view type) const noexcept;

  void unprepare() override final;

 private:
  containers::FlatHashMap<std::string_view, ViewFactory const*> _factories;
};

}  // namespace arangodb
