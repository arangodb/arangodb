/*global require, describe, expect, it, beforeEach, createSpyObj */

var FoxxRepository = require("org/arangodb/foxx/repository").Repository,
  Model = require("org/arangodb/foxx/model").Model;

describe('Model Events', function () {
  'use strict';

  var collection, instance;

  beforeEach(function () {
    collection = createSpyObj('collection', [
      'save'
    ]);
    instance = new Model({ random: '', beforeCalled: false, afterCalled: false });
  });

  it('should be possible to subscribe and emit events', function () {
    expect(instance.on).toBeDefined();
    expect(instance.emit).toBeDefined();
  });

  it('should emit beforeCreate and afterCreate events when creating the model', function () {
    var repository = new FoxxRepository(collection, {model: Model});
    addHooks(instance, 'Create');
    expect(repository.save(instance)).toEqual(instance);
    expect(instance.get('beforeCalled')).toBe(true);
    expect(instance.get('afterCalled')).toBe(true);
  });

  it('should emit beforeSave and afterSave events when creating the model', function () {
    var repository = new FoxxRepository(collection, {model: Model});
    addHooks(instance, 'Save');
    expect(repository.save(instance)).toEqual(instance);
    expect(instance.get('beforeCalled')).toBe(true);
    expect(instance.get('afterCalled')).toBe(true);
  });

});

function addHooks(model, ev) {
  'use strict';

  var random = String(Math.floor(Math.random() * 1000));

  model.on('before' + ev, function () {
    expect(this).toEqual(model);
    this.set('random', random);
    this.set('beforeCalled', true);
  });
  model.on('after' + ev, function () {
    expect(this).toEqual(model);
    this.set('afterCalled', true);
    expect(this.get('beforeCalled')).toBe(true);
    expect(this.get('random')).toEqual(random);
  });
}