/*global require, exports, module */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript base module
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

module.isSystem = true;

var common = require("org/arangodb-common");

Object.keys(common).forEach(function (key) {
  exports[key] = common[key];
});

var internal = require("internal"); // OK: db

var ShapedJson = require("org/arangodb/shaped-json").ShapedJson;

// -----------------------------------------------------------------------------
// --SECTION--                                                 module "arangodb"
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief class "ArangoCollection"
////////////////////////////////////////////////////////////////////////////////

// cannot yet not use arangodb
exports.ArangoCollection = require("org/arangodb/arango-collection").ArangoCollection;

////////////////////////////////////////////////////////////////////////////////
/// @brief class "ArangoDatabase"
////////////////////////////////////////////////////////////////////////////////

// cannot yet not use arangodb
exports.ArangoDatabase = require("org/arangodb/arango-database").ArangoDatabase;

////////////////////////////////////////////////////////////////////////////////
/// @brief class "ArangoStatement"
////////////////////////////////////////////////////////////////////////////////

// cannot yet not use arangodb
exports.ArangoStatement = require("org/arangodb/arango-statement").ArangoStatement;

////////////////////////////////////////////////////////////////////////////////
/// @brief class "ShapedJson"
////////////////////////////////////////////////////////////////////////////////

exports.ShapedJson = ShapedJson;

////////////////////////////////////////////////////////////////////////////////
/// @brief the global db object
////////////////////////////////////////////////////////////////////////////////

exports.db = internal.db;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:
