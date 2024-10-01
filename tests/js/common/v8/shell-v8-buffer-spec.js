/* global describe, it, beforeEach */
'use strict';
const { expect } = require('chai');

describe('Buffer', function () {
  describe('new', function () {
    it('should accept numbers', () => {
      const buf = new Buffer(32);
      buf.fill(0);
      expect(buf.length).to.equal(32);
    });
    it('should accept a string', () => {
      const value = "hello world";
      const buf = new Buffer(value);
      expect(buf.toString()).to.equal(value);
    });
    it('should accept a buffer and return a copy', () => {
      const value = "hello world";
      const buf0 = new Buffer(value);
      const buf = new Buffer(buf0);
      expect(buf.toString()).to.equal(value);
      buf0.fill(0);
      expect(buf.toString()).to.equal(value);
    });
    it('should accept a buffer with offset/length and return a slice', () => {
      const value = "hello world";
      const buf0 = new Buffer(value);
      const buf = new Buffer(buf0, 3, 1);
      expect(buf.length).to.equal(3);
      expect(buf.toString()).to.equal(value.slice(1, 4));
      buf.fill(0);
      expect(buf0[0]).to.equal("hello world".charCodeAt(0));
      expect(buf0[1]).to.equal(0);
      expect(buf0[2]).to.equal(0);
      expect(buf0[3]).to.equal(0);
      expect(buf0[4]).to.equal("hello world".charCodeAt(4));
    });
    it('should accept an ArrayBuffer', () => {
      const values = [4, 8, 15, 16, 23, 42];
      const ab = new ArrayBuffer(values.length);
      const dv = new DataView(ab);
      for (let i = 0; i < values.length; i++) {
        dv.setUint8(i, values[i]);
      }
      const buf = new Buffer(ab);
      expect([...buf]).to.eql(values);
    });
    it('should accept an array of byte values', () => {
      const values = [4, 8, 15, 16, 23, 42];
      const buf = new Buffer(values);
      expect([...buf]).to.eql(values);
    });
  });
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
    it('should accept a buffer and return a copy', () => {
      const value = "hello world";
      const buf0 = Buffer.from(value);
      const buf = Buffer.from(buf0);
      expect(buf.toString()).to.equal(value);
      buf0.fill(0);
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
    describe('slice', function () {
      it('should propagate changes from parent to child', () => {
        const parent = Buffer.from('hello');
        const slice = parent.slice(1, 4);
        slice.fill(0);
        expect(parent[0]).to.equal('hello'.charCodeAt(0));
        expect(parent[1]).to.equal(0);
        expect(parent[2]).to.equal(0);
        expect(parent[3]).to.equal(0);
        expect(parent[4]).to.equal('hello'.charCodeAt(4));
      });
      it('should propagate changes from child to parent', () => {
        const parent = Buffer.from('hello');
        const slice = parent.slice(1, 4);
        parent.fill(0);
        expect(slice[0]).to.equal(0);
        expect(slice[1]).to.equal(0);
        expect(slice[2]).to.equal(0);
      });
    });
  });
});
