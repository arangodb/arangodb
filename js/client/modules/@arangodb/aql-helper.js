/* jshint strict: false */
/* global assertTrue, assertFalse, assertEqual, fail, arango
  AQL_EXECUTEJSON */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const isEqual = require("@arangodb/test-helper-common").isEqual;
const db = require("@arangodb").db;

exports.isEqual = isEqual;

function normalizeProjections (p) {
  return (p || []).map((p) => {
    if (Array.isArray(p)) {
      return p;
    }
    if (typeof p === 'string') {
      return [p];
    }
    if (p.hasOwnProperty("path")) {
      return p.path;
    }
    return [];
  }).sort();
}

function normalizeRow (row, recursive) {
  if (row !== null &&
    typeof row === 'object' &&
    !Array.isArray(row)) {
    var keys = Object.keys(row);

    keys.sort();

    var i, n = keys.length, out = { };
    for (i = 0; i < n; ++i) {
      var key = keys[i];

      if (key[0] !== '_') {
        out[key] = row[key];
      }
    }

    return out;
  }

  if (recursive && Array.isArray(row)) {
    row = row.map(normalizeRow);
  }

  return row;
}

function executeQuery(query, bindVars = null, options = {}) {
  let stmt = db._createStatement({query, bindVars, count: true});
  return stmt.execute();
};

function executeJson (plan, options = {}) {
  let command = `return AQL_EXECUTEJSON(${JSON.stringify(plan)}, ${JSON.stringify(options)});`;
  return arango.POST("/_admin/execute", command);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the parse results for a query
// //////////////////////////////////////////////////////////////////////////////

function getParseResults (query) {
  return db._parse(query);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief assert a specific error code when parsing a query
// //////////////////////////////////////////////////////////////////////////////

function assertParseError (errorCode, query) {
  try {
    getParseResults(query);
    fail();
  } catch (e) {
    assertTrue(e.errorNum !== undefined, 'unexpected error format');
    assertEqual(errorCode, e.errorNum, 'unexpected error code (' + e.errorMessage + '): ');
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the results of a query explanation
// //////////////////////////////////////////////////////////////////////////////

function getQueryExplanation (query, bindVars) {
  return db._createStatement({query, bindVars}).explain();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the results of a modify-query
// //////////////////////////////////////////////////////////////////////////////

function getModifyQueryResults (query, bindVars, options = {}) {
  return db._createStatement({query, bindVars, options}).execute().getExtra().stats;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the results of a modify-query
// //////////////////////////////////////////////////////////////////////////////

function getModifyQueryResultsRaw (query, bindVars, options = {}) {
  return db._query(query, bindVars, options);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the results of a query, version
// //////////////////////////////////////////////////////////////////////////////

function getRawQueryResults (query, bindVars, options = {}) {
  let finalOptions = Object.assign({ count: true, batchSize: 3000 }, options);
  let queryResult = db._query(query, bindVars, finalOptions);
  return queryResult.toArray();
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the results of a query in a normalized way
// //////////////////////////////////////////////////////////////////////////////

function getQueryResults (query, bindVars, recursive, options = {}) {
  let result = getRawQueryResults(query, bindVars, options);

  if (Array.isArray(result)) {
    result = result.map(function (row) {
      return normalizeRow(row, recursive);
    });
  }

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief assert a specific error code when running a query
// //////////////////////////////////////////////////////////////////////////////

function assertQueryError (errorCode, query, bindVars, options = {}) {
  try {
    getQueryResults(query, bindVars, options);
    fail();
  } catch (e) {
    assertFalse(e === "fail", "no exception thrown by query");
    assertFalse(e === "fail(): invoked without message", "no exception thrown by query");
    assertTrue(e.errorNum !== undefined, 'unexpected error format while calling [' + query + ']');
    assertEqual(errorCode, e.errorNum, 'unexpected error code (' + e.errorMessage +
      " while executing: '" + query + "' expecting: " + errorCode + '): ');
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief assert a specific warning running a query
// //////////////////////////////////////////////////////////////////////////////

function assertQueryWarningAndNull (errorCode, query, bindVars) {
  let result = db._query(query, bindVars).data;
  let found = {};

  for (let i = 0; i < result.extra.warnings.length; ++i) {
    found[result.extra.warnings[i].code] = true;
  }

  assertTrue(found[errorCode]);
  assertEqual([ null ], result.result);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief get a linearized version of an execution plan
// //////////////////////////////////////////////////////////////////////////////

function getLinearizedPlan (explainResult) {
  var nodes = explainResult.plan.nodes, i;
  var lookup = { }, deps = { };

  for (i = 0; i < nodes.length; ++i) {
    var node = nodes[i];
    lookup[node.id] = node;
    var dependency = -1;
    if (node.dependencies.length > 0) {
      dependency = node.dependencies[0];
    }
    deps[dependency] = node.id;
  }

  var current = -1;
  var out = [];
  while (true) {
    if (!deps.hasOwnProperty(current)) {
      break;
    }

    var n = lookup[deps[current]];
    current = n.id;
    out.push(n);
  }

  return out;
}

function getCompactPlan (explainResult) {
  var out = [];

  function buildExpression (node) {
    var out = node.type;
    if (node.hasOwnProperty('name')) {
      out += '[' + node.name + ']';
    }
    if (node.hasOwnProperty('value')) {
      out += '[' + node.value + ']';
    }

    if (Array.isArray(node.subNodes)) {
      out += '(';
      node.subNodes.forEach(function (node, i) {
        if (i > 0) {
          out += ', ';
        }

        out += buildExpression(node);
      });

      out += ')';
    }
    return out;
  }

  getLinearizedPlan(explainResult).forEach(function (node) {
    var data = { type: node.type };

    if (node.expression) {
      data.expression = buildExpression(node.expression);
    }
    if (node.outVariable) {
      data.outVariable = node.outVariable.name;
    }

    out.push(data);
  });

  return out;
}

function findExecutionNodes (plan, nodeType) {
  let what = plan;
  if (plan.hasOwnProperty('plan')) {
    what = plan.plan;
  }
  return what.nodes.filter((node) => nodeType === undefined || node.type === nodeType);
}

function findReferencedNodes (plan, testNode) {
  let matches = [];
  if (testNode.elements) {
    testNode.elements.forEach(function (element) {
      plan.plan.nodes.forEach(function (node) {
        if (node.hasOwnProperty('outVariable') &&
          node.outVariable.id ===
          element.inVariable.id) {
          matches.push(node);
        }
      });
    });
  } else {
    plan.plan.nodes.forEach(function (node) {
      if (node.outVariable.id === testNode.inVariable.id) {
        matches.push(node);
      }
    });
  }

  return matches;
}

function getQueryMultiplePlansAndExecutions (query, bindVars, testObject, debug) {
  const printYaml = function (plan) {
    require('internal').print(require('js-yaml').safeDump(plan));
  };

  const paramNone = { optimizer: { rules: [ '-all' ]},  verbosePlans: true};
  const paramAllPlans = { allPlans: true, verbosePlans: true };
  
  let resetTest = false;

  if (testObject !== undefined) {
    resetTest = true;
  }

  if (debug === undefined) {
    debug = false;
  }

  // first fetch the unmodified version
  if (debug) {
    require('internal').print('Analyzing Query unoptimized: ' + query);
  }
  
  let plans = [ db._createStatement({query, bindVars, options: paramNone}).explain() ];
  // then all of the ones permuted by by the optimizer.
  if (debug) {
    require('internal').print('Unoptimized Plan (0):');
    printYaml(plans[0]);
  }
  
  let allPlans = db._createStatement({query, bindVars, options: paramAllPlans}).explain();

  let results = [];
  for (let i = 0; i < allPlans.plans.length; i++) {
    if (debug) {
      require('internal').print('Optimized Plan [' + (i + 1) + ']:');
      printYaml(allPlans.plans[i]);
    }
    plans[i + 1] = { plan: allPlans.plans[i]};
  }
  // Now execute each of these variations.
  for (let i = 0; i < plans.length; i++) {
    if (debug) {
      require('internal').print('Executing Plan No: ' + i + '\n');
    }
    if (resetTest) {
      if (debug) {
        require('internal').print('\nFLUSHING\n');
      }
      testObject.tearDown();
      testObject.setUp();
      if (debug) {
        require('internal').print('\n' + i + ' FLUSH DONE\n');
      }
    }

    results[i] = executeJson(plans[i].plan, paramNone);
    // ignore these statistics for comparisons
    results[i].stats = sanitizeStats(results[i].stats);

    if (debug) {
      require('internal').print('\n' + i + ' DONE\n');
    }
  }

  if (debug) {
    require('internal').print('done\n');
  }
  return {'plans': plans, 'results': results};
}

function removeAlwaysOnClusterRules (rules) {
  return rules.filter(function (rule) {
    return ([ 'distribute-filtercalc-to-cluster', 'scatter-in-cluster', 'distribute-in-cluster', 'remove-unnecessary-remote-scatter', 'parallelize-gather' ].indexOf(rule) === -1);
  });
}

function removeClusterNodes (nodeTypes) {
  return nodeTypes.filter(function (nodeType) {
    return ([ 'ScatterNode', 'GatherNode', 'DistributeNode', 'RemoteNode' ].indexOf(nodeType) === -1);
  });
}

function removeClusterNodesFromPlan (nodes) {
  return nodes.filter(function (node) {
    return ([ 'ScatterNode', 'GatherNode', 'DistributeNode', 'RemoteNode' ].indexOf(node.type) === -1);
  });
}

/// @brief recursively removes keys named "estimatedCost" or "selectivityEstimate" of a given object
/// used in tests where we do not want to test those values because of floating-point values used in "AsserEqual"
/// This method should only be used where we explicitly don't want to test those values. 
function removeCost (obj) {
  if (Array.isArray(obj)) {
    return obj.map(removeCost);
  } else if (typeof obj === 'object') {
    let result = {};
    for (let key in obj) {
      if (obj.hasOwnProperty(key) &&
          key !== "estimatedCost" && key !== "selectivityEstimate") {
        result[key] = removeCost(obj[key]);
      }
    }
    return result;
  } else {
    return obj;
  }
}

function unpackRawExpression (node, transform = false) {
  if (node.type === 'array') {
    if (node.hasOwnProperty('raw')) {
      node.subNodes = [];
      node.raw.forEach((n) => {
        node.subNodes.push(unpackRawExpression(n, true));
      });
      delete node.raw;
    } else {
      node.subNodes.map((s) => unpackRawExpression(s, false));
    }
  } else if (node.type === 'object') {
    if (node.hasOwnProperty('raw')) {
      node.subNodes = [];
      let keys = Object.keys(node.raw);
      keys.forEach((k) => {
        node.subNodes.push({ type: "object element", name: k, subNodes: [unpackRawExpression(node.raw[k], true)] });
      });
      delete node.raw;
    } else {
      node.subNodes.map((s) => unpackRawExpression(s, false));
    }
  } else if (node.hasOwnProperty('subNodes') && node.subNodes.length) {
    node.subNodes.map((s) => unpackRawExpression(s, false));
  } else if (transform) {
    return { type: 'value', value: node };
  }
  return node;
}

function sanitizeStats (stats) {
  // remove these members from the stats because they don't matter
  // for the comparisons
  delete stats.scannedFull;
  delete stats.scannedIndex;
  delete stats.cursorsCreated;
  delete stats.cursorsRearmed;
  delete stats.cacheHits;
  delete stats.cacheMisses;
  delete stats.filtered;
  delete stats.executionTime;
  delete stats.httpRequests;
  delete stats.fullCount;
  delete stats.peakMemoryUsage;
  delete stats.intermediateCommits;
  delete stats.seeks;
  delete stats.documentLookups;
  return stats;
}

function executeAllJson(plans, result, query) {
  plans.forEach(function (plan) {
    let jsonResult = executeJson(plan, {optimizer: {rules: ['-all']}}).json;
    assertEqual(jsonResult, result, query);
  });
};

exports.executeJson = executeJson;
exports.executeAllJson = executeAllJson;
exports.executeQuery = executeQuery;
exports.getParseResults = getParseResults;
exports.assertParseError = assertParseError;
exports.getQueryExplanation = getQueryExplanation;
exports.getModifyQueryResults = getModifyQueryResults;
exports.getModifyQueryResultsRaw = getModifyQueryResultsRaw;
exports.getRawQueryResults = getRawQueryResults;
exports.getQueryResults = getQueryResults;
exports.assertQueryError = assertQueryError;
exports.assertQueryWarningAndNull = assertQueryWarningAndNull;
exports.getLinearizedPlan = getLinearizedPlan;
exports.getCompactPlan = getCompactPlan;
exports.findExecutionNodes = findExecutionNodes;
exports.findReferencedNodes = findReferencedNodes;
exports.getQueryMultiplePlansAndExecutions = getQueryMultiplePlansAndExecutions;
exports.removeAlwaysOnClusterRules = removeAlwaysOnClusterRules;
exports.removeClusterNodes = removeClusterNodes;
exports.removeClusterNodesFromPlan = removeClusterNodesFromPlan;
exports.removeCost = removeCost;
exports.unpackRawExpression = unpackRawExpression;
exports.sanitizeStats = sanitizeStats;
exports.normalizeProjections = normalizeProjections;
