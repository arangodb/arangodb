/*global describe, it, beforeEach */
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

  describe('hashPassword', function () {
    var cfg = {}, crypto = {}, hashPassword;

    hashPassword = mockuire('../auth', {
      applicationContext: {configuration: cfg},
      '@arangodb/crypto': crypto
    }).hashPassword;

    beforeEach(function () {
      cfg.hashMethod = 'bogohash';
      crypto.bogohash = sinon.stub();
      crypto.genRandomAlphaNumbers = sinon.stub();
    });

    it('uses the hash method defined by the configuration', function () {
      var result = hashPassword('secret');
      expect(result.method).to.equal(cfg.hashMethod);
    });

    it('uses the salt length defined by the configuration', function () {
      cfg.saltLength = 42;
      hashPassword('secret');
      expect(crypto.genRandomAlphaNumbers.callCount).to.equal(1);
      expect(crypto.genRandomAlphaNumbers.args[0]).to.eql([42]);
    });

    it('prefixes the password with the salt', function () {
      crypto.genRandomAlphaNumbers.returns('keyboardcat');
      hashPassword('secret');
      expect(crypto.bogohash.callCount).to.equal(1);
      expect(crypto.bogohash.args[0]).to.eql(['keyboardcatsecret']);
    });

    it('retuns the hashed password', function () {
      crypto.bogohash.returns('somekindofhash');
      var result = hashPassword();
      expect(result.hash).to.equal('somekindofhash');
    });
  });
}());