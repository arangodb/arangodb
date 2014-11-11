/*global require, describe, expect, it */

var Model = require("org/arangodb/foxx/model").Model;

describe('Model Events', function () {
  'use strict';

  it('should be possible to subscribe and emit events', function () {
    var instance = new Model();
    expect(instance.on).toBeDefined();
    expect(instance.emit).toBeDefined();
  });

});