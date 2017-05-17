/* global print, describe, it, beforeEach, afterEach, it, before, after */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief spec for index figures
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
// / @author Jan Christoph Uhde
// / @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var arangodb = require('@arangodb');
var ArangoError = arangodb.ArangoError;
var internal = require('internal');
var db = internal.db;

var expect = require('chai').expect;
var should = require('chai').should();

describe('Index figures', function () {
  beforeEach(function () {
    try {
      // some setup
    } catch (e) {
      try {
        // clean up if failed
      } catch (e2) {
        // noop
      }
    }
  });

// edge index ///////////////////////////////////////////////////////////
  describe('edge index', function () {
    var col;
    before('create collection',function(){
      col = db._createEdgeCollection("UnitTestEdgar");
      for(var i = 0; i < 100; i++){
        col.insert({"_from":"source/1","_to":"sink/"+i});
      }
    });

    it('verify index types', function() {
      var indexes = col.getIndexes(true);
      expect(indexes.length).to.be.equal(2);
      expect(indexes[0].type).to.be.equal("primary");
      expect(indexes[1].type).to.be.equal("edge");
    });

    it.skip('verify index - memory', function() {
      var indexes = col.getIndexes(true);
      indexes.forEach((i) => {
        expect(i.figure).to.be.defined;
        expect(i.figure, "index of type " + i.type + "does not have memory property")
              .should.have.property('memory').above(0);
      });
    });

    it('verify figures - cache', function() {
      if(db._engine().name !== "rocksdb"){
        this.skip();
      }
    });

    after(function(){
      db._drop(col);
    });
  }); // end edge index

// hash index ///////////////////////////////////////////////////////////
  describe('hash index', function () {
  }); // end hash index

}); // end Index figures
