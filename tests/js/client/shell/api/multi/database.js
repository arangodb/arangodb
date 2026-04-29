/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, fail, arango, assertTrue, assertFalse, assertEqual, assertNotUndefined */

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
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const internal = require('internal');
const sleep = internal.sleep;
const forceJson = internal.options().hasOwnProperty('server.force-json') && internal.options()['server.force-json'];
const contentType = forceJson ? "application/json" :  "application/x-velocypack";
const jsunity = require("jsunity");
const errors = internal.errors;

let api = "/_api/database";

function dealing_with_database_information_methodsSuite () {
  return {

    ////////////////////////////////////////////////////////////////////////////////;
    // retrieving the list of databases;
    ////////////////////////////////////////////////////////////////////////////////;

    test_retrieves_the_list_of_databases: function() {
      let doc = arango.GET_RAW(api);

      assertEqual(doc.code, 200);
      assertTrue(doc.parsedBody["result"].findIndex( x => x === "_system") >= 0);
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // retrieving the list of databases for the current user;
    ////////////////////////////////////////////////////////////////////////////////;

    test_retrieves_the_list_of_user_specific_databases: function() {
      let doc = arango.GET_RAW(api + "/user");

      assertEqual(doc.code, 200);
      assertTrue(doc.parsedBody["result"].findIndex( x => x === "_system") >= 0);
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // checking information about current database;
    ////////////////////////////////////////////////////////////////////////////////;

    test_retrieves_information_about_the_current_database: function() {
      let doc = arango.GET_RAW(api + "/current");

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertEqual(doc.parsedBody["result"]["name"], "_system", doc);
      assertTrue(doc.parsedBody["result"]["isSystem"]);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// Unicode names;
////////////////////////////////////////////////////////////////////////////////;
function dealing_with_Unicode_database_namesSuite () {
  return {
    test_creates_a_new_database_with_non_normalized_names: function() {
      let names = [
        "\u212b", // Angstrom, not normalized;
        "\u0041\u030a", // Angstrom, NFD-normalized;
        "\u0073\u0323\u0307", // s with two combining marks, NFD-normalized;
        "\u006e\u0303\u00f1", // non-normalized sequence;
      ];

      names.forEach(name => {
        let body = {"name" : name };
        let doc = arango.POST_RAW(api, body);

        assertEqual(doc.code, 400);
        assertEqual(doc.headers['content-type'], contentType);
        assertTrue(doc.parsedBody["error"]);
        assertEqual(doc.parsedBody["errorNum"], errors.ERROR_ARANGO_ILLEGAL_NAME.code);
      });
    },

    test_creates_a_new_database_with_proper_Unicode_names: function() {
      //  some of these test strings are taken from https://www.w3.org/2001/06/utf-8-test/UTF-8-demo.html;
      let names = [
        "mötör",
        "TRÖÖÖÖÖÖÖÖÖTKÄKÄR",
        "∮ E⋅da = Q,  n → ∞, ∑ f(i) = ∏ g(i)",
        "∀x∈ℝ ⌈x⌉ = −⌊−x⌋",
        "α ∧ ¬β = ¬(¬α ∨ β)",
        "two H₂ + O₂ ⇌ 2H₂O",
        "R = 4.7 kΩ, ⌀ 200 mm",
        "ði ıntəˈnæʃənəl fəˈnɛtık əsoʊsiˈeıʃn",
        "Y [ˈʏpsilɔn], Yen [jɛn], Yoga [ˈjoːgɑ]",
        "Οὐχὶ ταὐτὰ παρίσταταί μοι",
        "γιγνώσκειν, ὦ ἄνδρες ᾿Αθηναῖοι",
        "გთხოვთ ახლავე გაიაროთ რეგისტრაცია",
        "Unicode-ის მეათე საერთაშორისო",
        "Зарегистрируйтесь сейчас на",
        "Десятую Международную Конференцию по",
        "ሰማይ አይታረስ ንጉሥ አይከሰስ።",
        "ᚻᛖ ᚳᚹᚫᚦ ᚦᚫᛏ ᚻᛖ ᛒᚢᛞᛖ ᚩᚾ ᚦᚫᛗ",
        "ᛚᚪᚾᛞᛖ ᚾᚩᚱᚦᚹᛖᚪᚱᛞᚢᛗ ᚹᛁᚦ ᚦᚪ ᚹᛖᛥᚫ",
        "⡌⠁⠧⠑ ⠼⠁⠒  ⡍⠜⠇⠑⠹⠰⠎ ⡣⠕⠌",
        "£ ß ó ę Я λ",
        "💩🍺🌧⛈🌩⚡🔥💥🌨",
        "😀 *grinning* 😬 *grimacing* 😅 *sweat_smile* 😆 *laughing*",
        "😁 *grin* 😂 *joy* 😃 *smiley* 😄 *smile*",
        "😇 *innocent* 😉 *wink* 😊 *blush* 🙂 *slight_smile*",
        "🙃 *upside_down* 😋 *yum* 😌 *relieved* 😍 *heart_eyes*",
        "😘 *kissing_heart* 😗 *kissing* 😙 *kissing_smiling_eyes* 😚 *kissing_closed_eyes*",
        "😜 *stuck_out_tongue_winking_eye* 😝 *stuck_out_tongue_closed_eyes*",
        "😛 *stuck_out_tongue* 🤑 *money_mouth*",
        "🤓 *nerd* 😎 *sunglasses* 🤗 *hugging* 😏 *smirk*",
        "😶 *no_mouth* 😐 *neutral_face*",
        "😑 *expressionless* 😒 *unamused* 🙄 *rolling_eyes* 🤔 *thinking*",
        "😳 *flushed* 😞 *disappointed* 😟 *worried* 😠 *angry*",
        "😡 *rage* 😔 *pensive* 😕 *confused*", 
        "\u00c5", // Angstrom;
        "\u1e69", // s with two combining marks;
      ];
      names.forEach(name => {
        name = String(name).normalize("NFC");
        let body = {"name" : name };
        let doc = arango.POST_RAW(api, body);

        assertEqual(doc.code, 201);
        assertEqual(doc.headers['content-type'], contentType);
        assertTrue(doc.parsedBody["result"]);
        assertFalse(doc.parsedBody["error"]);

        let cmd = api + "/" + encodeURIComponent(name);
        doc = arango.DELETE_RAW(cmd);
        assertEqual(doc.code, 200);
        assertEqual(doc.headers['content-type'], contentType);
        assertTrue(doc.parsedBody["result"]);
        assertFalse(doc.parsedBody["error"]);
      });
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// checking information about current database;
////////////////////////////////////////////////////////////////////////////////;
function dealing_with_database_manipulation_methodsSuite () {
  let name = "UnitTestsDatabase";
  return {

    setUp: function() {
      arango.DELETE(api + `/${name}`);
    },

    tearDown: function() {
      arango.DELETE(api + `/${name}`);
      db._flushCache();
      db._users.toArray().forEach(user => {
        if (user.user !== "root") {
          arango.DELETE_RAW("/_api/user/" + encodeURIComponent(user.user));
        }
      });
    },

    test_creates_a_new_database: function() {
      let body = {"name" : name };
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody["result"]);
      assertFalse(doc.parsedBody["error"]);
    },

    test_creates_a_database_without_a_name: function() {
      let body = "{ }";
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 400);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody["error"]);
      assertEqual(doc.parsedBody["errorNum"], errors.ERROR_ARANGO_ILLEGAL_NAME.code);
    },

    test_creates_a_database_with_an_empty_name: function() {
      let body = { "name": " " };
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 400);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody["error"]);
      assertEqual(doc.parsedBody["errorNum"], errors.ERROR_ARANGO_ILLEGAL_NAME.code);
    },

    test_creates_a_database_with_an_invalid_name: function() {
      let body = {"name" : `_${name}` };
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 400);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody["error"]);
      assertEqual(doc.parsedBody["errorNum"], errors.ERROR_ARANGO_ILLEGAL_NAME.code);
    },

    test_re_creates_an_existing_database: function() {
      let body = {"name" : name };
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      doc = arango.POST_RAW(api, body);
      assertEqual(doc.code, 409);
      assertEqual(doc.headers['content-type'], contentType);

      assertTrue(doc.parsedBody["error"]);
      assertEqual(doc.parsedBody["errorNum"], 1207);
    },

    test_creates_a_database_with_users_EQ_null: function() {
      let body = {"name" : name, "users" : null };
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody["result"]);
      assertFalse(doc.parsedBody["error"]);
    },

    test_creates_a_database_with_users_EQ_empty_array: function() {
      let body = {"name" : name, "users" : [ ] };
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody["result"]);
      assertFalse(doc.parsedBody["error"]);
    },

    test_drops_an_existing_database: function() {
      let cmd = api + `/${name}`;
      let body = {"name" : name };
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      doc = arango.DELETE_RAW(cmd);
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody["result"]);
      assertFalse(doc.parsedBody["error"]);
    },

    test_drops_a_non_existing_database: function() {
      let cmd = api + `/${name}`;
      let doc = arango.DELETE_RAW(cmd);

      assertEqual(doc.code, 404);
      assertEqual(doc.headers['content-type'], contentType);
      assertEqual(doc.parsedBody["errorNum"], errors.ERROR_ARANGO_DATABASE_NOT_FOUND.code);
      assertTrue(doc.parsedBody["error"]);
    },

    test_creates_a_new_database_and_retrieves_the_properties: function() {
      let body = {"name" : name };
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody["result"]);
      assertFalse(doc.parsedBody["error"]);

      //  list of databases should include the new database;
      doc = arango.GET_RAW(api);
      assertEqual(doc.code, 200);

      assertTrue(doc.parsedBody["result"].findIndex( x => x === "_system") >= 0);
      assertTrue(doc.parsedBody["result"].findIndex( x => x === name) >= 0);

      //  retrieve information about _system database;
      doc = arango.GET_RAW(api + "/current");
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['result']["name"], "_system");
      assertTrue(doc.parsedBody['result']["isSystem"]);

      //  retrieve information about new database;
      doc = arango.GET_RAW(`/_db/${name}${api}/current`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['result']["name"], name);
      assertFalse(doc.parsedBody['result']["isSystem"]);

      doc = arango.DELETE_RAW(api + `/${name}`);
      assertEqual(doc.code, 200);
      assertTrue(doc.parsedBody["result"]);
      assertFalse(doc.parsedBody["error"]);
    },

    test_creates_a_new_database_with_two_users: function() {
      let body = {"name" : name, "users": [ { "username": "admin", "password": "secret", "extra": { "gender": "m" } }, { "username": "foxx", "active": false } ] };
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody["result"]);
      assertFalse(doc.parsedBody["error"]);

      //  list of databases should include the new database;
      doc = arango.GET_RAW(api);
      assertEqual(doc.code, 200);

      assertTrue(doc.parsedBody["result"].findIndex( x => x === "_system") >= 0);
      assertTrue(doc.parsedBody["result"].findIndex( x => x === name) >= 0);

      //  retrieve information about new database;
      doc = arango.GET_RAW(`/_db/${name}${api}/current`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['result']["name"], name);
      assertFalse(doc.parsedBody['result']["isSystem"]);

      //  retrieve information about user "admin";
      doc = arango.GET_RAW("/_db/_system/_api/user/admin");
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody["user"], "admin");
      assertTrue(doc.parsedBody["active"]);
      assertEqual(doc.parsedBody["extra"]["gender"], "m");

      //  retrieve information about user "foxx";
      doc = arango.GET_RAW("/_db/_system/_api/user/foxx");
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody["user"], "foxx");
      assertFalse(doc.parsedBody["active"]);

      doc = arango.DELETE_RAW(api + `/${name}`);
      assertEqual(doc.code, 200);
      assertTrue(doc.parsedBody["result"]);
      assertFalse(doc.parsedBody["error"]);
    },

    test_creates_a_new_database_with_two_users__using_user_attribute: function() {
      let body = {"name" : name, "users": [ { "user": "admin", "password": "secret", "extra": { "gender": "m" } }, { "user": "foxx", "active": false } ] };
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody["result"]);
      assertFalse(doc.parsedBody["error"]);

      //  list of databases should include the new database;
      doc = arango.GET_RAW(api);
      assertEqual(doc.code, 200);

      assertTrue(doc.parsedBody["result"].findIndex( x => x === "_system") >= 0);
      assertTrue(doc.parsedBody["result"].findIndex( x => x === name) >= 0);

      //  retrieve information about new database;
      doc = arango.GET_RAW(`/_db/${name}${api}/current`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody["result"]["name"], name);
      assertFalse(doc.parsedBody["result"]["isSystem"]);

      //  retrieve information about user "admin";
      doc = arango.GET_RAW("/_db/_system/_api/user/admin");
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody["user"], "admin", doc);
      assertTrue(doc.parsedBody["active"]);
      assertEqual(doc.parsedBody["extra"]["gender"], "m");

      //  retrieve information about user "foxx";
      doc = arango.GET_RAW("/_db/_system/_api/user/foxx");
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody["user"], "foxx");
      assertFalse(doc.parsedBody["active"]);

      doc = arango.DELETE_RAW(api + `/${name}`);
      assertEqual(doc.code, 200);
      assertTrue(doc.parsedBody["result"]);
      assertFalse(doc.parsedBody["error"]);
    },

    test_creates_a_new_database_with_an_invalid_user_object: function() {
      let body = {"name" : "${name}", "users": [ { } ] };
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 400);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody["error"]);
    },

    test_creates_a_new_database_with_an_invalid_user: function() {
      let body = {"name" : name, "users": [ { "username": "" } ] };
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody["result"]);
      assertFalse(doc.parsedBody["error"]);

      //  list of databases should include the new database;
      doc = arango.GET_RAW(api);
      assertEqual(doc.code, 200);

      assertTrue(doc.parsedBody["result"].findIndex( x => x === "_system") >= 0);
      assertTrue(doc.parsedBody["result"].findIndex( x => x === name) >= 0);

      //  retrieve information about new database;
      doc = arango.GET_RAW(`/_db/${name}${api}/current`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody["result"]["name"], name);
      assertFalse(doc.parsedBody["result"]["isSystem"]);

      doc = arango.DELETE_RAW(api + `/${name}`);
      assertEqual(doc.code, 200, doc);
      assertTrue(doc.parsedBody["result"]);
      assertFalse(doc.parsedBody["error"]);
    }
  };
}
jsunity.run(dealing_with_database_information_methodsSuite);
jsunity.run(dealing_with_Unicode_database_namesSuite);
jsunity.run(dealing_with_database_manipulation_methodsSuite);
return jsunity.done();
