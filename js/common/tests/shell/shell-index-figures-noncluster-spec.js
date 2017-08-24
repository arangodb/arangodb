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

function debugOut(item){
  print("#############");
  print(item);
  print("#############");
}
var isRocksDB = db._engine().name === "rocksdb";
var isMMFiles = db._engine().name === "mmfiles";

function verifyMemory(index){
  expect(index.figures).to.be.ok;
  expect(index.figures.memory).to.be.a('number');
  if(isMMFiles){
    expect(index.figures.memory).to.be.a.above(0);
  }
}

function verifyCache(index){
  if (index.type !== 'primary') {
    expect(index.figures).to.be.ok;
    expect(index.figures.cacheInUse).to.be.true;
    expect(index.figures.cacheSize).to.be.a('number');
    expect(index.figures.cacheLifeTimeHitRate).to.be.a('number');
    expect(index.figures.cacheWindowedHitRate).to.be.a('number');
  }
}


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

  describe('primary/edge index', function () {
    const colName = 'UnitTestEdges';
    let col;

    before('create collection', function(){
      db._drop(colName);
      col = db._createEdgeCollection(colName);
    });


    after(function() {
      db._drop(colName);
    });

    describe('verify statistics', function() {

      before('insert documents', function() {
        for (let i = 0; i < 100; i++) {
          col.insert({"_from": "source/1", "_to": "sink/" + i});
        }
      });

      it('index types', function() {
        var indexes = col.getIndexes(true);
        expect(indexes.length).to.be.equal(2);
        expect(indexes[0].type).to.be.equal("primary");
        expect(indexes[1].type).to.be.equal("edge");
      });

      it('index - memory', function() {
        var indexes = col.getIndexes(true);
        indexes.forEach((i) => {
            verifyMemory(i);
        });
      });

      it('figures - cache', function() {
        if(!isRocksDB){
          this.skip();
        }
        var indexes = col.getIndexes(true);
        indexes.forEach((i) => {
          verifyCache(i);
        });
      });

    });

    describe('loading indexes into memory', function() {

      before('insert document', function() {
        // We insert enough documents to trigger resizing
        // of initial cache size.
        for (let i = 0; i < 23000; i++) {
          col.insert({"_from": "source/" + i, "_to": "sink/" + i});
        }
      });

      it('should fill the cache', function() {
        if(!isRocksDB){
          this.skip();
        }
        // The cache does not expose fillgrades an such.
        // We can only check memory consumption...
        let indexes = col.getIndexes(true);
        let edgeIndex = indexes[1];
        expect(edgeIndex.type).to.be.equal('edge');

        expect(edgeIndex.figures).to.be.ok;
        expect(edgeIndex.figures.cacheSize).to.be.a('number');
        let oldSize = edgeIndex.figures.cacheSize;

        col.loadIndexesIntoMemory();

        // Test if the memory consumption goes up
        let indexes2 = col.getIndexes(true);
        let edgeIndex2 = indexes2[1];
        expect(edgeIndex2.type).to.be.equal('edge');
        expect(edgeIndex2.figures.cacheSize).to.be.above(oldSize);
      });

    });

  }); // end edge index

// hash index ///////////////////////////////////////////////////////////
  describe('hash index', function () {
    var col;
    before('create collection',function(){
      col = db._createDocumentCollection("UnitTestDoggyHash");
      col.ensureIndex({type: "hash", fields: ["name"]});
      for(var i = 0; i < 100; i++){
        col.insert({"name":"Harry"+i});
      }
    });
    it('verify index types', function() {
      var indexes = col.getIndexes(true);
      expect(indexes.length).to.be.equal(2);
      expect(indexes[0].type).to.be.equal("primary");
      expect(indexes[1].type).to.be.equal("hash");
    });
    it('verify index - memory', function() {
      var indexes = col.getIndexes(true);
      indexes.forEach((i) => {
        verifyMemory(i);
      });
    });
    // FIXME not implemented
    it.skip('verify figures - cache', function() {
      if(!isRocksDB){
        this.skip();
      }
      var indexes = col.getIndexes(true);
      indexes.forEach((i) => {
        verifyCache(i);
      });
    });
    after(function(){
      db._drop(col);
    });
  }); // end hash index

// skiplist index ///////////////////////////////////////////////////////////
  describe('skiplist index', function () {
    var col;
    before('create collection',function(){
      col = db._createDocumentCollection("UnitTestDoggySkip");
      col.ensureIndex({type: "skiplist", fields: ["name"]});
      for(var i = 0; i < 100; i++){
        col.insert({"name":"Harry"+i});
      }
    });
    it('verify index types', function() {
      var indexes = col.getIndexes(true);
      expect(indexes.length).to.be.equal(2);
      expect(indexes[0].type).to.be.equal("primary");
      expect(indexes[1].type).to.be.equal("skiplist");
    });
    it('verify index - memory', function() {
      var indexes = col.getIndexes(true);
      indexes.forEach((i) => {
        verifyMemory(i);
      });
    });
    // FIXME not implemented
    it.skip('verify figures - cache', function() {
      if(!isRocksDB){
        this.skip();
      }
      var indexes = col.getIndexes(true);
      indexes.forEach((i) => {
        verifyCache(i);
      });
    });
    after(function(){
      db._drop(col);
    });
  }); // end skiplist index

// fulltest index ///////////////////////////////////////////////////////////
  describe('fulltext index', function () {
    var col;
    before('create collection',function(){
      col = db._createDocumentCollection("UnitTestDoggyFull");
      col.ensureIndex({type: "fulltext", fields: ["name"], minLength : 3});
      for(var i = 0; i < 100; i++){
        col.insert({"name":"Harry"+i});
      }
    });
    it('verify index types', function() {
      var indexes = col.getIndexes(true);
      expect(indexes.length).to.be.equal(2);
      expect(indexes[0].type).to.be.equal("primary");
      expect(indexes[1].type).to.be.equal("fulltext");
    });
    it('verify index - memory', function() {
      var indexes = col.getIndexes(true);
      indexes.forEach((i) => {
        verifyMemory(i);
      });
    });
    // FIXME not implemented
    it.skip('verify figures - cache', function() {
      if(!isRocksDB){
        this.skip();
      }
      var indexes = col.getIndexes(true);
      indexes.forEach((i) => {
        verifyCache(i);
      });
    });
    after(function(){
      db._drop(col);
    });
  }); // end fulltext index

// geo index ///////////////////////////////////////////////////////////
  describe('geo index', function () {
    var col;
    before('create collection',function(){
      col = db._createDocumentCollection("UnitTestGeoSpass");
      col.ensureIndex({type: "geo", fields: ["loc"]});
      for(var i = 0; i < 10; i++){
        for(var j = 0; j < 10; j++){
          col.insert({ "name": "place" + i + "/" + j,
                       loc : [i, j]
                    });
        }
      }
    });
    it('verify index types', function() {
      var indexes = col.getIndexes(true);
      expect(indexes.length).to.be.equal(2);
      expect(indexes[0].type).to.be.equal("primary");
      expect(indexes[1].type).to.be.equal("geo1");
    });
    it('verify index - memory', function() {
      var indexes = col.getIndexes(true);
      indexes.forEach((i) => {
        verifyMemory(i);
      });
    });
    // FIXME not implemented
    it.skip('verify figures - cache', function() {
      if(!isRocksDB){
        this.skip();
      }
      var indexes = col.getIndexes(true);
      indexes.forEach((i) => {
        verifyCache(i);
      });
    });
    after(function(){
      db._drop(col);
    });
  }); // end fulltext index
}); // end Index figures
