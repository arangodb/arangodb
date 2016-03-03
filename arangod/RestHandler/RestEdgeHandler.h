////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REST_HANDLER_REST_EDGE_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_EDGE_HANDLER_H 1

#include "Basics/Common.h"
#include "RestHandler/RestDocumentHandler.h"

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief collection request handler
////////////////////////////////////////////////////////////////////////////////

class RestEdgeHandler : public RestDocumentHandler {
 public:

  explicit RestEdgeHandler(rest::GeneralRequest*);
 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief get collection type
  //////////////////////////////////////////////////////////////////////////////

  TRI_col_type_e getCollectionType() const override final {
    return TRI_COL_TYPE_EDGE;
  }

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates an edge
  //////////////////////////////////////////////////////////////////////////////

  bool createDocument() override final;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates a document (an edge), coordinator case in a cluster
  //////////////////////////////////////////////////////////////////////////////

  bool createDocumentCoordinator(std::string const& collname, bool waitForSync,
                                 VPackSlice const& document, char const* from,
                                 char const* to);
};
}

#endif
