/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue, assertFalse, assertNull, assertMatch, fail */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const internal = require("internal");
const db = require("@arangodb").db;
const jsunity = require("jsunity");
const errors = require("internal").errors;

function alternativeSyntaxSuite() {
  const cn = "UnitTestsCollection";
  
  return {
    setUp: function () {
      let c = db._create(cn, { numberOfShards: 5 });
      const docs = [];
      for (let i = 1; i < 11; ++i) {
        docs.push({ value: i });
      }
      c.insert(docs);
    },

    tearDown: function () {
      db._drop(cn);
    },

    testAlternativeSyntaxUpdateOnly: function () {
      const q = `FOR doc IN ${cn} UPSERT FILTER $CURRENT.value == doc.value INSERT { _key: 'nono' } UPDATE { updated: true } IN ${cn}`;
      let res = db._query(q);
      assertEqual(10, res.getExtra().stats.writesExecuted);

      res = db._query(`FOR doc IN ${cn} FILTER doc.updated == true RETURN 1`).toArray();
      assertEqual(10, res.length);

      assertFalse(db[cn].exists('nono'));
    },
    
    testAlternativeSyntaxAlternativeVariableName: function () {
      // CURRENT and $CURRENT are equivalent
      const q = `FOR doc IN ${cn} UPSERT FILTER CURRENT.value == doc.value INSERT { _key: 'nono' } UPDATE { updated: true } IN ${cn}`;
      let res = db._query(q);
      assertEqual(10, res.getExtra().stats.writesExecuted);

      res = db._query(`FOR doc IN ${cn} FILTER doc.updated == true RETURN 1`).toArray();
      assertEqual(10, res.length);

      assertFalse(db[cn].exists('nono'));
    },
    
    testAlternativeSyntaxNonEqCondition: function () {
      const q = `FOR doc IN ${cn} UPSERT FILTER $CURRENT.value == doc.value && HAS($CURRENT, 'inserted') INSERT { inserted: true, value: doc.value } UPDATE { updated: true } IN ${cn}`;
      let res = db._query(q);
      assertEqual(10, res.getExtra().stats.writesExecuted);

      res = db._query(`FOR doc IN ${cn} FILTER doc.inserted == true RETURN 1`).toArray();
      assertEqual(10, res.length);
      
      res = db._query(`FOR doc IN ${cn} FILTER doc.updated == true RETURN 1`).toArray();
      assertEqual(0, res.length);
      
      // execute query again
      res = db._query(q);
      assertEqual(20, res.getExtra().stats.writesExecuted);

      res = db._query(`FOR doc IN ${cn} FILTER doc.inserted == true RETURN 1`).toArray();
      assertEqual(10, res.length);
      
      res = db._query(`FOR doc IN ${cn} FILTER doc.updated == true RETURN 1`).toArray();
      assertEqual(10, res.length);
    },
    
    testAlternativeSyntaxRepeatedCalls: function () {
      db[cn].truncate();

      const q = `UPSERT FILTER $CURRENT.cat == @cat && $CURRENT.period == @period INSERT { cat: @cat, period: @period, hits: 1 } UPDATE { hits: $OLD.hits + 1 } IN ${cn}`;

      const params = [
        {cat: "movies", period: 2001 },
        {cat: "movies", period: 2001 },
        {cat: "movies", period: 2002 },
        {cat: "movies", period: 2003 },
        {cat: "movies", period: 2004 },
        {cat: "movies", period: 2009 },
        {cat: "tools", period: 2011 },
        {cat: "tools", period: 2012 },
        {cat: "tools", period: 2012 },
        {cat: "tools", period: 2015 },
        {cat: "tools", period: 2019 },
        {cat: "tools", period: 2019 },
        {cat: "tools", period: 2019 },
        {cat: "cars", period: 2004 },
        {cat: "cars", period: 2014 },
        {cat: "cars", period: 2014 },
      ];

      params.forEach((p) => {
        let res = db._query(q, p);
        assertEqual(1, res.getExtra().stats.writesExecuted);
      });

      let res = db._query(`FOR doc IN ${cn} SORT doc.cat, doc.period RETURN [doc.cat, doc.period, doc.hits]`).toArray();
      assertEqual([
        ["cars", 2004, 1],
        ["cars", 2014, 2],
        ["movies", 2001, 2],
        ["movies", 2002, 1],
        ["movies", 2003, 1],
        ["movies", 2004, 1],
        ["movies", 2009, 1],
        ["tools", 2011, 1],
        ["tools", 2012, 2],
        ["tools", 2015, 1],
        ["tools", 2019, 3],
      ], res);
      
      // execute again
      params.forEach((p) => {
        let res = db._query(q, p);
        assertEqual(1, res.getExtra().stats.writesExecuted);
      });

      res = db._query(`FOR doc IN ${cn} SORT doc.cat, doc.period RETURN [doc.cat, doc.period, doc.hits]`).toArray();
      assertEqual([
        ["cars", 2004, 2],
        ["cars", 2014, 4],
        ["movies", 2001, 4],
        ["movies", 2002, 2],
        ["movies", 2003, 2],
        ["movies", 2004, 2],
        ["movies", 2009, 2],
        ["tools", 2011, 2],
        ["tools", 2012, 4],
        ["tools", 2015, 2],
        ["tools", 2019, 6],
      ], res);
    },
    
    testAlternativeSyntaxAccessCurrentAfterFilter: function () {
      // the CURRENT/$CURRENT variable can only be accessed inside the FILTER, but not outside
      const queries = [
        `FOR doc IN ${cn} UPSERT FILTER $CURRENT.value == doc.value INSERT { _key: $CURRENT._key } UPDATE { updated: true } IN ${cn}`,
        `FOR doc IN ${cn} UPSERT FILTER $CURRENT.value == doc.value INSERT { _key: CURRENT._key } UPDATE { updated: true } IN ${cn}`,
        `FOR doc IN ${cn} UPSERT FILTER $CURRENT.value == doc.value INSERT { _key: 'nono' } UPDATE { updated: $CURRENT.value } IN ${cn}`,
        `FOR doc IN ${cn} UPSERT FILTER $CURRENT.value == doc.value INSERT { _key: 'nono' } UPDATE { updated: CURRENT.value } IN ${cn}`,
        `FOR doc IN ${cn} UPSERT FILTER $CURRENT.value == doc.value INSERT { _key: 'nono' } UPDATE { updated: true } IN ${cn} RETURN $CURRENT`,
        `FOR doc IN ${cn} UPSERT FILTER $CURRENT.value == doc.value INSERT { _key: 'nono' } UPDATE { updated: true } IN ${cn} RETURN CURRENT`
      ];

      queries.forEach((q) => {
        try {
          db._query(q);
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
        }
      });
    },

  };
}

jsunity.run(alternativeSyntaxSuite);

return jsunity.done();
