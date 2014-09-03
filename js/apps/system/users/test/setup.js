/*global require, module, describe, it, beforeEach */
(function () {
  'use strict';
  var sinon = require('sinon'),
    expect = require('expect.js'),
    mockuire;

  mockuire = require('mockuire')(module, {
    'js': {compile: function (src) {
      return 'var applicationContext = require("applicationContext");\n' + src;
    }}
  });

  describe('setup.js', function () {
    var db = {}, ctx = {};

    beforeEach(function () {
      ctx.collectionName = sinon.stub();
      db._collection = sinon.stub();
      db._create = sinon.stub();
    });

    it('creates a users collection if it does not exist', function () {
      ctx.collectionName.withArgs('users').returns('magic');
      db._collection.returns(null);
      mockuire('../setup', {
        applicationContext: ctx,
        'org/arangodb': {db: db}
      });
      expect(db._create.callCount).to.equal(1);
      expect(db._create.args[0]).to.eql(['magic']);
    });

    it('does not overwrite an existing collection', function () {
      ctx.collectionName.returns('magic');
      db._collection.returns({});
      mockuire('../setup', {
        applicationContext: ctx,
        'org/arangodb': {db: db}
      });
      expect(db._create.callCount).to.equal(0);
    });
  });
}());