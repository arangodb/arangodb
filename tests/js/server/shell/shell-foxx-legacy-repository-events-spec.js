/* global describe, it, beforeEach */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Spec for foxx repository
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2015-2016 ArangoDB GmbH, Cologne, Germany
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
// / @author Alan Plum
// / @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const expect = require('chai').expect;
const sinon = require('sinon');
const FoxxRepository = require('@arangodb/foxx/legacy/repository').Repository;
const Model = require('@arangodb/foxx/legacy/model').Model;

require("@arangodb/test-helper").waitForFoxxInitialized();

describe('Model Events', function () {
  let collection, instance, repository;

  beforeEach(function () {
    collection = {
      type: sinon.stub().returns(2)
    };
    instance = new Model();
    repository = new FoxxRepository(collection, {model: Model, random: '', beforeCalled: false, afterCalled: false});
  });

  it('should be possible to subscribe and emit events', function () {
    expect(repository.on).to.be.a('function');
    expect(repository.emit).to.be.a('function');
  });

  it('should emit beforeCreate and afterCreate events when creating the model', function () {
    collection.save = sinon.stub();
    addHooks(repository, instance, 'Create');
    expect(repository.save(instance)).to.equal(instance);
    expect(repository.beforeCalled).to.equal(true);
    expect(repository.afterCalled).to.equal(true);
  });

  it('should emit beforeSave and afterSave events when creating the model', function () {
    collection.save = sinon.stub();
    addHooks(repository, instance, 'Save');
    expect(repository.save(instance)).to.equal(instance);
    expect(repository.beforeCalled).to.equal(true);
    expect(repository.afterCalled).to.equal(true);
  });

  it('should emit beforeUpdate and afterUpdate events when updating the model', function () {
    var newData = {newAttribute: 'test'};
    collection.update = sinon.stub();
    addHooks(repository, instance, 'Update', newData);
    expect(repository.update(instance, newData)).to.equal(instance);
    expect(repository.beforeCalled).to.equal(true);
    expect(repository.afterCalled).to.equal(true);
  });

  it('should emit beforeSave and afterSave events when updating the model', function () {
    var newData = {newAttribute: 'test'};
    collection.update = sinon.stub();
    addHooks(repository, instance, 'Save', newData);
    expect(repository.update(instance, newData)).to.equal(instance);
    expect(repository.beforeCalled).to.equal(true);
    expect(repository.afterCalled).to.equal(true);
  });

  it('should emit beforeRemove and afterRemove events when removing the model', function () {
    collection.remove = sinon.stub();
    addHooks(repository, instance, 'Remove');
    repository.remove(instance);
    expect(repository.beforeCalled).to.equal(true);
    expect(repository.afterCalled).to.equal(true);
  });
});

function addHooks (repo, model, ev, dataToReceive) {
  const random = String(Math.floor(Math.random() * 1000));

  repo.on('before' + ev, function (self, data) {
    expect(this).to.equal(repo);
    expect(self).to.equal(model);
    expect(data).to.equal(dataToReceive);
    this.random = random;
    this.beforeCalled = true;
  });

  repo.on('after' + ev, function (self, data) {
    expect(this).to.equal(repo);
    expect(self).to.equal(model);
    expect(data).to.equal(dataToReceive);
    this.afterCalled = true;
    expect(this.beforeCalled).to.equal(true);
    expect(this.random).to.equal(random);
  });
}
