/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual */

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
// //////////////////////////////////////////////////////////////////////////////

const internal = require("internal");
const jsunity = require("jsunity");
const db = internal.db;
const helper = require("@arangodb/aql-helper");
const assertQueryError = helper.assertQueryError;
const errors = internal.errors;

function destructuringSuite () {
  return {
    testArrayDestructuringNonArraySource : function ()  {
      [ 
        null,
        false,
        true,
        -1,
        1045,
        "",
        " ",
        "foobar"
      ].forEach(function(value) {
        let query = `LET a = ${JSON.stringify(value)} LET [x] = a RETURN x`;
        let res = db._query(query).toArray();
        assertEqual(null, res[0]);
      });
    },
    
    testArrayDestructuringDirectAssignment : function ()  {
      let query = `LET [] = [] RETURN {}`;
      // should just parse correctly
      let res = db._query(query).toArray();
      assertEqual({}, res[0]);
      
      query = `LET [y, z] = [] RETURN {y, z}`;
      res = db._query(query).toArray();
      assertEqual({y: null, z: null}, res[0]);
      
      query = `LET [y, z] = [1] RETURN {y, z}`;
      res = db._query(query).toArray();
      assertEqual({y: 1, z: null}, res[0]);
      
      query = `LET [y, z] = [1, 2] RETURN {y, z}`;
      res = db._query(query).toArray();
      assertEqual({y: 1, z: 2}, res[0]);
      
      query = `LET [y, z] = [1, 2, 3] RETURN {y, z}`;
      res = db._query(query).toArray();
      assertEqual({y: 1, z: 2}, res[0]);
      
      query = `LET [, y, z] = [1, 2, 3] RETURN {y, z}`;
      res = db._query(query).toArray();
      assertEqual({y: 2, z: 3}, res[0]);
      
      query = `LET [, , y, z] = [1, 2, 3] RETURN {y, z}`;
      res = db._query(query).toArray();
      assertEqual({y: 3, z: null}, res[0]);
    },
      
    testArrayDestructuringEmptySourceNoAssignments : function ()  {
      let query = `LET a = [] LET [] = a RETURN {}`;
      // this should simply parse correctly
      let res = db._query(query).toArray();
      assertEqual({}, res[0]);
    },
    
    testArrayDestructuringEmptySourceSomeAssignments : function ()  {
      let query = `LET a = [] LET [x] = a RETURN {x}`;
      let res = db._query(query).toArray();
      assertEqual({x: null}, res[0]);
      
      query = `LET a = [] LET [x, y] = a RETURN {x, y}`;
      res = db._query(query).toArray();
      assertEqual({x: null, y: null}, res[0]);
      
      query = `LET a = [] LET [x, y, z] = a RETURN {x, y, z}`;
      res = db._query(query).toArray();
      assertEqual({x: null, y: null, z: null}, res[0]);
    },
    
    testArrayDestructuringEmptySourceSomeAssignmentsHoles : function ()  {
      let query = `LET a = [] LET [, y, z] = a RETURN {y, z}`;
      let res = db._query(query).toArray();
      assertEqual({y: null, z: null}, res[0]);
      
      query = `LET a = [] LET [, , z] = a RETURN {z}`;
      res = db._query(query).toArray();
      assertEqual({z: null}, res[0]);
      
      query = `LET a = [] LET [x, , z] = a RETURN {x, z}`;
      res = db._query(query).toArray();
      assertEqual({x: null, z: null}, res[0]);
      
      query = `LET a = [] LET [x, y] = a RETURN {x, y}`;
      res = db._query(query).toArray();
      assertEqual({x: null, y: null}, res[0]);
      
      query = `LET a = [] LET [, y, z] = a RETURN {y, z}`;
      res = db._query(query).toArray();
      assertEqual({y: null, z: null}, res[0]);
    },
    
    testArrayDestructuringSomeAssignments : function ()  {
      let query = `LET a = [1, 2, 3, 4, 5] LET [x] = a RETURN {x}`;
      let res = db._query(query).toArray();
      assertEqual({x: 1}, res[0]);
      
      query = `LET a = [1, 2, 3, 4, 5] LET [x, y] = a RETURN {x, y}`;
      res = db._query(query).toArray();
      assertEqual({x: 1, y: 2}, res[0]);
      
      query = `LET a = [1, 2, 3, 4, 5] LET [x, y, z] = a RETURN {x, y, z}`;
      res = db._query(query).toArray();
      assertEqual({x: 1, y: 2, z: 3}, res[0]);
      
      query = `LET a = [1, 2, 3, 4, 5] LET [, x, y, z] = a RETURN {x, y, z}`;
      res = db._query(query).toArray();
      assertEqual({x: 2, y: 3, z: 4}, res[0]);
      
      query = `LET a = [1, 2, 3, 4, 5] LET [, , x, y, z] = a RETURN {x, y, z}`;
      res = db._query(query).toArray();
      assertEqual({x: 3, y: 4, z: 5}, res[0]);
      
      query = `LET a = [1, 2, 3, 4, 5] LET [, , , x, y, z] = a RETURN {x, y, z}`;
      res = db._query(query).toArray();
      assertEqual({x: 4, y: 5, z: null}, res[0]);
      
      query = `LET a = [1, 2, 3, 4, 5] LET [, , , , x, y, z] = a RETURN {x, y, z}`;
      res = db._query(query).toArray();
      assertEqual({x: 5, y: null, z: null}, res[0]);
      
      query = `LET a = [1, 2, 3, 4, 5] LET [, , , , , x, y, z] = a RETURN {x, y, z}`;
      res = db._query(query).toArray();
      assertEqual({x: null, y: null, z: null}, res[0]);
      
      query = `LET a = [1, 2, 3, 4, 5] LET [x, , z] = a RETURN {x, z}`;
      res = db._query(query).toArray();
      assertEqual({x: 1, z: 3}, res[0]);
      
      query = `LET a = [1, 2, 3, 4, 5] LET [, z] = a RETURN {z}`;
      res = db._query(query).toArray();
      assertEqual({z: 2}, res[0]);
      
      query = `LET a = [1, 2, 3, 4, 5] LET [, , z] = a RETURN {z}`;
      res = db._query(query).toArray();
      assertEqual({z: 3}, res[0]);
      
      query = `LET a = [1, 2, 3, 4, 5] LET [, , , z] = a RETURN {z}`;
      res = db._query(query).toArray();
      assertEqual({z: 4}, res[0]);
      
      query = `LET a = [1, 2, 3, 4, 5] LET [, , , , z] = a RETURN {z}`;
      res = db._query(query).toArray();
      assertEqual({z: 5}, res[0]);
      
      query = `LET a = [1, 2, 3, 4, 5] LET [, , , , , z] = a RETURN {z}`;
      res = db._query(query).toArray();
      assertEqual({z: null}, res[0]);
    },
    
    testArrayDestructuringRecursive : function ()  {
      let query = `LET a = [1, ["foo", "bar"], ["baz"], [9, 8, 7, 6]] LET [a1, a2, a3, a4] = a RETURN {a1, a2, a3, a4}`;
      let res = db._query(query).toArray();
      assertEqual({a1: 1, a2: ["foo", "bar"], a3: ["baz"], a4: [9, 8, 7, 6]}, res[0]);
      
      query = `LET a = [1, ["foo", "bar"], ["baz"], [9, 8, 7, 6]] LET [[a1], [a2], [a3], [a4]] = a RETURN {a1, a2, a3, a4}`;
      res = db._query(query).toArray();
      assertEqual({a1: null, a2: "foo", a3: "baz", a4: 9}, res[0]);
      
      query = `LET a = [1, ["foo", "bar"], ["baz"], [9, 8, 7, 6]] LET [[a11, a12], [a21, a22], [a31, a32], [a41, a42, a43, a44, a45]] = a RETURN {a11, a12, a21, a22, a31, a32, a41, a42, a43, a44, a45}`;
      res = db._query(query).toArray();
      assertEqual({a11: null, a12: null, a21: "foo", a22: "bar", a31: "baz", a32: null, a41: 9, a42: 8, a43: 7, a44: 6, a45: null}, res[0]);
      
      query = `LET a = [1, [2, [3, 4, [5, 6, [7, 8]]]]] LET [a1, a2, a3, a4, a5, a6] = a RETURN {a1, a2, a3, a4, a5, a6}`;
      res = db._query(query).toArray();
      assertEqual({a1: 1, a2: [2, [3, 4, [5, 6, [7, 8]]]], a3: null, a4: null, a5: null, a6: null}, res[0]);
      
      query = `LET a = [1, [2, [3, 4, [5, 6, [7, 8]]]]] LET [a1, [a2, [a3, a4, [a5, a6]]]] = a RETURN {a1, a2, a3, a4, a5, a6}`;
      res = db._query(query).toArray();
      assertEqual({a1: 1, a2: 2, a3: 3, a4: 4, a5: 5, a6: 6}, res[0]);
    },

    testArrayDestructuringExample : function ()  {
      let query = `LET a = ["foo", "bar", "baz", [1, 2, 3], false, "meow"] LET [foo, bar, baz, [sub1, , sub3], , meow] = a RETURN { foo, bar, baz, sub1, sub3, meow }`;

      let res = db._query(query).toArray();

      assertEqual({ foo: "foo", bar: "bar", baz: "baz", sub1: 1, sub3: 3, meow: "meow" }, res[0]);
    },
    
    testObjectDestructuringNonObjectSource : function ()  {
      [ 
        null,
        false,
        true,
        -1,
        1045,
        "",
        " ",
        "foobar"
      ].forEach(function(value) {
        let query = `LET a = ${JSON.stringify(value)} LET {x} = a RETURN x`;
        let res = db._query(query).toArray();
        assertEqual(null, res[0]);
      });
    },
    
    testObjectDestructuringEmptySourceNoAssignments : function ()  {
      let query = `LET a = {} LET {} = a RETURN {}`;
      // this should simply parse correctly
      let res = db._query(query).toArray();
      assertEqual({}, res[0]);
    },
    
    testObjectDestructuringEmptySourceSomeAssignments : function ()  {
      let query = `LET a = {} LET {x} = a RETURN {x}`;
      let res = db._query(query).toArray();
      assertEqual({x: null}, res[0]);
      
      query = `LET a = {} LET {x, y} = a RETURN {x, y}`;
      res = db._query(query).toArray();
      assertEqual({x: null, y: null}, res[0]);
      
      query = `LET a = {} LET {x, y, z} = a RETURN {x, y, z}`;
      res = db._query(query).toArray();
      assertEqual({x: null, y: null, z: null}, res[0]);
    },
    
    testObjectDestructuringEmptySourceSomeAssignmentsHoles : function ()  {
      let query = `LET a = {} LET {, y, z} = a RETURN {y, z}`;
      let res = db._query(query).toArray();
      assertEqual({y: null, z: null}, res[0]);
      
      query = `LET a = {} LET {, , z} = a RETURN {z}`;
      res = db._query(query).toArray();
      assertEqual({z: null}, res[0]);
      
      query = `LET a = {} LET {x, , z} = a RETURN {x, z}`;
      res = db._query(query).toArray();
      assertEqual({x: null, z: null}, res[0]);
      
      query = `LET a = {} LET {x, y} = a RETURN {x, y}`;
      res = db._query(query).toArray();
      assertEqual({x: null, y: null}, res[0]);
      
      query = `LET a = {} LET {, y, z} = a RETURN {y, z}`;
      res = db._query(query).toArray();
      assertEqual({y: null, z: null}, res[0]);
    },
    
    testObjectDestructuringSomeAssignments : function ()  {
      let query = `LET a = {c: 1, d: 2, e: 3, f: 4, g: 5} LET {c} = a RETURN {c}`;
      let res = db._query(query).toArray();
      assertEqual({c: 1}, res[0]);
      
      query = `LET a = {c: 1, d: 2, e: 3, f: 4, g: 5} LET {c, d} = a RETURN {c, d}`;
      res = db._query(query).toArray();
      assertEqual({c: 1, d: 2}, res[0]);
      
      query = `LET a = {c: 1, d: 2, e: 3, f: 4, g: 5} LET {c, d, e} = a RETURN {c, d, e}`;
      res = db._query(query).toArray();
      assertEqual({c: 1, d: 2, e: 3}, res[0]);
      
      query = `LET a = {c: 1, d: 2, e: 3, f: 4, g: 5} LET {c, d, e, f, g, h, i} = a RETURN {c, d, e, f, g, h, i}`;
      res = db._query(query).toArray();
      assertEqual({c: 1, d: 2, e: 3, f: 4, g: 5, h: null, i: null}, res[0]);
    },
    
    testObjectDestructuringRecursive : function ()  {
      let query = `LET a = {c: 1, d: { e: 1, f: 2 }, g: { h: ["one", "two"], i: "baz" } } LET {c, d, e, f, g, h, i} = a RETURN {c, d, e, f, g, h, i}`;
      let res = db._query(query).toArray();
      assertEqual({c: 1, d: {e: 1, f: 2}, e: null, f: null, g: {h: ["one", "two"], i: "baz"}, h: null, i: null}, res[0]);
     
      query = `LET a = {c: 1, d: { e: 2, f: 3 } } LET {c, d: {e}} = a RETURN {c, e}`;
      res = db._query(query).toArray();
      assertEqual({c: 1, e: 2}, res[0]);
      
      query = `LET a = {c: 1, d: { e: 2, f: 3 } } LET {c, d: {e}} = a RETURN {c, d, e}`;
      assertQueryError(errors.ERROR_QUERY_VARIABLE_NAME_UNKNOWN.code, query);
      
      query = `LET a = {c: 1, d: { e: 2, f: 3 } } LET {c, d: {e, f}} = a RETURN {c, e, f}`;
      res = db._query(query).toArray();
      assertEqual({c: 1, e: 2, f: 3}, res[0]);
    },
    
    testObjectDestructuringRenaming : function ()  {
      let query = `LET a = {c: 1, d: 2, e: 3, f: 4, g: 5} LET {c: z} = a RETURN {z}`;
      let res = db._query(query).toArray();
      assertEqual({z: 1}, res[0]);
      
      query = `LET a = {c: 1, d: 2, e: 3, f: 4, g: 5} LET {g: z, d: l, e: h} = a RETURN {z, l, h}`;
      res = db._query(query).toArray();
      assertEqual({z: 5, l: 2, h: 3}, res[0]);
      
      query = `LET a = {c: 1, d: 2, e: 3, f: 4, g: 5} LET {d: z, f: b} = a RETURN {z, b}`;
      res = db._query(query).toArray();
      assertEqual({z: 2, b: 4}, res[0]);
      
      query = `LET a = {c: 1, d: 2, e: 3, f: 4, g: 5} LET {c: one, d: two, e: three, f: four} = a RETURN {one, two, three, four}`;
      res = db._query(query).toArray();
      assertEqual({one: 1, two: 2, three: 3, four: 4}, res[0]);
      
      query = `LET a = {c: 1, d: 2, e: 3, f: 4, g: 5} LET {c: ´foo_bar_baz´, d: ´a.b.c´} = a RETURN {´foo_bar_baz´, ´a.b.c´}`;
      res = db._query(query).toArray();
      assertEqual({"foo_bar_baz": 1, "a.b.c": 2}, res[0]);
    },

    testObjectDestructuringExample: function () {
      let query = `LET data = { title: "Scratchpad", translations: [ { locale: "de", localization_tags: [], last_edit: "2014-04-14T08:43:37", url: "/de/docs/Tools/Scratchpad", title: "JavaScript-Umgebung" } ], url: "/en-US/docs/Tools/Scratchpad" } LET { title: englishTitle, translations: [{ title: localeTitle }] } = data RETURN { englishTitle, localeTitle }`;

      let res = db._query(query).toArray();
      assertEqual({ englishTitle: "Scratchpad", localeTitle: "JavaScript-Umgebung"}, res[0]);
    },
    
    testReassigning : function ()  {
      assertQueryError(errors.ERROR_QUERY_VARIABLE_REDECLARED.code, `LET a = [1, 2, 3] LET [c, c] = a RETURN c`);
      assertQueryError(errors.ERROR_QUERY_VARIABLE_REDECLARED.code, `LET a = [1, 2, 3] LET [c, d, c] = a RETURN c`);
      assertQueryError(errors.ERROR_QUERY_VARIABLE_REDECLARED.code, `LET a = [1, 2, 3] LET [c, d, a] = a RETURN c`);
      assertQueryError(errors.ERROR_QUERY_VARIABLE_REDECLARED.code, `LET a = {c: 1, d: 2, e: 3} LET {c, c} = a RETURN c`);
      assertQueryError(errors.ERROR_QUERY_VARIABLE_REDECLARED.code, `LET a = {c: 1, d: 2, e: 3} LET {c, d, c} = a RETURN c`);
      assertQueryError(errors.ERROR_QUERY_VARIABLE_REDECLARED.code, `LET a = {c: 1, d: 2, e: 3} LET {c, d, a} = a RETURN c`);
    },

  };
}

jsunity.run(destructuringSuite);
return jsunity.done();
