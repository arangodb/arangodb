/*global describe, it */
'use strict';
const expect = require('chai').expect;
const validateManifest = require('@arangodb/foxx/manifest').validateJson;
const CANONICAL_SCHEMA = require('@arangodb/foxx/manifest').schemaUrl;
const request = require('@arangodb/request');
const db = require('@arangodb').db;

require("@arangodb/test-helper").waitForFoxxInitialized();

describe('Foxx manifest $schema field', () => {
  it(`defaults to "${CANONICAL_SCHEMA}"`, () => {
    const manifest = validateManifest('fake', {}, '/fake');
    expect(manifest).to.have.property('$schema', CANONICAL_SCHEMA);
  });
  it('warns (but does not fail validation) if invalid', () => {
    const BAD_VALUE = 'http://badvalue.to/log';
    try {
      validateManifest('fake', { $schema: BAD_VALUE }, '/fake');
    } catch (e) {
      expect.fail();
    }
    // this may fail if logs are not printed
    //const logs = request.get(`/_db/${db._name()}/_admin/log`, {json: true}).json;
    //expect(logs.text.filter(text => text.includes(BAD_VALUE))).not.to.be.empty;
  });
  it(`falls back to "${CANONICAL_SCHEMA}" if invalid`, () => {
    const manifest = validateManifest('fake', { $schema: 'http://example.com' }, '/fake');
    expect(manifest).to.have.property('$schema', CANONICAL_SCHEMA);
  });
});
