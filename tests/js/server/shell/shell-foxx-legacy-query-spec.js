/* jshint -W083 */
/* eslint no-loop-func: 0 */
/* global describe, it, beforeEach, afterEach */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Lucas Doomen
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const expect = require('chai').expect;
const sinon = require('sinon');
const createQuery = require('@arangodb/foxx/legacy/query').createQuery;
const arangodb = require('@arangodb');
const db = arangodb.db;
const ArangoError = arangodb.ArangoError;
const errors = arangodb.errors;

require("@arangodb/test-helper").waitForFoxxInitialized();

describe('createQuery(query)', function () {
  it('should execute queries with no arguments', function () {
    const query = createQuery('RETURN 23');
    expect(query()).to.eql([23]);
  });

  it('should execute QueryBuilder instances with no arguments', function () {
    const query = createQuery({toAQL() {
        return 'RETURN 23';
    }});
    expect(query()).to.eql([23]);
  });

  it('should raise an ArangoError for unexpected arguments', function () {
    const query = createQuery('RETURN 23');
    expect(function () {
      query({foo: 'chicken'});
    }).to.throw(ArangoError)
      .with.property('errorNum', errors.ERROR_QUERY_BIND_PARAMETER_UNDECLARED.code);
  });

  it('should raise an ArangoError for missing arguments', function () {
    const query = createQuery('RETURN @x');
    expect(function () {
      query();
    }).to.throw(ArangoError)
      .with.property('errorNum', errors.ERROR_QUERY_BIND_PARAMETER_MISSING.code);
  });

  it('should execute a query with valid arguments', function () {
    const query = createQuery('RETURN @x');
    expect(query({x: 'chicken'})).to.eql(['chicken']);
    expect(query({x: 23})).to.eql([23]);
  });
});

describe('createQuery(cfg)', function () {
  function FakeModel (args) {
    this.args = args;
  }

  describe('with cfg.context', function () {
    const cn = 'UnitTestFoxxQueues';

    beforeEach(function () {
      db._drop(cn);
      db._drop(`${cn}_suffixed`);
      db._create(cn, {waitForSync: false});
      db._collection(cn).save({foo: 'potato'});
      db._create(`${cn}_suffixed`, {waitForSync: false});
      db._collection(`${cn}_suffixed`).save({foo: 'tomato'});
    });

    afterEach(function () {
      db._drop(cn);
      db._drop(`${cn}_suffixed`);
    });

    it('should pass collection arguments through cfg.context.collectionName', function () {
      const spy = sinon.spy(function (cn) {
        return `${cn}_suffixed`;
      });
      const query = createQuery({
        query: 'FOR doc IN @@col RETURN doc.foo',
        context: {collectionName: spy}
      });
      expect(spy.called).to.equal(false);
      const result = query({'@col': cn});
      expect(spy.calledWith(cn)).to.equal(true);
      expect(result).to.eql(['tomato']);
    });

    it('should pass cfg.defaults collection names through cfg.context.collectionName', function () {
      const spy = sinon.spy(function (cn) {
        return `${cn}_suffixed`;
      });
      const query = createQuery({
        query: 'FOR doc IN @@col RETURN doc.foo',
        context: {collectionName: spy},
        defaults: {'@col': cn}
      });
      expect(spy.called).to.equal(false);
      const result = query();
      expect(spy.calledWith(cn)).to.equal(true);
      expect(result).to.eql(['tomato']);
    });
  });

  it('should mix cfg.defaults into arguments', function () {
    const query = createQuery({
      query: 'RETURN [@x, @y]',
      defaults: {x: 'chicken', y: 'kittens'}
    });
    expect(query()).to.eql([['chicken', 'kittens']]);
    expect(query({y: 'puppies'})).to.eql([['chicken', 'puppies']]);
  });

  it('should pass the query result to cfg.transform', function () {
    const spy = sinon.spy(function (arg) {
      expect(arg).to.eql([1, 2, 3]);
    });
    const query = createQuery({
      query: 'FOR x IN 1..3 RETURN x',
      transform: spy
    });
    expect(spy.called).to.equal(false);
    query();
    expect(spy.called).to.equal(true);
  });

  it('should return the result of cfg.transform', function () {
    const spy = sinon.spy(function () {
      return 'chicken';
    });
    const query = createQuery({
      query: 'FOR x IN 1..3 RETURN x',
      transform: spy
    });
    expect(query()).to.eql('chicken');
    expect(spy.called).to.equal(true);
  });

  it('should convert the result to instances of cfg.model', function () {
    const result = createQuery({
      query: 'FOR x IN 1..3 RETURN {foo: x}',
      model: FakeModel
    })();
    expect(result.constructor).to.equal(Array);
    expect(result.length).to.eql(3);
    expect(result[0].constructor).to.equal(FakeModel);
    expect(result[0].args).to.eql({foo: 1});
    expect(result[1].constructor).to.equal(FakeModel);
    expect(result[1].args).to.eql({foo: 2});
    expect(result[2].constructor).to.equal(FakeModel);
    expect(result[2].args).to.eql({foo: 3});
  });

  it('should convert the result to models before transforming them', function () {
    const spy = sinon.spy(function (args) {
      expect(args[0].constructor).to.equal(FakeModel);
      expect(args[1].constructor).to.equal(FakeModel);
      expect(args[2].constructor).to.equal(FakeModel);
      return 'chicken';
    });
    const query = createQuery({
      query: 'FOR x IN 1..3 RETURN x',
      transform: spy,
      model: FakeModel
    });
    expect(query()).to.eql('chicken');
    expect(spy.called).to.equal(true);
  });

  it('should throw an error if cfg.query is missing', function () {
    expect(function () {
      createQuery({noQuery: 'no fun'});
    }).to.throw();
  });

  it('should throw an error if cfg.query is invalid', function () {
    for (let value of [23, {invalid: true}, function () {}, {toAQL: 'not a function'}]) {
      expect(function () {
        createQuery({query: value});
      }).to.throw();
    }
  });

  it('should throw an error if cfg.model is a non-function', function () {
    for (let value of [{}, 'garbage', true, 23, Infinity]) {
      expect(function () {
        createQuery({query: 'RETURN 23', model: value});
      }).to.throw();
    }
  });

  it('should throw an error if cfg.transform is a non-function', function () {
    for (let value of [{}, 'garbage', true, 23, Infinity]) {
      expect(function () {
        createQuery({query: 'RETURN 23', transform: value});
      }).to.throw();
    }
  });

  it('should throw an error if cfg.context has no "collectionName" method', function () {
    for (let value of [{}, 'garbage', true, 23, Infinity, function () {}, {collectionName: 'not a function'}]) {
      expect(function () {
        createQuery({query: 'RETURN 23', context: value});
      }).to.throw();
    }
  });
});
