////////////////////////////////////////////////////////////////////////////////
/// @brief test the cluster helper functions
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

var cluster = require("org/arangodb/cluster");
var jsunity = require("jsunity");

// -----------------------------------------------------------------------------
// --SECTION--                                                           cluster
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: cluster enabled
////////////////////////////////////////////////////////////////////////////////

function ClusterEnabledSuite () {
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test isCluster
////////////////////////////////////////////////////////////////////////////////

    testIsCluster : function () {
      assertTrue(cluster.isCluster());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test role
////////////////////////////////////////////////////////////////////////////////
    
    testRole : function () {
      assertTrue(cluster.role() !== undefined);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test status
////////////////////////////////////////////////////////////////////////////////
    
    testStatus : function () {
      assertTrue(cluster.status() !== undefined);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test isCoordinatorRequest
////////////////////////////////////////////////////////////////////////////////
    
    testIsCoordinatorRequest : function () {
      assertFalse(cluster.isCoordinatorRequest(null));
      assertFalse(cluster.isCoordinatorRequest(undefined));
      assertFalse(cluster.isCoordinatorRequest({ }));
      assertFalse(cluster.isCoordinatorRequest({ headers: { } }));
      assertFalse(cluster.isCoordinatorRequest({ headers: { test: "" } }));
      assertTrue(cluster.isCoordinatorRequest({ headers: { "x-arango-coordinator": "abc" } }));
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: cluster disabled
////////////////////////////////////////////////////////////////////////////////

function ClusterDisabledSuite () {
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test isCluster
////////////////////////////////////////////////////////////////////////////////

    testIsCluster : function () {
      assertFalse(cluster.isCluster());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test role
////////////////////////////////////////////////////////////////////////////////
    
    testRole : function () {
      assertTrue(cluster.role() === undefined);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test status
////////////////////////////////////////////////////////////////////////////////
    
    testStatus : function () {
      assertTrue(cluster.status() === undefined);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test isCoordinatorRequest
////////////////////////////////////////////////////////////////////////////////
    
    testIsCoordinatorRequest : function () {
      assertFalse(cluster.isCoordinatorRequest(null));
      assertFalse(cluster.isCoordinatorRequest(undefined));
      assertFalse(cluster.isCoordinatorRequest({ }));
      assertFalse(cluster.isCoordinatorRequest({ headers: { } }));
      assertFalse(cluster.isCoordinatorRequest({ headers: { test: "" } }));
      assertTrue(cluster.isCoordinatorRequest({ headers: { "x-arango-coordinator": "abc" } }));
    }
  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

if (cluster.isCluster()) {
  jsunity.run(ClusterEnabledSuite);
}
else {
  jsunity.run(ClusterDisabledSuite);
}

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

