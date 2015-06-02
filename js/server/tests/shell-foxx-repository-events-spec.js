/*jshint globalstrict:false, strict:false */
/*global describe, expect, it, beforeEach, createSpyObj */

var FoxxRepository = require("org/arangodb/foxx/repository").Repository,
  Model = require("org/arangodb/foxx/model").Model;

describe('Model Events', function () {
  'use strict';

  var collection, instance, repository;

  beforeEach(function () {
    collection = createSpyObj('collection', [
      'update',
      'save',
      'type',
      'remove'
    ]);
    collection.type.and.returnValue(2);
    instance = new Model();
    repository = new FoxxRepository(collection, {model: Model, random: '', beforeCalled: false, afterCalled: false});
  });

  it('should be possible to subscribe and emit events', function () {
    expect(repository.on).toBeDefined();
    expect(repository.emit).toBeDefined();
  });

  it('should emit beforeCreate and afterCreate events when creating the model', function () {
    addHooks(repository, instance, 'Create');
    expect(repository.save(instance)).toEqual(instance);
    expect(repository.beforeCalled).toBe(true);
    expect(repository.afterCalled).toBe(true);
  });

  it('should emit beforeSave and afterSave events when creating the model', function () {
    addHooks(repository, instance, 'Save');
    expect(repository.save(instance)).toEqual(instance);
    expect(repository.beforeCalled).toBe(true);
    expect(repository.afterCalled).toBe(true);
  });

  it('should emit beforeUpdate and afterUpdate events when updating the model', function () {
    var newData = { newAttribute: 'test' };
    addHooks(repository, instance, 'Update', newData);
    expect(repository.update(instance, newData)).toEqual(instance);
    expect(repository.beforeCalled).toBe(true);
    expect(repository.afterCalled).toBe(true);
  });

  it('should emit beforeSave and afterSave events when updating the model', function () {
    var newData = { newAttribute: 'test' };
    addHooks(repository, instance, 'Save', newData);
    expect(repository.update(instance, newData)).toEqual(instance);
    expect(repository.beforeCalled).toBe(true);
    expect(repository.afterCalled).toBe(true);
  });

  it('should emit beforeRemove and afterRemove events when removing the model', function () {
    addHooks(repository, instance, 'Remove');
    repository.remove(instance);
    expect(repository.beforeCalled).toBe(true);
    expect(repository.afterCalled).toBe(true);
  });

});

function addHooks(repo, model, ev, dataToReceive) {
  'use strict';

  var random = String(Math.floor(Math.random() * 1000));

  repo.on('before' + ev, function (self, data) {
    expect(this).toEqual(repo);
    expect(self).toEqual(model);
    expect(data).toEqual(dataToReceive);
    this.random = random;
    this.beforeCalled = true;
  });
  repo.on('after' + ev, function (self, data) {
    expect(this).toEqual(repo);
    expect(self).toEqual(model);
    expect(data).toEqual(dataToReceive);
    this.afterCalled = true;
    expect(this.beforeCalled).toBe(true);
    expect(this.random).toEqual(random);
  });
}