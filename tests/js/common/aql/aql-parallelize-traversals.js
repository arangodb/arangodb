/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertEqual, assertNotEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for traversal parallelization
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

let db = require("@arangodb").db;
let internal = require("internal");
let jsunity = require("jsunity");
  
const vn = "UnitTestsAqlOptimizerRule";
const en = "UnitTestsAqlOptimizerRuleEdges";

const graphSize = 5000;
const connections = 4;

let insertVertices = function(n) {
  let docs = [];
  for (let i = 0; i < n; ++i) {
    docs.push({ _key: "test" + i, value: i });
    if (docs.length === 5000) {
      db[vn].insert(docs);
      docs = [];
    }
  }
  if (docs.length) {
    db[vn].insert(docs);
  }
};
let insertEdges = function(n, c) {
  let docs = [];
  for (let i = 0; i < n; ++i) {
    for (let j = i + 1; j < i + c; ++j) {
      if (j < n) {
        docs.push({ _from: vn + "/test" + i, _to: vn + "/test" + j });
        if (docs.length === 5000) {
          db[en].insert(docs);
          docs = [];
        }
      }
    }
  }
  if (docs.length) {
    db[en].insert(docs);
  }
};
let buildStartNodes = function(l, r) {
  let result = [];
  while (l <= r) {
    result.push(l++);
  }
  return result;
};
let buildNestedResult = function(n, c, startNodes, maxDepth, minDepth = 0) {
  let result = [];
  for (let start of startNodes) {
    let build = function(startNode, depth) {
      for (let j = startNode + 1; j < startNode + c; ++j) {
        if (j < n) {
          if (depth > minDepth) {
            result.push({ level: depth, value: j });
          }
          if (depth < maxDepth) {
            build(j, depth + 1);
          }
        }
      }
    };
    build(start, 1);
  }
  result.sort(function(l, r) {
    if (l.level !== r.level) {
      return l.level - r.level;
    }
    return l.value - r.value;
  });
  return result;
};
  
let runTest = function(depth, startNodes, parallelism) {
  if (Array.isArray(startNodes)) {
    startNodes = buildStartNodes(startNodes[0], startNodes[1]);
  } else {
    startNodes = [startNodes];
  }

  // common prefix
  let query = "WITH " + vn + " ";
  let s = [];

  for (let startNode of startNodes) {
    s.push(vn + "/test" + startNode);
  }
  if (s.length === 1) {
    query += "FOR v, e, p IN 1.." + depth + " OUTBOUND " + JSON.stringify(s[0]) + " " + en;
  } else {
    query += "FOR start IN " + vn + " FILTER start._id IN " + JSON.stringify(s) + " FOR v, e, p IN 1.." + depth + " OUTBOUND start " + en;
  }

  // common suffix
  query += " OPTIONS { parallelism: " + parallelism + " } LET level = LENGTH(p.edges) SORT level, v.value RETURN { level, value: v.value }";

  print(query);
  let actual = db._query(query).toArray();
  let expected = buildNestedResult(graphSize, connections, startNodes, depth);

  assertEqual(actual, expected);
};
  
function parallelizeTraversalsTestSuite () {
  let suite = {

    setUpAll : function () {
      db._drop(vn);
      db._drop(en);
      db._createEdgeCollection(en, { numberOfShards: 5 });
      db._create(vn, { numberOfShards: 5 });

      insertVertices(graphSize);
      insertEdges(graphSize, connections);
    },

    tearDownAll : function () {
      db._drop(vn);
      db._drop(en);
    },

    testFake : function () {
    }
  };

  // generate tests for different combinations
  [1, 2, 4].forEach(function(parallelism) {
    [1, 2, 4, 6].forEach(function(depth) {
      [[0, 0], [0, 10], [0, 100]].forEach(function(startNode) {
        if (depth >= 6 && startNode[1] > 10) {
          return;
        }
        suite["testResults" + "Parallelism" + parallelism + "Depth" + depth + "Nodes" + startNode[0] + "_" + startNode[1]] = function() {
          runTest(depth, startNode, parallelism);
        };
        
      });
    });
  });
  
  [1, 2, 4].forEach(function(parallelism) {
    suite["testResults" + "Parallelism" + parallelism + "FirstLevelQuery"] = function() {
      let s = vn + "/test0";
      let query = "RETURN (FOR v, e, p IN 1..1 OUTBOUND " + JSON.stringify(s) + " " + en + " OPTIONS { parallelism: " + parallelism + " } RETURN 1)";

      let actual = db._query(query).toArray();
      let expected = [ [ 1, 1, 1 ] ];
      assertEqual(actual, expected);
    };
  });

  // nested parallel traversals

  [1, 2, 4].forEach(function(parallelism) {
    suite["testResults" + "Parallelism" + parallelism + "NestedTraversalsQuery"] = function() {
      let s = vn + "/test0";
      let query = `FOR v, e, p IN 1..2 OUTBOUND '${s}' ${en} OPTIONS { parallelism: ${parallelism} } 
                    FOR v2 IN 1..1 OUTBOUND v ${en} OPTIONS { parallelism: ${parallelism} }
                    LET level = LENGTH(p.edges) + 1 
                    SORT level, v2.value 
                    RETURN { level, value: v2.value } `;

      let actual = db._query(query).toArray();
      let expected = buildNestedResult(graphSize, connections, [0], 3, 1);//[ [ 1, 1, 1 ] ];
      assertEqual(actual, expected);
    };
  });

  // use V8 query

  /*[1, 2, 4].forEach(function(parallelism) {
    suite["testResultsOneShard" + "Parallelism" + parallelism + "FirstLevelQuery"] = function() {
      let s = vn + "/test0";
      let query = `FOR v, e, p IN 1..2 OUTBOUND '${s}' ${en} OPTIONS { parallelism: ${parallelis} } 
                    FOR v2 IN 1..2 OUTBOUND v ${en} OPTIONS { parallelism: ${parallelism} }
                    RETURN v2.depth`;

      let actual = db._query(query).toArray();
      let expected = [ [ 3, 3, 3, ] ];
      assertEqual(actual, expected);
    };
  });*/


  return suite;
}

function parallelizeTraversalsOneShardTestSuite () {
  let suite = {

    setUpAll : function () {
      db._drop(vn);
      db._drop(en);
      db._createEdgeCollection(en, { numberOfShards: 1 });
      db._create(vn, { numberOfShards: 1, distributeShardsLike: en });

      insertVertices(graphSize);
      insertEdges(graphSize, connections);
    },

    tearDownAll : function () {
      db._drop(vn);
      db._drop(en);
    },

    testFakeOneShard : function () {
    }
  };

  // generate tests for different combinations
  [1, 2, 4].forEach(function(parallelism) {
    [1, 2, 4, 6].forEach(function(depth) {
      [[0, 0], [0, 10], [0, 100]].forEach(function(startNode) {
        if (depth >= 6 && startNode[1] > 10) {
          return;
        }
        suite["testResultsOneShard" + "Parallelism" + parallelism + "Depth" + depth + "Nodes" + startNode[0] + "_" + startNode[1]] = function() {
          runTest(depth, startNode, parallelism);
        };
      });
    });
  });
  
  [1, 2, 4].forEach(function(parallelism) {
    suite["testResultsOneShard" + "Parallelism" + parallelism + "FirstLevelQuery"] = function() {
      let s = vn + "/test0";
      let query = "RETURN (FOR v, e, p IN 1..1 OUTBOUND " + JSON.stringify(s) + " " + en + " OPTIONS { parallelism: " + parallelism + " } RETURN 1)";

      let actual = db._query(query).toArray();
      let expected = [ [ 1, 1, 1 ] ];
      assertEqual(actual, expected);
    };
  });

  // nested parallel traversals

  [1, 2, 4].forEach(function(parallelism) {
    suite["testResultsOneShard" + "Parallelism" + parallelism + "NestedTraversalsQuery"] = function() {
      let s = vn + "/test0";
      let query = `FOR v, e, p IN 1..2 OUTBOUND '${s}' ${en} OPTIONS { parallelism: ${parallelism} } 
                    FOR v2 IN 1..1 OUTBOUND v ${en} OPTIONS { parallelism: ${parallelism} }
                    LET level = LENGTH(p.edges) + 1 
                    SORT level, v2.value 
                    RETURN { level, value: v2.value } `;

      let actual = db._query(query).toArray();
      let expected = buildNestedResult(graphSize, connections, [0], 3, 1);//[ [ 1, 1, 1 ] ];
      assertEqual(actual, expected);
    };
  });
  

  return suite;
}

jsunity.run(parallelizeTraversalsTestSuite);
if (internal.isCluster()) {
  jsunity.run(parallelizeTraversalsOneShardTestSuite);
}

return jsunity.done();
