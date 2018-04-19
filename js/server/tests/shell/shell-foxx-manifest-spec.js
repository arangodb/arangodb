/*global describe, it */
'use strict';
const expect = require('chai').expect;
const validateManifest = require('@arangodb/foxx/manifest').validateJson;
const request = require('@arangodb/request');
const db = require('@arangodb').db;

describe('Foxx manifest $schema field', () => {
  it('is ignored', () => {
    const BAD_VALUE = 'http://badvalue.not/to/log';
    let manifest;
    try {
      manifest = validateManifest('fake', { $schema: BAD_VALUE }, '/fake');
    } catch (e) {
      expect.fail();
    }
    expect(manifest).not.to.have.property('$schema');
    const logs = request.get(`/_db/${db._name()}/_admin/log`, {json: true}).json;
    expect(logs.text.filter(text => text.includes(BAD_VALUE))).to.be.empty;
  });
});
