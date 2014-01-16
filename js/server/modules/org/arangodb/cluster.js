/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global ArangoAgency, ArangoServerState, require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript cluster functionality
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

var console = require("console");

var isCluster = function () {
  return (typeof ArangoServerState !== "undefined" &&
          ArangoServerState.initialised());
};

var isCoordinator = function () {
  if (! isCluster()) {
    return false;
  }

  return ArangoServerState.isCoordinator();
};

var role = function () {
  if (! isCluster()) {
    return undefined;
  }

  return ArangoServerState.role();
};

var status = function () {
  if (! isCluster()) {
    return undefined;
  }

  return ArangoServerState.status();
};

var isCoordinatorRequest = function (req) {
  if (! req || ! req.hasOwnProperty("headers")) {
    return false;
  }

  return req.headers.hasOwnProperty("x-arango-coordinator");
};

var handlePlanChange = function () {
  if (! isCluster() || isCoordinator()) {
    return;
  }

  try {
    console.info("%s", "plan change handling successful");
  }
  catch (err) {
    console.error("%s", "plan change handling failed");
  }
};

exports.isCluster            = isCluster;
exports.isCoordinator        = isCoordinator;
exports.role                 = role;
exports.status               = status;
exports.isCoordinatorRequest = isCoordinatorRequest;
exports.handlePlanChange     = handlePlanChange;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:
