/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global ArangoAgency, ArangoClusterComm, ArangoClusterInfo, ArangoServerState, require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief General unittest framework
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
/// @author Max Neunhoeffer
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_UnitTest
/// @brief framework to perform unittests
///
/// This function gets one or two arguments, the first describes which tests
/// to perform and the second is an options object. For `which` the following
/// values are allowed:
///
///   - "all": do all tests
///   - "make"
///   - "config"
///   - "boost"
///   - "shell_server"
///   - "shell_server_ahuacatl"
///   - "http_server"
///   - "ssl_server"
///   - "shell_client"
///   - "dump"
///   - "arangob"
///   - "import"
///   - "upgrade"
///   - "dfdb"
///   - "foxx-manager"
///   - "authentication"
///   - "authentication-parameters
///
/// The following properties of `options` are defined:
///
///   - `force`: if set to true the tests are continued even if one fails
///   - `skipBoost`: if set to true the boost unittests are skipped
///   - `skipGeo`: if set to true the geo index tests are skipped
///   - `skipAhuacatl`: if set to true the ahuacatl tests are skipped
///   - `skipRanges`: if set to true the ranges tests are skipped
///   - `valgrind`: if set to true the arangods are run with the valgrind
///     memory checker
///   - `cluster`: if set to true the tests are run with the coordinator
///     of a small local cluster
////////////////////////////////////////////////////////////////////////////////

var _ = require("underscore");

var testFuncs = {};
var print = require("internal").print;

function startInstance (cluster) {
  // to be done
  return "tcp://localhost:PORT";
}

function shutdownInstance (cluster) {
  // to be done
}

testFuncs.shell_server = function (options) {
  print('Doing tests "shell_server"...');
  return "OK";
};

var optionsDefaults = { "cluster": false,
                        "valgrind": false,
                        "force": false,
                        "skipBoost": false,
                        "skipGeo": false,
                        "skipAhuacatl": false,
                        "skipRanges": false };

function UnitTest (which, options) {
  if (typeof options !== "object") {
    options = {};
  }
  _.defaults(options, optionsDefaults);
  if (which === undefined) {
    print("Usage: UnitTest( which, options )");
    print('       where "which" is one of:');
    print('         "all": do all tests');
    print('         "make"');
    print('         "config"');
    print('         "boost"');
    print('         "shell_server"');
    print('         "shell_server_ahuacatl"');
    print('         "http_server"');
    print('         "ssl_server"');
    print('         "shell_client"');
    print('         "dump"');
    print('         "arangob"');
    print('         "import"');
    print('         "upgrade"');
    print('         "dfdb"');
    print('         "foxx-manager"');
    print('         "authentication"');
    print('         "authentication-parameters');
    print('       and options can contain the following boolean properties:');
    print('         "force": continue despite a failed test'); 
    print('         "skipBoost": skip the boost unittests');
    print('         "skipGeo": skip the geo index tests');
    print('         "skipAhuacatl": skip the ahuacatl tests');
    print('         "skipRanges": skip the ranges tests');
    print('         "valgrind": arangods are run with valgrind');
    print('         "cluster": tests are run on a small local cluster');
    return;
  }
  if (which === "all") {
    var n;
    var results = {};
    for (n in testFuncs) {
      if (testFuncs.hasOwnProperty(n)) {
        results[n] = testFuncs[n](options);
      }
    }
  }
  else {
    if (!testFuncs.hasOwnProperty(which)) {
      throw 'Unknown test "'+which+'"';
    }
    return testFuncs[which](options);
  }
}

exports.UnitTest = UnitTest;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:
