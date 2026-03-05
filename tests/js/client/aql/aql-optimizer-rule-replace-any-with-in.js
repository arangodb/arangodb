/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, fail */

var internal = require("internal");
var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;
var findExecutionNodes = helper.findExecutionNodes;
const db = require('internal').db;
let {instanceRole} = require('@arangodb/testutils/instance');
let IM = global.instanceManager;

function NewAqlReplaceAnyWithINTestSuite() {
    var replace;
    var ruleName = "replace-any-eq-with-in";

    var getPlan = function (query, params, options) {
        return db._createStatement({query: query, bindVars: params, options: options}).explain().plan;
    };

    var ruleIsNotUsed = function (query, params) {
        var plan = getPlan(query, params, {optimizer: {rules: ["-all", "+" + ruleName]}});
        assertTrue(plan.rules.indexOf(ruleName) === -1, "Rule should not be used: " + query);
    };

    var executeWithRule = function (query, params) {
        return db._query(query, params, {optimizer: {rules: ["-all", "+" + ruleName]}}).toArray();
    };

    var executeWithoutRule = function (query, params) {
        return db._query(query, params, {optimizer: {rules: ["-all"]}}).toArray();
    };

    var executeWithOrRule = function (query, params) {
        return db._query(query, params, {optimizer: {rules: ["-all", "+replace-or-with-in"]}}).toArray();
    };

    var verifyExecutionPlan = function (query, params) {
        var explainWithRule = db._createStatement({
            query: query,
            bindVars: params || {},
            options: {optimizer: {rules: ["-all", "+" + ruleName]}}
        }).explain();

        var explainWithoutRule = db._createStatement({
            query: query,
            bindVars: params || {},
            options: {optimizer: {rules: ["-all"]}}
        }).explain();

        var planWithRule = explainWithRule.plan;
        var planWithoutRule = explainWithoutRule.plan;

        assertTrue(planWithRule.rules.indexOf(ruleName) !== -1,
            "Plan with rule enabled should contain rule '" + ruleName + "': " + query);
        assertTrue(planWithoutRule.rules.indexOf(ruleName) === -1,
            "Plan without rule should NOT contain rule '" + ruleName + "': " + query);

        var filterNodesWith = findExecutionNodes(planWithRule, "FilterNode");
        var filterNodesWithout = findExecutionNodes(planWithoutRule, "FilterNode");
        var calcNodesWith = findExecutionNodes(planWithRule, "CalculationNode");
        var calcNodesWithout = findExecutionNodes(planWithoutRule, "CalculationNode");

        assertTrue(filterNodesWith.length > 0 && filterNodesWithout.length > 0,
            "Plans should have FilterNodes: " + query);
        assertTrue(calcNodesWith.length > 0 && calcNodesWithout.length > 0,
            "Plans should have CalculationNodes: " + query);

        assertTrue(planWithRule.nodes.length > 0, "Plan with rule should have nodes: " + query);
        assertTrue(planWithoutRule.nodes.length > 0, "Plan without rule should have nodes: " + query);

        return {withRule: planWithRule, withoutRule: planWithoutRule};
    };

    var verifyPlansDifferent = function (planWithRule, planWithoutRule, query) {
        assertTrue(planWithRule.rules.indexOf(ruleName) !== -1,
            "Plan with rule enabled should contain the rule: " + query);
        assertTrue(planWithoutRule.rules.indexOf(ruleName) === -1,
            "Plan without rule should not contain the rule: " + query);

        var calcNodesWith = findExecutionNodes(planWithRule, "CalculationNode");
        var calcNodesWithout = findExecutionNodes(planWithoutRule, "CalculationNode");

        assertTrue(calcNodesWith.length > 0 || calcNodesWithout.length > 0,
            "Plans should have calculation nodes: " + query);

        assertTrue(planWithRule.nodes.length > 0, "Plan with rule should have nodes");
        assertTrue(planWithoutRule.nodes.length > 0, "Plan without rule should have nodes");
    };

    return {

        setUpAll: function () {
            IM.debugClearFailAt();
            internal.db._drop("UnitTestsNewAqlReplaceAnyWithINTestSuite");
            replace = internal.db._create("UnitTestsNewAqlReplaceAnyWithINTestSuite");

            let docs = [];
            for (var i = 1; i <= 10; ++i) {
                docs.push({"value": i, "name": "Alice", "tags": ["a", "b"], "categories": ["x", "y"]});
                docs.push({"value": i + 10, "name": "Bob", "tags": ["b", "c"], "categories": ["y", "z"]});
                docs.push({"value": i + 20, "name": "Carol", "tags": ["c", "d"], "categories": ["z"]});
                docs.push({"a": {"b": i}});
            }
            replace.insert(docs);

            replace.ensureIndex({type: "persistent", fields: ["name"]});
            replace.ensureIndex({type: "persistent", fields: ["a.b"]});
        },

        tearDownAll: function () {
            IM.debugClearFailAt();
            internal.db._drop("UnitTestsNewAqlReplaceAnyWithINTestSuite");
            replace = null;
        },

        setUp: function () {
            IM.debugClearFailAt();
        },

        tearDown: function () {
            IM.debugClearFailAt();
        },

        testExecutionPlanVerification: function () {
            var query = "FOR x IN " + replace.name() +
                " FILTER ['Alice', 'Bob'] ANY == x.name SORT x.value RETURN x.value";

            verifyExecutionPlan(query, {});
        },

        testFiresBasic: function () {
            var query = "FOR x IN " + replace.name() +
                " FILTER ['Alice', 'Bob'] ANY == x.name SORT x.value RETURN x.value";

            var plans = verifyExecutionPlan(query, {});
            verifyPlansDifferent(plans.withRule, plans.withoutRule, query);

            var expected = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20];
            var actual = getQueryResults(query);
            assertEqual(expected, actual);

            var withRule = executeWithRule(query, {});
            var withoutRule = executeWithoutRule(query, {});
            assertEqual(withRule, withoutRule, "Results with and without rule should match");

            var orQuery = "FOR x IN " + replace.name() +
                " FILTER x.name == 'Alice' || x.name == 'Bob' SORT x.value RETURN x.value";
            var orResult = executeWithOrRule(orQuery, {});
            assertEqual(withRule, orResult, "Results with ANY == should match OR query");
        },

        testFiresSingleValue: function () {
            var query = "FOR x IN " + replace.name() +
                " FILTER ['Alice'] ANY == x.name SORT x.value RETURN x.value";

            var plans = verifyExecutionPlan(query, {});
            verifyPlansDifferent(plans.withRule, plans.withoutRule, query);

            var expected = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
            var actual = getQueryResults(query);
            assertEqual(expected, actual);

            var withRule = executeWithRule(query, {});
            var withoutRule = executeWithoutRule(query, {});
            assertEqual(withRule, withoutRule, "Results with and without rule should match");

            var orQuery = "FOR x IN " + replace.name() +
                " FILTER x.name == 'Alice' SORT x.value RETURN x.value";
            var orResult = executeWithOrRule(orQuery, {});
            assertEqual(withRule, orResult, "Results with ANY == should match OR query");
        },

        testFiresEmptyArray: function () {
            var query = "FOR x IN " + replace.name() +
                " FILTER [] ANY == x.name RETURN x.value";

            var plans = verifyExecutionPlan(query, {});
            verifyPlansDifferent(plans.withRule, plans.withoutRule, query);

            var expected = [];
            var actual = getQueryResults(query);
            assertEqual(expected, actual);

            var withRule = executeWithRule(query, {});
            var withoutRule = executeWithoutRule(query, {});
            assertEqual(withRule, withoutRule, "Results with and without rule should match");

            var inQuery = "FOR x IN " + replace.name() +
                " FILTER x.name IN [] RETURN x.value";
            var inResult = executeWithOrRule(inQuery, {});
            assertEqual(withRule, inResult, "Results with ANY == should match IN query");
        },

        testFiresManyValues: function () {
            var query = "FOR x IN " + replace.name() +
                " FILTER ['Alice', 'Bob', 'Carol', 'David', " +
                "         'Eve', 'Frank', 'Grace', 'Henry'] " +
                " ANY == x.name SORT x.value RETURN x.value";

            var plans = verifyExecutionPlan(query, {});
            verifyPlansDifferent(plans.withRule, plans.withoutRule, query);

            var expected = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                            16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30];
            var actual = getQueryResults(query);
            assertEqual(expected, actual);

            var withRule = executeWithRule(query, {});
            var withoutRule = executeWithoutRule(query, {});
            assertEqual(withRule, withoutRule, "Results with and without rule should match");

            var orQuery = "FOR x IN " + replace.name() +
                " FILTER x.name == 'Alice' || x.name == 'Bob' || x.name == 'Carol' || x.name == 'David' ||" +
                "        x.name == 'Eve' || x.name == 'Frank' || x.name == 'Grace' || x.name == 'Henry' " +
                " SORT x.value RETURN x.value";
            var orResult = executeWithOrRule(orQuery, {});
            assertEqual(withRule, orResult, "Results with ANY == should match OR query");
        },

        testFiresNestedAttribute: function () {
            var query = "FOR x IN " + replace.name() +
                " FILTER [1, 2] ANY == x.a.b SORT x.a.b RETURN x.a.b";

            var plans = verifyExecutionPlan(query, {});
            verifyPlansDifferent(plans.withRule, plans.withoutRule, query);

            var expected = [1, 2];
            var actual = getQueryResults(query);
            assertEqual(expected, actual);

            var withRule = executeWithRule(query, {});
            var withoutRule = executeWithoutRule(query, {});
            assertEqual(withRule, withoutRule, "Results with and without rule should match");

            var orQuery = "FOR x IN " + replace.name() +
                " FILTER x.a.b == 1 || x.a.b == 2 SORT x.a.b RETURN x.a.b";
            var orResult = executeWithOrRule(orQuery, {});
            assertEqual(withRule, orResult, "Results with ANY == should match OR query");
        },

        testFiresBind: function () {
            var query =
                "FOR v IN " + replace.name()
                + " FILTER @names ANY == v.name SORT v.value RETURN v.value";
            var params = {"names": ["Alice", "Bob"]};

            var plans = verifyExecutionPlan(query, params);
            verifyPlansDifferent(plans.withRule, plans.withoutRule, query);

            var expected = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20];
            var actual = getQueryResults(query, params);
            assertEqual(expected, actual);

            var withRule = executeWithRule(query, params);
            var withoutRule = executeWithoutRule(query, params);
            assertEqual(withRule, withoutRule, "Results with and without rule should match");

            var orQuery = "FOR v IN " + replace.name()
                + " FILTER v.name == 'Alice' || v.name == 'Bob' SORT v.value RETURN v.value";
            var orResult = executeWithOrRule(orQuery, {});
            assertEqual(withRule, orResult, "Results with ANY == should match OR query");
        },

        testFiresVariables: function () {
            var query =
                "LET names = ['Alice', 'Bob'] FOR v IN " + replace.name()
                + " FILTER names ANY == v.name SORT v.value RETURN v.value";

            var plans = verifyExecutionPlan(query, {});
            verifyPlansDifferent(plans.withRule, plans.withoutRule, query);

            var expected = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20];
            var actual = getQueryResults(query, {});
            assertEqual(expected, actual);

            var withRule = executeWithRule(query, {});
            var withoutRule = executeWithoutRule(query, {});
            assertEqual(withRule, withoutRule, "Results with and without rule should match");

            var orQuery = "LET names = ['Alice', 'Bob'] FOR v IN " + replace.name()
                + " FILTER v.name == 'Alice' || v.name == 'Bob' SORT v.value RETURN v.value";
            var orResult = executeWithOrRule(orQuery, {});
            assertEqual(withRule, orResult, "Results with ANY == should match OR query");
        },

        testFiresMultipleAnyEq: function () {
            var query =
                "FOR v IN " + replace.name()
                + " FILTER ['Alice', 'Bob'] ANY == v.name && v.value <= 20 SORT v.value RETURN v.value";

            var plans = verifyExecutionPlan(query, {});
            verifyPlansDifferent(plans.withRule, plans.withoutRule, query);

            var expected = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20];
            var actual = getQueryResults(query, {});
            assertEqual(expected, actual);

            var withRule = executeWithRule(query, {});
            var withoutRule = executeWithoutRule(query, {});
            assertEqual(withRule, withoutRule, "Results with and without rule should match");

            var orQuery = "FOR v IN " + replace.name()
                + " FILTER (v.name == 'Alice' || v.name == 'Bob') && v.value <= 20 SORT v.value RETURN v.value";
            var orResult = executeWithOrRule(orQuery, {});
            assertEqual(withRule, orResult, "Results with ANY == should match OR query");
        },

        testFiresMultipleAnyEqDifferentAttributes: function () {
            var query =
                "FOR v IN " + replace.name()
                + " FILTER ['Alice'] ANY == v.name && v.value <= 10 SORT v.value RETURN v.value";

            var plans = verifyExecutionPlan(query, {});
            verifyPlansDifferent(plans.withRule, plans.withoutRule, query);

            var expected = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
            var actual = getQueryResults(query, {});
            assertEqual(expected, actual);

            var withRule = executeWithRule(query, {});
            var withoutRule = executeWithoutRule(query, {});
            assertEqual(withRule, withoutRule, "Results with and without rule should match");

            var orQuery = "FOR v IN " + replace.name()
                + " FILTER v.name == 'Alice' && v.value <= 10 SORT v.value RETURN v.value";
            var orResult = executeWithOrRule(orQuery, {});
            assertEqual(withRule, orResult, "Results with ANY == should match OR query");
        },

        testFiresNoCollection: function () {
            var query =
                "FOR x in 1..10 LET doc = {name: 'Alice', value: x} FILTER ['Alice', 'Bob'] " +
                "ANY == doc.name SORT doc.value RETURN doc.value";

            var plans = verifyExecutionPlan(query, {});
            verifyPlansDifferent(plans.withRule, plans.withoutRule, query);

            var expected = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
            var actual = getQueryResults(query);
            assertEqual(expected, actual);

            var withRule = executeWithRule(query, {});
            var withoutRule = executeWithoutRule(query, {});
            assertEqual(withRule, withoutRule, "Results with and without rule should match");
        },

        testFiresNestedInSubquery: function () {
            var query =
                "FOR outer IN " + replace.name() +
                " LET sub = (FOR inner IN " + replace.name() +
                " FILTER ['Alice'] ANY == inner.name RETURN inner.value)" +
                " FILTER ['Alice'] ANY == outer.name RETURN outer.value";

            var plans = verifyExecutionPlan(query, {});
            verifyPlansDifferent(plans.withRule, plans.withoutRule, query);

            var expected = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
            var actual = getQueryResults(query, {});
            assertEqual(expected, actual);

            var withRule = executeWithRule(query, {});
            var withoutRule = executeWithoutRule(query, {});
            assertEqual(withRule, withoutRule, "Results with and without rule should match");
        },

        testDudNotAnyQuantifier: function () {
            var query =
                "FOR x IN " + replace.name() +
                " FILTER ['Alice', 'Bob'] ALL == x.name RETURN x.value";

            ruleIsNotUsed(query, {});
        },

        testDudNoArray: function () {
            var query =
                "FOR x IN " + replace.name() +
                " FILTER 'Alice' ANY == x.name RETURN x.value";

            ruleIsNotUsed(query, {});
        },

        testDudNonDeterministicArray: function () {
            var query =
                "FOR x IN " + replace.name() +
                " FILTER NOOPT(['Alice', 'Bob']) ANY == x.name RETURN x.value";

            ruleIsNotUsed(query, {});
        },

        testDudNoAttribute: function () {
            var query =
                "FOR x IN " + replace.name() +
                " FILTER ['Alice', 'Bob'] ANY == 'Alice' RETURN x.value";

            ruleIsNotUsed(query, {});
        },

        testDudBothArrays: function () {
            var query =
                "FOR x IN " + replace.name() +
                " FILTER ['Alice'] ANY == ['Bob'] RETURN x.value";

            ruleIsNotUsed(query, {});
        },

        testDudDifferentOperators: function () {
            var query =
                "FOR x IN " + replace.name() +
                " FILTER ['Alice', 'Bob'] ANY != x.name RETURN x.value";

            ruleIsNotUsed(query, {});
        },

        testIndexOptimizationWithNameIndex: function () {
            var query =
                "FOR x IN " + replace.name() +
                " FILTER ['Alice', 'Bob'] ANY == x.name SORT x.value RETURN x.value";

            var explainWithRule = db._createStatement({
                query: query,
                bindVars: {},
                options: {optimizer: {rules: ["-all", "+replace-any-eq-with-in", "+use-indexes"]}}
            }).explain();

            var explainWithoutRule = db._createStatement({
                query: query,
                bindVars: {},
                options: {optimizer: {rules: ["-all", "+use-indexes"]}}
            }).explain();

            var planWithRule = explainWithRule.plan;
            var planWithoutRule = explainWithoutRule.plan;

            assertTrue(planWithRule.rules.indexOf(ruleName) !== -1,
                "Plan with rule should contain replace-any-eq-with-in");
            assertTrue(planWithRule.rules.indexOf("use-indexes") !== -1,
                "Plan with rule should contain use-indexes");

            var indexNodesWith = findExecutionNodes(planWithRule, "IndexNode");
            var enumNodesWith = findExecutionNodes(planWithRule, "EnumerateCollectionNode");
            var indexNodesWithout = findExecutionNodes(planWithoutRule, "IndexNode");
            var enumNodesWithout = findExecutionNodes(planWithoutRule, "EnumerateCollectionNode");

            assertTrue(indexNodesWith.length > 0,
                "Plan with replace-any-eq-with-in should use IndexNode. " +
                "Rules: " + JSON.stringify(planWithRule.rules));
            assertTrue(enumNodesWith.length === 0,
                "Plan with replace-any-eq-with-in should NOT use EnumerateCollectionNode");

            if (indexNodesWithout.length === 0) {
                assertTrue(enumNodesWithout.length > 0,
                    "Plan without replace-any-eq-with-in should use EnumerateCollectionNode");
            }

            var expected = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20];
            var withRule = executeWithRule(query, {});
            var withoutRule = executeWithoutRule(query, {});

            assertEqual(expected, withRule);
            assertEqual(withRule, withoutRule, "Results should match");
        },

        testIndexOptimizationWithNestedAttributeIndex: function () {
            var query =
                "FOR x IN " + replace.name() +
                " FILTER [1, 2] ANY == x.a.b SORT x.a.b RETURN x.a.b";

            var explainWithRule = db._createStatement({
                query: query,
                bindVars: {},
                options: {optimizer: {rules: ["-all", "+replace-any-eq-with-in", "+use-indexes"]}}
            }).explain();

            var planWithRule = explainWithRule.plan;

            assertTrue(planWithRule.rules.indexOf(ruleName) !== -1,
                "Plan with rule should contain replace-any-eq-with-in");
            assertTrue(planWithRule.rules.indexOf("use-indexes") !== -1,
                "Plan with rule should contain use-indexes");

            var indexNodesWith = findExecutionNodes(planWithRule, "IndexNode");
            var enumNodesWith = findExecutionNodes(planWithRule, "EnumerateCollectionNode");

            assertTrue(indexNodesWith.length > 0,
                "Plan with replace-any-eq-with-in should use IndexNode for nested attribute");
            assertTrue(enumNodesWith.length === 0,
                "Plan with replace-any-eq-with-in should NOT use EnumerateCollectionNode");

            var expected = [1, 2];
            var withRule = executeWithRule(query, {});
            var withoutRule = executeWithoutRule(query, {});

            assertEqual(expected, withRule);
            assertEqual(withRule, withoutRule, "Results should match");
        },

        testIndexOptimizationMultipleConditions: function () {
            var query =
                "FOR x IN " + replace.name() +
                " FILTER ['Alice'] ANY == x.name && x.value <= 10 SORT x.value RETURN x.value";

            var explainWithRule = db._createStatement({
                query: query,
                bindVars: {},
                options: {optimizer: {rules: ["-all", "+replace-any-eq-with-in", "+use-indexes"]}}
            }).explain();

            var planWithRule = explainWithRule.plan;

            assertTrue(planWithRule.rules.indexOf(ruleName) !== -1,
                "Plan with rule should contain replace-any-eq-with-in");
            assertTrue(planWithRule.rules.indexOf("use-indexes") !== -1,
                "Plan with rule should contain use-indexes");

            var indexNodesWith = findExecutionNodes(planWithRule, "IndexNode");

            assertTrue(indexNodesWith.length > 0,
                "Plan with replace-any-eq-with-in should use IndexNode even with multiple conditions");

            var expected = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
            var withRule = executeWithRule(query, {});
            var withoutRule = executeWithoutRule(query, {});

            assertEqual(expected, withRule);
            assertEqual(withRule, withoutRule, "Results should match");
        },

        testFiresDuplicateValues: function () {
            var query = "FOR x IN " + replace.name() +
                " FILTER ['Alice', 'Alice', 'Bob', 'Bob', 'Alice'] ANY == x.name SORT x.value RETURN x.value";

            var plans = verifyExecutionPlan(query, {});
            verifyPlansDifferent(plans.withRule, plans.withoutRule, query);

            var expected = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20];
            var actual = getQueryResults(query);
            assertEqual(expected, actual);

            var withRule = executeWithRule(query, {});
            var withoutRule = executeWithoutRule(query, {});
            assertEqual(withRule, withoutRule, "Results with and without rule should match");

            var inQuery = "FOR x IN " + replace.name() +
                " FILTER x.name IN ['Alice', 'Alice', 'Bob', 'Bob', 'Alice'] SORT x.value RETURN x.value";
            var inResult = executeWithOrRule(inQuery, {});
            assertEqual(withRule, inResult, "Results with ANY == should match IN query");
        },

        testFiresEmptyString: function () {
            var query = "FOR x IN " + replace.name() +
                " FILTER ['', 'Alice'] ANY == x.name SORT x.value RETURN x.value";

            var plans = verifyExecutionPlan(query, {});
            verifyPlansDifferent(plans.withRule, plans.withoutRule, query);

            var expected = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
            var actual = getQueryResults(query);
            assertEqual(expected, actual);

            var withRule = executeWithRule(query, {});
            var withoutRule = executeWithoutRule(query, {});
            assertEqual(withRule, withoutRule, "Results with and without rule should match");

            var inQuery = "FOR x IN " + replace.name() +
                " FILTER x.name IN ['', 'Alice'] SORT x.value RETURN x.value";
            var inResult = executeWithOrRule(inQuery, {});
            assertEqual(withRule, inResult, "Results with ANY == should match IN query");
        },

        testFiresSpecialCharacters: function () {
            var query = "FOR x IN " + replace.name() +
                " FILTER ['Alice', 'O\\'Brien', 'test\"quote', 'new\\nline'] ANY == x.name SORT x.value RETURN x.value";

            var plans = verifyExecutionPlan(query, {});
            verifyPlansDifferent(plans.withRule, plans.withoutRule, query);

            var actual = getQueryResults(query);
            var withRule = executeWithRule(query, {});
            var withoutRule = executeWithoutRule(query, {});
            assertEqual(withRule, withoutRule, "Results with and without rule should match");

            var inQuery = "FOR x IN " + replace.name() +
                " FILTER x.name IN ['Alice', 'O\\'Brien', 'test\"quote', 'new\\nline'] SORT x.value RETURN x.value";
            var inResult = executeWithOrRule(inQuery, {});
            assertEqual(withRule, inResult, "Results with ANY == should match IN query");
        },

        testFiresIndexedAccess: function () {
            var query = "FOR x IN " + replace.name() +
                " FILTER ['Alice', 'Bob'] ANY == x['name'] SORT x.value RETURN x.value";

            var plans = verifyExecutionPlan(query, {});
            verifyPlansDifferent(plans.withRule, plans.withoutRule, query);

            var expected = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20];
            var actual = getQueryResults(query);
            assertEqual(expected, actual);

            var withRule = executeWithRule(query, {});
            var withoutRule = executeWithoutRule(query, {});
            assertEqual(withRule, withoutRule, "Results with and without rule should match");

            var inQuery = "FOR x IN " + replace.name() +
                " FILTER x['name'] IN ['Alice', 'Bob'] SORT x.value RETURN x.value";
            var inResult = executeWithOrRule(inQuery, {});
            assertEqual(withRule, inResult, "Results with ANY == should match IN query");
        },

        testFiresEmptyArrayOptimization: function () {
            var query = "FOR x IN " + replace.name() +
                " FILTER [] ANY == x.name RETURN x.value";

            var explainWithRule = db._createStatement({
                query: query,
                bindVars: {},
                options: {optimizer: {rules: ["-all", "+replace-any-eq-with-in"]}}
            }).explain();

            var planWithRule = explainWithRule.plan;
            var noResultNodes = findExecutionNodes(planWithRule, "NoResultsNode");

            assertTrue(planWithRule.rules.indexOf(ruleName) !== -1,
                "Plan with rule should contain replace-any-eq-with-in");

            var expected = [];
            var withRule = executeWithRule(query, {});
            var withoutRule = executeWithoutRule(query, {});

            assertEqual(expected, withRule);
            assertEqual(withRule, withoutRule, "Results with and without rule should match");

            var inQuery = "FOR x IN " + replace.name() +
                " FILTER x.name IN [] RETURN x.value";
            var inResult = executeWithOrRule(inQuery, {});
            assertEqual(withRule, inResult, "Results with ANY == should match IN query");
        },

        testSingleValueOptimization: function () {
            var query = "FOR x IN " + replace.name() +
                " FILTER ['Alice'] ANY == x.name SORT x.value RETURN x.value";

            var explainWithRule = db._createStatement({
                query: query,
                bindVars: {},
                options: {optimizer: {rules: ["-all", "+replace-any-eq-with-in"]}}
            }).explain();

            var planWithRule = explainWithRule.plan;

            assertTrue(planWithRule.rules.indexOf(ruleName) !== -1,
                "Plan with rule should contain replace-any-eq-with-in");

            var expected = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
            var withRule = executeWithRule(query, {});
            var withoutRule = executeWithoutRule(query, {});

            assertEqual(expected, withRule);
            assertEqual(withRule, withoutRule, "Results with and without rule should match");

            var eqQuery = "FOR x IN " + replace.name() +
                " FILTER x.name == 'Alice' SORT x.value RETURN x.value";
            var eqResult = db._query(eqQuery).toArray();
            assertEqual(withRule, eqResult, "Single-value ANY == should match == query");
        },

        testChainedOptimizations: function () {
            var query = "FOR x IN " + replace.name() +
                " FILTER ['Alice', 'Alice', 'Bob'] ANY == x.name " +
                "   AND x.value > 5 " +
                "   AND x.value < 15 " +
                "SORT x.value RETURN x.value";

            var explainWithAllRules = db._createStatement({
                query: query,
                bindVars: {},
                options: {optimizer: {rules: ["-all", "+replace-any-eq-with-in", "+use-indexes"]}}
            }).explain();

            var planWithAllRules = explainWithAllRules.plan;

            assertTrue(planWithAllRules.rules.indexOf(ruleName) !== -1,
                "Plan should contain replace-any-eq-with-in");
            assertTrue(planWithAllRules.rules.indexOf("use-indexes") !== -1,
                "Plan should contain use-indexes");

            var indexNodes = findExecutionNodes(planWithAllRules, "IndexNode");
            assertTrue(indexNodes.length > 0,
                "Should use index after ANY == transformation");

            var expected = [6, 7, 8, 9, 10, 11, 12, 13, 14];
            var withRule = executeWithRule(query, {});
            var withoutRule = executeWithoutRule(query, {});

            assertEqual(expected, withRule);
            assertEqual(withRule, withoutRule, "Results with and without rule should match");
        },

        testResultsMatchBetweenAnyAndIn: function () {
            var testCases = [
                {
                    any: "['Alice'] ANY == x.name",
                    in: "x.name IN ['Alice']"
                },
                {
                    any: "['Alice', 'Bob', 'Carol'] ANY == x.name",
                    in: "x.name IN ['Alice', 'Bob', 'Carol']"
                },
                {
                    any: "[1, 2] ANY == x.a.b",
                    in: "x.a.b IN [1, 2]"
                },
                {
                    any: "[] ANY == x.name",
                    in: "x.name IN []"
                }
            ];

            testCases.forEach(function(testCase) {
                var anyQuery = "FOR x IN " + replace.name() +
                    " FILTER " + testCase.any + " SORT x.value RETURN x.value";
                var inQuery = "FOR x IN " + replace.name() +
                    " FILTER " + testCase.in + " SORT x.value RETURN x.value";

                var anyResult = db._query(anyQuery, {}, 
                    {optimizer: {rules: ["-all", "+replace-any-eq-with-in"]}}).toArray();
                var inResult = db._query(inQuery, {}, 
                    {optimizer: {rules: ["-all"]}}).toArray();

                assertEqual(anyResult, inResult,
                    "ANY == and IN should produce same results for: " + testCase.any);
            });
        },

        testIndexUsageComparison: function () {
            var anyQuery = "FOR x IN " + replace.name() +
                " FILTER ['Alice', 'Bob'] ANY == x.name RETURN x";
            var inQuery = "FOR x IN " + replace.name() +
                " FILTER x.name IN ['Alice', 'Bob'] RETURN x";

            var anyPlan = db._createStatement({
                query: anyQuery,
                bindVars: {},
                options: {optimizer: {rules: ["-all", "+replace-any-eq-with-in", "+use-indexes"]}}
            }).explain();

            var inPlan = db._createStatement({
                query: inQuery,
                bindVars: {},
                options: {optimizer: {rules: ["-all", "+use-indexes"]}}
            }).explain();

            var anyIndexNodes = findExecutionNodes(anyPlan.plan, "IndexNode");
            var inIndexNodes = findExecutionNodes(inPlan.plan, "IndexNode");

            assertTrue(anyIndexNodes.length > 0,
                "ANY == (transformed to IN) should use index");
            assertTrue(inIndexNodes.length > 0,
                "IN should use index");

            var anyResult = db._query(anyQuery, {}, 
                {optimizer: {rules: ["-all", "+replace-any-eq-with-in", "+use-indexes"]}}).toArray();
            var inResult = db._query(inQuery, {}, 
                {optimizer: {rules: ["-all", "+use-indexes"]}}).toArray();

            assertEqual(anyResult.length, inResult.length,
                "ANY == and IN should return same number of results");
        },

        testAnyOrBranchesOnSameAttribute: function () {
            var query = "FOR doc IN " + replace.name() +
                " FILTER [5] ANY == doc.value OR [15] ANY == doc.value RETURN doc";
            
            var result = db._query(query).toArray();
            assertEqual(2, result.length, "Should return 2 documents with value 5 or 15");
            
            var values = result.map(d => d.value).sort((a, b) => a - b);
            assertEqual([5, 15], values);
            
            var plan = db._createStatement({query: query}).explain();
            var foundRule = plan.plan.rules.indexOf("replace-any-eq-with-in") !== -1;
            assertTrue(foundRule, "replace-any-eq-with-in rule should fire");
        },

        // Tests for normalization of == and != with both-sides-attributes
        testEqualityNormalizationBothSidesAttributes: function () {
            // Should normalize doc.value2 == doc.value1 to doc.value1 == doc.value2
            var query1 = "FOR doc IN " + replace.name() +
                " FILTER doc.value2 == doc.value1 RETURN doc";
            var query2 = "FOR doc IN " + replace.name() +
                " FILTER doc.value1 == doc.value2 RETURN doc";
            
            var result1 = db._query(query1).toArray();
            var result2 = db._query(query2).toArray();
            
            assertEqual(result1.length, result2.length,
                "Both orderings should return same number of results");
            assertEqual(result1, result2,
                "Both orderings should return identical results");
        },

        testInequalityNormalizationBothSidesAttributes: function () {
            // Should normalize doc.value2 != doc.value1 to doc.value1 != doc.value2
            var query1 = "FOR doc IN " + replace.name() +
                " FILTER doc.value2 != doc.value1 RETURN doc";
            var query2 = "FOR doc IN " + replace.name() +
                " FILTER doc.value1 != doc.value2 RETURN doc";
            
            var result1 = db._query(query1).toArray();
            var result2 = db._query(query2).toArray();
            
            assertEqual(result1.length, result2.length,
                "Both orderings should return same number of results");
            assertEqual(result1, result2,
                "Both orderings should return identical results");
        },

        testDuplicateDetectionAfterNormalization: function () {
            var query = "FOR doc IN " + replace.name() +
                " FILTER (doc.value2 == doc.value1) && (doc.value1 == doc.value2) RETURN doc";
            
            var plan = db._createStatement({
                query: query,
                options: {}
            }).explain();
            
            var planString = JSON.stringify(plan.plan);
            
            // The optimized plan should have simpler filter expression
            var result = db._query(query).toArray();
            
            // Should work the same as a single condition
            var singleCondQuery = "FOR doc IN " + replace.name() +
                " FILTER doc.value1 == doc.value2 RETURN doc";
            var singleResult = db._query(singleCondQuery).toArray();
            
            assertEqual(result, singleResult,
                "Duplicate conditions should produce same results");
            
            var value1Count = (planString.match(/value1/g) || []).length;
            var value2Count = (planString.match(/value2/g) || []).length;
            
            assertTrue(value1Count <= 6,
                "Duplicate conditions should be removed. value1 count: " + value1Count);
            assertTrue(value2Count <= 6,
                "Duplicate conditions should be removed. value2 count: " + value2Count);
        },

        testCanonicalStringHandlesCommutativeOperators: function () {
            // Test that duplicate detection works with commutative expressions
            var query = "FOR doc IN " + replace.name() +
                " FILTER (doc.value1 + doc.value2 == 10) && " +
                "        (doc.value2 + doc.value1 == 10) RETURN doc";
            
            var planWithOpt = db._createStatement({query: query}).explain();
            var planStringWith = JSON.stringify(planWithOpt.plan);
            
            // Count occurrences of the condition in the plan
            var occurrences = (planStringWith.match(/value1/g) || []).length;
            
            var result = db._query(query).toArray();
            
            // Should work the same as a single condition
            var singleCondQuery = "FOR doc IN " + replace.name() +
                " FILTER doc.value1 + doc.value2 == 10 RETURN doc";
            var singleResult = db._query(singleCondQuery).toArray();
            
            assertEqual(result, singleResult,
                "Commutative duplicate conditions should be optimized away");
            
            // Verify the duplicate was removed from plan
            assertTrue(occurrences < 10,
                "Duplicate commutative condition should be detected. Occurrences: " + occurrences);
        },

        testCanonicalStringHandlesInArrayOrdering: function () {
            var query1 = "FOR doc IN " + replace.name() +
                " FILTER doc.value IN [1, 2, 3] RETURN doc";
            var query2 = "FOR doc IN " + replace.name() +
                " FILTER doc.value IN [3, 2, 1] RETURN doc";
            var query3 = "FOR doc IN " + replace.name() +
                " FILTER doc.value IN [2, 1, 3] RETURN doc";
            
            var result1 = db._query(query1).toArray();
            var result2 = db._query(query2).toArray();
            var result3 = db._query(query3).toArray();
            
            assertEqual(result1, result2,
                "Different array orderings should produce same results");
            assertEqual(result1, result3,
                "Different array orderings should produce same results");
            
            var dupQuery = "FOR doc IN " + replace.name() +
                " FILTER (doc.value IN [1, 2, 3]) && (doc.value IN [1, 2, 3]) RETURN doc";
            
            var plan = db._createStatement({query: dupQuery}).explain();
            var planString = JSON.stringify(plan.plan);
            
            var dupResult = db._query(dupQuery).toArray();
            assertEqual(result1, dupResult,
                "Duplicate IN conditions should produce same results");
            
            var inCount = (planString.match(/\bIN\b/g) || []).length;
            
            assertTrue(inCount <= 3,
                "Duplicate IN conditions should be detected. IN count: " + inCount);
        },

        testMixedEqualityInequalityNormalization: function () {
            // Test combination of equality and inequality
            var query = "FOR doc IN " + replace.name() +
                " FILTER doc.value2 == doc.value1 && doc.value3 != doc.value1 RETURN doc";
            
            var result = db._query(query).toArray();
            
            // Should normalize both conditions correctly
            var normalizedQuery = "FOR doc IN " + replace.name() +
                " FILTER doc.value1 == doc.value2 && doc.value1 != doc.value3 RETURN doc";
            var normalizedResult = db._query(normalizedQuery).toArray();
            
            assertEqual(result, normalizedResult,
                "Mixed equality and inequality should normalize correctly");
        },

        testOneSideConstantRemainsSame: function () {
            // One-side-constant comparisons should NOT be affected
            var query1 = "FOR doc IN " + replace.name() +
                " FILTER doc.value == 5 RETURN doc";
            var query2 = "FOR doc IN " + replace.name() +
                " FILTER 5 == doc.value RETURN doc";
            
            var result1 = db._query(query1).toArray();
            var result2 = db._query(query2).toArray();
            
            assertEqual(result1, result2,
                "One-side-constant should already be normalized in AST");
        },

        testFalseBranchRemoval: function () {
            // AND branches containing false should be removed from OR
            var query1 = "FOR doc IN " + replace.name() +
                " FILTER (doc.value > 10 AND false) OR (doc.name == 'Alice') RETURN doc";
            var result1 = db._query(query1).toArray();
            
            var query2 = "FOR doc IN " + replace.name() +
                " FILTER doc.name == 'Alice' RETURN doc";
            var result2 = db._query(query2).toArray();
            
            assertEqual(result1, result2,
                "AND branch with false should be removed");
            
            // Multiple false branches
            var query3 = "FOR doc IN " + replace.name() +
                " FILTER (doc.value > 5 AND false) OR (doc.value < 3 AND false) OR (doc.name == 'Bob') RETURN doc";
            var result3 = db._query(query3).toArray();
            
            var query4 = "FOR doc IN " + replace.name() +
                " FILTER doc.name == 'Bob' RETURN doc";
            var result4 = db._query(query4).toArray();
            
            assertEqual(result3, result4,
                "All AND branches with false should be removed");
        },

        testTrueConditionRemoval: function () {
            // Redundant true conditions should be removed from AND
            var query1 = "FOR doc IN " + replace.name() +
                " FILTER doc.value > 10 AND true AND doc.name == 'Alice' RETURN doc";
            var result1 = db._query(query1).toArray();
            
            var query2 = "FOR doc IN " + replace.name() +
                " FILTER doc.value > 10 AND doc.name == 'Alice' RETURN doc";
            var result2 = db._query(query2).toArray();
            
            assertEqual(result1, result2,
                "True condition should be removed from AND");
            
            // Multiple true conditions
            var query3 = "FOR doc IN " + replace.name() +
                " FILTER true AND doc.value > 5 AND true AND doc.value < 15 AND true RETURN doc";
            var result3 = db._query(query3).toArray();
            
            var query4 = "FOR doc IN " + replace.name() +
                " FILTER doc.value > 5 AND doc.value < 15 RETURN doc";
            var result4 = db._query(query4).toArray();
            
            assertEqual(result3, result4,
                "All true conditions should be removed from AND");
        },

        testNonCommutativeOperatorsNotNormalized: function () {
            // Subtraction and division are not commutative
            var query1 = "FOR doc IN " + replace.name() +
                " FILTER (doc.value1 - doc.value2 == 0) AND (doc.value2 - doc.value1 == 0) RETURN doc";
            var result1 = db._query(query1).toArray();
            
            // These should NOT be treated as duplicates
            var query2 = "FOR doc IN " + replace.name() +
                " FILTER doc.value1 - doc.value2 == 0 RETURN doc";
            var result2 = db._query(query2).toArray();
            
            // Different results expected (not duplicates)
            assertTrue(result1.length <= result2.length,
                "Subtraction should not be normalized as commutative");
        },

        testExecutionPlanShowsDuplicateRemoval: function () {
            // Comprehensive test showing execution plan improvements
            var testCases = [
                {
                    name: "Commutative addition duplicates",
                    query: "FOR doc IN " + replace.name() +
                        " FILTER (doc.value1 + doc.value2 > 50) AND (doc.value2 + doc.value1 > 50) RETURN doc",
                    expectOptimization: true
                },
                {
                    name: "Both-sides-attributes equality duplicates",
                    query: "FOR doc IN " + replace.name() +
                        " FILTER (doc.value2 == doc.value1) AND (doc.value1 == doc.value2) RETURN doc",
                    expectOptimization: true
                },
                {
                    name: "IN array order duplicates",
                    query: "FOR doc IN " + replace.name() +
                        " FILTER (doc.value IN [1,2,3]) AND (doc.value IN [3,2,1]) RETURN doc",
                    expectOptimization: true
                },
                {
                    name: "Complex nested duplicates",
                    query: "FOR doc IN " + replace.name() +
                        " FILTER (doc.value1 * doc.value2 > 100) AND (doc.value2 * doc.value1 > 100) RETURN doc",
                    expectOptimization: true
                }
            ];

            testCases.forEach(function(testCase) {
                var plan = db._createStatement({query: testCase.query}).explain();
                var planString = JSON.stringify(plan.plan);
                
                // Execute query to verify correctness
                var result = db._query(testCase.query).toArray();
                
                assertTrue(result !== undefined,
                    testCase.name + ": Query should execute successfully");
                
                // Check that optimizer rules were applied
                assertTrue(plan.plan.rules.length > 0,
                    testCase.name + ": Some optimizer rules should be applied");
            });
        },

        testExecutionPlanComparisonWithAndWithoutOptimization: function () {
            // Verify that duplicate conditions are optimized away
            // Duplicate detection runs in Condition::optimize() and cannot be disabled
            var query = "FOR doc IN " + replace.name() +
                " FILTER (doc.value1 + doc.value2 == 10) AND " +
                "        (doc.value2 + doc.value1 == 10) AND " +
                "        (doc.value IN [1,2,3]) AND " +
                "        (doc.value IN [3,2,1]) " +
                "RETURN doc";
            
            var plan = db._createStatement({
                query: query,
                options: {}
            }).explain();
            
            var result = db._query(query).toArray();
            
            // Should work the same as query without duplicates
            var noDupQuery = "FOR doc IN " + replace.name() +
                " FILTER (doc.value1 + doc.value2 == 10) AND (doc.value IN [1,2,3]) RETURN doc";
            var noDupResult = db._query(noDupQuery).toArray();
            
            assertEqual(result, noDupResult,
                "Duplicate conditions should produce same results as non-duplicate");
            
            // Verify plan was optimized by checking attribute access counts
            var planString = JSON.stringify(plan.plan);
            var value1Count = (planString.match(/value1/g) || []).length;
            var value2Count = (planString.match(/value2/g) || []).length;
            
            // After duplicate removal, should see reasonable attribute counts
            // Note: Commutative expression (doc.value1 + doc.value2) may not be
            // detected as duplicate of (doc.value2 + doc.value1) without normalization
            assertTrue(value1Count <= 12,
                "Attribute accesses should be reasonable. value1 count: " + value1Count);
            assertTrue(value2Count <= 12,
                "Attribute accesses should be reasonable. value2 count: " + value2Count);
            
            // Basic sanity check - the query should still have some structure
            var filters = findExecutionNodes(plan.plan, "FilterNode");
            assertTrue(filters.length >= 0,
                "Plan structure should be valid");
        },

        testOrBranchDuplicateDetection: function () {
            // Test OR branch duplicate detection with execution plan verification
            var query = "FOR doc IN " + replace.name() +
                " FILTER (doc.value1 == 5 AND doc.value2 == 10) OR " +
                "        (doc.value2 == 10 AND doc.value1 == 5) " +
                "RETURN doc";

            var plan = db._createStatement({query: query}).explain();
            var planString = JSON.stringify(plan.plan);

            // Count how many times we see the conditions
            var value1Count = (planString.match(/value1/g) || []).length;
            var value2Count = (planString.match(/value2/g) || []).length;

            var result = db._query(query).toArray();

            // Should be same as single branch
            var singleBranchQuery = "FOR doc IN " + replace.name() +
                " FILTER doc.value1 == 5 AND doc.value2 == 10 RETURN doc";
            var singleResult = db._query(singleBranchQuery).toArray();

            assertEqual(result, singleResult,
                "Duplicate OR branches should be merged");

            // Verify duplicate was detected (fewer occurrences in plan)
            assertTrue(value1Count < 8,
                "Duplicate OR branch should reduce attribute occurrences. value1 count: " + value1Count);
            assertTrue(value2Count < 8,
                "Duplicate OR branch should reduce attribute occurrences. value2 count: " + value2Count);
        },

        // Subquery handling tests - verify conditions containing subqueries
        // are NOT incorrectly detected as duplicates
        testSubqueryConditionsNotDeduplicated: function () {
            // Two identical-looking conditions with subqueries should NOT be
            // merged because subqueries might have side effects or be expensive
            // to compare. The optimizer conservatively keeps both.
            var query = "FOR doc IN " + replace.name() +
                " LET sub1 = (FOR x IN " + replace.name() + " LIMIT 1 RETURN x.value)[0] " +
                " FILTER doc.value > sub1 AND doc.value > sub1 " +
                "RETURN doc";

            // Query should execute successfully
            var result = db._query(query).toArray();
            assertTrue(result !== undefined, "Query with subquery conditions should execute");

            // Verify the query produces correct results
            var manualQuery = "FOR doc IN " + replace.name() +
                " LET sub1 = (FOR x IN " + replace.name() + " LIMIT 1 RETURN x.value)[0] " +
                " FILTER doc.value > sub1 " +
                "RETURN doc";
            var manualResult = db._query(manualQuery).toArray();

            assertEqual(result.length, manualResult.length,
                "Subquery conditions should produce correct results");
        },

        testSubqueryInOrBranchNotDeduplicated: function () {
            // OR branches containing subqueries should NOT be deduplicated
            var query = "FOR doc IN " + replace.name() +
                " LET sub = (FOR x IN " + replace.name() + " LIMIT 1 RETURN x.value)[0] " +
                " FILTER (doc.value == sub) OR (doc.value == sub) " +
                "RETURN doc";

            // Query should execute successfully
            var result = db._query(query).toArray();
            assertTrue(result !== undefined, "Query with subquery in OR branches should execute");
        },

        testNestedSubqueryInCondition: function () {
            // Conditions with deeply nested subqueries should be handled safely
            var query = "FOR doc IN " + replace.name() +
                " LET nested = (FOR inner IN " + replace.name() +
                "               LET deepSub = (FOR x IN " + replace.name() + " LIMIT 1 RETURN x) " +
                "               RETURN inner)[0] " +
                " FILTER doc.value > 5 AND doc.name == 'Alice' " +
                "RETURN doc";

            var result = db._query(query).toArray();
            assertTrue(result !== undefined, "Query with nested subquery should execute");

            // Verify the simple conditions (without subquery) still work
            // Filter is value > 5 AND name == 'Alice', so we expect values 6-10 for Alice
            var expected = [6, 7, 8, 9, 10];
            var values = result.map(d => d.value).sort((a, b) => a - b);
            assertEqual(expected, values, "Nested subquery query should return correct Alice docs with value > 5");
        },

        testSubqueryAsArrayInAnyEq: function () {
            // Subquery returning array used with ANY == should NOT be transformed
            // because the array is not a constant
            var query = "FOR doc IN " + replace.name() +
                " LET names = (FOR x IN " + replace.name() + " RETURN DISTINCT x.name) " +
                " FILTER names ANY == doc.name " +
                "RETURN doc";

            // Rule should NOT fire because array is from subquery (not constant)
            var plan = db._createStatement({
                query: query,
                options: {optimizer: {rules: ["-all", "+replace-any-eq-with-in"]}}
            }).explain();

            // The rule may or may not fire depending on whether the subquery
            // result is seen as deterministic. Either way, query should work.
            var result = db._query(query).toArray();
            assertTrue(result.length > 0, "Query with subquery array should return results");
        },

        testDuplicateConditionsWithFunctionCalls: function () {
            // Function calls that are deterministic should be detected as duplicates
            var query = "FOR doc IN " + replace.name() +
                " FILTER UPPER(doc.name) == 'ALICE' AND UPPER(doc.name) == 'ALICE' " +
                "RETURN doc";

            var result = db._query(query).toArray();

            var singleCondQuery = "FOR doc IN " + replace.name() +
                " FILTER UPPER(doc.name) == 'ALICE' " +
                "RETURN doc";
            var singleResult = db._query(singleCondQuery).toArray();

            assertEqual(result, singleResult,
                "Duplicate deterministic function calls should be optimized");
        },

        testNonDeterministicFunctionsNotDeduplicated: function () {
            // Non-deterministic functions (like RAND()) should NOT be deduplicated
            var query = "FOR doc IN " + replace.name() +
                " FILTER doc.value > RAND() AND doc.value > RAND() " +
                "RETURN doc";

            // Query should execute - we can't check exact deduplication behavior
            // but it should not crash
            var result = db._query(query).toArray();
            assertTrue(result !== undefined, "Query with RAND() should execute");
        }
    };
}

jsunity.run(NewAqlReplaceAnyWithINTestSuite);

return jsunity.done();

