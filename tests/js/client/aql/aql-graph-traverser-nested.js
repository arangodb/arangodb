/* jshint esnext: true */

// //////////////////////////////////////////////////////////////////////////////
// / @brief Spec for the AQL FOR x IN GRAPH name statement
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2014 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
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
// / @author Michael Hackstein
// / @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require('jsunity');
const {assertEqual, assertTrue, assertFalse, fail} = jsunity.jsUnity.assertions;

const internal = require('internal');
const db = require('internal').db;


const vn = 'UnitTestVertexCollection';
const en = 'UnitTestEdgeCollection';


function nestedSuite() {
  const gn = 'UnitTestGraph';
  var objects, tags, tagged;

  return {

    setUpAll: function () {
      tags = db._create(gn + 'tags');
      objects = db._create(gn + 'objects');
      tagged = db._createEdgeCollection(gn + 'tagged');

      ['airplane', 'bicycle', 'train', 'car', 'boat'].forEach(function (_key) {
        objects.insert({_key});
      });

      ['public', 'private', 'fast', 'slow', 'land', 'air', 'water'].forEach(function (_key) {
        tags.insert({_key});
      });

      [
        ['air', 'airplane'],
        ['land', 'car'],
        ['land', 'bicycle'],
        ['land', 'train'],
        ['water', 'boat'],
        ['fast', 'airplane'],
        ['fast', 'car'],
        ['slow', 'bicycle'],
        ['fast', 'train'],
        ['slow', 'boat'],
        ['public', 'airplane'],
        ['private', 'car'],
        ['private', 'bicycle'],
        ['public', 'train'],
        ['public', 'boat']
      ].forEach(function (edge) {
        tagged.insert({_from: tags.name() + '/' + edge[0], _to: objects.name() + '/' + edge[1]});
      });
    },

    tearDownAll: function () {
      db._drop(gn + 'tags');
      db._drop(gn + 'objects');
      db._drop(gn + 'tagged');
    },

    testNested: function () {
      var query = 'with ' + objects.name() + ', ' + tags.name() + ' for vehicle in any @start1 @@tagged for type in any @start2 @@tagged filter vehicle._id == type._id return vehicle._key';

      var result = db._query(query, {
        start1: tags.name() + '/land',
        start2: tags.name() + '/public',
        '@tagged': tagged.name()
      }).toArray();
      assertEqual(['train'], result);

      result = db._query(query, {
        start1: tags.name() + '/air',
        start2: tags.name() + '/fast',
        '@tagged': tagged.name()
      }).toArray();
      assertEqual(['airplane'], result);

      result = db._query(query, {
        start1: tags.name() + '/air',
        start2: tags.name() + '/slow',
        '@tagged': tagged.name()
      }).toArray();
      assertEqual([], result);

      result = db._query(query, {
        start1: tags.name() + '/land',
        start2: tags.name() + '/fast',
        '@tagged': tagged.name()
      }).toArray();
      assertEqual(['car', 'train'], result.sort());

      result = db._query(query, {
        start1: tags.name() + '/land',
        start2: tags.name() + '/private',
        '@tagged': tagged.name()
      }).toArray();
      assertEqual(['bicycle', 'car'], result.sort());

      result = db._query(query, {
        start1: tags.name() + '/public',
        start2: tags.name() + '/slow',
        '@tagged': tagged.name()
      }).toArray();
      assertEqual(['boat'], result);

      result = db._query(query, {
        start1: tags.name() + '/public',
        start2: tags.name() + '/fast',
        '@tagged': tagged.name()
      }).toArray();
      assertEqual(['airplane', 'train'], result.sort());

      result = db._query(query, {
        start1: tags.name() + '/public',
        start2: tags.name() + '/foo',
        '@tagged': tagged.name()
      }).toArray();
      assertEqual([], result);

      result = db._query(query, {
        start1: tags.name() + '/foo',
        start2: tags.name() + '/fast',
        '@tagged': tagged.name()
      }).toArray();
      assertEqual([], result);
    }
  };
}
jsunity.run(nestedSuite);
return jsunity.done();
