/* global describe, it, beforeEach */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Spec for foxx repository
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
// / @author Copyright 2016, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const expect = require('chai').expect;
const sinon = require('sinon');
const FoxxRepository = require('@arangodb/foxx/legacy/repository').Repository;
const FoxxModel = require('@arangodb/foxx/legacy/model').Model;

require("@arangodb/test-helper").waitForFoxxInitialized();

describe('Repository Events', function () {
  let collection, Model;

  beforeEach(function () {
    Model = FoxxModel.extend({}, {});
    collection = {};
  });

  it('should emit beforeCreate and afterCreate events when creating the model', function () {
    var model = prepInstance(Model, 'Create');
    collection.type = sinon.stub().returns(2);
    collection.save = sinon.stub();
    var repository = new FoxxRepository(collection, {model: Model});
    expect(repository.save(model)).to.equal(model);
    expect(model.get('beforeCalled')).to.equal(true);
    expect(model.get('afterCalled')).to.equal(true);
  });

  it('should emit beforeSave and afterSave events when creating the model', function () {
    var model = prepInstance(Model, 'Save');
    collection.type = sinon.stub().returns(2);
    collection.save = sinon.stub();
    var repository = new FoxxRepository(collection, {model: Model});
    expect(repository.save(model)).to.equal(model);
    expect(model.get('beforeCalled')).to.equal(true);
    expect(model.get('afterCalled')).to.equal(true);
  });

  it('should emit beforeUpdate and afterUpdate events when updating the model', function () {
    var newData = {newAttribute: 'test'};
    var model = prepInstance(Model, 'Update', newData);
    collection.type = sinon.stub().returns(2);
    collection.update = sinon.stub();
    var repository = new FoxxRepository(collection, {model: Model});
    expect(repository.update(model, newData)).to.equal(model);
    expect(model.get('beforeCalled')).to.equal(true);
    expect(model.get('afterCalled')).to.equal(true);
  });

  it('should emit beforeSave and afterSave events when updating the model', function () {
    var newData = {newAttribute: 'test'};
    var model = prepInstance(Model, 'Save', newData);
    collection.type = sinon.stub().returns(2);
    collection.update = sinon.stub();
    var repository = new FoxxRepository(collection, {model: Model});
    expect(repository.update(model, newData)).to.equal(model);
    expect(model.get('beforeCalled')).to.equal(true);
    expect(model.get('afterCalled')).to.equal(true);
  });

  it('should emit beforeRemove and afterRemove events when removing the model', function () {
    var model = prepInstance(Model, 'Remove');
    collection.type = sinon.stub().returns(2);
    collection.remove = sinon.stub();
    var repository = new FoxxRepository(collection, {model: Model});
    repository.remove(model);
    expect(model.get('beforeCalled')).to.equal(true);
    expect(model.get('afterCalled')).to.equal(true);
  });
});

function prepInstance (Model, ev, dataToReceive) {
  let model;
  const random = String(Math.floor(Math.random() * 1000));

  Model['before' + ev] = function (instance, data) {
    expect(instance).to.equal(model);
    expect(data).to.equal(dataToReceive);
    instance.set('random', random);
    instance.set('beforeCalled', true);
  };

  Model['after' + ev] = function (instance, data) {
    expect(instance).to.equal(model);
    expect(data).to.equal(dataToReceive);
    instance.set('afterCalled', true);
    expect(instance.get('beforeCalled')).to.equal(true);
    expect(instance.get('random')).to.equal(random);
  };

  model = new Model({
    random: '',
    beforeCalled: false,
    afterCalled: false
  });

  return model;
}
