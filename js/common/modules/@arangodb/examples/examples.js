// //////////////////////////////////////////////////////////////////////////////
// / @brief example-users
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2014 triagens GmbH, Cologne, Germany
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
// / @author Wilfried Goesgens
// / @author Copyright 2018, ArangoDB Inc
// //////////////////////////////////////////////////////////////////////////////

let db = require("internal").db;
let examples = require("@arangodb/graph-examples/example-graph.js");

exports.Examples = {
  'traversalGraph': {
    createDS: function() {
      examples.loadGraph("traversalGraph");
    },
    removeDS: function() {
      examples.dropGraph("traversalGraph");
    }
  },
  'kShortestPathsGraph': {
    createDS: function() {
      examples.loadGraph("kShortestPathsGraph");
    },
    removeDS: function() {
      examples.dropGraph("kShortestPathsGraph");
    }
  },
  'mps_graph': {
    createDS: function() {
      examples.loadGraph("mps_graph");
    },  
    removeDS: function() {
      examples.dropGraph("mps_graph");
    }   
  }, 
  'knows_graph': {
    createDS: function() {
      examples.loadGraph("knows_graph");
    },  
    removeDS: function() {
      examples.dropGraph("knows_graph");
    }   
  },
  'routeplanner': {
    createDS: function() {
      examples.loadGraph("routeplanner");
    },  
    removeDS: function() {
      examples.dropGraph("routeplanner");
    }   
  },
  'joinSampleDataset': {
    createDS: function() {
      db._create("users");
      db._create("relations");
      
      [ [1, "Abigail", true ],
        [2, "Fred", true ],
        [3, "Mary", true ],
        [4, "Mariah", true ],
        [5, "John", false]
      ].forEach(function (v) {
        db.users.save( {
          _key: v[1],
          name: v[1],
          active: v[2],
          userId: v[0]
        });
      });

      [
        [1,2,"friend"],
        [1,3,"friend"],
        [1,4,"friend"],
        [2,5,"friend"],
        [2,2,"friend"],
        [3,4,"friend"],
        [3,1,"friend"],
        [4,1,"friend"],
        [4,2,"friend"]
      ].forEach(function (v) {
        db.relations.save( {
          type: v[2],
          friendOf: v[0],
          thisUser: v[1]
        });
      });
    },
    removeDS: function() {
      try {
        db._drop("users");
      } catch (e) {}
      try {
        db._drop("relations");
      } catch (e) {}
    }
  }
};
