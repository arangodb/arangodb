/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, fail, arango, assertTrue, assertFalse, assertEqual, assertNotUndefined, assertNotEqual, assertMatch */

// //////////////////////////////////////////////////////////////////////////////
// / @brief 
// /
// /
// / DISCLAIMER
// /
// / Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author 
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const internal = require('internal');
const sleep = internal.sleep;
const forceJson = internal.options().hasOwnProperty('server.force-json') && internal.options()['server.force-json'];
const contentType = forceJson ? "application/json" :  "application/x-velocypack";
const jsunity = require("jsunity");
const testHelper = require('@arangodb/test-helper');
const isEqual = testHelper.isEqual;
const deriveTestSuite = testHelper.deriveTestSuite;

let sync = false;
let api = "/_api/gharial";

function drop_graph (waitForSync, graph_name) {
  let cmd = api + "/" + graph_name;
  cmd = cmd + "?waitForSync=${waitForSync}";
  let doc = arango.DELETE_RAW(cmd);
  return doc;
};

function get_graph (graph_name) {
  let cmd = api + "/" + graph_name;
  let doc = arango.GET_RAW(cmd);
  return doc;
};

function create_graph (waitForSync, name, edge_definitions) {
  let cmd = api + `?waitForSync=${waitForSync}`;
  let body = {'name': name, 'edgeDefinitions':  edge_definitions};
  let doc = arango.POST_RAW(cmd, body);
  return doc;
}

function create_graph_orphans (waitForSync, name, edge_definitions, orphans) {
  let cmd = api + `?waitForSync=${waitForSync}`;
  let body = {'name': name, 'edgeDefinitions': edge_definitions, 'orphanCollections': orphans};
  let doc = arango.POST_RAW(cmd, body);
  return doc;
}


function endpoint (type, graph_name, collection, key) {
  let result = api + "/" + encodeURIComponent(graph_name) + "/" + encodeURIComponent(type);
  if (collection !== null) {
    result =  result + "/" + collection;
  }
  if (key !== null) {
    result =  result + "/" + encodeURIComponent(key);
  }
  return result;
}

function vertex_endpoint (graph_name, collection = null, key = null) {
  return endpoint("vertex", graph_name, collection, key);
}

function edge_endpoint (graph_name, collection = null, key = null) {
  return endpoint("edge", graph_name, collection, key);
}

function list_edge_collections (graph_name) {
  let cmd = edge_endpoint(graph_name);
  let doc = arango.GET_RAW(cmd);
  return doc;
}

function additional_edge_definition (waitForSync, graph_name, edge_definitions) {
  let cmd = edge_endpoint(graph_name)  + `?waitForSync=${waitForSync}`;
  let doc = arango.POST_RAW(cmd, edge_definitions);
  return doc;
}

function change_edge_definition (waitForSync, graph_name, definition_name, edge_definitions) {
  let cmd = edge_endpoint(graph_name, definition_name) + `?waitForSync=${waitForSync}`;
  let doc = arango.PUT_RAW(cmd, edge_definitions);
  return doc;
}

function delete_edge_definition (waitForSync, graph_name, definition_name) {
  let cmd = edge_endpoint(graph_name, definition_name) + `?waitForSync=${waitForSync}`;
  let doc = arango.DELETE_RAW(cmd);
  return doc;
}

function list_vertex_collections (graph_name) {
  let cmd = vertex_endpoint(graph_name);
  let doc = arango.GET_RAW(cmd);
  return doc;
}

function additional_vertex_collection (waitForSync, graph_name, collection_name) {
  let cmd = vertex_endpoint(graph_name)  + `?waitForSync=${waitForSync}`;
  let body = { 'collection': collection_name };
  let doc = arango.POST_RAW(cmd, body);
  return doc;
}

function delete_vertex_collection (waitForSync, graph_name, collection_name) {
  let cmd = vertex_endpoint(graph_name, collection_name) + `?waitForSync=${waitForSync}`;
  let doc = arango.DELETE_RAW(cmd);
  return doc;
}

function create_vertex (waitForSync, graph_name, collection, body, options={}) {
  let cmd = vertex_endpoint(graph_name, collection) + `?waitForSync=${waitForSync}`;
  for (const [key, value] of Object.entries(options)) {
    cmd = cmd + "&" + encodeURIComponent(key) + "=" + encodeURIComponent(value);
  }
  let doc = arango.POST_RAW(cmd, body);
  return doc;
}

function get_vertex (graph_name, collection, key) {
  let cmd = vertex_endpoint(graph_name, collection, key);
  let doc = arango.GET_RAW(cmd);
  return doc;
}

function update_vertex (waitForSync, graph_name, collection, key, body, keepNull, options={}) {
  let cmd = vertex_endpoint(graph_name, collection, key) + `?waitForSync=${waitForSync}`;
  if (keepNull !== '') {
    cmd = cmd + `&keepNull=${keepNull}`;
  }
  for (const [key, value] of Object.entries(options)) {
    cmd = cmd + "&" + encodeURIComponent(key) + "=" + encodeURIComponent(value);
  }
  let doc = arango.PATCH_RAW(cmd, body);
  return doc;
}

function replace_vertex (waitForSync, graph_name, collection, key, body, options={}) {
  let cmd = vertex_endpoint(graph_name, collection, key) + `?waitForSync=${waitForSync}`;
  for (const [key, value] of Object.entries(options)) {
    cmd = cmd + "&" + encodeURIComponent(key) + "=" + encodeURIComponent(value);
  }
  let doc = arango.PUT_RAW(cmd, body);
  return doc;
}

function delete_vertex (waitForSync, graph_name, collection, key, options={}) {
  let cmd = vertex_endpoint(graph_name, collection, key) + `?waitForSync=${waitForSync}`;
  for (const [key, value] of Object.entries(options)) {
    cmd = cmd + "&" + encodeURIComponent(key) + "=" + encodeURIComponent(value);
  }
  let doc = arango.DELETE_RAW(cmd);
  return doc;
}

function create_edge (waitForSync, graph_name, collection, from, to, body, options={}) {
  let cmd = edge_endpoint(graph_name, collection) + `?waitForSync=${waitForSync}`;
  body["_from"] = from;
  body["_to"] = to;
  for (const [key, value] of Object.entries(options)) {
    cmd = cmd + "&" + encodeURIComponent(key) + "=" + encodeURIComponent(value);
  }
  let doc = arango.POST_RAW(cmd, body);
  return doc;
}

function get_edge (graph_name, collection, key) {
  let cmd = edge_endpoint(graph_name, collection, key);
  let doc = arango.GET_RAW(cmd);
  return doc;
}

function update_edge (waitForSync, graph_name, collection, key, body, keepNull='', options={}) {
  let cmd = edge_endpoint(graph_name, collection, key) + `?waitForSync=${waitForSync}`;
  if (keepNull !== '') {
    cmd = cmd + `&keepNull=${keepNull}`;
  }
  for (const [key, value] of Object.entries(options)) {
    cmd = cmd + "&" + encodeURIComponent(key) + "=" + encodeURIComponent(value);
  }
  let doc = arango.PATCH_RAW(cmd, body);
  return doc;
}

function replace_edge (waitForSync, graph_name, collection, key, body, options={}) {
  let cmd = edge_endpoint(graph_name, collection, key) + `?waitForSync=${waitForSync}`;
  for (const [key, value] of Object.entries(options)) {
    cmd = cmd + "&" + encodeURIComponent(key) + "=" + encodeURIComponent(value);
  }
  let doc = arango.PUT_RAW(cmd, body);
  return doc;
}

function delete_edge (waitForSync, graph_name, collection, key, options={}) {
  let cmd = edge_endpoint(graph_name, collection, key) + `?waitForSync=${waitForSync}`;
  for (const [key, value] of Object.entries(options)) {
    cmd = cmd + "&" + encodeURIComponent(key) + "=" + encodeURIComponent(value);
  }
  let doc = arango.DELETE_RAW(cmd);
  return doc;
}

const user_collection = "UnitTestUsers";
const product_collection = "UnitTestProducts";
const friend_collection = "UnitTestFriends";
const bought_collection = "UnitTestBoughts";
const graph_name = "UnitTestGraph";
const unknown_name = "UnitTestUnknown";
const unknown_collection = "UnitTestUnknownCollection";
const unknown_key = "UnitTestUnknownKey";

////////////////////////////////////////////////////////////////////////////////;
// checking graph creation process;
////////////////////////////////////////////////////////////////////////////////;
function check_creation_of_graphSuite () {
  return {
    setUp: function() {
      drop_graph(sync, graph_name);
    },

    tearDown: function() {
      drop_graph(sync, graph_name);
      db._drop(bought_collection);
      db._drop(friend_collection);
      db._drop(product_collection);
      db._drop(user_collection);
    },

    test_can_create_an_empty_graph: function() {
      let edge_definition = [];
      let doc = create_graph(sync, graph_name, edge_definition );

      assertEqual(doc.code, sync ? 201 : 202);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], sync ? 201 : 202);
      assertEqual(doc.parsedBody['graph']['name'], graph_name);
      assertEqual(doc.parsedBody['graph']['_rev'], doc.headers['etag']);
      assertEqual(doc.parsedBody['graph']['edgeDefinitions'], edge_definition);
    },

    test_can_create_a_graph_with_definitions: function() {
      let first_def = { "collection": friend_collection, "from": [user_collection], "to": [user_collection] };
      let edge_definition = [first_def];
      let doc = create_graph(sync, graph_name, edge_definition);

      assertEqual(doc.code, sync ? 201 : 202);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], sync ? 201 : 202);
      assertEqual(doc.parsedBody['graph']['name'], graph_name);
      assertEqual(doc.parsedBody['graph']['_rev'], doc.headers['etag']);
      assertEqual(doc.parsedBody['graph']['edgeDefinitions'], edge_definition);
    },

    test_can_create_a_graph_with_orphan_collections: function() {
      let orphans = [product_collection];
      let doc = create_graph_orphans(sync, graph_name, [], orphans);
      assertEqual(doc.code, sync ? 201 : 202);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], sync ? 201 : 202);
      assertEqual(doc.parsedBody['graph']['name'], graph_name);
      assertEqual(doc.parsedBody['graph']['_rev'], doc.headers['etag']);
      assertEqual(doc.parsedBody['graph']['edgeDefinitions'], []);
      assertEqual(doc.parsedBody['graph']['orphanCollections'], orphans);
    },

    test_can_add_additional_edge_definitions: function() {
      let first_def = { "collection": friend_collection, "from": [user_collection], "to": [user_collection] };
      let edge_definition = [first_def];
      create_graph(sync, graph_name, edge_definition);
      let second_def = { "collection": bought_collection, "from": [user_collection], "to": [product_collection] };
      let doc = additional_edge_definition(sync, graph_name, second_def );
      edge_definition.push(second_def);
      assertEqual(doc.code, 202);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 202);
      assertEqual(doc.parsedBody['graph']['name'], graph_name);
      assertEqual(doc.parsedBody['graph']['_rev'], doc.headers['etag']);
      function sortByCollection(x, y) {
        if (x['collection'] < y['collection']) {
          return -1;
        }
        if (x['collection'] > y['collection']) {
          return 1;
        }
        return 0;
      }
      assertEqual(doc.parsedBody['graph']['edgeDefinitions'].sort(sortByCollection),
                  edge_definition.sort(sortByCollection), doc);
    },

    test_can_modify_existing_edge_definitions: function() {
      let first_def = { "collection": friend_collection, "from": [user_collection], "to": [user_collection] };
      create_graph(sync, graph_name, [first_def] );
      let second_def = { "collection": friend_collection, "from": [product_collection], "to": [user_collection] };
      let edge_definition = [second_def];

      let doc = change_edge_definition(sync,  graph_name, friend_collection, second_def );
      assertEqual(doc.code, 202);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 202);
      assertEqual(doc.parsedBody['graph']['name'], graph_name);
      assertEqual(doc.parsedBody['graph']['_rev'], doc.headers['etag']);
      assertEqual(doc.parsedBody['graph']['edgeDefinitions'], edge_definition);
    },

    test_can_delete_an_edge_definition: function() {
      let first_def = { "collection": friend_collection, "from": [user_collection], "to": [user_collection] };
      let edge_definition = [first_def];
      create_graph(sync, graph_name, edge_definition );
      let doc = delete_edge_definition(sync,  graph_name, friend_collection );

      assertEqual(doc.code, 202);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 202);
      assertEqual(doc.parsedBody['graph']['name'], graph_name);
      assertEqual(doc.parsedBody['graph']['_rev'], doc.headers['etag']);
      assertEqual(doc.parsedBody['graph']['edgeDefinitions'], []);
    },

    test_can_add_an_additional_orphan_collection: function() {
      let first_def = { "collection": friend_collection, "from": [user_collection], "to": [user_collection] };
      let edge_definition = [first_def];
      create_graph(sync, graph_name, edge_definition );
      let doc = additional_vertex_collection(sync, graph_name, product_collection );

      assertEqual(doc.code, 202);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 202);
      assertEqual(doc.parsedBody['graph']['name'], graph_name);
      assertEqual(doc.parsedBody['graph']['_rev'], doc.headers['etag']);
      assertEqual(doc.parsedBody['graph']['edgeDefinitions'], edge_definition);
      assertEqual(doc.parsedBody['graph']['orphanCollections'], [product_collection]);
    },

    test_can_delete_an_orphan_collection: function() {
      let first_def = { "collection": friend_collection, "from": [user_collection], "to": [user_collection] };
      let edge_definition = [first_def];
      create_graph(sync, graph_name, edge_definition );
      additional_vertex_collection(sync,  graph_name, product_collection );
      let doc = delete_vertex_collection(sync,  graph_name, product_collection );

      assertEqual(doc.code, 202);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 202);
      assertEqual(doc.parsedBody['graph']['name'], graph_name);
      assertEqual(doc.parsedBody['graph']['_rev'], doc.headers['etag']);
      assertEqual(doc.parsedBody['graph']['edgeDefinitions'], edge_definition);
      assertEqual(doc.parsedBody['graph']['orphanCollections'], []);
    },

    test_can_delete_a_graph_again: function() {
      let definition = { "collection": friend_collection, "from": [user_collection], "to": [user_collection] };
      create_graph(sync, graph_name, [definition]);
      let doc = drop_graph(sync, graph_name);
      assertEqual(doc.code, 202);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 202);
    },

    test_can_not_delete_a_graph_twice: function() {
      let definition = { "collection": friend_collection, "from": [user_collection], "to": [user_collection] };
      create_graph(sync, graph_name, [definition]);
      drop_graph(sync, graph_name);
      let doc = drop_graph(sync, graph_name);
      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorMessage'], "graph 'UnitTestGraph' not found");
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
    },

    test_can_not_create_a_graph_twice: function() {
      let definition = { "collection": friend_collection, "from": [user_collection], "to": [user_collection] };
      create_graph(sync, graph_name, [definition]);
      let doc = create_graph(sync, graph_name, [definition]);
      assertEqual(doc.code, 409);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorMessage'], "graph already exists");
      assertEqual(doc.parsedBody['code'], 409);
    },

    test_can_get_a_graph_by_name: function() {
      let orphans = [product_collection];;
      let doc = create_graph_orphans(sync, graph_name, [], orphans);
      let rev = doc.parsedBody['graph']['_rev'];

      doc = get_graph(graph_name);
      assertEqual(doc.code, 200);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['graph']['name'], graph_name);
      assertEqual(doc.parsedBody['graph']['_rev'], rev);
      assertEqual(doc.parsedBody['graph']['edgeDefinitions'], []);
      assertEqual(doc.parsedBody['graph']['orphanCollections'], orphans);
    },

    test_can_get_a_list_of_vertex_collections: function() {
      let definition = { "collection": friend_collection, "from": [user_collection], "to": [user_collection] };
      create_graph(sync, graph_name, [definition]);
      additional_vertex_collection(sync, graph_name, product_collection);

      let doc = list_vertex_collections(graph_name);
      assertEqual(doc.code, 200);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['collections'], [product_collection, user_collection]);
    },

    test_can_get_a_list_of_edge_collections: function() {
      let definition1 = { "collection": friend_collection, "from": [user_collection], "to": [user_collection] };
      let definition2 = { "collection": bought_collection, "from": [user_collection], "to": [product_collection] };
      create_graph(sync, graph_name, [definition1, definition2]);

      let doc = list_edge_collections(graph_name);
      assertEqual(doc.code, 200);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['collections'], [bought_collection, friend_collection]);
    }
  };
}

function check_vertex_operationSuite () {
  return {
    setUp: function() {
      drop_graph(sync, graph_name);
      let definition = { "collection": friend_collection, "from": [user_collection], "to": [user_collection] };
      create_graph(sync, graph_name, [definition]);
    },

    tearDown: function() {
      drop_graph(sync, graph_name);
      db._drop(friend_collection);
      db._drop(user_collection);
    },

    test_can_create_a_vertex: function() {
      let name = "Alice";
      let doc = create_vertex(sync, graph_name, user_collection, {"name": name}, {});
      assertEqual(doc.code, sync ? 201 : 202);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], sync ? 201 : 202);
      assertEqual(doc.parsedBody['vertex']['_rev'], doc.headers['etag']);

      assertEqual(doc.parsedBody['old'], undefined);
      assertEqual(doc.parsedBody['new'], undefined);
    },

    test_can_create_a_vertex__returnNew: function() {
      let name = "Alice";
      let doc = create_vertex(sync, graph_name, user_collection, {"name": name}, { "returnNew": "true" });
      assertEqual(doc.code, sync ? 201 : 202);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], sync ? 201 : 202);
      assertEqual(doc.parsedBody['vertex']['_rev'], doc.headers['etag']);

      assertEqual(doc.parsedBody['old'], undefined);
      assertTrue(doc.parsedBody['new']['_key'] !== undefined, doc);
      assertEqual(doc.parsedBody['new']['name'], name);
    },

    test_can_get_a_vertex: function() {
      let name = "Alice";
      let doc = create_vertex(sync, graph_name, user_collection, {"name": name});
      let key = doc.parsedBody['vertex']['_key'];

      doc = get_vertex(graph_name, user_collection, key);
      assertEqual(doc.code, 200);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['vertex']['name'], name);
      assertEqual(doc.parsedBody['vertex']['_rev'], doc.headers['etag']);
      assertEqual(doc.parsedBody['vertex']['_key'], key);
    },

    test_can_not_get_a_non_existing_vertex: function() {
      let key = "unknownKey";
      let doc = get_vertex(graph_name, user_collection, key);
      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.parsedBody['error']);
      assertMatch(/.*document not found.*/, doc.parsedBody['errorMessage'], doc);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
    },

    test_can_replace_a_vertex: function() {
      let name = "Alice";
      let doc = create_vertex(sync, graph_name, user_collection, {"name": name});
      let key = doc.parsedBody['vertex']['_key'];
      let oldTag = doc.parsedBody['vertex']['_rev'];
      assertEqual(oldTag, doc.headers['etag']);
      name = "Bob";

      doc = replace_vertex(sync, graph_name, user_collection, key, {"name2": name});
      assertEqual(doc.code, sync ? 200 : 202);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], sync ? 200 : 202);
      assertEqual(doc.parsedBody['vertex']['_rev'], doc.headers['etag']);
      assertEqual(doc.parsedBody['vertex']['_key'], key);

      assertEqual(doc.parsedBody['old'], undefined);
      assertEqual(doc.parsedBody['new'], undefined);

      doc = get_vertex(graph_name, user_collection, key);
      assertEqual(doc.parsedBody['vertex']['name'], undefined);
      assertEqual(doc.parsedBody['vertex']['name2'], name);
      assertEqual(doc.parsedBody['vertex']['_rev'], doc.headers['etag']);
      assertNotEqual(oldTag, doc.headers['etag']);
      assertEqual(doc.parsedBody['vertex']['_key'], key);
    },

    test_can_replace_a_vertex__returnOld: function() {
      let name = "Alice";
      let doc = create_vertex(sync, graph_name, user_collection, {"name": name});
      let key = doc.parsedBody['vertex']['_key'];
      let oldTag = doc.parsedBody['vertex']['_rev'];
      assertEqual(oldTag, doc.headers['etag']);
      name = "Bob";

      doc = replace_vertex(sync, graph_name, user_collection, key, {"name2": name}, { "returnOld": "true", "returnNew": true });
      assertEqual(doc.code, sync ? 200 : 202);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], sync ? 200 : 202);
      assertEqual(doc.parsedBody['vertex']['_rev'], doc.headers['etag']);
      assertEqual(doc.parsedBody['vertex']['_key'], key);

      assertEqual(doc.parsedBody['old']['_key'], key);
      assertEqual(doc.parsedBody['old']['name'], "Alice");
      assertEqual(doc.parsedBody['old']['name2'], undefined);
      assertEqual(doc.parsedBody['new']['_key'], key);
      assertEqual(doc.parsedBody['new']['name'], undefined);
      assertEqual(doc.parsedBody['new']['name2'], "Bob");

      doc = get_vertex(graph_name, user_collection, key);
      assertEqual(doc.parsedBody['vertex']['name'], undefined);
      assertEqual(doc.parsedBody['vertex']['name2'], name);
      assertEqual(doc.parsedBody['vertex']['_rev'], doc.headers['etag']);
      assertNotEqual(oldTag, doc.headers['etag']);
      assertEqual(doc.parsedBody['vertex']['_key'], key);
    },

    test_can_not_replace_a_non_existing_vertex: function() {
      let key = "unknownKey";

      let doc = replace_vertex(sync, graph_name, user_collection, key, {"name2": "bob"});
      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.parsedBody['error']);
      assertMatch(/.*document not found.*/, doc.parsedBody['errorMessage'], doc);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
    },

    test_can_update_a_vertex: function() {
      let name = "Alice";
      let doc = create_vertex(sync, graph_name, user_collection, {"name": name});
      let key = doc.parsedBody['vertex']['_key'];
      let name2 = "Bob";

      doc = update_vertex(sync, graph_name, user_collection, key, {"name2": name2}, "");
      assertEqual(doc.code, sync ? 200 : 202);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], sync ? 200 : 202);
      assertEqual(doc.parsedBody['vertex']['_rev'], doc.headers['etag']);
      assertEqual(doc.parsedBody['vertex']['_key'], key);

      assertEqual(doc.parsedBody['old'], undefined);
      assertEqual(doc.parsedBody['new'], undefined);

      doc = get_vertex(graph_name, user_collection, key);
      assertEqual(doc.parsedBody['vertex']['name'], name);
      assertEqual(doc.parsedBody['vertex']['name2'], name2);
      assertEqual(doc.parsedBody['vertex']['_rev'], doc.headers['etag']);
      assertEqual(doc.parsedBody['vertex']['_key'], key);
    },

    test_can_update_a_vertex__returnOld: function() {
      let name = "Alice";
      let doc = create_vertex(sync, graph_name, user_collection, {"name": name});
      let key = doc.parsedBody['vertex']['_key'];
      let name2 = "Bob";

      doc = update_vertex(sync, graph_name, user_collection, key, {"name2": name2}, "", { "returnOld": "true", "returnNew": "true" });
      assertEqual(doc.code, sync ? 200 : 202);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], sync ? 200 : 202);
      assertEqual(doc.parsedBody['vertex']['_rev'], doc.headers['etag']);
      assertEqual(doc.parsedBody['vertex']['_key'], key);

      assertEqual(doc.parsedBody['old']['_key'], key);
      assertEqual(doc.parsedBody['old']['name'], name);
      assertEqual(doc.parsedBody['old']['name2'], undefined);
      assertEqual(doc.parsedBody['new']['_key'], key);
      assertEqual(doc.parsedBody['new']['name'], name);
      assertEqual(doc.parsedBody['new']['name2'], name2);

      doc = get_vertex(graph_name, user_collection, key);
      assertEqual(doc.parsedBody['vertex']['name'], name);
      assertEqual(doc.parsedBody['vertex']['name2'], name2);
      assertEqual(doc.parsedBody['vertex']['_rev'], doc.headers['etag']);
      assertEqual(doc.parsedBody['vertex']['_key'], key);
    },

    test_can_update_a_vertex_keepUndefined_defaulttrue: function() {
      let doc = create_vertex(sync, graph_name, user_collection, {"name": "Alice"});
      let key = doc.parsedBody['vertex']['_key'];
      doc = update_vertex(sync, graph_name, user_collection, key, {"name": null}, "");
      assertEqual(doc.code, sync ? 200 : 202);
      doc = get_vertex(graph_name, user_collection, key);
      assertTrue(doc.parsedBody['vertex'].hasOwnProperty('name'));
      assertEqual(doc.parsedBody['vertex']['name'], null);
    },

    test_can_update_a_vertex_keepUndefined_false: function() {
      let doc = create_vertex(sync, graph_name, user_collection, {"name": "Alice"});
      let key = doc.parsedBody['vertex']['_key'];
      doc = update_vertex(sync, graph_name, user_collection, key, {"name": null}, false);
      assertEqual(doc.code, sync ? 200 : 202);
      doc = get_vertex(graph_name, user_collection, key);
      assertFalse(doc.parsedBody['vertex'].hasOwnProperty('name'));
    },

    test_can_update_a_vertex_keepUndefined_true: function() {
      let doc = create_vertex(sync, graph_name, user_collection, {"name": "Alice"});
      let key = doc.parsedBody['vertex']['_key'];
      doc = update_vertex(sync, graph_name, user_collection, key, {"name": null}, true);
      assertEqual(doc.code, sync ? 200 : 202);
      doc = get_vertex(graph_name, user_collection, key);
      assertTrue(doc.parsedBody['vertex'].hasOwnProperty('name'));
      assertEqual(doc.parsedBody['vertex']['name'], null);
    },

    test_can_not_update_a_non_existing_vertex: function() {
      let key = "unknownKey";
      let name2 = "Bob";

      let doc = update_vertex(sync, graph_name, user_collection, key, {"name2": name2}, "");
      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.parsedBody['error']);
      assertMatch(/.*document not found.*/, doc.parsedBody['errorMessage'], doc);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
    },

    test_can_delete_a_vertex: function() {
      let name = "Alice";
      let doc = create_vertex(sync, graph_name, user_collection, {"name": name});
      let key = doc.parsedBody['vertex']['_key'];

      doc = delete_vertex(sync, graph_name, user_collection, key);
      assertEqual(doc.code, sync ? 200 : 202);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], sync ? 200 : 202);
      assertEqual(doc.parsedBody['old'], undefined);
      assertEqual(doc.parsedBody['new'], undefined);

      doc = get_vertex(graph_name, user_collection, key);
      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.parsedBody['error']);
      assertMatch(/.*document not found.*/, doc.parsedBody['errorMessage'], doc);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
    },

    test_can_delete_a_vertex__returnOld: function() {
      let name = "Alice";
      let doc = create_vertex(sync, graph_name, user_collection, {"name": name});
      let key = doc.parsedBody['vertex']['_key'];

      doc = delete_vertex(sync, graph_name, user_collection, key, { "returnOld": "true" });
      assertEqual(doc.code, sync ? 200 : 202);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], sync ? 200 : 202);
      assertTrue(doc.parsedBody['removed']);
      assertEqual(doc.parsedBody['old']['_key'], key);
      assertEqual(doc.parsedBody['old']['name'], name);
      assertEqual(doc.parsedBody['new'], undefined);

      doc = get_vertex(graph_name, user_collection, key);
      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.parsedBody['error']);
      assertMatch(/.*document not found.*/, doc.parsedBody['errorMessage'], doc);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
    },

    test_can_not_delete_a_non_existing_vertex: function() {
      let key = "unknownKey";

      let doc = delete_vertex(sync, graph_name, user_collection, key);
      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.parsedBody['error']);
      assertMatch(/.*document not found.*/, doc.parsedBody['errorMessage'], doc);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
    }
  };
}

function check_edge_operationSuite () {
  return {
    setUp: function() {
      drop_graph(sync, graph_name);
      let definition = { "collection": friend_collection, "from": [user_collection], "to": [user_collection] };
      create_graph(sync, graph_name, [definition]);
    },

    tearDown: function() {
      drop_graph(sync, graph_name);
      db._drop(friend_collection);
      db._drop(user_collection);
    },

    test_can_create_an_edge: function() {
      let v1 = create_vertex(sync, graph_name, user_collection, {});
      assertEqual(v1.code, sync ? 201 : 202);
      v1 = v1.parsedBody['vertex']['_id'];
      let v2 = create_vertex(sync, graph_name, user_collection, {});
      assertEqual(v2.code, sync ? 201 : 202);
      v2 = v2.parsedBody['vertex']['_id'];
      let doc = create_edge(sync, graph_name, friend_collection, v1, v2, {});
      assertEqual(doc.code, sync ? 201 : 202);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], sync ? 201 : 202);
      assertEqual(doc.parsedBody['edge']['_rev'], doc.headers['etag']);

      assertEqual(doc.parsedBody['old'], undefined);
      assertEqual(doc.parsedBody['new'], undefined);
    },

    test_can_create_an_edge__returnNew: function() {
      let v1 = create_vertex(sync, graph_name, user_collection, {});
      assertEqual(v1.code, sync ? 201 : 202);
      v1 = v1.parsedBody['vertex']['_id'];
      let v2 = create_vertex(sync, graph_name, user_collection, {});
      assertEqual(v2.code, sync ? 201 : 202);
      v2 = v2.parsedBody['vertex']['_id'];
      let doc = create_edge(sync, graph_name, friend_collection, v1, v2, { "value": "foo" }, { "returnNew": "true" });
      assertEqual(doc.code, sync ? 201 : 202);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], sync ? 201 : 202);
      assertEqual(doc.parsedBody['edge']['_rev'], doc.headers['etag']);

      assertEqual(doc.parsedBody['old'], undefined);
      assertEqual(doc.parsedBody['new']['value'], "foo");
    },

    test_can_not_create_edge_with_unknown__from_vertex_collection: function() {
      let v1 = create_vertex(sync, graph_name, user_collection, {});
      v1 = v1.parsedBody['vertex']['_id'];
      let doc = create_edge(sync, graph_name, friend_collection, "MISSING/v2", v1, {});
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertMatch(/.*referenced _from collection 'MISSING' is not part of the graph.*/, doc.parsedBody['errorMessage'], doc);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_GRAPH_REFERENCED_VERTEX_COLLECTION_NOT_USED.code);
    },

    test_can_not_create_edge_with_unknown__to_vertex_collection: function() {
      let v1 = create_vertex(sync, graph_name, user_collection, {});
      v1 = v1.parsedBody['vertex']['_id'];
      let doc = create_edge(sync, graph_name, friend_collection, v1, "MISSING/v2", {});
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertMatch(/.*referenced _to collection 'MISSING' is not part of the graph.*/, doc.parsedBody['errorMessage'], doc);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_GRAPH_REFERENCED_VERTEX_COLLECTION_NOT_USED.code);
    },

    test_should_not_replace_an_edge_in_case_the_collection_does_not_exist: function() {
      let v1 = create_vertex(sync, graph_name, user_collection, {});
      v1 = v1.parsedBody['vertex']['_id'];
      let v2 = create_vertex(sync, graph_name, user_collection, {});
      v2 = v2.parsedBody['vertex']['_id'];
      let doc = replace_edge(sync, graph_name, unknown_collection, unknown_key, {"_from": `${v1}/1`, "_to": `${v2}/2`});
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
      assertMatch(/.*collection or view not found.*/, doc.parsedBody['errorMessage'], doc);
    },

    test_can_get_an_edge: function() {
      let v1 = create_vertex(sync, graph_name, user_collection, {});
      assertEqual(v1.code, sync ? 201 : 202);
      v1 = v1.parsedBody['vertex']['_id'];
      let v2 = create_vertex(sync, graph_name, user_collection, {});
      assertEqual(v2.code, sync ? 201 : 202);
      v2 = v2.parsedBody['vertex']['_id'];
      let type = "married";
      let doc = create_edge(sync, graph_name, friend_collection, v1, v2, {"type": type});
      assertEqual(doc.code, sync ? 201 : 202);
      let key = doc.parsedBody['edge']['_key'];

      doc = get_edge(graph_name, friend_collection, key);
      assertEqual(doc.code, 200);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['edge']['type'], type);
      assertEqual(doc.parsedBody['edge']['_rev'], doc.headers['etag']);
      assertEqual(doc.parsedBody['edge']['_key'], key);
    },

    test_can_not_get_a_non_existing_edge: function() {
      let key = "unknownKey";

      let doc = get_edge(graph_name, friend_collection, key);
      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.parsedBody['error']);
      assertMatch(/.*document not found.*/, doc.parsedBody['errorMessage'], doc);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
    },

    test_can_replace_an_edge: function() {
      let v1 = create_vertex(sync, graph_name, user_collection, {});
      assertEqual(v1.code, sync ? 201 : 202);
      v1 = v1.parsedBody['vertex']['_id'];
      let v2 = create_vertex(sync, graph_name, user_collection, {});
      assertEqual(v2.code, sync ? 201 : 202);
      v2 = v2.parsedBody['vertex']['_id'];
      let type = "married";
      let doc = create_edge(sync, graph_name, friend_collection, v1, v2, {"type": type});
      assertEqual(doc.code, sync ? 201 : 202);
      let key = doc.parsedBody['edge']['_key'];
      let oldTag = doc.parsedBody['edge']['_rev'];
      assertEqual(oldTag, doc.headers['etag']);
      type = "divorced";

      doc = replace_edge(sync, graph_name, friend_collection, key, {"type2": type, "_from": v1, "_to": v2});
      assertEqual(doc.code, sync ? 200 : 202);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], sync ? 200 : 202);
      assertEqual(doc.parsedBody['edge']['_rev'], doc.headers['etag']);
      assertEqual(doc.parsedBody['edge']['_key'], key);

      assertEqual(doc.parsedBody['old'], undefined);
      assertEqual(doc.parsedBody['new'], undefined);

      doc = get_edge(graph_name, friend_collection, key);
      assertEqual(doc.parsedBody['edge']['type2'], type);
      assertEqual(doc.parsedBody['edge']['type'], undefined);
      assertEqual(doc.parsedBody['edge']['_rev'], doc.headers['etag']);
      assertNotEqual(oldTag, doc.headers['etag']);
      assertEqual(doc.parsedBody['edge']['_key'], key);
    },

    test_can_replace_an_edge__returnOld: function() {
      let v1 = create_vertex(sync, graph_name, user_collection, {});
      assertEqual(v1.code, sync ? 201 : 202);
      v1 = v1.parsedBody['vertex']['_id'];
      let v2 = create_vertex(sync, graph_name, user_collection, {});
      assertEqual(v2.code, sync ? 201 : 202);
      v2 = v2.parsedBody['vertex']['_id'];
      let type = "married";
      let doc = create_edge(sync, graph_name, friend_collection, v1, v2, {"type": type});
      assertEqual(doc.code, sync ? 201 : 202);
      let key = doc.parsedBody['edge']['_key'];
      let oldTag = doc.parsedBody['edge']['_rev'];
      assertEqual(oldTag, doc.headers['etag']);
      type = "divorced";

      doc = replace_edge(sync, graph_name, friend_collection, key,
                         {"type2": type, "_from": v1, "_to": v2},
                         { "returnOld": "true", "returnNew": "true" });
      assertEqual(doc.code, sync ? 200 : 202);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], sync ? 200 : 202);
      assertEqual(doc.parsedBody['edge']['_rev'], doc.headers['etag']);
      assertEqual(doc.parsedBody['edge']['_key'], key);

      assertEqual(doc.parsedBody['old']['_key'], key);
      assertEqual(doc.parsedBody['old']['type'], "married");
      assertEqual(doc.parsedBody['old']['type2'], undefined);
      assertEqual(doc.parsedBody['old']['_from'], v1);
      assertEqual(doc.parsedBody['old']['_to'], v2);

      assertEqual(doc.parsedBody['new']['_key'], key);
      assertEqual(doc.parsedBody['new']['type'], undefined);
      assertEqual(doc.parsedBody['new']['type2'], "divorced");
      assertEqual(doc.parsedBody['new']['_from'], v1);
      assertEqual(doc.parsedBody['new']['_to'], v2);

      doc = get_edge(graph_name, friend_collection, key);
      assertEqual(doc.parsedBody['edge']['type2'], type);
      assertEqual(doc.parsedBody['edge']['type'], undefined);
      assertEqual(doc.parsedBody['edge']['_rev'], doc.headers['etag']);
      assertNotEqual(oldTag, doc.headers['etag']);
      assertEqual(doc.parsedBody['edge']['_key'], key);
    },

    test_can_not_replace_a_non_existing_edge: function() {
      let key = "unknownKey";
      // Added _from and _to, because otherwise a 400 might conceal the;
      // 404. Another test checking that missing _from or _to trigger;
      // errors was added to api-gharial-spec.js.;
      let doc = replace_edge(sync, graph_name, friend_collection, key, {"type2": "divorced", "_from": `${user_collection}/1`, "_to": `${user_collection}/2`});
      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.parsedBody['error']);
      assertMatch(/.*document not found.*/, doc.parsedBody['errorMessage'], doc);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
    },

    test_can_not_replace_a_non_valid_edge: function() {
      let key = "unknownKey";
      let doc = replace_edge(sync, graph_name, friend_collection, key, {"type2": "divorced", "_from": "1", "_to": "2"});
      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertMatch(/.*edge attribute missing or invalid.*/, doc.parsedBody['errorMessage'], doc);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
    },

    test_can_update_an_edge: function() {
      let v1 = create_vertex(sync, graph_name, user_collection, {});
      assertEqual(v1.code, sync ? 201 : 202);
      v1 = v1.parsedBody['vertex']['_id'];
      let v2 = create_vertex(sync, graph_name, user_collection, {});
      assertEqual(v2.code, sync ? 201 : 202);
      v2 = v2.parsedBody['vertex']['_id'];
      let type = "married";
      let doc = create_edge(sync, graph_name, friend_collection, v1, v2, {"type": type});
      assertEqual(doc.code, sync ? 201 : 202);
      let key = doc.parsedBody['edge']['_key'];
      let type2 = "divorced";

      doc = update_edge(sync, graph_name, friend_collection, key, {"type2": type2}, "");
      assertEqual(doc.code, sync ? 200 : 202);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], sync ? 200 : 202);
      assertEqual(doc.parsedBody['edge']['_rev'], doc.headers['etag']);
      assertEqual(doc.parsedBody['edge']['_key'], key);

      assertEqual(doc.parsedBody['old'], undefined);
      assertEqual(doc.parsedBody['new'], undefined);

      doc = get_edge(graph_name, friend_collection, key);
      assertEqual(doc.parsedBody['edge']['type'], type);
      assertEqual(doc.parsedBody['edge']['type2'], type2);
      assertEqual(doc.parsedBody['edge']['_rev'], doc.headers['etag']);
      assertEqual(doc.parsedBody['edge']['_key'], key);
    },

    test_can_update_an_edge__returnOld: function() {
      let v1 = create_vertex(sync, graph_name, user_collection, {});
      assertEqual(v1.code, sync ? 201 : 202);
      v1 = v1.parsedBody['vertex']['_id'];
      let v2 = create_vertex(sync, graph_name, user_collection, {});
      assertEqual(v2.code, sync ? 201 : 202);
      v2 = v2.parsedBody['vertex']['_id'];
      let type = "married";
      let doc = create_edge(sync, graph_name, friend_collection, v1, v2, {"type": type});
      assertEqual(doc.code, sync ? 201 : 202);
      let key = doc.parsedBody['edge']['_key'];
      let type2 = "divorced";

      doc = update_edge(sync, graph_name, friend_collection, key, {"type2": type2}, "", { "returnOld": "true", "returnNew": "true" });
      assertEqual(doc.code, sync ? 200 : 202);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], sync ? 200 : 202);
      assertEqual(doc.parsedBody['edge']['_rev'], doc.headers['etag']);
      assertEqual(doc.parsedBody['edge']['_key'], key);
      assertEqual(doc.parsedBody['old']['_key'], key);
      assertEqual(doc.parsedBody['old']['type'], "married");
      assertEqual(doc.parsedBody['old']['type2'], undefined);
      assertEqual(doc.parsedBody['old']['_from'], v1);
      assertEqual(doc.parsedBody['old']['_to'], v2);
      assertEqual(doc.parsedBody['new']['_key'], key);
      assertEqual(doc.parsedBody['new']['type'], "married");
      assertEqual(doc.parsedBody['new']['type2'], "divorced");
      assertEqual(doc.parsedBody['new']['_from'], v1);
      assertEqual(doc.parsedBody['new']['_to'], v2);

      doc = get_edge(graph_name, friend_collection, key);
      assertEqual(doc.parsedBody['edge']['type'], type);
      assertEqual(doc.parsedBody['edge']['type2'], type2);
      assertEqual(doc.parsedBody['edge']['_rev'], doc.headers['etag']);
      assertEqual(doc.parsedBody['edge']['_key'], key);
    },

    test_can_update_an_edge_keepUndefined_defaulttrue: function() {
      let v1 = create_vertex(sync, graph_name, user_collection, {});
      assertEqual(v1.code, sync ? 201 : 202);
      v1 = v1.parsedBody['vertex']['_id'];
      let v2 = create_vertex(sync, graph_name, user_collection, {});
      assertEqual(v2.code, sync ? 201 : 202);
      v2 = v2.parsedBody['vertex']['_id'];
      let type = "married";
      let doc = create_edge(sync, graph_name, friend_collection, v1, v2, {"type": type});
      assertEqual(doc.code, sync ? 201 : 202);
      let key = doc.parsedBody['edge']['_key'];

      doc = update_edge(sync, graph_name, friend_collection, key, {"type": null}, "");
      assertEqual(doc.code, sync ? 200 : 202);

      doc = get_edge(graph_name, friend_collection, key);
      assertTrue(doc.parsedBody['edge'].hasOwnProperty('type'));
      assertEqual(doc.parsedBody['edge']['type'], null);
    },

    test_can_update_an_edge_keepUndefined_true: function() {
      let v1 = create_vertex(sync, graph_name, user_collection, {});
      assertEqual(v1.code, sync ? 201 : 202);
      v1 = v1.parsedBody['vertex']['_id'];
      let v2 = create_vertex(sync, graph_name, user_collection, {});
      assertEqual(v2.code, sync ? 201 : 202);
      v2 = v2.parsedBody['vertex']['_id'];
      let type = "married";
      let doc = create_edge(sync, graph_name, friend_collection, v1, v2, {"type": type});
      assertEqual(doc.code, sync ? 201 : 202);
      let key = doc.parsedBody['edge']['_key'];

      doc = update_edge(sync, graph_name, friend_collection, key, {"type": null}, true);
      assertEqual(doc.code, sync ? 200 : 202);

      doc = get_edge(graph_name, friend_collection, key);
      assertTrue(doc.parsedBody['edge'].hasOwnProperty('type'));
      assertEqual(doc.parsedBody['edge']['type'], null);
    },

    test_can_update_an_edge_keepUndefined_false: function() {
      let v1 = create_vertex(sync, graph_name, user_collection, {});
      assertEqual(v1.code, sync ? 201 : 202);
      v1 = v1.parsedBody['vertex']['_id'];
      let v2 = create_vertex(sync, graph_name, user_collection, {});
      assertEqual(v2.code, sync ? 201 : 202);
      v2 = v2.parsedBody['vertex']['_id'];
      let type = "married";
      let doc = create_edge(sync, graph_name, friend_collection, v1, v2, {"type": type});
      assertEqual(doc.code, sync ? 201 : 202);
      let key = doc.parsedBody['edge']['_key'];

      doc = update_edge(sync, graph_name, friend_collection, key, {"type": null}, false);
      assertEqual(doc.code, sync ? 200 : 202);

      doc = get_edge(graph_name, friend_collection, key);
      assertFalse(doc.parsedBody['edge'].hasOwnProperty('type'));
    },

    test_can_not_update_a_non_existing_edge: function() {
      let key = "unknownKey";

      let doc = update_edge(sync, graph_name, friend_collection, key, {"type2": "divorced"}, "");
      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.parsedBody['error']);
      assertMatch(/.*document not found.*/, doc.parsedBody['errorMessage'], doc);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
    },

    test_can_delete_an_edge: function() {
      let v1 = create_vertex(sync, graph_name, user_collection, {});
      assertEqual(v1.code, sync ? 201 : 202);
      v1 = v1.parsedBody['vertex']['_id'];
      let v2 = create_vertex(sync, graph_name, user_collection, {});
      assertEqual(v2.code, sync ? 201 : 202);
      v2 = v2.parsedBody['vertex']['_id'];
      let type = "married";
      let doc = create_edge(sync, graph_name, friend_collection, v1, v2, {"type": type});
      assertEqual(doc.code, sync ? 201 : 202);
      let key = doc.parsedBody['edge']['_key'];

      doc = delete_edge(sync, graph_name, friend_collection, key);
      assertEqual(doc.code, sync ? 200 : 202);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], sync ? 200 : 202);
      assertEqual(doc.parsedBody['old'], undefined);
      assertEqual(doc.parsedBody['new'], undefined);

      doc = get_edge(graph_name, friend_collection, key);
      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.parsedBody['error']);
      assertMatch(/.*document not found.*/, doc.parsedBody['errorMessage'], doc);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
    },

    test_can_delete_an_edge__returnOld: function() {
      let v1 = create_vertex(sync, graph_name, user_collection, {});
      assertEqual(v1.code, sync ? 201 : 202);
      v1 = v1.parsedBody['vertex']['_id'];
      let v2 = create_vertex(sync, graph_name, user_collection, {});
      assertEqual(v2.code, sync ? 201 : 202);
      v2 = v2.parsedBody['vertex']['_id'];
      let type = "married";
      let doc = create_edge(sync, graph_name, friend_collection, v1, v2, {"type": type});
      assertEqual(doc.code, sync ? 201 : 202);
      let key = doc.parsedBody['edge']['_key'];

      doc = delete_edge(sync, graph_name, friend_collection, key, { "returnOld": "true" });
      assertEqual(doc.code, sync ? 200 : 202);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], sync ? 200 : 202);
      assertEqual(doc.parsedBody['old']['_from'], v1);
      assertEqual(doc.parsedBody['old']['_to'], v2);
      assertEqual(doc.parsedBody['old']['type'], type);
      assertEqual(doc.parsedBody['new'], undefined);

      doc = get_edge(graph_name, friend_collection, key);
      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.parsedBody['error']);
      assertMatch(/.*document not found.*/, doc.parsedBody['errorMessage'], doc);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
    },

    test_can_not_delete_a_non_existing_edge: function() {
      let key = "unknownKey";

      let doc = delete_edge(sync, graph_name, friend_collection, key);
      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.parsedBody['error']);
      assertMatch(/.*document not found.*/, doc.parsedBody['errorMessage'], doc);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
    }
  };
}

function check400 (doc) {
  assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
  assertTrue(doc.parsedBody['error']);
  assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
  // puts doc.parsedBody['errorMessage'];
  assertMatch(/.*edge attribute missing or invalid.*/, doc.parsedBody['errorMessage'], doc);
}

function check404Edge (doc) {
  assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code, doc);
  assertTrue(doc.parsedBody['error'], doc);
  assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code, doc);
  assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_GRAPH_EDGE_COLLECTION_NOT_USED.code);
  assertEqual(doc.parsedBody['errorMessage'], "edge collection not used in graph");
}

function check404Vertex (doc) {
  assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code, doc);
  assertTrue(doc.parsedBody['error'], doc);
  assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code, doc);
  assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.code);
}

function check400VertexUnused (doc) {
  assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_GRAPH_NOT_IN_ORPHAN_COLLECTION.code);
  assertTrue(doc.parsedBody['error']);
  assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
  /// puts doc.parsedBody['errorMessage'];
}

function check404CRUD (doc) {
  assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code, doc);
  assertTrue(doc.parsedBody['error'], doc);
  assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code, doc);
  assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
  assertMatch(/.*collection or view not found:.*/, doc.parsedBody['errorMessage'], doc);
}

function check400CRUD (doc) {
  assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code, doc);
  assertTrue(doc.parsedBody['error'], doc);
  assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code, doc);
  assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code);
}

// describe "should throw 404 if document is unknown on route" do;

function check404 (doc) {
  assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code, doc);
  assertTrue(doc.parsedBody['error'], doc);
  assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code, doc);
  assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, doc);
  assertMatch(/.*document not found.*/, doc.parsedBody['errorMessage'], doc);
}

function check404Collection (doc) {
  assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
  assertTrue(doc.parsedBody['error']);
  assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
  assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
  assertMatch(/.*collection or view not found.*/, doc.parsedBody['errorMessage'], doc);
}

function check400Collection (doc) {
  assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
  assertTrue(doc.parsedBody['error']);
  assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
  assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
  assertMatch(/.*no collection name specified.*/, doc.parsedBody['errorMessage'], doc);
}

function check400_edge_missing (doc) {
  assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
  assertTrue(doc.parsedBody['error']);
  assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
  assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code);
  assertMatch(/.*edge attribute missing or invalid.*/, doc.parsedBody['errorMessage'], doc);
}

function check404_graph_not_found (doc) {
  assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
  assertTrue(doc.parsedBody['error']);
  assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
  assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_GRAPH_NOT_FOUND.code);
  assertEqual(doc.parsedBody['errorMessage'], "graph 'UnitTestUnknown' not found");
};

function check_error_codeSuite () {
  return {
    setUp: function() {
      drop_graph(sync, graph_name);
      let definition = { "collection": friend_collection, "from": [user_collection], "to": [user_collection] };
      create_graph(sync, graph_name, [definition]);
    },

    tearDown: function() {
      drop_graph(sync, graph_name);
      db._drop(friend_collection);
      db._drop(user_collection);
    },

    test_get_graph: function() {
      check404_graph_not_found(get_graph(unknown_name));
    },

    test_delete_graph: function() {
      check404_graph_not_found(drop_graph(sync, unknown_name));
    },

    test_list_edge_collections: function() {
      check404_graph_not_found(list_edge_collections(unknown_name));
    },

    test_add_edge_definition: function() {
      let definition = { "collection": friend_collection, "from": [user_collection], "to": [user_collection] };
      check404_graph_not_found(additional_edge_definition(sync, unknown_name, definition));
    },

    test_change_edge_definition: function() {
      let definition = { "collection": friend_collection, "from": [user_collection], "to": [user_collection] };
      check404_graph_not_found(change_edge_definition(sync, unknown_name, friend_collection, definition));
    },

    test_delete_edge_definition: function() {
      check404_graph_not_found(delete_edge_definition(sync, unknown_name, friend_collection));
    },

    test_list_vertex_collections: function() {
      check404_graph_not_found(list_vertex_collections(unknown_name));
    },

    test_add_vertex_collection: function() {
      check404_graph_not_found(additional_vertex_collection(sync, unknown_name, user_collection));
    },

    test_delete_vertex_collection: function() {
      check404_graph_not_found(delete_vertex_collection(sync, unknown_name, user_collection));
    },

    test_create_vertex: function() {
      check404_graph_not_found(create_vertex(sync, unknown_name, unknown_name, {}));
    },

    test_get_vertex: function() {
      check404_graph_not_found(get_vertex(unknown_name, unknown_name, unknown_name));
    },

    test_update_vertex: function() {
      check404_graph_not_found(update_vertex(sync, unknown_name, unknown_name, unknown_name, {}));
    },

    test_replace_vertex: function() {
      check404_graph_not_found(replace_vertex(sync, unknown_name, unknown_name, unknown_name, {}));
    },

    test_delete_vertex: function() {
      check404_graph_not_found(delete_vertex(sync, unknown_name, unknown_name, unknown_name));
    },

    test_create_edge: function() {
      check404_graph_not_found(create_edge(sync, unknown_name, unknown_name, unknown_name, unknown_name, {}));
    },

    test_get_edge: function() {
      check404_graph_not_found(get_edge(unknown_name, unknown_name, unknown_name));
    },

    test_update_edge: function() {
      check404_graph_not_found(update_edge(sync, unknown_name, unknown_name, unknown_name, {}));
    },

    test_replace_edge: function() {
      check404_graph_not_found(replace_edge(sync, unknown_name, unknown_name, unknown_name, {}));
    },

    test_delete_edge: function() {
      check404_graph_not_found(delete_edge(sync, unknown_name, unknown_name, unknown_name));
    },



    
    test_change_edge_definition_unknown: function() {
      let definition = { "collection": friend_collection, "from": [user_collection], "to": [user_collection] };
      check404Edge(change_edge_definition(sync, graph_name, unknown_name, definition));
    },

    test_delete_edge_definition_unknown: function() {
      check404Edge(delete_edge_definition(sync, graph_name, unknown_name));
    },

    test_delete_vertex_collection_unknown: function() {
      // this checks if a not used vertex collection can be removed of a graph;
      check400VertexUnused(delete_vertex_collection(sync, graph_name, unknown_name));
    },

    test_create_vertex_unknown: function() {
      check404CRUD(create_vertex(sync, graph_name, unknown_name, {}));
    },

    test_get_vertex_unknown: function() {
      check404CRUD(get_vertex(graph_name, unknown_name, unknown_name));
    },

    
    // TODO add tests where the edge/vertex collection is not part of the graph, but;
    // the given key exists!;
    test_update_vertex_unknown: function() {
      check404CRUD(update_vertex(sync, graph_name, unknown_name, unknown_name, {}));
    },

    test_replace_vertex_unknown: function() {
      check404CRUD(replace_vertex(sync, graph_name, unknown_name, unknown_name, {}));
    },

    test_delete_vertex_unknown: function() {
      check404CRUD(delete_vertex(sync, graph_name, unknown_name, unknown_name));
    },

    test_create_edge_unknown: function() {
      check400CRUD(create_edge(sync, graph_name, unknown_name, unknown_name, unknown_name, {}));
    },

    test_get_edge_unknown: function() {
      check404CRUD(get_edge(graph_name, unknown_name, unknown_name));
    },

    test_update_edge_unknown: function() {
      check404CRUD(update_edge(sync, graph_name, unknown_name, unknown_name, {}));
    },

    test_replace_edge_invalid_key_unknown: function() {
      check400CRUD(replace_edge(sync, graph_name, unknown_name, unknown_name, {}));
    },

    // TODO: BTS-827 - re-enable me!
    // test_replace_edge_valid_key__but_not_existing_unknown: function() {
    //   check404(replace_edge(sync, graph_name, user_collection + "/" + unknown_name, unknown_name, {}));
    // },

    test_delete_edge_unknown: function() {
      check404CRUD(delete_edge(sync, graph_name, unknown_name, unknown_name));
    },




    test_get_vertex_throw: function() {
      check404(get_vertex(graph_name, user_collection, unknown_name));
    },

    test_update_vertex_throw: function() {
      check404(update_vertex(sync, graph_name, user_collection, unknown_name, {}));
    },

    test_replace_vertex_throw: function() {
      check404(replace_vertex(sync, graph_name, user_collection, unknown_name, {}));
    },

    test_delete_vertex_throw: function() {
      check404(delete_vertex(sync, graph_name, user_collection, unknown_name));
    },

    test_get_edge_throw: function() {
      check404(get_edge(graph_name, friend_collection, unknown_name));
    },

    test_update_edge_throw: function() {
      check404(update_edge(sync, graph_name, friend_collection, unknown_name, {}));
    },

    test_replace_edge_invalid_throw: function() {
      check400(replace_edge(sync, graph_name, friend_collection, unknown_name, {"_from": "1", "_to": "2"}));
    },

    test_replace_edge_document_does_not_exist_not_found: function() {
      // Added _from and _to, because otherwise a 400 might conceal the;
      // 404. Another test checking that missing _from or _to trigger;
      // errors was added to api-gharial-spec.js.;
      check404(replace_edge(sync, graph_name, friend_collection, unknown_name, {"_from": `${user_collection}/1`, "_to": `${user_collection}/2`}));
    },

    test_delete_edge_throw: function() {
      check404(delete_edge(sync, graph_name, friend_collection, unknown_name));
    }
  };
}


jsunity.run(check_creation_of_graphSuite);
jsunity.run(check_vertex_operationSuite);
jsunity.run(check_edge_operationSuite);
jsunity.run(check_error_codeSuite);

sync = true;

jsunity.run(function() {
  let suite = {};
  deriveTestSuite(
    check_creation_of_graphSuite(),
    suite, '_WaitForSync');
  return suite;
});

jsunity.run(function() {
  let suite = {};
  deriveTestSuite(
    check_vertex_operationSuite(),
    suite, '_WaitForSync');
  return suite;
});
jsunity.run(function() {
  let suite = {};
  deriveTestSuite(
    check_edge_operationSuite(),
    suite, '_WaitForSync');
  return suite;
});
jsunity.run(function() {
  let suite = {};
  deriveTestSuite(
    check_error_codeSuite(),
    suite, '_WaitForSync');
  return suite;
});

return jsunity.done();
