/* global describe, it */
"use strict";
const util = require("@arangodb/util");
const { expect } = require("chai");

describe("isZipBuffer", () => {
  const { isZipBuffer } = util;
  it("handles empty buffers", () => {
    const buf = new Buffer(0);
    expect(isZipBuffer(buf)).to.equal(false);
  });
  it("recognizes zip buffers", () => {
    const buf = new Buffer("PK\u0003\u0004");
    expect(isZipBuffer(buf)).to.equal(true);
  });
  it("does not recognize non-zip buffers", () => {
    const buf = new Buffer("PK\u0000\u0004");
    expect(isZipBuffer(buf)).to.equal(false);
  });
});
