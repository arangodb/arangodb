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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Aql/Function.h"
#include "RestServer/arangod.h"

namespace arangodb {

namespace velocypack {
class Builder;
}

namespace aql {

class AqlFunctionFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "AQLFunctions"; }

  explicit AqlFunctionFeature(Server& server);

  void prepare() override final;

  void add(Function const& func);

  // add a function alias
  void addAlias(std::string const& alias, std::string const& original);

  void toVelocyPack(arangodb::velocypack::Builder& builder) const;
  Function const* byName(std::string const& name) const;

  bool exists(std::string const& name) const;

 private:
  // Internal functions
  void addTypeCheckFunctions();
  void addTypeCastFunctions();
  void addStringFunctions();
  void addNumericFunctions();
  void addListFunctions();
  void addDocumentFunctions();
  void addGeoFunctions();
  void addGeometryConstructors();
  void addDateFunctions();
  void addMiscFunctions();

  /// @brief AQL user-callable function names
  std::unordered_map<std::string, Function const> _functionNames;
};

}  // namespace aql
}  // namespace arangodb
