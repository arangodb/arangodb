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

#ifndef ARANGOD_VOC_BASE_COLLECTION_H
#define ARANGOD_VOC_BASE_COLLECTION_H 1

#include "Basics/Common.h"
#include "VocBase/vocbase.h"

/// @brief predefined collection name for users
#define TRI_COL_NAME_USERS "_users"

struct TRI_collection_t {
 public:
  TRI_collection_t(TRI_collection_t const&) = delete;
  TRI_collection_t& operator=(TRI_collection_t const&) = delete;
  TRI_collection_t() = delete;
  
  explicit TRI_collection_t(TRI_vocbase_t* vocbase);
  ~TRI_collection_t();

 public:
  /// @brief create a new collection
  static TRI_collection_t* create(TRI_vocbase_t*, TRI_voc_cid_t);

  /// @brief opens an existing collection
  static TRI_collection_t* open(TRI_vocbase_t*, arangodb::LogicalCollection*, bool);

 /// @brief checks if a collection name is allowed
  /// returns true if the name is allowed and false otherwise
  static bool IsAllowedName(bool isSystem, std::string const& name);
  
 public:
  TRI_vocbase_t* _vocbase;
};

#endif
