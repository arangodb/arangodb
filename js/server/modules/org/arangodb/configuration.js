/*jshint strict: false */
/*global require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief global configurations
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 triagens GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler, Lucas Dohmen
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var db = require("org/arangodb").db;
var internal = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                               module "org/arangodb/configuration"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief the configuration collection
////////////////////////////////////////////////////////////////////////////////

function getConfigurationCollection () {
  var configuration = db._collection("_configuration");

  if (configuration === null) {
    throw new Error("_configuration collection not (yet) available");
  }
  return configuration;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief the notification configuration
////////////////////////////////////////////////////////////////////////////////

exports.notifications = {};

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the versions notification configuration
////////////////////////////////////////////////////////////////////////////////

exports.notifications.versions = function () {
  var n = "notifications";
  var v = "versions";
  var d;
  var configuration = getConfigurationCollection();

  try {
    d = configuration.document(n);
  }
  catch (err) {
    try {
      d = configuration.save({ _key: n });
    }
    catch (err2) {
      d = {};
    }
  }

  d = d._shallowCopy;

  if (! d.hasOwnProperty(v)) {
    d.versions = {};
  }

  if (! internal.frontendVersionCheck) {
    d.enableVersionNotification = false;
  }

  return d;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the versions notification collections
////////////////////////////////////////////////////////////////////////////////

exports.notifications.setVersions = function (data) {
  var n = "notifications";
  var d;
  var configuration = getConfigurationCollection();

  try {
    d = configuration.document(n);
  }
  catch (err) {
    d = configuration.save({ _key: n });
  }

  configuration.update(n, data);
};

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
