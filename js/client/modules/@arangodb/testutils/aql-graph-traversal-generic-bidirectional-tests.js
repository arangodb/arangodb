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
  formatTimestamp: function() {
    const now = new Date();
    const day = now.getDate().toString().padStart(2, '0');
    const month = (now.getMonth() + 1).toString().padStart(2, '0');
    const year = now.getFullYear();
    const hours = now.getHours().toString().padStart(2, '0');
    const minutes = now.getMinutes().toString().padStart(2, '0');
    const seconds = now.getSeconds().toString().padStart(2, '0');
    const milliseconds = now.getMilliseconds().toString().padStart(3, '0');

    return `[${day}/${month}/${year} ${hours}:${minutes}:${seconds}.${milliseconds}]`;
  },
  debugPrint: function(...args) {
    print(this.formatTimestamp(), ...args);
  },
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
      this.debugPrint("Checking for running query:", normalizedQueryString);
    }
    for (let i = 0; i < maxAttempts; i++) {
      const runningQueries = this.getRunningQueries();
      if (debug) {
        this.debugPrint("Attempt", i + 1, "of", maxAttempts);
        this.debugPrint("Number of running queries:", runningQueries.length);
      }
      const matchingQuery = runningQueries.find(query => 
        this.normalizeQueryString(query.query) === normalizedQueryString
      );
      if (matchingQuery) {
        if (debug) {
          this.debugPrint("Found matching query with ID:", matchingQuery.id);
        }
        return matchingQuery;
      }
      if (debug) {
        this.debugPrint("No matching query found, waiting...");
      }
      internal.sleep(1);
    }
    if (debug) {
      this.debugPrint("Failed to find running query after", maxAttempts, "attempts");
    }
    assertTrue(false, "Query not found");
  },
  waitForQueryTermination: function(queryId, maxAttempts = 10, debug = false) {
    for (let i = 0; i < maxAttempts; i++) {
      const runningQueries = this.getRunningQueries();
      const queryStillRunning = runningQueries.some(query => query.id === queryId);
      if (!queryStillRunning) {
        if (debug) {
          this.debugPrint("Query", queryId, "terminated after", i + 1, "attempts");
        }
        return true;
      }
      if (debug) {
        this.debugPrint("Waiting for query", queryId, "to terminate, attempt", i + 1, "of", maxAttempts);
      }
      internal.sleep(1);
    }
    assertTrue(false, "Query did not terminate within expected time");
  },
  killQuery: function(queryId, debug = false) {
    const dbName = db._name();
    if (debug) {
      this.debugPrint("Attempting to kill query ID:", queryId);
    }
    const response = arango.DELETE('/_db/' + dbName + '/_api/query/' + queryId);
    if (debug) {
      this.debugPrint("Kill query response code:", response.code);
      if (response.code !== 200) {
        this.debugPrint("Kill query failed with error:", response.error, "errorNum:", response.errorNum);
      }
    }
    if (response.code !== 200) {
      if (response.code === 404 && response.errorNum === 1591) {
        // Query not found - likely already completed
        if (debug) {
          this.debugPrint("Query not found (404/1591) - likely already completed");
        }
      } else {
        print("Failed to kill query:", response.code, response.error, response.errorNum);
        assertEqual(response.code, 200);
      }
    }
    return response;
  },
  executeAsyncQuery: function(queryString, debug = false, disableAsyncHeader = false) {
    // This helper will just accept the query. It will return the async id.
    if (debug) {
      this.debugPrint("Executing async query:", this.normalizeQueryString(queryString));
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
        this.debugPrint(response);
      }
      this.debugPrint("Async query response code:", response.code);
      this.debugPrint("Async query ID:", response.headers['x-arango-async-id']);
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
      this.debugPrint("Starting first query execution...");
    }
    const queryJobId = this.executeAsyncQuery(queryString, debug, disableAsyncHeader);
    if (debug) {
      this.debugPrint("First query job ID:", queryJobId);
    }
    const runningQuery = this.checkRunningQuery(queryString, 10, debug);
    const queryId = runningQuery.id;
    if (debug) {
      this.debugPrint("First query ID:", queryId);
    }
    this.killQuery(queryId, debug);
    this.checkJobStatus(queryJobId);
    this.waitForQueryTermination(queryId, 10, debug);
    const firstExecutionTime = (Date.now() - startTime) / 1000; // Convert to seconds
    if (debug) {
      this.debugPrint("First query execution time:", firstExecutionTime, "seconds");
    }

    // Second run - verify query stays running
    if (debug) {
      this.debugPrint("Starting second query execution...");
    }
    const startTime2 = Date.now();
    const queryJobId2 = this.executeAsyncQuery(queryString, debug, disableAsyncHeader);
    if (debug) {
      this.debugPrint("Second query job ID:", queryJobId2);
    }
    const runningQuery2 = this.checkRunningQuery(queryString, 10, debug);
    const queryId2 = runningQuery2.id;
    if (debug) {
      this.debugPrint("Second query ID:", queryId2);
    }

    // Wait for a reasonable time to verify the query is long-running, then kill it
    // We'll wait for firstExecutionTime + small buffer, then kill to test the kill functionality
    const killAttemptTime = firstExecutionTime + Math.min(VERIFICATION_TIME_BUFFER / 2, 2.0); // Kill after first exec time + max 2 seconds
    if (debug) {
      this.debugPrint("Will attempt to kill query at:", killAttemptTime, "seconds");
    }

    const startVerification = Date.now();
    let queryKilled = false;

    while ((Date.now() - startVerification) / 1000 < killAttemptTime) {
      const runningQueries = this.getRunningQueries();
      const queryStillRunning = runningQueries.some(query => query.id === queryId2);
      if (debug) {
        this.debugPrint("Query still running:", queryStillRunning);
      }

      if (!queryStillRunning) {
        // Query completed before we could kill it
        const secondExecutionTime = (Date.now() - startTime2) / 1000;
        if (debug) {
          this.debugPrint("Query completed naturally before kill attempt. Second query execution time:", secondExecutionTime, "seconds");
          this.debugPrint("This indicates the query is completing faster than expected, which defeats the purpose of the kill test.");
        }

        assertTrue(false,
          `Query completed naturally in ${secondExecutionTime}s before kill attempt. This test requires a query that runs longer than ${killAttemptTime}s to properly test the kill functionality.`);
      }

      internal.sleep(1);
    }

    // Now attempt to kill the query
    if (debug) {
      this.debugPrint("Attempting to kill query after verification period...");
    }

    const runningQueries = this.getRunningQueries();
    const queryStillRunning = runningQueries.some(query => query.id === queryId2);

    if (debug) {
      this.debugPrint("Query still running at kill time:", queryStillRunning);
    }

    if (queryStillRunning) {
      const killResponse = this.killQuery(queryId2, debug);

      // Check if kill was successful or query already completed
      if (killResponse.code === 200) {
        // Query was successfully killed
        this.checkJobStatus(queryJobId2);
        this.waitForQueryTermination(queryId2, 10, debug);

        // Verify that second query ran longer than first query
        const secondExecutionTime = (Date.now() - startTime2) / 1000;
        if (debug) {
          this.debugPrint("Second query execution time:", secondExecutionTime, "seconds");
        }
        assertTrue(secondExecutionTime > firstExecutionTime,
          `Second query (${secondExecutionTime}s) did not run longer than first query (${firstExecutionTime}s). Kill may not have worked.`);

        if (debug) {
          this.debugPrint("Test completed successfully - query was killed mid-execution");
        }
      } else if (killResponse.code === 404 && killResponse.errorNum === 1591) {
        // Query completed between the check and kill attempt - race condition
        const secondExecutionTime = (Date.now() - startTime2) / 1000;
        if (debug) {
          this.debugPrint("Query completed between check and kill attempt (race condition). Second query execution time:", secondExecutionTime, "seconds");
        }

        // This is still a race condition, but much less likely now
        assertTrue(false,
          `Query completed naturally in ${secondExecutionTime}s during kill attempt. Race condition occurred.`);
      }
    } else {
      // Query completed right before kill attempt
      const secondExecutionTime = (Date.now() - startTime2) / 1000;
      if (debug) {
        this.debugPrint("Query completed just before kill attempt. Second query execution time:", secondExecutionTime, "seconds");
      }

      assertTrue(false,
        `Query completed naturally in ${secondExecutionTime}s just before kill attempt. This test requires a query that runs longer to properly test the kill functionality.`);
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

  localHelper.testKillLongRunningQuery(queryString, true);
}

function testBidirectionalCircleBfsLongRunning(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.bidirectionalCircle.name()));
  
  const queryString = `
    FOR v, e IN 1..999 OUTBOUND "${testGraph.vertex('A')}"
    GRAPH ${testGraph.name()}
    OPTIONS {order: "bfs", uniqueVertices: "none", uniqueEdges: "none"}
    RETURN v.key
  `;

  localHelper.testKillLongRunningQuery(queryString, true);
}

function testBidirectionalCircleWeightedPathLongRunning(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.bidirectionalCircle.name()));
  
  const queryString = `
    FOR v, e IN 1..999 OUTBOUND "${testGraph.vertex('A')}"
    GRAPH ${testGraph.name()}
    OPTIONS {order: "weighted", weightAttribute: "${testGraph.weightAttribute()}", uniqueVertices: "none", uniqueEdges: "none"}
    RETURN v.key
  `;

  localHelper.testKillLongRunningQuery(queryString, true);
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

  localHelper.testKillLongRunningQuery(queryString, true);
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

  localHelper.testKillLongRunningQuery(queryString, true);
}

function testHugeGridGraphAllShortestPathsLongRunning(testGraph) {
  assertTrue(testGraph.name().startsWith(protoGraphs.hugeGridGraph.name()));
  
  const queryString = `
    FOR path IN ANY ALL_SHORTEST_PATHS "${testGraph.vertex('1')}" TO "${testGraph.vertex('1000000')}"
    GRAPH ${testGraph.name()}
    OPTIONS {useCache: false}
    RETURN path
  `;

  localHelper.testKillLongRunningQuery(queryString, true);
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