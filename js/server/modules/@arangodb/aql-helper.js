/* jshint strict: false */
/* global assertTrue, assertEqual, fail, 
  AQL_EXECUTE, AQL_PARSE, AQL_EXPLAIN, AQL_EXECUTEJSON */

// //////////////////////////////////////////////////////////////////////////////
// / @brief aql test helper functions
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2011-2012 triagens GmbH, Cologne, Germany
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
// / @author Jan Steemann
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief normalize a single row result
// //////////////////////////////////////////////////////////////////////////////

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

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the parse results for a query
// //////////////////////////////////////////////////////////////////////////////

function getParseResults (query) {
  return AQL_PARSE(query);
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
  return AQL_EXPLAIN(query, bindVars);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the results of a modify-query
// //////////////////////////////////////////////////////////////////////////////

function getModifyQueryResults (query, bindVars) {
  var queryResult = AQL_EXECUTE(query, bindVars);

  return queryResult.stats;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the results of a modify-query
// //////////////////////////////////////////////////////////////////////////////

function getModifyQueryResultsRaw (query, bindVars) {
  var queryResult = AQL_EXECUTE(query, bindVars);

  return queryResult;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the results of a query, version
// //////////////////////////////////////////////////////////////////////////////

function getRawQueryResults (query, bindVars) {
  var queryResult = AQL_EXECUTE(query, bindVars, {
    count: true,
    batchSize: 3000
  });
  return queryResult.json;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the results of a query in a normalized way
// //////////////////////////////////////////////////////////////////////////////

function getQueryResults (query, bindVars, recursive) {
  var result = getRawQueryResults(query, bindVars);

  if (Array.isArray(result)) {
    result = result.map(function (row) {
      return normalizeRow(row, recursive);
    });
  }

  return result;
}

function typeName (value) {
  if (value === null) {
    return 'null';
  }
  if (value === undefined) {
    return 'undefined';
  }
  if (Array.isArray(value)) {
    return 'array';
  }

  var type = typeof value;

  if (type === 'object') {
    return 'object';
  }
  if (type === 'string') {
    return 'string';
  }
  if (type === 'boolean') {
    return 'boolean';
  }
  if (type === 'number') {
    return 'number';
  }

  throw 'unknown variable type';
}

function isEqual (lhs, rhs) {
  var ltype = typeName(lhs), rtype = typeName(rhs), i;

  if (ltype !== rtype) {
    return false;
  }

  if (ltype === 'null' || ltype === 'undefined') {
    return true;
  }

  if (ltype === 'array') {
    if (lhs.length !== rhs.length) {
      return false;
    }
    for (i = 0; i < lhs.length; ++i) {
      if (!isEqual(lhs[i], rhs[i])) {
        return false;
      }
    }
    return true;
  }

  if (ltype === 'object') {
    var lkeys = Object.keys(lhs), rkeys = Object.keys(rhs);
    if (lkeys.length !== rkeys.length) {
      return false;
    }
    for (i = 0; i < lkeys.length; ++i) {
      var key = lkeys[i];
      if (!isEqual(lhs[key], rhs[key])) {
        return false;
      }
    }
    return true;
  }

  if (ltype === 'boolean') {
    return (lhs === rhs);
  }
  if (ltype === 'string') {
    return (lhs === rhs);
  }
  if (ltype === 'number') {
    if (isNaN(lhs)) {
      return isNaN(rhs);
    }
    if (!isFinite(lhs)) {
      return (lhs === rhs);
    }
    return (lhs.toFixed(10) === rhs.toFixed(10));
  }

  throw 'unknown variable type';
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief assert a specific error code when running a query
// //////////////////////////////////////////////////////////////////////////////

function assertQueryError (errorCode, query, bindVars) {
  try {
    getQueryResults(query, bindVars);
    fail();
  } catch (e) {
    assertTrue(e.errorNum !== undefined, 'unexpected error format while calling [' + query + ']');
    assertEqual(errorCode, e.errorNum, 'unexpected error code (' + e.errorMessage +
      " while executing: '" + query + "' expecting: " + errorCode + '): ');
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief assert a specific warning running a query
// //////////////////////////////////////////////////////////////////////////////

function assertQueryWarningAndNull (errorCode, query, bindVars) {
  var result = AQL_EXECUTE(query, bindVars), i, found = { };

  for (i = 0; i < result.warnings.length; ++i) {
    found[result.warnings[i].code] = true;
  }

  assertTrue(found[errorCode]);
  assertEqual([ null ], result.json);
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

function findExecutionNodes (plan, nodetype) {
  var matches = [];
  var what = plan;
  if (plan.hasOwnProperty('plan')) {
    what = plan.plan;
  }
  what.nodes.forEach(function(node) {
    if (nodetype === undefined || node.type === nodetype) {
      matches.push(node);
    } else if (node.type === 'SubqueryNode') {
      var subPlan = {'plan': node.subquery};
      matches = matches.concat(findExecutionNodes(subPlan, nodetype));
    }
  });
  return matches;
}

function findReferencedNodes (plan, testNode) {
  var matches = [];
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
  var printYaml = function (plan) {
    require('internal').print(require('js-yaml').safeDump(plan));
  };
  var i;
  var plans = [];
  var allPlans = [];
  var results = [];
  var resetTest = false;
  var paramNone = { optimizer: { rules: [ '-all' ]},  verbosePlans: true};
  var paramAllPlans = { allPlans: true, verbosePlans: true};

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
  plans[0] = AQL_EXPLAIN(query, bindVars, paramNone);
  // then all of the ones permuted by by the optimizer.
  if (debug) {
    require('internal').print('Unoptimized Plan (0):');
    printYaml(plans[0]);
  }
  allPlans = AQL_EXPLAIN(query, bindVars, paramAllPlans);

  for (i = 0; i < allPlans.plans.length; i++) {
    if (debug) {
      require('internal').print('Optimized Plan [' + (i + 1) + ']:');
      printYaml(allPlans.plans[i]);
    }
    plans[i + 1] = { plan: allPlans.plans[i]};
  }
  // Now execute each of these variations.
  for (i = 0; i < plans.length; i++) {
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

    results[i] = AQL_EXECUTEJSON(plans[i].plan, paramNone);
    // ignore these statistics for comparisons
    delete results[i].stats.scannedFull;
    delete results[i].stats.scannedIndex;
    delete results[i].stats.filtered;
    delete results[i].stats.executionTime;
    delete results[i].stats.httpRequests;

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
    return ([ 'distribute-filtercalc-to-cluster', 'scatter-in-cluster', 'distribute-in-cluster', 'remove-unnecessary-remote-scatter' ].indexOf(rule) === -1);
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

exports.isEqual = isEqual;
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
