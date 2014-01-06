////////////////////////////////////////////////////////////////////////////////
/// @brief test the cluster info functionality
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
/// @author Jan Steemann
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");

var compareStringIds = function (l, r) {
  if (l.length != r.length) {
    return l.length - r.length < 0 ? -1 : 1;
  }

  // length is equal
  for (i = 0; i < l.length; ++i) {
    if (l[i] != r[i]) {
      return l[i] < r[i] ? -1 : 1;
    }
  }

  return 0;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                      cluster info
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: agency
////////////////////////////////////////////////////////////////////////////////

function ClusterInfoSuite () {
  var ci;

  return {
    setUp : function () {
      ci = new ArangoClusterInfo(); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test uniqid
////////////////////////////////////////////////////////////////////////////////

    testUniqid : function () {
      var last = "0";

      for (var i = 0; i < 1000; ++i) {
        var id = ci.uniqid();
        assertEqual(-1, compareStringIds(last, id));
        last = id;
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test uniqid
////////////////////////////////////////////////////////////////////////////////

    testUniqidRange : function () {
      var last = "0";

      for (var i = 0; i < 100; ++i) {
        var id = ci.uniqid(10);
        assertEqual(-1, compareStringIds(last, id));
        last = id;
      }
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

var agency = new ArangoAgency();
if (agency.isEnabled()) {
  jsunity.run(ClusterInfoSuite);
}

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

