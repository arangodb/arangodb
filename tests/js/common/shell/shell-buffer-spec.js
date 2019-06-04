/* global describe, it, beforeEach */
'use strict';
const { expect } = require('chai');

describe('Buffer', function () {
  describe('from', function () {
    it('should not accept numbers', () => {
      try {
        Buffer.from(32);
      } catch (e) {
        expect(e).to.be.instanceof(TypeError);
        return;
      }
      expect.fail("Expected an exception");
    });
    it('should accept a string', () => {
      const value = "hello world";
      const buf = Buffer.from(value);
      expect(buf.toString()).to.equal(value);
    });
    it('should accept a buffer', () => {
      const value = "hello world";
      const buf0 = new Buffer(value);
      const buf = Buffer.from(buf0);
      expect(buf.toString()).to.equal(value);
    });
    it('should accept an ArrayBuffer', () => {
      const values = [4, 8, 15, 16, 23, 42];
      const ab = new ArrayBuffer(values.length);
      const dv = new DataView(ab);
      for (let i = 0; i < values.length; i++) {
        dv.setUint8(i, values[i]);
      }
      const buf = Buffer.from(ab);
      expect([...buf]).to.eql(values);
    });
    it('should accept an array of byte values', () => {
      const values = [4, 8, 15, 16, 23, 42];
      const buf = Buffer.from(values);
      expect([...buf]).to.eql(values);
    });
  });
  describe('of', function () {
    it('should create a buffer of a byte sequence', () => {
      const values = [4, 8, 15, 16, 23, 42];
      const buf = Buffer.of(...values);
      expect([...buf]).to.eql(values);
    });
  });
  describe('alloc', function () {
    it('should create a zeroed buffer of the given size', () => {
      const size = 23;
      const buf = Buffer.alloc(size);
      expect(buf).to.have.property('length', size);
      expect([...buf]).to.eql(Array(size).fill(0));
    });
  });
  describe('allocUnsafe', function () {
    it('should create an uninitialized buffer of the given size', () => {
      const size = 23;
      const buf = Buffer.alloc(size);
      expect(buf).to.have.property('length', size);
    });
  });
  describe('instance', function () {
    it('should be iterable', () => {
      const values = [4, 8, 15, 16, 23, 42];
      expect(Buffer.prototype).to.have.property(Symbol.iterator);
      expect([...Buffer.of(...values)]).to.eql(values);
    });
    describe('values', function () {
      it('should return values iterator', () => {
        const values = [4, 8, 15, 16, 23, 42];
        const iterator = Buffer.of(...values).values();
        expect(iterator).to.have.property('next');
        expect([...iterator]).to.eql([...values.values()]);
      });
    });
    describe('keys', function () {
      it('should return keys iterator', () => {
        const values = [4, 8, 15, 16, 23, 42];
        const iterator = Buffer.of(...values).keys();
        expect(iterator).to.have.property('next');
        expect([...iterator]).to.eql([...values.keys()]);
      });
    });
    describe('entries', function () {
      it('should return key-value tuples', () => {
        const values = [4, 8, 15, 16, 23, 42];
        const iterator = Buffer.of(...values).entries();
        expect(iterator).to.have.property('next');
        expect([...iterator]).to.eql([...values.entries()]);
      });
    });
  });
});
