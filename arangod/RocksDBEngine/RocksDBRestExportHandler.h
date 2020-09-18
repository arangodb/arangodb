////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ROCKSDB_REST_EXPORT_HANDLER_H
#define ARANGOD_ROCKSDB_ROCKSDB_REST_EXPORT_HANDLER_H 1

#include "Basics/Common.h"
#include "RestHandler/RestCursorHandler.h"
#include "Utils/CollectionExport.h"

namespace arangodb {
namespace aql {
class QueryRegistry;
}

class RocksDBRestExportHandler : public RestCursorHandler {
 public:
  RocksDBRestExportHandler(application_features::ApplicationServer&,
                           GeneralRequest*, GeneralResponse*, aql::QueryRegistry*);

 public:
  RestStatus execute() override;

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief build options for the query as JSON
  //////////////////////////////////////////////////////////////////////////////

  VPackBuilder buildQueryOptions(std::string const& cname, VPackSlice const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief create an export cursor and return the first results
  //////////////////////////////////////////////////////////////////////////////

  RestStatus createCursor();

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief restrictions for export
  //////////////////////////////////////////////////////////////////////////////

  CollectionExport::Restrictions _restrictions;
};
}  // namespace arangodb

#endif
