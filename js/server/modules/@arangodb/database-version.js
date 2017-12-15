/* jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief database version check
// /
// / @file
// /
// / Checks if the database needs to be upgraded.
// /
// / DISCLAIMER
// /
// / Copyright 2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Dr. Frank Celler
// / @author Copyright 2014, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var cluster = require('@arangodb/cluster');
var db = require('@arangodb').db;
var console = require('console');

// //////////////////////////////////////////////////////////////////////////////
// / @brief CURRENT_VERSION
// //////////////////////////////////////////////////////////////////////////////

exports.CURRENT_VERSION = (function () {
  var v = db._version().replace(/-[a-zA-Z0-9_\-]*$/g, '').split('.');

  var major = parseFloat(v[0], 10) || 0;
  var minor = parseFloat(v[1], 10) || 0;
  var patch = parseFloat(v[2], 10) || 0;

  return (((major * 100) + minor) * 100) + patch;
}());
