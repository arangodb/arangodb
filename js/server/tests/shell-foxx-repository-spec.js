/*global require, describe, beforeEach, it, expect, spyOn, createSpy, createSpyObj */

var FoxxRepository = require("org/arangodb/foxx/repository").Repository,
  Model = require("org/arangodb/foxx/model").Model;

describe('Repository', function () {
  'use strict';
  var TestRepository, instance, prefix, collection, ModelPrototype, model, modelData;

  beforeEach(function () {
    prefix = "myApp";
    collection = null;
    ModelPrototype = createSpy('ModelPrototype');
    model = new ModelPrototype();
    modelData = { fancy: 1 };
    model.forDB = function () {
      return modelData;
    };
  });

  it('should initialize with collection only', function () {
    instance = new FoxxRepository(collection);

    expect(instance.collection).toEqual(collection);
    expect(instance.modelPrototype).toEqual(Model);
  });

  it('should initialize with collection and model prototype', function () {
    instance = new FoxxRepository(collection, {
      model: ModelPrototype
    });

    expect(instance.collection).toEqual(collection);
    expect(instance.modelPrototype).toEqual(ModelPrototype);
  });

  it('should initialize with collection and prefix', function () {
    instance = new FoxxRepository(collection, {
      prefix: prefix
    });

    expect(instance.collection).toEqual(collection);
    expect(instance.modelPrototype).toEqual(Model);
    expect(instance.prefix).toEqual(prefix);
  });

  it('should add a method with extend', function () {
    TestRepository = FoxxRepository.extend({
      test: function () {
        return "test";
      }
    });

    instance = new TestRepository(collection);

    expect(instance.test()).toEqual("test");
  });
});

describe('Repository Indexes', function () {
  'use strict';
  it('should create indexes on instantiation', function () {
    var collection = createSpyObj('collection', [
        'ensureIndex'
      ]),
      indexes = [
        {type: 'skiplist', xyz: 'abcdef'},
        {type: 'geo', more: 'args'},
        {type: 'foo', bar: 'qux'}
      ],
      Repository = FoxxRepository.extend({
        indexes: indexes
      });

    new Repository(collection, {model: Model});

    expect(collection.ensureIndex.calls.count()).toEqual(3);
    expect(collection.ensureIndex).toHaveBeenCalledWith(indexes[0]);
    expect(collection.ensureIndex).toHaveBeenCalledWith(indexes[1]);
    expect(collection.ensureIndex).toHaveBeenCalledWith(indexes[2]);
  });

  it('should add skiplist methods to the prototype', function () {
    expect(FoxxRepository.prototype.range).toBeUndefined();
    var Repository = FoxxRepository.extend({
      indexes: [
        {type: 'skiplist'}
      ]
    });

    expect(typeof Repository.prototype.range).toBe('function');
  });

  it('should add geo methods to the prototype', function () {
    expect(FoxxRepository.prototype.near).toBeUndefined();
    expect(FoxxRepository.prototype.within).toBeUndefined();
    var Repository = FoxxRepository.extend({
      indexes: [
        {type: 'geo'}
      ]
    });
    expect(typeof Repository.prototype.near).toBe('function');
    expect(typeof Repository.prototype.within).toBe('function');
  });

  it('should add fulltext methods to the prototype', function () {
    expect(FoxxRepository.prototype.fulltext).toBeUndefined();
    var Repository = FoxxRepository.extend({
      indexes: [
        {type: 'fulltext'}
      ]
    });
    expect(typeof Repository.prototype.fulltext).toBe('function');
  });
});

describe('Repository Methods', function () {
  'use strict';
  var collection,
    instance;

  beforeEach(function () {
    collection = createSpyObj('collection', [
      'all',
      'save',
      'count',
      'document',
      'byExample',
      'firstExample',
      'remove',
      'removeByExample',
      'update',
      'updateByExample',
      'replace',
      'replaceByExample'
    ]);
    instance = new FoxxRepository(collection, { model: Model });
  });

  describe('for adding entries', function () {
    it('should allow to save', function () {
      var modelData = {},
        idAndRev = createSpy('IdAndRev'),
        model = new Model(modelData);

      collection.save.and.returnValue(idAndRev);
      spyOn(model, 'forDB').and.returnValue(modelData);
      spyOn(model, 'set');

      expect(instance.save(model)).toEqual(model);
      expect(collection.save.calls.argsFor(0)).toEqual([modelData]);
      expect(model.forDB.calls.argsFor(0)).toEqual([]);
      expect(model.set.calls.argsFor(0)).toEqual([idAndRev]);
    });
  });

  describe('for finding entries', function () {
    it('should allow to find by ID', function () {
      var data = createSpy('data'),
        id = createSpy('id'),
        model;

      collection.document.and.returnValue(data);

      model = instance.byId(id);

      // TODO: Would prefer to mock the constructor and check for the specific instance
      expect(model instanceof Model).toBe(true);
      expect(collection.document.calls.argsFor(0)).toEqual([id]);
    });

    it('should find by example', function () {
      var example = { color: 'red' },
        cursor = createSpyObj('cursor', ['toArray']),
        data = createSpy('data'),
        models;

      collection.byExample.and.returnValue(cursor);
      cursor.toArray.and.returnValue([data]);

      models = instance.byExample(example);

      // TODO: Would prefer to mock the constructor and check for the specific instance
      expect(models[0] instanceof Model).toBe(true);
      expect(collection.byExample.calls.argsFor(0)).toEqual([example]);
    });

    it('should find first by example', function () {
      var example = { color: 'red' },
        data = createSpy('data'),
        model;

      collection.firstExample.and.returnValue(data);

      model = instance.firstExample(example);

      // TODO: Would prefer to mock the constructor and check for the specific instance
      expect(model instanceof Model).toBe(true);
      expect(collection.firstExample.calls.argsFor(0)).toEqual([example]);
    });

    it('should find all', function () {
      var cursor = createSpyObj('cursor', ['skip', 'limit', 'toArray']),
        result = [{}],
        models;

      collection.all.and.returnValue(cursor);
      cursor.skip.and.returnValue(cursor);
      cursor.limit.and.returnValue(cursor);
      cursor.toArray.and.returnValue(result);

      // TODO: Would prefer to mock the constructor and check for the specific instance
      models = instance.all({ skip: 4, limit: 2 });
      expect(models[0] instanceof Model).toBe(true);
      expect(cursor.skip.calls.argsFor(0)).toEqual([4]);
      expect(cursor.limit.calls.argsFor(0)).toEqual([2]);
    });
  });

  describe('for removing entries', function () {
    it('should allow to remove a model', function () {
      var model = new Model(),
        id = createSpy('id');

      spyOn(model, 'get').and.returnValue(id);

      instance.remove(model);

      expect(collection.remove.calls.argsFor(0)).toEqual([id]);
      expect(model.get.calls.argsFor(0)).toEqual(['_id']);
    });

    it('should allow to remove by ID', function () {
      var id = createSpy('id');

      instance.removeById(id);

      expect(collection.remove.calls.argsFor(0)).toEqual([id]);
    });

    it('should allow to remove by example', function () {
      var example = createSpy('example');

      instance.removeByExample(example);

      expect(collection.removeByExample.calls.argsFor(0)).toEqual([example]);
    });
  });

  // TODO: Less mocking, just build an according model
  describe('for replacing entries', function () {
    it('should allow to replace by model', function () {
      var model = new Model({}),
        idAndRev = createSpy('idAndRev'),
        id = createSpy('id'),
        data = createSpy('data'),
        result;

      spyOn(model, 'get').and.returnValue(id);
      spyOn(model, 'forDB').and.returnValue(data);
      spyOn(model, 'set');
      collection.replace.and.returnValue(idAndRev);

      result = instance.replace(model);

      expect(result).toBe(model);
      expect(model.set.calls.argsFor(0)).toEqual([idAndRev]);
      expect(collection.replace.calls.argsFor(0)).toEqual([id, data]);
      expect(model.get.calls.argsFor(0)).toEqual(['_id']);
    });

    it('should allow to replace by ID', function () {
      var model = new Model({}),
        idAndRev = createSpy('idAndRev'),
        id = createSpy('id'),
        data = createSpy('data'),
        result;

      spyOn(model, 'forDB').and.returnValue(data);
      spyOn(model, 'set');
      collection.replace.and.returnValue(idAndRev);

      result = instance.replaceById(id, model);

      expect(result).toBe(model);
      expect(model.set.calls.argsFor(0)).toEqual([idAndRev]);
      expect(collection.replace.calls.argsFor(0)).toEqual([id, data]);
    });

    it('should replace by example', function () {
      var example = createSpy('example'),
        data = createSpy('data');

      instance.replaceByExample(example, data);

      expect(collection.replaceByExample.calls.argsFor(0)).toEqual([example, data]);
    });
  });

  describe('for updating entries', function () {
    it('should allow to update by model', function () {
      var model = new Model({}),
        idAndRev = createSpy('idAndRev'),
        id = createSpy('id'),
        data = createSpy('data'),
        updates = createSpy('updates'),
        result;

      spyOn(model, 'get').and.returnValue(id);
      spyOn(model, 'forDB').and.returnValue(data);
      spyOn(model, 'set');
      collection.update.and.returnValue(idAndRev);

      result = instance.update(model, updates);

      expect(result).toBe(model);
      expect(model.set.calls.allArgs().length).toEqual(2);
      expect(model.set.calls.allArgs()).toContain([idAndRev]);
      expect(model.set.calls.allArgs()).toContain([updates]);
      expect(collection.update.calls.argsFor(0)).toEqual([id, updates]);
      expect(model.get.calls.argsFor(0)).toEqual(['_id']);
    });

    it('should update by id', function () {
      var id = createSpy('id'),
        updates = createSpy('updates'),
        idAndRev = createSpy('idAndRev');

      collection.update.and.returnValue(idAndRev);
      instance.updateById(id, updates);

      expect(collection.update.calls.argsFor(0)).toEqual([id, updates]);
    });

    it('should update by example', function () {
      var example = createSpy('example'),
        data = createSpy('data');

      instance.updateByExample(example, data);

      expect(collection.updateByExample.calls.argsFor(0)).toEqual([example, data]);
    });
  });

  describe('for counting entries', function () {
    it('should count all', function () {
      var count = createSpy('count');
      collection.count.and.returnValue(count);
      expect(instance.count()).toBe(count);
    });
  });
});
