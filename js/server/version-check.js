////////////////////////////////////////////////////////////////////////////////
/// @brief version check at the start of the server
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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

// -----------------------------------------------------------------------------
// --SECTION--                                                     version check
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the database
////////////////////////////////////////////////////////////////////////////////

(function() {
  var console = require("console");

  // path to the VERSION file
  var versionFile = DATABASEPATH + "/VERSION";
  var lastVersion = null;

  if (! FS_EXISTS(versionFile)) {
    console.error("No version information file found in database directory.");
    return false;
  }

   // VERSION file exists, read its contents
  var versionInfo = SYS_READ(versionFile);
  if (versionInfo != '') {
    var versionValues = JSON.parse(versionInfo);
    if (versionValues && versionValues.version && ! isNaN(versionValues.version)) {
      lastVersion = parseFloat(versionValues.version);
    }
  }

  if (lastVersion == null) {
    console.error("No version information file found in database directory.");
    return false;
  }

  var currentServerVersion = VERSION.match(/^(\d+\.\d+).*$/);
  if (! currentServerVersion) {
    // server version is invalid for some reason
    console.error("Unexpected arangodb server version found.");
    return false;
  }
  var currentVersion = parseFloat(currentServerVersion[1]);

  if (lastVersion != null && lastVersion > currentVersion) {
    // downgrade??
    console.warn("Database directory version is higher than server version. This seems like you are running arangodb on a database directory that was created with a newer version. This is not supported.");
    // still, allow the start
  }

  return true;
})();

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
