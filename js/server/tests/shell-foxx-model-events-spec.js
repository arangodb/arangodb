/*jshint globalstrict:false, strict:false */
/*global describe, expect, it, beforeEach, createSpyObj */

var FoxxRepository = require("@arangodb/foxx/legacy/repository").Repository,
  Model = require("@arangodb/foxx/legacy/model").Model;

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
    instance = new Model({ random: '', beforeCalled: false, afterCalled: false });
    repository = new FoxxRepository(collection, {model: Model});
  });

  it('should be possible to subscribe and emit events', function () {
    expect(instance.on).toBeDefined();
    expect(instance.emit).toBeDefined();
  });

  it('should emit beforeCreate and afterCreate events when creating the model', function () {
    addHooks(instance, 'Create');
    expect(repository.save(instance)).toEqual(instance);
    expect(instance.get('beforeCalled')).toBe(true);
    expect(instance.get('afterCalled')).toBe(true);
  });

  it('should emit beforeSave and afterSave events when creating the model', function () {
    addHooks(instance, 'Save');
    expect(repository.save(instance)).toEqual(instance);
    expect(instance.get('beforeCalled')).toBe(true);
    expect(instance.get('afterCalled')).toBe(true);
  });

  it('should emit beforeUpdate and afterUpdate events when updating the model', function () {
    var newData = { newAttribute: 'test' };
    addHooks(instance, 'Update', newData);
    expect(repository.update(instance, newData)).toEqual(instance);
    expect(instance.get('beforeCalled')).toBe(true);
    expect(instance.get('afterCalled')).toBe(true);
  });

  it('should emit beforeSave and afterSave events when updating the model', function () {
    var newData = { newAttribute: 'test' };
    addHooks(instance, 'Save', newData);
    expect(repository.update(instance, newData)).toEqual(instance);
    expect(instance.get('beforeCalled')).toBe(true);
    expect(instance.get('afterCalled')).toBe(true);
  });

  it('should emit beforeRemove and afterRemove events when removing the model', function () {
    addHooks(instance, 'Remove');
    repository.remove(instance);
    expect(instance.get('beforeCalled')).toBe(true);
    expect(instance.get('afterCalled')).toBe(true);
  });

});

function addHooks(model, ev, dataToReceive) {
  'use strict';

  var random = String(Math.floor(Math.random() * 1000));

  model.on('before' + ev, function (data) {
    expect(this).toEqual(model);
    expect(data).toEqual(dataToReceive);
    this.set('random', random);
    this.set('beforeCalled', true);
  });
  model.on('after' + ev, function (data) {
    expect(this).toEqual(model);
    expect(data).toEqual(dataToReceive);
    this.set('afterCalled', true);
    expect(this.get('beforeCalled')).toBe(true);
    expect(this.get('random')).toEqual(random);
  });
}