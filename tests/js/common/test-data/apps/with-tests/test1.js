/* global describe, it */
'use strict';

const expect = require('chai').expect;

describe('boolean comparison', () => {
  it('should succeed for identity', () => {
    expect(true).to.equal(true);
  });

  it('should fail for different values', () => {
    expect(true).to.equal(false);
  });
});

describe('another boolean comparison', () => {
  it('should succeed for identity', () => {
    expect(true).to.equal(true);
  });
});
