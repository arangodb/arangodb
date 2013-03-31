/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global require, exports, module */

////////////////////////////////////////////////////////////////////////////////
/// @brief foxx administration actions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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

var internal = require("internal");

var fs = require("fs");

var arangodb = require("org/arangodb");
var actions = require("org/arangodb/actions");
var foxxManager = require("org/arangodb/foxx-manager");

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief installs a FOXX application
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/foxx/install",
  context : "admin",
  prefix : false,

  callback : function (req, res) {
    'use strict';

    var result;
    var body = actions.getJsonBody(req, res);

    if (body === undefined) {
      return;
    }

    var appId = body.appId;
    var mount = body.mount;
    var options = body.options || {};

    try {
      result = foxxManager.installApp(appId, mount, options);
      actions.resultOk(req, res, actions.HTTP_OK, result);
    }
    catch (err) {
      actions.resultException(req, res, err);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief installs a FOXX application
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/foxx/dev-install",
  context : "admin",
  prefix : false,

  callback : function (req, res) {
    'use strict';

    var result;
    var body = actions.getJsonBody(req, res);

    if (body === undefined) {
      return;
    }

    var name = body.name;
    var mount = body.mount;
    var options = body.options || {};

    try {
      result = foxxManager.installDevApp(name, mount, options);
      actions.resultOk(req, res, actions.HTTP_OK, result);
    }
    catch (err) {
      actions.resultException(req, res, err);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief uninstalls a FOXX application
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/foxx/uninstall",
  context : "admin",
  prefix : false,

  callback : function (req, res) {
    'use strict';

    var result;
    var body = actions.getJsonBody(req, res);

    if (body === undefined) {
      return;
    }

    var key = body.key;

    try {
      result = foxxManager.uninstallApp(key);
      actions.resultOk(req, res, actions.HTTP_OK, result);
    }
    catch (err) {
      actions.resultException(req, res, err);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a FOXX application
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/foxx/load",
  context : "admin",
  prefix : false,

  callback : function (req, res) {
    'use strict';

    var safe = /^[0-9a-zA-Z_\-\.]+$/;

    if (req.suffix.length !== 0) {
      actions.resultBad(req, res, arangodb.ERROR_HTTP_BAD_PARAMETER);
      return;
    }

    var json = actions.getJsonBody(req, res, actions.HTTP_BAD);
  
    if (json === undefined) {
      return;
    }

    var serverFile = json.filename;
    var realFile = fs.join(fs.getTempPath(), serverFile); 

    if (! fs.isFile(realFile)) {
      actions.resultNotFound(req, res, arangodb.errors.ERROR_FILE_NOT_FOUND.code);
      return;
    }

    try {
      var name = json.name;
      var version = json.version;

      if (! safe.test(name)) {
        fs.remove(realFile);
        throw "rejecting unsafe name '" + name + "'";
      }

      if (! safe.test(version)) {
        fs.remove(realFile);
        throw "rejecting unsafe version '" + version + "'";
      }

      var appPath = module.appPath();

      if (typeof appPath === "undefined") {
        fs.remove(realFile);
        throw "javascript.app-path not set, rejecting app loading";
      }

      var path = fs.join(appPath, name + "-" + version);

      if (fs.exists(path)) {
        fs.remove(realFile);
        throw "destination path '" + path + "' already exists";
      }

      fs.makeDirectoryRecursive(path);
      fs.unzipFile(realFile, path, false, true);

      foxxManager.scanAppDirectory();
   
      actions.resultOk(req, res, actions.HTTP_OK, { path: path });
    }
    catch (err) {
      actions.resultException(req, res, err);
    }
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}"
// End:
