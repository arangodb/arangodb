/*jshint globalstrict:true, strict:true, esnext: true */
/*global print */

"use strict";

const jsunity = require("jsunity");
const {assertTrue, assertEqual} = jsunity.jsUnity.assertions;
const protoGraphs = require('@arangodb/testutils/aql-graph-traversal-generic-graphs').protoGraphs;
const internal = require("internal");
const db = internal.db;
const arango = internal.arango;

// Seconds to add to execution time for verification.
// This is to account for the time it takes for the query to be scheduled and executed
// and killed.
const VERIFICATION_TIME_BUFFER = internal.isCluster() ? 10 : 5;

const localHelper = {
  getRunningQueries: function() {
    return arango.GET('/_api/query/current');
  },
  normalizeQueryString: function(query) {
    // Remove extra whitespace, newlines and normalize spaces
    return query.replace(/\s+/g, ' ').trim();
  },
  checkRunningQuery: function(queryString, maxAttempts = 10, debug = false) {
    const normalizedQueryString = this.normalizeQueryString(queryString);
    if (debug) {
      print("Checking for running query:", normalizedQueryString);
    }
    for (let i = 0; i < maxAttempts; i++) {
      const runningQueries = this.getRunningQueries();
      if (debug) {
        print("Attempt", i + 1, "of", maxAttempts);
        print("Number of running queries:", runningQueries.length);
      }
      const matchingQuery = runningQueries.find(query => 
        this.normalizeQueryString(query.query) === normalizedQueryString
      );
      if (matchingQuery) {
        if (debug) {
          print("Found matching query with ID:", matchingQuery.id);
        }
        return matchingQuery;
      }
      if (debug) {
        print("No matching query found, waiting...");
      }
      internal.sleep(1);
    }
    if (debug) {
      print("Failed to find running query after", maxAttempts, "attempts");
    }
    assertTrue(false, "Query not found");
  },
  waitForQueryTermination: function(queryId, maxAttempts = 10) {
    for (let i = 0; i < maxAttempts; i++) {
      const runningQueries = this.getRunningQueries();
      const queryStillRunning = runningQueries.some(query => query.id === queryId);
      if (!queryStillRunning) {
        return true;
      }
      internal.sleep(1);
    }
    assertTrue(false, "Query did not terminate within expected time");
  },
  killQuery: function(queryId) {
    const dbName = db._name();
    const response = arango.DELETE('/_db/' + dbName + '/_api/query/' + queryId);
    assertEqual(response.code, 200);
    return response;
  },
  executeAsyncQuery: function(queryString, debug = false, disableAsyncHeader = false) {
    // This helper will just accept the query. It will return the async id.
    if (debug) {
      print("Executing async query:", this.normalizeQueryString(queryString));
    }
    const headers = disableAsyncHeader ? {} : {'x-arango-async': 'store'};
    const response = arango.POST_RAW('/_api/cursor', 
      JSON.stringify({
        query: queryString,
        bindVars: {}
      }), 
      headers
    );
    if (debug) {
      if (disableAsyncHeader) {
        print(response);
      }
      print("Async query response code:", response.code);
      print("Async query ID:", response.headers['x-arango-async-id']);
    }
    assertEqual(response.code, 202);
    assertTrue(response.headers.hasOwnProperty("x-arango-async-id"));
    
    return response.headers['x-arango-async-id'];
  },
  checkJobStatus: function(jobId, maxAttempts = 10) {
    for (let i = 0; i < maxAttempts; i++) {
      const response = arango.GET_RAW('/_api/job/' + jobId);
      if (response.code === 200) {
        return response;
      }
      internal.sleep(1);
    }
    assertTrue(false, "Job not found");
  },
  testKillLongRunningQuery: function(queryString, debug = false, disableAsyncHeader = false) {
    // First run - measure execution time
    const startTime = Date.now();
    if (debug) {
      print("Starting first query execution...");
    }
    const queryJobId = this.executeAsyncQuery(queryString, debug, disableAsyncHeader);
    if (debug) {
      print("First query job ID:", queryJobId);
    }
    const runningQuery = this.checkRunningQuery(queryString, 10, debug);
    const queryId = runningQuery.id;
    if (debug) {
      print("First query ID:", queryId);
    }
    this.killQuery(queryId);
    this.checkJobStatus(queryJobId);
    this.waitForQueryTermination(queryId);
    const firstExecutionTime = (Date.now() - startTime) / 1000; // Convert to seconds
    if (debug) {
      print("First query execution time:", firstExecutionTime, "seconds");
    }

    // Second run - verify query stays running
    if (debug) {
      print("Starting second query execution...");
    }
    const startTime2 = Date.now();
    const queryJobId2 = this.executeAsyncQuery(queryString, debug, disableAsyncHeader);
    if (debug) {
      print("Second query job ID:", queryJobId2);
    }
    const runningQuery2 = this.checkRunningQuery(queryString, 10, debug);
    const queryId2 = runningQuery2.id;
    if (debug) {
      print("Second query ID:", queryId2);
    }

    // Wait for executionTime + VERIFICATION_TIME_BUFFER seconds while checking if query is still running
    const verificationTime = firstExecutionTime + VERIFICATION_TIME_BUFFER;
    if (debug) {
      print("Verification time:", verificationTime, "seconds (" + VERIFICATION_TIME_BUFFER + " seconds more than the first query)");
    }
    const startVerification = Date.now();
    while ((Date.now() - startVerification) / 1000 < verificationTime) {
      const runningQueries = this.getRunningQueries();
      const queryStillRunning = runningQueries.some(query => query.id === queryId2);
      if (debug) {
        print("Query still running:", queryStillRunning);
      }
      assertTrue(queryStillRunning, "Query terminated before verification time");
      internal.sleep(1);
    }

    // Now kill the query and verify termination
    if (debug) {
      print("Killing second query...");
    }
    this.killQuery(queryId2);
    this.checkJobStatus(queryJobId2);
    this.waitForQueryTermination(queryId2);
    
    // Verify that second query ran longer than first query
    const secondExecutionTime = (Date.now() - startTime2) / 1000;
    if (debug) {
      print("Second query execution time:", secondExecutionTime, "seconds");
    }
    assertTrue(secondExecutionTime > firstExecutionTime, 
      `Second query (${secondExecutionTime}s) did not run longer than first query (${firstExecutionTime}s). Kill may not have worked.`);
    
    if (debug) {
      print("Test completed successfully");
    }
  }
};

/*
  Bidirectional Circle
  - DFS
  - BFS
  - Weighted Path
*/

function testBidirectionalCircleDfsLongRunning(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.bidirectionalCircle.name()));
  
  const queryString = `
    FOR v, e IN 1..999 OUTBOUND "${testGraph.vertex('A')}"
    GRAPH ${testGraph.name()}
    OPTIONS {order: "dfs", uniqueVertices: "none", uniqueEdges: "none"}
    RETURN v.key
  `;

  localHelper.testKillLongRunningQuery(queryString);
}

function testBidirectionalCircleBfsLongRunning(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.bidirectionalCircle.name()));
  
  const queryString = `
    FOR v, e IN 1..999 OUTBOUND "${testGraph.vertex('A')}"
    GRAPH ${testGraph.name()}
    OPTIONS {order: "bfs", uniqueVertices: "none", uniqueEdges: "none"}
    RETURN v.key
  `;

  localHelper.testKillLongRunningQuery(queryString);
}

function testBidirectionalCircleWeightedPathLongRunning(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.bidirectionalCircle.name()));
  
  const queryString = `
    FOR v, e IN 1..999 OUTBOUND "${testGraph.vertex('A')}"
    GRAPH ${testGraph.name()}
    OPTIONS {order: "weighted", weightAttribute: "${testGraph.weightAttribute()}", uniqueVertices: "none", uniqueEdges: "none"}
    RETURN v.key
  `;

  localHelper.testKillLongRunningQuery(queryString);
}

/*
  HugeCompleteGraph
  - K Paths
*/

function testHugeCompleteGraphKPathsLongRunning(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.hugeCompleteGraph.name()));
  
  const queryString = `
    FOR path IN 1..999 ANY K_PATHS "${testGraph.vertex('A')}" TO "${testGraph.vertex('B')}"
    GRAPH ${testGraph.name()}
    OPTIONS {useCache: false}
    RETURN path
  `;

  localHelper.testKillLongRunningQuery(queryString);
}

/*
  HugeGridGraph
  - Shortest Path
  - All Shortest Paths
*/

function testHugeGridGraphShortestPathLongRunning(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.hugeGridGraph.name()));
  
  const queryString = `
    FOR v, e IN OUTBOUND SHORTEST_PATH "${testGraph.vertex('1')}" TO "${testGraph.vertex('1000000')}"
    GRAPH ${testGraph.name()}
    OPTIONS {useCache: false}
    RETURN v.key
  `;

  const debug = false;
  localHelper.testKillLongRunningQuery(queryString, debug, debug);
}

function testHugeGridGraphAllShortestPathsLongRunning(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.hugeGridGraph.name()));
  
  const queryString = `
    FOR path IN ANY ALL_SHORTEST_PATHS "${testGraph.vertex('1')}" TO "${testGraph.vertex('1000000')}"
    GRAPH ${testGraph.name()}
    OPTIONS {useCache: false}
    RETURN path
  `;

  localHelper.testKillLongRunningQuery(queryString);
}

// DFS, BFS, Weighted Path
exports.testBidirectionalCircleDfsLongRunning = testBidirectionalCircleDfsLongRunning;
exports.testBidirectionalCircleBfsLongRunning = testBidirectionalCircleBfsLongRunning;
exports.testBidirectionalCircleWeightedPathLongRunning = testBidirectionalCircleWeightedPathLongRunning; 

// K Paths
exports.testHugeCompleteGraphKPathsLongRunning = testHugeCompleteGraphKPathsLongRunning;

// Shortest Path, All Shortest Paths
exports.testHugeGridGraphShortestPathLongRunning = testHugeGridGraphShortestPathLongRunning;
exports.testHugeGridGraphAllShortestPathsLongRunning = testHugeGridGraphAllShortestPathsLongRunning;