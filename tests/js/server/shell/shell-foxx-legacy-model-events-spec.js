/* global describe, it, beforeEach*/
'use strict';

var sinon = require('sinon');
const expect = require('chai').expect;
var FoxxRepository = require('@arangodb/foxx/legacy/repository').Repository;
var Model = require('@arangodb/foxx/legacy/model').Model;

require("@arangodb/test-helper").waitForFoxxInitialized();

describe('Model Events', function () {
  var collection, instance, repository;

  beforeEach(function () {
    collection = {
      update: function () {},
      save: function () {},
      type: sinon.stub().returns(2),
      remove: function () {}
    };
    instance = new Model({ random: '', beforeCalled: false, afterCalled: false });
    repository = new FoxxRepository(collection, {model: Model});
  });

  it('should be possible to subscribe and emit events', function () {
    expect(instance.on).to.be.a('function');
    expect(instance.emit).to.be.a('function');
  });

  it('should emit beforeCreate and afterCreate events when creating the model', function () {
    addHooks(instance, 'Create');
    expect(repository.save(instance)).to.eql(instance);
    expect(instance.get('beforeCalled')).to.be.true;
    expect(instance.get('afterCalled')).to.be.true;
  });

  it('should emit beforeSave and afterSave events when creating the model', function () {
    addHooks(instance, 'Save');
    expect(repository.save(instance)).to.eql(instance);
    expect(instance.get('beforeCalled')).to.be.true;
    expect(instance.get('afterCalled')).to.be.true;
  });

  it('should emit beforeUpdate and afterUpdate events when updating the model', function () {
    var newData = { newAttribute: 'test' };
    addHooks(instance, 'Update', newData);
    expect(repository.update(instance, newData)).to.eql(instance);
    expect(instance.get('beforeCalled')).to.be.true;
    expect(instance.get('afterCalled')).to.be.true;
  });

  it('should emit beforeSave and afterSave events when updating the model', function () {
    var newData = { newAttribute: 'test' };
    addHooks(instance, 'Save', newData);
    expect(repository.update(instance, newData)).to.eql(instance);
    expect(instance.get('beforeCalled')).to.be.true;
    expect(instance.get('afterCalled')).to.be.true;
  });

  it('should emit beforeRemove and afterRemove events when removing the model', function () {
    addHooks(instance, 'Remove');
    repository.remove(instance);
    expect(instance.get('beforeCalled')).to.be.true;
    expect(instance.get('afterCalled')).to.be.true;
  });
});

function addHooks (model, ev, dataToReceive) {
  var random = String(Math.floor(Math.random() * 1000));

  model.on('before' + ev, function (data) {
    expect(this).to.eql(model);
    expect(data).to.eql(dataToReceive);
    this.set('random', random);
    this.set('beforeCalled', true);
  });
  model.on('after' + ev, function (data) {
    expect(this).to.eql(model);
    expect(data).to.eql(dataToReceive);
    this.set('afterCalled', true);
    expect(this.get('beforeCalled')).to.be.true;
    expect(this.get('random')).to.eql(random);
  });
}
