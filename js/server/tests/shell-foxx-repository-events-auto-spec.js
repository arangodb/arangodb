/*jshint globalstrict:false, strict:false */
/*global describe, expect, it, beforeEach, createSpyObj */

var FoxxRepository = require("org/arangodb/foxx/repository").Repository,
  FoxxModel = require("org/arangodb/foxx/model").Model;

describe('Repository Events', function () {
  'use strict';

  var collection, Model;

  beforeEach(function () {
    Model = FoxxModel.extend({}, {});
    collection = createSpyObj('collection', [
      'update',
      'save',
      'type',
      'remove'
    ]);
    collection.type.and.returnValue(2);
  });

  it('should emit beforeCreate and afterCreate events when creating the model', function () {
    var model = prepInstance(Model, 'Create');
    var repository = new FoxxRepository(collection, {model: Model});
    expect(repository.save(model)).toEqual(model);
    expect(model.get('beforeCalled')).toBe(true);
    expect(model.get('afterCalled')).toBe(true);
  });

  it('should emit beforeSave and afterSave events when creating the model', function () {
    var model = prepInstance(Model, 'Save');
    var repository = new FoxxRepository(collection, {model: Model});
    expect(repository.save(model)).toEqual(model);
    expect(model.get('beforeCalled')).toBe(true);
    expect(model.get('afterCalled')).toBe(true);
  });

  it('should emit beforeUpdate and afterUpdate events when updating the model', function () {
    var newData = { newAttribute: 'test' };
    var model = prepInstance(Model, 'Update', newData);
    var repository = new FoxxRepository(collection, {model: Model});
    expect(repository.update(model, newData)).toEqual(model);
    expect(model.get('beforeCalled')).toBe(true);
    expect(model.get('afterCalled')).toBe(true);
  });

  it('should emit beforeSave and afterSave events when updating the model', function () {
    var newData = { newAttribute: 'test' };
    var model = prepInstance(Model, 'Save', newData);
    var repository = new FoxxRepository(collection, {model: Model});
    expect(repository.update(model, newData)).toEqual(model);
    expect(model.get('beforeCalled')).toBe(true);
    expect(model.get('afterCalled')).toBe(true);
  });

  it('should emit beforeRemove and afterRemove events when removing the model', function () {
    var model = prepInstance(Model, 'Remove');
    var repository = new FoxxRepository(collection, {model: Model});
    repository.remove(model);
    expect(model.get('beforeCalled')).toBe(true);
    expect(model.get('afterCalled')).toBe(true);
  });

});

function prepInstance(Model, ev, dataToReceive) {
  'use strict';
  var model;
  var random = String(Math.floor(Math.random() * 1000));

  Model['before' + ev] = function (instance, data) {
    expect(instance).toEqual(model);
    expect(data).toEqual(dataToReceive);
    instance.set('random', random);
    instance.set('beforeCalled', true);
  };

  Model['after' + ev] = function (instance, data) {
    expect(instance).toEqual(model);
    expect(data).toEqual(dataToReceive);
    instance.set('afterCalled', true);
    expect(instance.get('beforeCalled')).toBe(true);
    expect(instance.get('random')).toEqual(random);
  };

  model = new Model({
    random: '',
    beforeCalled: false,
    afterCalled: false
  });

  return model;
}
