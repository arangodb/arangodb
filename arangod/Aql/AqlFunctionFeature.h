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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Aql/Function.h"

namespace arangodb {

namespace velocypack {
class Builder;
}

namespace aql {

class AqlFunctionFeature final
    : public application_features::ApplicationFeature {
 public:
  explicit AqlFunctionFeature(application_features::ApplicationServer& server);

  void prepare() override final;

  void add(Function const& func);

  // add a function alias
  void addAlias(std::string const& alias, std::string const& original);

  void toVelocyPack(arangodb::velocypack::Builder&) const;
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
