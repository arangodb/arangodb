/*globals describe, it */
'use strict';
const expect = require('chai').expect;

describe('@arangodb/locals', () => {
  const context = {hello: 'world'};
  it('should expose module.context', () => {
    module.context = context;
    const locals = require('@arangodb/locals');
    expect(locals.context).to.equal(module.context);
    expect(locals.context).to.equal(context);
  });
});
