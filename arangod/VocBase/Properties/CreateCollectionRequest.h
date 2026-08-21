////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "VocBase/Properties/CollectionCreateOptions.h"
#include "VocBase/Properties/CollectionDescriptor.h"
#include "VocBase/voc-types.h"

#include <string>

namespace arangodb {

template<typename T>
class ResultT;
struct DatabaseConfiguration;

/// One create request: the properties of the collection to create, plus the
/// options that apply only to this call. Both are parsed from the same body.
/// The descriptor is kept by the collection, the options are consumed while
/// creating it and then discarded.
struct CreateCollectionRequest {
  CollectionDescriptor descriptor;
  CollectionCreateOptions options;

  static ResultT<CreateCollectionRequest> fromCreateAPIBody(
      velocypack::Slice input, DatabaseConfiguration const& config,
      bool activateBackwardsCompatibility = true);

  static ResultT<CreateCollectionRequest> fromCreateAPIV8(
      velocypack::Slice properties, std::string const& name,
      TRI_col_type_e type, DatabaseConfiguration const& config);

  static ResultT<CreateCollectionRequest> fromRestoreAPIBody(
      velocypack::Slice input, DatabaseConfiguration const& config);
};

template<class Inspector>
auto inspect(Inspector& f, CreateCollectionRequest& r) {
  return f.object(r).fields(f.embedFields(r.descriptor),
                            f.embedFields(r.options));
}

}  // namespace arangodb
