/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue, AQL_EXPLAIN, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for constrained sort arangosearch rule
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2022, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

(function () {
  let db = require("@arangodb").db;
  const isEnterprise = require("internal").isEnterprise();

  return function (isSearchAlias) {
    const cn = "UnitTestsCollection";
    const cn1 = "UnitTestsCollection1";
    const vn = "UnitTestsView";
    return {
      setUp: function () {
        db._dropView(vn);
        db._drop(cn);
        db._drop(cn1);

        db._create(cn, {numberOfShards: 3});
        db._create(cn1, {numberOfShards: 3});
      },

      tearDown: function () {
        db._dropView(vn);
        db._drop(cn);
        db._drop(cn1);
      },

      /// @brief test that rule has no effect
      testNoSortedness: function () {
        if (isSearchAlias) {
          let c = db._collection(cn);
          let indexMeta = {};
          if (isEnterprise) {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
              "value",
              "value1",
              "value2"
            ]};
          } else {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested[*]"},
              "value",
              "value1",
              "value2"
            ]};
          }
          let i = c.ensureIndex(indexMeta);
          db._createView(vn, "search-alias", {indexes: [{collection: cn, index: i.name}]});
        } else {
          let meta = {};
          if (isEnterprise) {
            meta = {links: {[cn]: {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}}};
          } else {
            meta = {links: {[cn]: {includeAllFields: true, "fields": { "value_nested": {}}}}};
          }
          db._createView(vn, "arangosearch", meta);
        }

        let queries = [
          "FOR doc IN " + vn + " SORT doc.value RETURN doc",
          "FOR doc IN " + vn + " SORT doc.value ASC RETURN doc",
          "FOR doc IN " + vn + " SORT doc.value DESC RETURN doc",
          "FOR doc IN " + vn + " SORT doc RETURN doc",
          "FOR doc IN " + vn + " SORT doc._key RETURN doc",
          "FOR doc IN " + vn + " SORT doc._key ASC RETURN doc",
          "FOR doc IN " + vn + " SORT doc._key DESC RETURN doc",
          "FOR doc IN " + vn + " SORT doc.value1, doc.value2 RETURN doc",
        ];

        queries.forEach(function (query) {
          let result = AQL_EXPLAIN(query);
          assertNotEqual(-1, result.plan.nodes.map(function (node) {
            return node.type;
          }).indexOf("SortNode"));
          assertNotEqual(-1, result.plan.nodes.filter(node => node.type === "SortNode").map(function (node) {
            return node.strategy;
          }).indexOf("standard"));
        });
      },

      /// @brief test that rule has no effect
      testRuleSingleAttributeNoEffect: function () {
        if (isSearchAlias) {
          let c = db._collection(cn);
          let indexMeta = {};
          if (isEnterprise) {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
              "value1",
              "value",
              "value2"
            ], primarySort: {fields: [{field: "value", direction: "asc"}]}};
          } else {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested[*]"},
              "value1",
              "value",
              "value2"
            ], primarySort: {fields: [{field: "value", direction: "asc"}]}};
          }
          let i = c.ensureIndex(indexMeta);
          db._createView(vn, "search-alias", {indexes: [{collection: cn, index: i.name}]});
        } else {
          let meta = {};
          if (isEnterprise) {
            meta = {links: {[cn]: {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}},
            primarySort: [{field: "value", direction: "asc"}]}}};
          } else {
            meta = {links: {[cn]: {includeAllFields: true, "fields": { "value_nested":{}} }},
            primarySort: [{field: "value", direction: "asc"}]};
          }
          db._createView(vn, "arangosearch", meta);
        }

        let queries = [
          "FOR doc IN " + vn + " SORT doc.value1 RETURN doc",
          "FOR doc IN " + vn + " SORT doc.value1 ASC RETURN doc",
          "FOR doc IN " + vn + " SORT doc.value1 DESC RETURN doc",
          "FOR doc IN " + vn + " SORT doc.value1, doc.value2 RETURN doc",
          "FOR doc IN " + vn + " SORT doc.value2, doc.value1 RETURN doc",
          "FOR doc IN " + vn + " SORT doc.value2, doc.value1 RETURN doc",
          "FOR doc IN " + vn + " SORT doc.value, doc.value1 RETURN doc",
          "FOR doc IN " + vn + " SORT doc.value1, doc.value RETURN doc",
        ];

        queries.forEach(function (query) {
          let result = AQL_EXPLAIN(query);
          assertNotEqual(-1, result.plan.nodes.map(function (node) {
            return node.type;
          }).indexOf("SortNode"));
          assertNotEqual(-1, result.plan.nodes.filter(node => node.type === "SortNode").map(function (node) {
            return node.strategy;
          }).indexOf("standard"));
        });
      },

      /// @brief test that rule has no effect
      testRuleMultipleAttributesNoEffect: function () {
        if (isSearchAlias) {
          let c = db._collection(cn);
          let indexMeta = {};
          if (isEnterprise) {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
              "value",
              "value1",
              "value2"
            ], primarySort: {fields: [{field: "value1", direction: "asc"}, {field: "value2", direction: "asc"}]}};
          } else {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested[*]"},
              "value",
              "value1",
              "value2"
            ], primarySort: {fields: [{field: "value1", direction: "asc"}, {field: "value2", direction: "asc"}]}};
          }
          let i = c.ensureIndex(indexMeta);
          db._createView(vn, "search-alias", {indexes: [{collection: cn, index: i.name}]});
        } else {
          let meta = {};
          if (isEnterprise) {
            meta = {links: {[cn]: {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}},
            primarySort: [{field: "value1", direction: "asc"}, {field: "value2", direction: "asc"}]};
          } else {
            meta = {links: {[cn]: {includeAllFields: true, "fields": { "value_nested":{}}}},
            primarySort: [{field: "value1", direction: "asc"}, {field: "value2", direction: "asc"}]};
          }
          db._createView(vn, "arangosearch", meta);
        }

        let queries = [
          "FOR doc IN " + vn + " SORT doc.value1, doc.value2 DESC RETURN doc",
          "FOR doc IN " + vn + " SORT doc.value1 ASC, doc.value2 DESC RETURN doc",
          "FOR doc IN " + vn + " SORT doc.value1 DESC, doc.value2 RETURN doc",
          "FOR doc IN " + vn + " SORT doc.value1 DESC, doc.value2 ASC RETURN doc",
          "FOR doc IN " + vn + " SORT doc.value2, doc.value1 RETURN doc",
          "FOR doc IN " + vn + " SORT doc.value1, doc.value3 RETURN doc",
          "FOR doc IN " + vn + " SORT doc.value1, doc.value1 RETURN doc",
          "FOR doc IN " + vn + " SORT doc.value2, doc.value2 RETURN doc",
          "FOR doc IN " + vn + " SORT doc.value3, doc.value4 RETURN doc",
          "FOR doc IN " + vn + " SORT doc.value1, doc.value2, doc.value3 RETURN doc",
        ];

        queries.forEach(function (query) {
          let result = AQL_EXPLAIN(query);
          assertNotEqual(-1, result.plan.nodes.map(function (node) {
            return node.type;
          }).indexOf("SortNode"), query);
          assertNotEqual(-1, result.plan.nodes.filter(node => node.type === "SortNode").map(function (node) {
            return node.strategy;
          }).indexOf("standard"));
        });
      },

      /// @brief test that rule has no effect
      testRuleSingleAttributeDifferentOrderNoEffect: function () {
        if (isSearchAlias) {
          let c = db._collection(cn);
          let indexMeta = {};
          if (isEnterprise) {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
              "value",
              "value1",
              "value2"
            ], primarySort: {fields: [{field: "value", direction: "asc"}]}};
          } else {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested[*]"},
              "value",
              "value1",
              "value2"
            ], primarySort: {fields: [{field: "value", direction: "asc"}]}};
          }
          let i = c.ensureIndex(indexMeta);
          db._createView(vn, "search-alias", {indexes: [{collection: cn, index: i.name}]});
        } else {
          let meta = {};
          if (isEnterprise) {
            meta = {links: {[cn]: {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}},
            primarySort: [{field: "value", direction: "asc"}]};
          } else {
            meta = {links: {[cn]: {includeAllFields: true, "fields": { "value_nested":{}}}},
            primarySort: [{field: "value", direction: "asc"}]};
          }
          db._createView(vn, "arangosearch", meta);
        }

        let queries = [
          "FOR doc IN " + vn + " SORT doc.value DESC RETURN doc",
          "FOR doc IN " + vn + " SORT doc.value DESC, doc.value DESC RETURN doc",
        ];

        queries.forEach(function (query) {
          let result = AQL_EXPLAIN(query);
          assertNotEqual(-1, result.plan.nodes.map(function (node) {
            return node.type;
          }).indexOf("SortNode"));
          assertNotEqual(-1, result.plan.nodes.filter(node => node.type === "SortNode").map(function (node) {
            return node.strategy;
          }).indexOf("standard"));
        });
      },

      /// @brief test that rule has no effect
      testRuleMultipleAttributesDifferentOrderNoEffect: function () {
        if (isSearchAlias) {
          let c = db._collection(cn);
          let indexMeta = {};
          if (isEnterprise) {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
              "value",
              "value1",
              "value2"
            ], primarySort: {fields: [{field: "value1", direction: "asc"}, {field: "value2", direction: "asc"}]}};
          } else {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested[*]"},
              "value",
              "value1",
              "value2"
            ], primarySort: {fields: [{field: "value1", direction: "asc"}, {field: "value2", direction: "asc"}]}};
          }
          let i = c.ensureIndex(indexMeta);
          db._createView(vn, "search-alias", {indexes: [{collection: cn, index: i.name}]});
        } else {
          let meta = {};
          if (isEnterprise) {
            meta = {links: {[cn]: {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}},
            primarySort: [{field: "value1", direction: "asc"}, {field: "value2", direction: "asc"}]};
          } else {
            meta = {links: {[cn]: {includeAllFields: true, "fields": { "value_nested":{}}}},
            primarySort: [{field: "value1", direction: "asc"}, {field: "value2", direction: "asc"}]};
          }
          db._createView(vn, "arangosearch", meta);
        }

        let queries = [
          "FOR doc IN " + vn + " SORT doc.value1 DESC RETURN doc",
          "FOR doc IN " + vn + " SORT doc.value2 ASC RETURN doc",
          "FOR doc IN " + vn + " SORT doc.value2 DESC RETURN doc",
          "FOR doc IN " + vn + " SORT doc.value1 ASC, doc.value2 DESC RETURN doc",
          "FOR doc IN " + vn + " SORT doc.value1 DESC, doc.value2 ASC RETURN doc",
        ];

        queries.forEach(function (query) {
          let result = AQL_EXPLAIN(query);
          assertNotEqual(-1, result.plan.nodes.map(function (node) {
            return node.type;
          }).indexOf("SortNode"));
          assertNotEqual(-1, result.plan.nodes.filter(node => node.type === "SortNode").map(function (node) {
            return node.strategy;
          }).indexOf("standard"));
        });
      },

      /// @brief test that rule has an effect
      testRuleSingleAttributeHasEffect: function () {
        if (isSearchAlias) {
          let indexMeta = {};
          if (isEnterprise) {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
              "value",
              "value1",
              "value2"
            ], primarySort: {fields: [{field: "value", direction: "asc"}]}};
          } else {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested[*]"},
              "value",
              "value1",
              "value2"
            ], primarySort: {fields: [{field: "value", direction: "asc"}]}};
          }
          let c = db._collection(cn);
          let i = c.ensureIndex(indexMeta);
          let c1 = db._collection(cn1);
          let i1 = c1.ensureIndex(indexMeta);
          db._createView(vn, "search-alias", {
            indexes: [{collection: cn, index: i.name}, {collection: cn1, index: i1.name}]
          });
        } else {
          let meta = {};
          if (isEnterprise) {
            meta = {links: {
              [cn]: {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}},
              [cn1]: {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}},
              primarySort: [{field: "value", direction: "asc"}]};
          } else {
            meta = {
              links: {[cn]: {includeAllFields: true, "fields": { "value_nested":{}}}, [cn1]: {includeAllFields: true, "fields": { "value_nested":{}}}},
              primarySort: [{field: "value", direction: "asc"}]
            };
          }
          db._createView(vn, "arangosearch", meta);
        }

        let queries = [
          "FOR doc IN " + vn + " SORT doc.value RETURN doc",
          "FOR doc IN " + vn + " SORT doc.value ASC RETURN doc",
        ];

        queries.forEach(function (query) {
          let result = AQL_EXPLAIN(query);
          assertEqual(-1, result.plan.nodes.map(function (node) {
            return node.type;
          }).indexOf("SortNode"));
        });
      },

      /// @brief test that rule has an effect
      testRuleMultipleAttributesHasEffect: function () {
        if (isSearchAlias) {
          let c = db._collection(cn);
          let indexMeta = {};
          if (isEnterprise) {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
              "value",
              "value1",
              "value2"
            ], primarySort: {fields: [{field: "value1", direction: "asc"}, {field: "value2", direction: "asc"}]}};
          } else {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested[*]"},
              "value",
              "value1",
              "value2"
            ], primarySort: {fields: [{field: "value1", direction: "asc"}, {field: "value2", direction: "asc"}]}};
          }
          let i = c.ensureIndex(indexMeta);
          let c1 = db._collection(cn1);
          let i1 = c1.ensureIndex(indexMeta);
          db._createView(vn, "search-alias", {
            indexes: [{collection: cn, index: i.name}, {collection: cn1, index: i1.name}]
          });
        } else {
          let meta = {};
          if (isEnterprise) {
            meta = {links: {
              [cn]: {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}},
              [cn1]: {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}},
              primarySort: [{field: "value1", direction: "asc"}, {field: "value2", direction: "asc"}]};
          } else {
            meta = {
              links: {[cn]: {includeAllFields: true, "fields": { "value_nested":{}}}, [cn1]: {includeAllFields: true, "fields": { "value_nested":{}}}},
              primarySort: [{field: "value1", direction: "asc"}, {field: "value2", direction: "asc"}]
            };
          }
          db._createView(vn, "arangosearch", meta);
        }

        let queries = [
          "FOR doc IN " + vn + " SORT doc.value1 RETURN doc",
          "FOR doc IN " + vn + " SORT doc.value1 ASC RETURN doc",
          "FOR doc IN " + vn + " SORT doc.value1, doc.value2 RETURN doc",
          "FOR doc IN " + vn + " SORT doc.value1, doc.value2 ASC RETURN doc",
        ];

        queries.forEach(function (query) {
          let result = AQL_EXPLAIN(query);
          assertEqual(-1, result.plan.nodes.map(function (node) {
            return node.type;
          }).indexOf("SortNode"));
        });
      },

      /// @brief test that rule has an effect
      testRuleSingleAttributeResults: function () {
        if (isSearchAlias) {
          let c = db._collection(cn);
          let indexMeta = {};
          if (isEnterprise) {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
              "value",
              "value1",
              "value2"
            ], primarySort: {fields: [{field: "value", direction: "asc"}]}};
          } else {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested[*]"},
              "value",
              "value1",
              "value2"
            ], primarySort: {fields: [{field: "value", direction: "asc"}]}};
          }
          let i = c.ensureIndex(indexMeta);
          let c1 = db._collection(cn1);
          let i1 = c1.ensureIndex(indexMeta);
          db._createView(vn, "search-alias", {
            indexes: [{collection: cn, index: i.name}, {collection: cn1, index: i1.name}]
          });
        } else {
          let meta = {};
          if (isEnterprise) {
            meta = {links: {
              [cn]: {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}},
              [cn1]: {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}},
            primarySort: [{field: "value", direction: "asc"}]};
          } else {
            meta = {
              links: {[cn]: {includeAllFields: true, "fields": { "value_nested":{}}}, [cn1]: {includeAllFields: true, "fields": { "value_nested":{}}}},
              primarySort: [{field: "value", direction: "asc"}]
            };
          }
          db._createView(vn, "arangosearch", meta);
        }

        // insert in reverse order
        let values = [];
        for (let i = 0; i < 2000; ++i) {
          values.push({value: "test" + (4000 - i)});
          values.push({ value: "test" + (4000 + (i + 1)), name_1: i.toString(), "value_nested": [{ "nested_1": [{ "nested_2": `foo${i}`}]}]});
        }
        db[cn].insert(values);

        values = [];
        for (let i = 2000; i < 4000; ++i) {
          values.push({value: "test" + (4000 - i)});
          values.push({value: "test" + (4000 + (i + 1)), name_2: i.toString(), "value_nested": [{ "nested_1": [{ "nested_2": `bar${i}`}]}]});
        }
        db[cn1].insert(values);

        let query = "FOR doc IN " + vn + " OPTIONS { waitForSync: true } SORT doc.value RETURN doc";
        let result = AQL_EXPLAIN(query);
        assertEqual(-1, result.plan.nodes.map(function (node) {
          return node.type;
        }).indexOf("SortNode"));

        result = AQL_EXECUTE(query).json;
        assertEqual(2 * 4000, result.length);
        let last = "";
        result.forEach(function (doc) {
          assertTrue(doc.value > last);
          last = doc.value;
        });
      },

      /// @brief test that rule has an effect
      testRuleSingleAttributeResultsDesc: function () {
        if (isSearchAlias) {
          let c = db._collection(cn);
          let indexMeta = {};
          if (isEnterprise) {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
              "value",
              "value1",
              "value2"
            ], primarySort: {fields: [{field: "value", direction: "desc"}]}};
          } else {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested[*]"},
              "value",
              "value1",
              "value2"
            ], primarySort: {fields: [{field: "value", direction: "desc"}]}};
          }
          let i = c.ensureIndex(indexMeta);
          let c1 = db._collection(cn1);
          let i1 = c1.ensureIndex(indexMeta);
          db._createView(vn, "search-alias", {
            indexes: [{collection: cn, index: i.name}, {collection: cn1, index: i1.name}]
          });
        } else {
          let meta = {};
          if (isEnterprise) {
            meta = {links: {
              [cn]: {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}},
              [cn1]: {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}},
              primarySort: [{field: "value", direction: "desc"}]};
          } else {
            meta = {
              links: {[cn]: {includeAllFields: true, "fields": { "value_nested":{}}}, [cn1]: {includeAllFields: true, "fields": { "value_nested":{}}}},
              primarySort: [{field: "value", direction: "desc"}]
            };
          }
          db._createView(vn, "arangosearch", meta);
        }
        // insert in forward order
        let values = [];
        for (let i = 0; i < 2000; ++i) {
          values.push({value: "test" + i});
        }
        db[cn].insert(values);

        values = [];
        for (let i = 2000; i < 4000; ++i) {
          values.push({value: "test" + i});
        }
        db[cn1].insert(values);

        let query = "FOR doc IN " + vn + " OPTIONS { waitForSync: true } SORT doc.value DESC RETURN doc";
        let result = AQL_EXPLAIN(query);
        assertEqual(-1, result.plan.nodes.map(function (node) {
          return node.type;
        }).indexOf("SortNode"));

        result = AQL_EXECUTE(query).json;
        assertEqual(4000, result.length);
        let last = "test999999999";
        result.forEach(function (doc) {
          assertTrue(doc.value < last);
          last = doc.value;
        });
      },

      /// @brief test that rule has an effect
      testRuleMultipleAttributesResults: function () {
        if (isSearchAlias) {
          let c = db._collection(cn);
          let indexMeta = {};
          if (isEnterprise) {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
              "value",
              "value1",
              "value2"
            ], primarySort: {fields: [{field: "value1", direction: "asc"}, {field: "value2", direction: "asc"}]}};
          } else {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested[*]"},
              "value",
              "value1",
              "value2"
            ], primarySort: {fields: [{field: "value1", direction: "asc"}, {field: "value2", direction: "asc"}]}};
          }
          let i = c.ensureIndex(indexMeta);
          let c1 = db._collection(cn1);
          let i1 = c1.ensureIndex(indexMeta);
          db._createView(vn, "search-alias", {
            indexes: [{collection: cn, index: i.name}, {collection: cn1, index: i1.name}]
          });
        } else {
          let meta = {};
          if (isEnterprise) {
            meta = {links: {
              [cn]: {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}},
              [cn1]: {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}},
              primarySort: [{field: "value1", direction: "asc"}, {field: "value2", direction: "asc"}]};
          } else {
            meta = {
              links: {[cn]: {includeAllFields: true, "fields": { "value_nested":{}}}, [cn1]: {includeAllFields: true, "fields": { "value_nested":{}}}},
              primarySort: [{field: "value1", direction: "asc"}, {field: "value2", direction: "asc"}]
            };
          }
          db._createView(vn, "arangosearch", meta);
        }

        // insert in reverse order
        let values = [];
        for (let i = 0; i < 2000; ++i) {
          values.push({value1: (4000 - i), value2: i});
        }
        db[cn].insert(values);

        values = [];
        for (let i = 2000; i < 4000; ++i) {
          values.push({value1: (4000 - i), value2: i});
        }
        db[cn1].insert(values);

        let query = "FOR doc IN " + vn + " OPTIONS { waitForSync: true } SORT doc.value1, doc.value2 RETURN doc";
        let result = AQL_EXPLAIN(query);
        assertEqual(-1, result.plan.nodes.map(function (node) {
          return node.type;
        }).indexOf("SortNode"));

        result = AQL_EXECUTE(query).json;
        assertEqual(4000, result.length);
        let last1 = -1;
        let last2 = 99999;
        result.forEach(function (doc) {
          assertTrue(doc.value1 > last1);
          assertTrue(doc.value2 < last2);
          last1 = doc.value1;
          last2 = doc.value2;
        });
      },

      /// @brief test that rule has an effect
      testRuleMultipleAttributesResultsDesc: function () {
        if (isSearchAlias) {
          let c = db._collection(cn);
          let indexMeta = {};
          if (isEnterprise) {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
              "value",
              "value1",
              "value2"
            ], primarySort: {fields: [{field: "value1", direction: "desc"}, {field: "value2", direction: "asc"}]}};
          } else {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested[*]"},
              "value",
              "value1",
              "value2"
            ], primarySort: {fields: [{field: "value1", direction: "desc"}, {field: "value2", direction: "asc"}]}};
          }
          let i = c.ensureIndex(indexMeta);
          let c1 = db._collection(cn1);
          let i1 = c1.ensureIndex(indexMeta);
          db._createView(vn, "search-alias", {
            indexes: [{collection: cn, index: i.name}, {collection: cn1, index: i1.name}]
          });
        } else {
          let meta = {};
          if (isEnterprise) {
            meta = {links: {
              [cn]: {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}},
              [cn1]: {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}},
              primarySort: [{field: "value1", direction: "desc"}, {field: "value2", direction: "asc"}]};
          } else {
            meta = {
              links: {[cn]: {includeAllFields: true, "fields": { "value_nested":{}}}, [cn1]: {includeAllFields: true, "fields": { "value_nested":{}}}},
              primarySort: [{field: "value1", direction: "desc"}, {field: "value2", direction: "asc"}]
            };
          }
          db._createView(vn, "arangosearch", meta);
        }

        // insert in forward order
        let values = [];
        for (let i = 0; i < 2000; ++i) {
          values.push({value1: i, value2: i});
        }
        db[cn].insert(values);

        values = [];
        for (let i = 2000; i < 4000; ++i) {
          values.push({value1: i, value2: i});
        }
        db[cn1].insert(values);

        let query = "FOR doc IN " + vn + " OPTIONS { waitForSync: true } SORT doc.value1 DESC, doc.value2 RETURN doc";
        let result = AQL_EXPLAIN(query);
        assertEqual(-1, result.plan.nodes.map(function (node) {
          return node.type;
        }).indexOf("SortNode"));

        result = AQL_EXECUTE(query).json;
        assertEqual(4000, result.length);
        let last1 = 99999;
        let last2 = 99999;
        result.forEach(function (doc) {
          assertTrue(doc.value1 < last1);
          assertTrue(doc.value2 < last2);
          last1 = doc.value1;
          last2 = doc.value2;
        });
      },

      /// @brief test that rule has an effect
      testRuleSameAttributeValuesDesc: function () {
        if (isSearchAlias) {
          let c = db._collection(cn);
          let indexMeta = {};
          if (isEnterprise) {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
              "value",
              "value1",
              "value2"
            ], primarySort: {fields: [{field: "value1", direction: "desc"}, {field: "value2", direction: "asc"}]}};
          } else {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested[*]"},
              "value",
              "value1",
              "value2"
            ], primarySort: {fields: [{field: "value1", direction: "desc"}, {field: "value2", direction: "asc"}]}};
          }
          let i = c.ensureIndex(indexMeta);
          let c1 = db._collection(cn1);
          let i1 = c1.ensureIndex(indexMeta);
          db._createView(vn, "search-alias", {
            indexes: [{collection: cn, index: i.name}, {collection: cn1, index: i1.name}]
          });
        } else {
          let meta = {};
          if (isEnterprise) {
            meta = {links: {
              [cn]: {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}},
              [cn1]: {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}},
              primarySort: [{field: "value1", direction: "desc"}, {field: "value2", direction: "asc"}]};
          } else {
            meta = {
              links: {[cn]: {includeAllFields: true, "fields": { "value_nested":{}}}, [cn1]: {includeAllFields: true, "fields": { "value_nested":{}}}},
              primarySort: [{field: "value1", direction: "desc"}, {field: "value2", direction: "asc"}]
            };
          }
          db._createView(vn, "arangosearch", meta);
        }

        // insert in forward order
        let values = [];
        for (let i = 0; i < 2000; ++i) {
          values.push({value1: "same", value2: (4000 - i)});
        }
        db[cn].insert(values);

        values = [];
        for (let i = 2000; i < 4000; ++i) {
          values.push({value1: "same", value2: (4000 - i)});
        }
        db[cn1].insert(values);

        let query = "FOR doc IN " + vn + " OPTIONS { waitForSync: true } SORT doc.value1 DESC, doc.value2 RETURN doc";
        let result = AQL_EXPLAIN(query);
        assertEqual(-1, result.plan.nodes.map(function (node) {
          return node.type;
        }).indexOf("SortNode"));

        result = AQL_EXECUTE(query).json;
        assertEqual(4000, result.length);
        let last2 = -1;
        result.forEach(function (doc) {
          assertEqual("same", doc.value1);
          assertTrue(doc.value2 > last2);
          last2 = doc.value2;
        });
      },

      /// @brief test that rule has an effect
      testRuleSameAttributeValuesDescFullCount: function () {
        if (isSearchAlias) {
          let indexMeta = {};
          if (isEnterprise) {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
              "value",
              "value1",
              "value2"
            ], primarySort: {fields: [{field: "value1", direction: "desc"}, {field: "value2", direction: "asc"}]}};
          } else {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested[*]"},
              "value",
              "value1",
              "value2"
            ], primarySort: {fields: [{field: "value1", direction: "desc"}, {field: "value2", direction: "asc"}]}};
          }
          let c = db._collection(cn);
          let i = c.ensureIndex(indexMeta);
          let c1 = db._collection(cn1);
          let i1 = c1.ensureIndex(indexMeta);
          db._createView(vn, "search-alias", {
            indexes: [{collection: cn, index: i.name}, {collection: cn1, index: i1.name}]
          });
        } else {
          let meta = {};
          if (isEnterprise) {
            meta = {links: {
              [cn]: {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}},
              [cn1]: {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}},
              primarySort: [{field: "value1", direction: "desc"}, {field: "value2", direction: "asc"}]};
          } else {
            meta = {
              links: {[cn]: {includeAllFields: true, "fields": { "value_nested":{}}}, [cn1]: {includeAllFields: true, "fields": { "value_nested":{}}}},
              primarySort: [{field: "value1", direction: "desc"}, {field: "value2", direction: "asc"}]
            };
          }
          db._createView(vn, "arangosearch", meta);
        }

        // insert in forward order
        let values = [];
        for (let i = 0; i < 2000; ++i) {
          values.push({value1: "same", value2: (4000 - i)});
        }
        db[cn].insert(values);

        values = [];
        for (let i = 2000; i < 4000; ++i) {
          values.push({value1: "same", value2: (4000 - i)});
        }
        db[cn1].insert(values);

        let query = "FOR doc IN " + vn + " OPTIONS { waitForSync: true } SORT doc.value1 DESC, doc.value2 RETURN doc";
        let result = AQL_EXPLAIN(query);
        assertEqual(-1, result.plan.nodes.map(function (node) {
          return node.type;
        }).indexOf("SortNode"));

        result = AQL_EXECUTE(query, {}, {fullCount: true});
        assertEqual(4000, result.json.length);
        assertEqual(4000, result.stats.fullCount);
        let last2 = -1;
        result.json.forEach(function (doc) {
          assertEqual("same", doc.value1);
          assertTrue(doc.value2 > last2);
          last2 = doc.value2;
        });
      },

      /// @brief test that rule has an effect
      testRuleMultipleAttributesResultsDescWithOffset: function () {
        if (isSearchAlias) {
          let indexMeta = {};
          if (isEnterprise) {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
              "value",
              "value1",
              "value2"
            ], primarySort: {fields: [{field: "value1", direction: "desc"}, {field: "value2", direction: "asc"}]}};
          } else {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested[*]"},
              "value",
              "value1",
              "value2"
            ], primarySort: {fields: [{field: "value1", direction: "desc"}, {field: "value2", direction: "asc"}]}};
          }
          let c = db._collection(cn);
          let i = c.ensureIndex(indexMeta);
          let c1 = db._collection(cn1);
          let i1 = c1.ensureIndex(indexMeta);
          db._createView(vn, "search-alias", {
            indexes: [{collection: cn, index: i.name}, {collection: cn1, index: i1.name}]
          });
        } else {
          let meta = {};
          if (isEnterprise) {
            meta = {links: {
              [cn]: {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}},
              [cn1]: {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}},
              primarySort: [{field: "value1", direction: "desc"}, {field: "value2", direction: "asc"}]};
          } else {
            meta = {
              links: {[cn]: {includeAllFields: true, "fields": { "value_nested":{}}}, [cn1]: {includeAllFields: true, "fields": { "value_nested":{}}}},
              primarySort: [{field: "value1", direction: "desc"}, {field: "value2", direction: "asc"}]
            };
          }
          db._createView(vn, "arangosearch", meta);
        }

        // insert in forward order
        let firstHalf = [];
        for (let i = 0; i < 2000; ++i) {
          firstHalf.push({value1: i, value2: i});
        }
        db[cn].insert(firstHalf);

        let secondHalf = [];
        for (let i = 2000; i < 4000; ++i) {
          secondHalf.push({value1: i, value2: i});
        }
        db[cn1].insert(secondHalf);

        let query = "FOR doc IN " + vn + " OPTIONS { waitForSync: true } SORT doc.value1 DESC, doc.value2 LIMIT 2000, 100000 RETURN doc";
        let result = AQL_EXPLAIN(query);
        assertEqual(-1, result.plan.nodes.map(function (node) {
          return node.type;
        }).indexOf("SortNode"));
        firstHalf.sort(function (lhs, rhs) {
          if (lhs.value1 === rhs.value1) {
            if (lhs.value2 === rhs.value2) {
              return 0;
            }

            if (lhs.value2 < rhs.value2) {
              return -1;
            }

            return 1;
          }

          if (lhs.value1 < rhs.value1) {
            return 1;
          }

          return -1;
        });

        result = AQL_EXECUTE(query).json;
        assertEqual(firstHalf.length, result.length);
        let last1 = 99999;
        let last2 = 99999;
        let i = 0;
        result.forEach(function (doc) {
          assertEqual(doc.value1, firstHalf[i].value1);
          assertEqual(doc.value2, firstHalf[i].value2);
          assertTrue(doc.value1 < last1);
          assertTrue(doc.value2 < last2);
          last1 = doc.value1;
          last2 = doc.value2;
          ++i;
        });
      },

      testRuleMultipleAttributesResultsDescWithOffsetFullCount: function () {
        if (isSearchAlias) {
          let indexMeta = {};
          if (isEnterprise) {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
              "value",
              "value1",
              "value2"
            ], primarySort: {fields: [{field: "value1", direction: "desc"}, {field: "value2", direction: "asc"}]}};
          } else {
            indexMeta = {type: "inverted", includeAllFields: true, fields:[
              {"name": "value_nested[*]"},
              "value",
              "value1",
              "value2"
            ], primarySort: {fields: [{field: "value1", direction: "desc"}, {field: "value2", direction: "asc"}]}};
          }
          let c = db._collection(cn);
          let i = c.ensureIndex(indexMeta);
          let c1 = db._collection(cn1);
          let i1 = c1.ensureIndex(indexMeta);
          db._createView(vn, "search-alias", {
            indexes: [{collection: cn, index: i.name}, {collection: cn1, index: i1.name}]
          });
        } else {
          let meta = {};
          if (isEnterprise) {
            meta = {links: {
              [cn]: {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}},
              [cn1]: {includeAllFields: true, "fields": { "value_nested": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}},
              primarySort: [{field: "value1", direction: "desc"}, {field: "value2", direction: "asc"}]};
          } else {
            meta = {
              links: {[cn]: {includeAllFields: true, "fields": { "value_nested":{}}}, [cn1]: {includeAllFields: true, "fields": { "value_nested":{}}}},
              primarySort: [{field: "value1", direction: "desc"}, {field: "value2", direction: "asc"}]
            };
          }
          db._createView(vn, "arangosearch", meta);
        }

        // insert in forward order
        let firstHalf = [];
        for (let i = 0; i < 2000; ++i) {
          firstHalf.push({value1: i, value2: i});
        }
        db[cn].insert(firstHalf);

        let secondHalf = [];
        for (let i = 2000; i < 4000; ++i) {
          secondHalf.push({value1: i, value2: i});
        }
        db[cn1].insert(secondHalf);

        let query = "FOR doc IN " + vn + " OPTIONS { waitForSync: true } SORT doc.value1 DESC, doc.value2 LIMIT 2000, 100 RETURN doc";
        let result = AQL_EXPLAIN(query);
        assertEqual(-1, result.plan.nodes.map(function (node) {
          return node.type;
        }).indexOf("SortNode"));
        firstHalf.sort(function (lhs, rhs) {
          if (lhs.value1 === rhs.value1) {
            if (lhs.value2 === rhs.value2) {
              return 0;
            }

            if (lhs.value2 < rhs.value2) {
              return -1;
            }

            return 1;
          }

          if (lhs.value1 < rhs.value1) {
            return 1;
          }

          return -1;
        });

        result = AQL_EXECUTE(query, {}, {fullCount: true});
        assertEqual(100, result.json.length);
        assertEqual(4000, result.stats.fullCount);
        let last1 = 99999;
        let last2 = 99999;
        let i = 0;
        result.json.forEach(function (doc) {
          assertEqual(doc.value1, firstHalf[i].value1);
          assertEqual(doc.value2, firstHalf[i].value2);
          assertTrue(doc.value1 < last1);
          assertTrue(doc.value2 < last2);
          last1 = doc.value1;
          last2 = doc.value2;
          ++i;
        });
      },
    };
  };
}());
