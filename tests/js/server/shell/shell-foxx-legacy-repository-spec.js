/* global describe, it, beforeEach */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Spec for foxx repository
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
// / @author Lucas Dohmen
// / @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const expect = require('chai').expect;
const sinon = require('sinon');
const FoxxRepository = require('@arangodb/foxx/legacy/repository').Repository;
const FoxxModel = require('@arangodb/foxx/legacy/model').Model;

require("@arangodb/test-helper").waitForFoxxInitialized();

describe('Repository', function () {
  const prefix = 'myApp';
  let collection;
  let TestModel;

  beforeEach(function () {
    collection = {};
    TestModel = FoxxModel.extend({
      forDB() {
        return {fancy: 1};
      }
    });
  });

  it('should initialize with collection only', function () {
    const instance = new FoxxRepository(collection);

    expect(instance.collection).to.equal(collection);
    expect(instance.model).to.equal(FoxxModel);
  });

  it('should initialize with collection and model prototype', function () {
    const instance = new FoxxRepository(collection, {
      model: TestModel
    });

    expect(instance.collection).to.equal(collection);
    expect(instance.model).to.equal(TestModel);
  });

  it('should initialize with collection and prefix', function () {
    const instance = new FoxxRepository(collection, {
      prefix: prefix
    });

    expect(instance.collection).to.equal(collection);
    expect(instance.model).to.equal(FoxxModel);
    expect(instance.prefix).to.equal(prefix);
  });

  it('should add a method with extend', function () {
    const result = Symbol();
    const testFn = sinon.spy(function () {
      return result;
    });
    const TestRepository = FoxxRepository.extend({
      test: testFn
    });

    const instance = new TestRepository(collection);
    expect(testFn.called).to.equal(false);
    expect(instance.test()).to.equal(result);
    expect(testFn.called).to.equal(true);
  });
});

describe('Repository Indexes', function () {
  it('should create indexes on instantiation', function () {
    const collection = {ensureIndex: sinon.spy()};
    const indexes = Object.freeze([
      {type: 'skiplist', xyz: 'abcdef'},
      {type: 'geo', more: 'args'},
      {type: 'foo', bar: 'qux'}
    ]);
    const TestRepository = FoxxRepository.extend({
      indexes: indexes
    });

    const rep = new TestRepository(collection);
    expect(rep).to.be.an.instanceOf(TestRepository);

    expect(collection.ensureIndex.callCount).to.equal(3);
    expect(collection.ensureIndex.firstCall.calledWithExactly(indexes[0])).to.equal(true);
    expect(collection.ensureIndex.secondCall.calledWithExactly(indexes[1])).to.equal(true);
    expect(collection.ensureIndex.thirdCall.calledWithExactly(indexes[2])).to.equal(true);
  });

  it('should add skiplist methods to the prototype', function () {
    expect(FoxxRepository.prototype).not.to.have.a.property('range');
    const TestRepository = FoxxRepository.extend({
      indexes: [
        {type: 'skiplist'}
      ]
    });

    expect(TestRepository.prototype.range).to.be.a('function');
  });

  it('should add geo methods to the prototype', function () {
    expect(FoxxRepository.prototype).not.to.have.a.property('near');
    expect(FoxxRepository.prototype).not.to.have.a.property('within');
    const TestRepository = FoxxRepository.extend({
      indexes: [
        {type: 'geo'}
      ]
    });
    expect(TestRepository.prototype.near).to.be.a('function');
    expect(TestRepository.prototype.within).to.be.a('function');
  });

  it('should add fulltext methods to the prototype', function () {
    expect(FoxxRepository.prototype).not.to.have.a.property('fulltext');
    const TestRepository = FoxxRepository.extend({
      indexes: [
        {type: 'fulltext'}
      ]
    });
    expect(TestRepository.prototype.fulltext).to.be.a('function');
  });
});

describe('Repository Methods', function () {
  let collection;
  let repository;

  beforeEach(function () {
    collection = {};
    repository = new FoxxRepository(collection, {model: FoxxModel});
  });

  describe('for adding entries', function () {
    it('should allow to save', function () {
      const modelData = Symbol();
      const idAndRev = Symbol();
      const model = new FoxxModel(modelData);

      collection.type = sinon.stub().returns(2);
      collection.save = sinon.stub().returns(idAndRev);
      model.forDB = sinon.stub().returns(modelData);
      model.set = sinon.spy();

      expect(repository.save(model)).to.equal(model);
      expect(collection.save.callCount).to.equal(1);
      expect(collection.save.calledWithExactly(modelData)).to.equal(true);
      expect(model.forDB.callCount).to.equal(1);
      expect(model.forDB.calledWithExactly()).to.equal(true);
      expect(model.set.callCount).to.equal(1);
      expect(model.set.calledWithExactly(idAndRev)).to.equal(true);
    });
  });

  describe('for finding entries', function () {
    it('should allow to find by ID', function () {
      const data = Symbol();
      const id = Symbol();

      collection.document = sinon.stub().returns(data);
      const model = repository.byId(id);

      expect(model).to.be.an.instanceOf(FoxxModel);
      expect(collection.document.callCount).to.equal(1);
      expect(collection.document.calledWithExactly(id)).to.equal(true);
    });

    it('should find by example', function () {
      const example = {color: 'red'};
      const cursor = {toArray: sinon.stub().returns([Symbol()])};
      collection.byExample = sinon.stub().returns(cursor);

      const models = repository.byExample(example);

      expect(models.length).to.equal(1);
      expect(models[0]).to.be.an.instanceOf(FoxxModel);
      expect(collection.byExample.callCount).to.equal(1);
      expect(collection.byExample.calledWithExactly(example)).to.equal(true);
    });

    it('should find first by example', function () {
      const example = {color: 'red'};
      collection.firstExample = sinon.stub().returns(Symbol());

      const model = repository.firstExample(example);

      expect(model).to.be.an.instanceOf(FoxxModel);
      expect(collection.firstExample.callCount).to.equal(1);
      expect(collection.firstExample.calledWithExactly(example)).to.equal(true);
    });

    it('should find all', function () {
      const cursor = {
        skip: sinon.stub().returnsThis(),
        limit: sinon.stub().returnsThis(),
        toArray: sinon.stub().returns([Symbol()])
      };
      collection.all = sinon.stub().returns(cursor);

      const models = repository.all({skip: 4, limit: 2});
      expect(models.length).to.equal(1);
      expect(models[0]).to.be.an.instanceOf(FoxxModel);
      expect(cursor.skip.callCount).to.equal(1);
      expect(cursor.skip.calledWithExactly(4)).to.equal(true);
      expect(cursor.limit.callCount).to.equal(1);
      expect(cursor.limit.calledWithExactly(2)).to.equal(true);
    });
  });

  describe('for removing entries', function () {
    it('should allow to remove a model', function () {
      const model = new FoxxModel();
      const id = Symbol();
      model.get = sinon.stub().returns(id);
      collection.remove = sinon.spy();

      repository.remove(model);

      expect(collection.remove.callCount).to.equal(1);
      expect(collection.remove.calledWithExactly(id)).to.equal(true);
      expect(model.get.callCount).to.equal(1);
      expect(model.get.calledWithExactly('_id')).to.equal(true);
    });

    it('should allow to remove by ID', function () {
      const id = Symbol();
      collection.remove = sinon.spy();

      repository.removeById(id);

      expect(collection.remove.callCount).to.equal(1);
      expect(collection.remove.calledWithExactly(id)).to.equal(true);
    });

    it('should allow to remove by example', function () {
      const example = Symbol();
      collection.removeByExample = sinon.spy();

      repository.removeByExample(example);

      expect(collection.removeByExample.callCount).to.equal(1);
      expect(collection.removeByExample.calledWithExactly(example)).to.equal(true);
    });
  });

  describe('for replacing entries', function () {
    it('should allow to replace by model', function () {
      const model = new FoxxModel();
      const idAndRev = Symbol();
      const id = Symbol();
      const data = Symbol();
      model.get = sinon.stub().returns(id);
      model.forDB = sinon.stub().returns(data);
      model.set = sinon.spy();
      collection.replace = sinon.stub().returns(idAndRev);

      const result = repository.replace(model);

      expect(result).to.equal(model);
      expect(model.set.callCount).to.equal(1);
      expect(model.set.calledWithExactly(idAndRev)).to.equal(true);
      expect(collection.replace.callCount).to.equal(1);
      expect(collection.replace.calledWithExactly(id, data)).to.equal(true);
      expect(model.get.callCount).to.equal(1);
      expect(model.get.calledWithExactly('_id')).to.equal(true);
    });

    it('should allow to replace by ID', function () {
      const model = new FoxxModel();
      const idAndRev = Symbol();
      const id = Symbol();
      const data = Symbol();

      model.forDB = sinon.stub().returns(data);
      model.set = sinon.spy();
      collection.replace = sinon.stub().returns(idAndRev);

      const result = repository.replaceById(id, model);

      expect(result).to.equal(model);
      expect(model.set.callCount).to.equal(1);
      expect(model.set.calledWithExactly(idAndRev)).to.equal(true);
      expect(collection.replace.callCount).to.equal(1);
      expect(collection.replace.calledWithExactly(id, data)).to.equal(true);
    });

    it('should replace by example', function () {
      const example = Symbol();
      const data = Symbol();

      collection.replaceByExample = sinon.spy();
      repository.replaceByExample(example, data);

      expect(collection.replaceByExample.callCount).to.equal(1);
      expect(collection.replaceByExample.calledWithExactly(example, data)).to.equal(true);
    });
  });

  describe('for updating entries', function () {
    it('should allow to update by model', function () {
      const model = new FoxxModel();
      const idAndRev = Symbol();
      const id = Symbol();
      const data = Symbol();
      const updates = Symbol();

      model.get = sinon.stub().returns(id);
      model.forDB = sinon.stub().returns(data);
      model.set = sinon.spy();
      collection.update = sinon.stub().returns(idAndRev);

      const result = repository.update(model, updates);

      expect(result).to.equal(model);
      expect(model.set.callCount).to.equal(2);
      expect(model.set.firstCall.calledWithExactly(updates)).to.equal(true);
      expect(model.set.secondCall.calledWithExactly(idAndRev)).to.equal(true);
      expect(collection.update.callCount).to.equal(1);
      expect(collection.update.calledWithExactly(id, updates)).to.equal(true);
      expect(model.get.callCount).to.equal(1);
      expect(model.get.calledWithExactly('_id')).to.equal(true);
    });

    it('should update by id', function () {
      const id = Symbol();
      const updates = Symbol();
      const idAndRev = Symbol();

      collection.update = sinon.stub().returns(idAndRev);
      repository.updateById(id, updates);

      expect(collection.update.callCount).to.equal(1);
      expect(collection.update.calledWithExactly(id, updates)).to.equal(true);
    });

    it('should update by example', function () {
      const example = Symbol();
      const data = Symbol();
      collection.updateByExample = sinon.spy();

      repository.updateByExample(example, data);

      expect(collection.updateByExample.callCount).to.equal(1);
      expect(collection.updateByExample.calledWithExactly(example, data, undefined)).to.equal(true);
    });
  });

  describe('for counting entries', function () {
    it('should count all', function () {
      const count = Symbol();
      collection.count = sinon.stub().returns(count);

      const result = repository.count();

      expect(result).to.equal(count);
      expect(collection.count.callCount).to.equal(1);
    });
  });
});
