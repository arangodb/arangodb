/*jshint strict: false */
/*global require, module */

////////////////////////////////////////////////////////////////////////////////
/// @brief foxx administration actions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");

var fs = require("fs");

var arangodb = require("org/arangodb");
var actions = require("org/arangodb/actions");
var foxxManager = require("org/arangodb/foxx/manager");

var easyPostCallback = actions.easyPostCallback;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up a Foxx application
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/foxx/setup",
  prefix : false,

  callback: easyPostCallback({
    body: true,
    callback: function (body) {
      var mount = body.mount;

      return foxxManager.setup(mount);
    }
  })
});

////////////////////////////////////////////////////////////////////////////////
/// @brief tears down a Foxx application
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/foxx/teardown",
  prefix : false,

  callback: easyPostCallback({
    body: true,
    callback: function (body) {
      var mount = body.mount;

      return foxxManager.teardown(mount);
    }
  })
});

////////////////////////////////////////////////////////////////////////////////
/// @brief installs a Foxx application
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/foxx/install",
  prefix : false,

  callback: easyPostCallback({
    body: true,
    callback: function (body) {
      var appInfo = body.appInfo;
      var mount = body.mount;
      var options = body.options;

      return foxxManager.install(appInfo, mount, options);
    }
  })
});

////////////////////////////////////////////////////////////////////////////////
/// @brief uninstalls a Foxx application
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/foxx/uninstall",
  prefix : false,

  callback: easyPostCallback({
    body: true,
    callback: function (body) {
      var mount = body.mount;

      return foxxManager.uninstall(mount);
    }
  })
});

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces a Foxx application
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/foxx/replace",
  prefix : false,

  callback: easyPostCallback({
    body: true,
    callback: function (body) {
      var appInfo = body.appInfo;
      var mount = body.mount;
      var options = body.options;

      return foxxManager.replace(appInfo, mount, options);
    }
  })
});

////////////////////////////////////////////////////////////////////////////////
/// @brief upgrades a Foxx application
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url : "_admin/foxx/upgrade",
  prefix : false,

  callback: easyPostCallback({
    body: true,
    callback: function (body) {
      var appInfo = body.appInfo;
      var mount = body.mount;
      var options = body.options;

      return foxxManager.upgrade(appInfo, mount, options);
    }
  })
});

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
