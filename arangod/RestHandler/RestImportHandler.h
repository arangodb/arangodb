////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_REST_HANDLER_REST_IMPORT_HANDLER_H
#define ARANGOD_REST_HANDLER_REST_IMPORT_HANDLER_H 1

#include "Basics/Common.h"
#include "Basics/Result.h"
#include "RestHandler/RestVocbaseBaseHandler.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
struct OperationOptions;
class SingleCollectionTransaction;

struct RestImportResult {
 public:
  RestImportResult()
      : _numErrors(0), _numEmpty(0), _numCreated(0), _numIgnored(0), _numUpdated(0), _errors() {}

  ~RestImportResult() = default;

  size_t _numErrors;
  size_t _numEmpty;
  size_t _numCreated;
  size_t _numIgnored;
  size_t _numUpdated;

  std::vector<std::string> _errors;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief import request handler
////////////////////////////////////////////////////////////////////////////////

class RestImportHandler : public RestVocbaseBaseHandler {
 public:
  explicit RestImportHandler(application_features::ApplicationServer&,
                             GeneralRequest*, GeneralResponse*);

 public:
  RestStatus execute() override final;
  char const* name() const override final { return "RestImportHandler"; }
  RequestLane lane() const override final { return RequestLane::CLIENT_SLOW; }

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief create a position string
  //////////////////////////////////////////////////////////////////////////////

  std::string positionize(size_t) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief register an error
  //////////////////////////////////////////////////////////////////////////////

  void registerError(RestImportResult&, std::string const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief construct an error message
  //////////////////////////////////////////////////////////////////////////////

  std::string buildParseError(size_t, char const*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief process a single VelocyPack document
  //////////////////////////////////////////////////////////////////////////////

  ErrorCode handleSingleDocument(SingleCollectionTransaction& trx,
                                 VPackBuilder& tempBuilder, RestImportResult& result,
                                 arangodb::velocypack::Builder& babies,
                                 arangodb::velocypack::Slice slice,
                                 bool isEdgeCollection, size_t i);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates documents by JSON objects
  /// each line of the input stream contains an individual JSON object
  //////////////////////////////////////////////////////////////////////////////

  bool createFromJson(std::string const&);
  bool createFromVPack(std::string const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates documents by JSON objects
  /// the input stream is one big JSON array containing all documents
  //////////////////////////////////////////////////////////////////////////////

  bool createByDocumentsList();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates a documents from key/value lists
  //////////////////////////////////////////////////////////////////////////////

  bool createFromKeyValueList();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief perform the actual import (insert/update/replace) operations
  //////////////////////////////////////////////////////////////////////////////

  Result performImport(SingleCollectionTransaction& trx, RestImportResult& result,
                       std::string const& collectionName, VPackBuilder const& babies,
                       bool complete, OperationOptions const& opOptions);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates the result
  //////////////////////////////////////////////////////////////////////////////

  void generateDocumentsCreated(RestImportResult const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief parses a string
  //////////////////////////////////////////////////////////////////////////////

  void parseVelocyPackLine(VPackBuilder&, char const*, char const*, bool&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief builds a VPackBuilder object from a key and value list
  //////////////////////////////////////////////////////////////////////////////

  void createVelocyPackObject(VPackBuilder&, arangodb::velocypack::Slice const&,
                              arangodb::velocypack::Slice const&, std::string&, size_t);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks the keys, returns true if all values in the list are
  /// strings.
  //////////////////////////////////////////////////////////////////////////////

  bool checkKeys(arangodb::velocypack::Slice const&) const;

  OperationOptions buildOperationOptions() const;

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief enumeration for unique constraint handling
  //////////////////////////////////////////////////////////////////////////////

  enum OnDuplicateActionType {
    DUPLICATE_ERROR,    // fail on unique constraint violation
    DUPLICATE_UPDATE,   // try updating existing document on unique constraint
                        // violation
    DUPLICATE_REPLACE,  // try replacing existing document on unique constraint
                        // violation
    DUPLICATE_IGNORE    // ignore document on unique constraint violation
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief unique constraint handling
  //////////////////////////////////////////////////////////////////////////////

  OnDuplicateActionType _onDuplicateAction;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief prefixes for edge collections
  //////////////////////////////////////////////////////////////////////////////

  std::string _fromPrefix;
  std::string _toPrefix;

  /// @brief whether or not we will tolerate missing values for the CSV import
  bool _ignoreMissing;
};
}  // namespace arangodb

#endif
