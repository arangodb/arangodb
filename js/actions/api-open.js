/*jshint strict: false */
/*global require */

////////////////////////////////////////////////////////////////////////////////
/// @brief open actions
///
/// @file
/// Actions that are mapped under the "_open" path. Allowing to execute the
/// actions without authorization.
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
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var actions = require("org/arangodb/actions");

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief ceberus password manager
////////////////////////////////////////////////////////////////////////////////

actions.defineHttp({
  url: "_open/cerberus",
  prefix : true,

  callback : function (req, res) {
    req.user = null;
    req.database = "_system";

    var suffix = "_system/cerberus";
    suffix = suffix.split("/");
    suffix = suffix.concat(req.suffix);

    req.suffix = suffix;

    actions.routeRequest(req, res);
  }
});

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
