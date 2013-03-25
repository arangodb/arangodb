////////////////////////////////////////////////////////////////////////////////
/// @brief actions for arango application launcher
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var arangodb = require("org/arangodb");
var actions = require("org/arangodb/actions");
var ArangoError = require("org/arangodb/arango-error").ArangoError; 
var internal = require("internal");
var fs = require("fs");
var db = internal.db;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

function GetStorage () {
  return db._collection('_aal');
}

function BuildPath (name, version) {
  if (internal.APP_PATH.length == 0) {
    throw "no app-path set";
  }
  return fs.join(internal.APP_PATH[0], name.replace(/\./g, '_'), version.replace(/\./g, '_')); 
}

function ValidateName (name) {
  if (! /^[0-9a-zA-Z_\-\.]+$/.test(name)) {
    throw "invalid application name";
  }
}

function ValidateVersion (version) {
  if (! /^[0-9a-zA-Z_\-\.]+$/.test(version)) {
    throw "invalid application version";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unpacks an aal application
///
/// @RESTHEADER{POST /_api/aal,unpacks an aal application}
///
/// @REST{POST /_api/aal}
////////////////////////////////////////////////////////////////////////////////

function POST_api_aal (req, res) {
  if (req.suffix.length != 0) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var json = actions.getJsonBody(req, res, actions.HTTP_BAD);
  
  if (json === undefined) {
    return;
  }

  var name = json.name;
  var version = json.version;

  ValidateName(name);
  ValidateVersion(version);
 
  var serverFile = json.filename;
  var realFile = fs.join(internal.getTempPath(), serverFile); 

  if (! fs.isFile(realFile)) {
    actions.resultNotFound(req, res, arangodb.errors.ERROR_FILE_NOT_FOUND.code);
    return;
  } 

  var path = BuildPath(name, version);
  fs.createDirectory(path, true);
  internal.unzipFile(realFile, path, false, true);

  var storage = GetStorage();
  var previous = storage.firstExample({ name: name, version: version });
  if (previous !== null) {
    storage.remove(previous._id);
  }

  storage.save({ 
    name: name, 
    version: version,
    active: false
  });
   
  actions.resultOk(req, res, actions.HTTP_OK, { });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief mount an aal app
///
/// @RESTHEADER{PUT /_api/aal,mounts an aal app}
///
/// @REST{PUT /_api/aal/@FA{name}/@FA{version}}
////////////////////////////////////////////////////////////////////////////////

function PUT_api_aal (req, res) {
  if (req.suffix.length != 0) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var json = actions.getJsonBody(req, res, actions.HTTP_BAD);
  
  if (json === undefined) {
    return;
  }
  
  var name = json.name;
  var version = json.version;
  var path = json.path;
  var dbPrefix = json.dbPrefix;
  
  ValidateName(name);
  ValidateVersion(version);
  
  var installPath = BuildPath(name, version);
  var storage = GetStorage();
  var app = storage.firstExample({ name: name, version: version });

  if (app === null) {
    actions.resultNotFound(req, res, arangodb.errors.ERROR_APPLICATION_NOT_FOUND.code);
    return;
  }

  storage.remove(app._id);

  storage.save({ 
    name: name,
    version: version,
    active: true,
    installPath: installPath, 
    path: path,
    dbPrefix: dbPrefix
  });
  
  actions.resultOk(req, res, actions.HTTP_OK, { });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove an application
///
/// @RESTHEADER{DELETE /_api/aal,removes an application}
///
/// @REST{DELETE /_api/aal/@FA{name}/@FA{version}}
///
/// Removes an existing application, identified by @FA{name} and @FA{version}.
////////////////////////////////////////////////////////////////////////////////

function DELETE_api_aal (req, res) {
  if (req.suffix.length != 2) {
    actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
    return;
  }

  var name = decodeURIComponent(req.suffix[0]);
  var version = decodeURIComponent(req.suffix[1]);
  
  ValidateName(name);
  ValidateVersion(version);

  var storage = GetStorage();
  var app = storage.firstExample({ name: name, version: version });

  if (app === null) {
    actions.resultNotFound(req, res, arangodb.errors.ERROR_APPLICATION_NOT_FOUND.code);
    return;
  }

  storage.remove(app._id);
  DeleteDirectory(BuildPath(name, version));

  actions.resultOk(req, res, actions.HTTP_OK, { });
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       initialiser
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief aal actions gateway 
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_api/aal",
  context : "api",

  callback : function (req, res) {
    try {
      switch (req.requestType) {
        case actions.POST: 
          POST_api_aal(req, res); 
          break;

        case actions.PUT:  
          PUT_api_aal(req, res); 
          break;
        
        case actions.DELETE:  
          DELETE_api_aal(req, res); 
          break;

        default:
          actions.resultUnsupported(req, res);
      }
    }
    catch (err) {
      actions.resultException(req, res, err);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
