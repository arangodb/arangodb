/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, fail */

var internal = require("internal");
var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;
var findExecutionNodes = helper.findExecutionNodes;
const db = require('internal').db;
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

        assertTrue(calcNodesWith.length > 0 && calcNodesWithout.length > 0,
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
        },

        testFiresVariableArray: function () {
            // lhs is a computed (non-literal) array — rule still fires
            var query = "FOR x IN " + replace.name() +
                " FILTER [x.value, x.value + 1] ANY == 5 SORT x.value RETURN x.value";

            var plans = verifyExecutionPlan(query, {});
            verifyPlansDifferent(plans.withRule, plans.withoutRule, query);

            var withRule = executeWithRule(query, {});
            var withoutRule = executeWithoutRule(query, {});
            assertEqual(withRule, withoutRule, "Results with and without rule should match");
            // x.value==4: [4,5] ANY == 5 is true; x.value==5: [5,6] ANY == 5 is true
            assertTrue(withRule.indexOf(4) !== -1, "value 4 should match");
            assertTrue(withRule.indexOf(5) !== -1, "value 5 should match");
        },

        testFiresArrayOfArrays: function () {
            // lhs is an array of arrays — rule fires, element comparison is deep
            // [['ALICE'], ['BOB']] ANY == ['Bob'] → ['Bob'] IN [['ALICE'], ['BOB']]
            // 'BOB' != 'Bob' (case-sensitive) → false → 0 docs
            var query = "FOR x IN " + replace.name() +
                " FILTER [['ALICE'], ['BOB']] ANY == ['Bob'] RETURN x.value";

            var plan = getPlan(query, {}, {optimizer: {rules: ["-all", "+" + ruleName]}});
            assertTrue(plan.rules.indexOf(ruleName) !== -1, "Rule should fire");

            var withRule = executeWithRule(query, {});
            var withoutRule = executeWithoutRule(query, {});
            assertEqual(0, withRule.length);
            assertEqual(withRule, withoutRule, "Results with and without rule should match");
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

        testSkipsAllQuantifier: function () {
            var query =
                "FOR x IN " + replace.name() +
                " FILTER ['Alice', 'Bob'] ALL == x.name RETURN x.value";

            ruleIsNotUsed(query, {});
        },

        testFiresScalarLhs: function () {
            // lhs='Alice' is not an array → rule fires, x.name IN 'Alice' → false → 0 docs.
            var query =
                "FOR x IN " + replace.name() +
                " FILTER 'Alice' ANY == x.name RETURN x.value";

            var plan = getPlan(query, {}, {optimizer: {rules: ["-all", "+" + ruleName]}});
            assertTrue(plan.rules.indexOf(ruleName) !== -1, "Rule should fire");
            var withRule = executeWithRule(query, {});
            var withoutRule = executeWithoutRule(query, {});
            assertEqual(0, withRule.length);
            assertEqual(withRule, withoutRule, "Results with and without rule should match");
        },

        testFiresNonDeterministicLhs: function () {
            // lhs=NOOPT([...]) is non-deterministic → rule fires, results match ANY ==.
            var query =
                "FOR x IN " + replace.name() +
                " FILTER NOOPT(['Alice', 'Bob']) ANY == x.name RETURN x.value";

            var plan = getPlan(query, {}, {optimizer: {rules: ["-all", "+" + ruleName]}});
            assertTrue(plan.rules.indexOf(ruleName) !== -1, "Rule should fire");
            var withRule = executeWithRule(query, {});
            var withoutRule = executeWithoutRule(query, {});
            assertEqual(withoutRule.length, withRule.length);
        },

        testFiresConstantRhs: function () {
            // ['Alice', 'Bob'] ANY == 'Alice' → 'Alice' IN ['Alice', 'Bob'] = true constant.
            // All 40 docs are returned.
            var query =
                "FOR x IN " + replace.name() +
                " FILTER ['Alice', 'Bob'] ANY == 'Alice' RETURN x.value";

            var plan = getPlan(query, {}, {optimizer: {rules: ["-all", "+" + ruleName]}});
            assertTrue(plan.rules.indexOf(ruleName) !== -1, "Rule should fire");
            var withRule = executeWithRule(query, {});
            var withoutRule = executeWithoutRule(query, {});
            assertEqual(40, withRule.length);
            assertEqual(withRule, withoutRule, "Results with and without rule should match");
        },

        testFiresBothConstantArrays: function () {
            // ['Alice'] ANY == ['Bob'] → ['Bob'] IN ['Alice'] = false constant.
            // 0 docs are returned.
            var query =
                "FOR x IN " + replace.name() +
                " FILTER ['Alice'] ANY == ['Bob'] RETURN x.value";

            var plan = getPlan(query, {}, {optimizer: {rules: ["-all", "+" + ruleName]}});
            assertTrue(plan.rules.indexOf(ruleName) !== -1, "Rule should fire");
            var withRule = executeWithRule(query, {});
            var withoutRule = executeWithoutRule(query, {});
            assertEqual(0, withRule.length);
            assertEqual(withRule, withoutRule, "Results with and without rule should match");
        },

        testSkipsAnyNe: function () {
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

        testFiresSpecialCharacters: function () {
            var query = "FOR x IN " + replace.name() +
                " FILTER ['Alice', 'O\\'Brien', 'test\"quote', 'new\\nline'] ANY == x.name SORT x.value RETURN x.value";

            var plans = verifyExecutionPlan(query, {});
            verifyPlansDifferent(plans.withRule, plans.withoutRule, query);

            var withRule = executeWithRule(query, {});
            var withoutRule = executeWithoutRule(query, {});
            assertEqual(withRule, withoutRule, "Results with and without rule should match");
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
        },

        testAnyOrBranchesOnSameAttribute: function () {
            var query = "FOR doc IN " + replace.name() +
                " FILTER [5] ANY == doc.value OR [15] ANY == doc.value RETURN doc";

            var plan = getPlan(query, {}, {optimizer: {rules: ["-all", "+" + ruleName]}});
            assertTrue(plan.rules.indexOf(ruleName) !== -1, "Rule should fire");

            var withRule = executeWithRule(query, {});
            var withoutRule = executeWithoutRule(query, {});
            assertEqual(2, withRule.length, "Should return 2 documents with value 5 or 15");
            var values = withRule.map(d => d.value).sort((a, b) => a - b);
            assertEqual([5, 15], values);
            assertEqual(withRule, withoutRule, "Results with and without rule should match");
        }
    };
}

jsunity.run(NewAqlReplaceAnyWithINTestSuite);

return jsunity.done();

