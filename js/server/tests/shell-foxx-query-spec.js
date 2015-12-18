/*jshint globalstrict:false, strict:false */
/*global describe, beforeEach, it, expect, createSpy, afterEach */

////////////////////////////////////////////////////////////////////////////////
/// @brief
///
/// @file
///
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
/// @author Lucas Doomen
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var _ = require('lodash'),
  createQuery = require('@arangodb/foxx/legacy/query').createQuery,
  arangodb = require('@arangodb'),
  db = arangodb.db;

describe('createQuery(query)', function () {
  'use strict';

  it('should execute queries with no arguments', function () {
    var query = createQuery('RETURN 23');
    expect(query()).toEqual([23]);
  });

  it('should execute QueryBuilder instances with no arguments', function () {
    var query = createQuery({toAQL: function () {return 'RETURN 23';}});
    expect(query()).toEqual([23]);
  });

  it('should raise an ArangoError for unexpected arguments', function () {
    var err = null;
    var query = createQuery('RETURN 23');
    try {
      query({foo: 'chicken'});
    } catch (e) {
      err = e;
    }
    expect(err).not.toBe(null);
    expect(err.errorNum).toBe(arangodb.errors.ERROR_QUERY_BIND_PARAMETER_UNDECLARED.code);
    expect(err.constructor).toBe(arangodb.ArangoError);
  });

  it('should raise an ArangoError for missing arguments', function () {
    var err = null;
    var query = createQuery('RETURN @x');
    try {
      query();
    } catch (e) {
      err = e;
    }
    expect(err).not.toBe(null);
    expect(err.errorNum).toBe(arangodb.errors.ERROR_QUERY_BIND_PARAMETER_MISSING.code);
    expect(err.constructor).toBe(arangodb.ArangoError);
  });

  it('should execute a query with valid arguments', function () {
    var query = createQuery('RETURN @x');
    expect(query({x: 'chicken'})).toEqual(['chicken']);
    expect(query({x: 23})).toEqual([23]);
  });
});

describe('createQuery(cfg)', function () {
  'use strict';

  function FakeModel(args) {
    this.args = args;
  }

  describe('with cfg.context', function () {
    var cn = 'UnitTestFoxxQueues';

    beforeEach(function () {
      db._drop(cn);
      db._drop(cn + '_suffixed');
      db._create(cn, {waitForSync: false});
      db._collection(cn).save({foo: 'potato'});
      db._create(cn + '_suffixed', {waitForSync: false});
      db._collection(cn + '_suffixed').save({foo: 'tomato'});
    });

    afterEach(function () {
      db._drop(cn);
      db._drop(cn + '_suffixed');
    });

    it('should pass collection arguments through cfg.context.collectionName', function () {
      var spy, query;
      spy = createSpy().and.callFake(function (cn) {
        return cn + '_suffixed';
      });
      query = createQuery({
        query: 'FOR doc IN @@col RETURN doc.foo',
        context: {collectionName: spy}
      });
      expect(spy).not.toHaveBeenCalled();
      var result = query({'@col': cn});
      expect(spy).toHaveBeenCalledWith(cn);
      expect(result).toEqual(['tomato']);
    });

    it('should pass cfg.defaults collection names through cfg.context.collectionName', function () {
      var spy, query;
      spy = createSpy().and.callFake(function (cn) {
        return cn + '_suffixed';
      });
      query = createQuery({
        query: 'FOR doc IN @@col RETURN doc.foo',
        context: {collectionName: spy},
        defaults: {'@col': cn}
      });
      expect(spy).not.toHaveBeenCalled();
      var result = query();
      expect(spy).toHaveBeenCalledWith(cn);
      expect(result).toEqual(['tomato']);
    });
  });

  it('should mix cfg.defaults into arguments', function () {
    var query = createQuery({
      query: 'RETURN [@x, @y]',
      defaults: {x: 'chicken', y: 'kittens'}
    });
    expect(query()).toEqual([['chicken', 'kittens']]);
    expect(query({y: 'puppies'})).toEqual([['chicken', 'puppies']]);
  });

  it('should pass the query result to cfg.transform', function () {
    var spy, query;
    spy = createSpy().and.callFake(function (arg) {
      expect(arg).toEqual([1, 2, 3]);
    });
    query = createQuery({
      query: 'FOR x IN 1..3 RETURN x',
      transform: spy
    });
    expect(spy).not.toHaveBeenCalled();
    query();
    expect(spy).toHaveBeenCalled();
  });

  it('should return the result of cfg.transform', function () {
    var spy, query;
    spy = createSpy().and.callFake(function () {
      return 'chicken';
    });
    query = createQuery({
      query: 'FOR x IN 1..3 RETURN x',
      transform: spy
    });
    expect(query()).toEqual('chicken');
  });

  it('should convert the result to instances of cfg.model', function () {
    var result = createQuery({
      query: 'FOR x IN 1..3 RETURN {foo: x}',
      model: FakeModel
    })();
    expect(result.constructor).toBe(Array);
    expect(result.length).toEqual(3);
    expect(result[0].constructor).toBe(FakeModel);
    expect(result[0].args).toEqual({foo: 1});
    expect(result[1].constructor).toBe(FakeModel);
    expect(result[1].args).toEqual({foo: 2});
    expect(result[2].constructor).toBe(FakeModel);
    expect(result[2].args).toEqual({foo: 3});
  });

  it('should convert the result to models before transforming them', function () {
    var spy, query;
    spy = createSpy().and.callFake(function (args) {
      expect(args[0].constructor).toBe(FakeModel);
      expect(args[1].constructor).toBe(FakeModel);
      expect(args[2].constructor).toBe(FakeModel);
      return 'chicken';
    });
    query = createQuery({
      query: 'FOR x IN 1..3 RETURN x',
      transform: spy,
      model: FakeModel
    });
    expect(query()).toEqual('chicken');
    expect(spy).toHaveBeenCalled();
  });

  it('should throw an error if cfg.query is missing', function () {
    expect(function () {
      createQuery({noQuery: 'no fun'});
    }).toThrow();
  });

  it('should throw an error if cfg.query is invalid', function () {
    _.each(
      [23, {invalid: true}, function () {}, {toAQL: 'not a function'}],
      function (value) {
        expect(function () {
          createQuery({query: value});
        }).toThrow();
      }
    );
  });

  it('should throw an error if cfg.model is a non-function', function () {
    _.each(
      [{}, 'garbage', true, 23, Infinity],
      function (value) {
        expect(function () {
          createQuery({query: 'RETURN 23', model: value});
        }).toThrow();
      }
    );
  });

  it('should throw an error if cfg.transform is a non-function', function () {
    _.each(
      [{}, 'garbage', true, 23, Infinity],
      function (value) {
        expect(function () {
          createQuery({query: 'RETURN 23', transform: value});
        }).toThrow();
      }
    );
  });

  it('should throw an error if cfg.context has no "collectionName" method', function () {
    _.each(
      [{}, 'garbage', true, 23, Infinity, function () {}, {collectionName: 'not a function'}],
      function (value) {
        expect(function () {
          createQuery({query: 'RETURN 23', context: value});
        }).toThrow();
      }
    );
  });
});