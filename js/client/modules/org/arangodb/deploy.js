/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief deployment tools
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var arangodb = require("org/arangodb");

// -----------------------------------------------------------------------------
// --SECTION--                                                         ArangoApp
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDeployment
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief application
////////////////////////////////////////////////////////////////////////////////

function ArangoApp (routing, description) {
  this._routing = routing;
  this._description = description;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDeployment
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief mounts a bunch of static pages
////////////////////////////////////////////////////////////////////////////////

ArangoApp.prototype.mountStaticPages = function (url, collection) {
};

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new app
////////////////////////////////////////////////////////////////////////////////

exports.createApp = function (name) {
  var routing = arangodb.db._collection("_routing");
  var doc = routing.firstExample({ application: name });

  if (doc !== null) {
    var error = new arangodb.ArangoError();
    error.errorNum = arangodb.ERROR_ARANGO_DUPLICATE_IDENTIFIER;
    error.errorMessage = "application name must be unique";

    throw error;
  }

  doc = routing.save({ application: name,
		       urlPrefix: "",
		       routes: [],
		       middleware: [] });

  return new ArangoApp(routing, doc);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief loads an existing app
////////////////////////////////////////////////////////////////////////////////

exports.readApp = function (name) {
  var routing = arangodb.db._collection("_routing");
  var doc = routing.firstExample({ application: name });

  if (doc === null) {
    var error = new arangodb.ArangoError();
    error.errorNum = arangodb.ERROR_ARANGO_DOCUMENT_NOT_FOUND;
    error.errorMessage = "application unknown";

    throw error;
  }

  return new ArangoApp(routing, doc);
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:
