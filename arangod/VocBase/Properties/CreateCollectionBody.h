////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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

#include "Inspection/Status.h"
#include "Inspection/VPackLoadInspector.h"

#include "VocBase/Properties/UserInputCollectionProperties.h"
#include "VocBase/Properties/CollectionCreateOptions.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>

namespace arangodb {

struct DatabaseConfiguration;
class Result;

template<typename T>
class ResultT;

namespace inspection {
struct Status;
}
namespace velocypack {
class Slice;
}

struct CreateCollectionBody : public UserInputCollectionProperties,
                              public CollectionCreateOptions {
  CreateCollectionBody();

  bool operator==(CreateCollectionBody const& other) const = default;

  static ResultT<CreateCollectionBody> fromCreateAPIBody(
      arangodb::velocypack::Slice input, DatabaseConfiguration const& config);

  static ResultT<CreateCollectionBody> fromCreateAPIV8(
      arangodb::velocypack::Slice properties, std::string const& name,
      TRI_col_type_e type, DatabaseConfiguration const& config);

  static ResultT<CreateCollectionBody> fromRestoreAPIBody(
      arangodb::velocypack::Slice properties,
      DatabaseConfiguration const& config);

  static arangodb::velocypack::Builder toCreateCollectionProperties(
      std::vector<CreateCollectionBody> const& collections);

  // Temporary method to handOver information from
  [[nodiscard]] arangodb::velocypack::Builder toCollectionsCreate() const;
};

template<class Inspector>
auto inspect(Inspector& f, CreateCollectionBody& body) {
  return f.object(body).fields(
      f.template embedFields<UserInputCollectionProperties>(body),
      f.template embedFields<CollectionCreateOptions>(body));
}

}  // namespace arangodb
