/*global suite, test */
"use strict";
const { expect } = require("chai");
const { getReadableName } = require("@arangodb/foxx/manager-utils");

require("@arangodb/test-helper").waitForFoxxInitialized();

suite("getReadableName", () => {
  for (const [input, output] of [
    ["catch-fire", "Catch Fire"],
    ["catchFire", "Catch Fire"],
    ["CatchFire", "Catch Fire"],
    ["catch fire", "Catch Fire"],
    ["CATCH FIRE", "CATCH FIRE"],
    ["CATCHFIRE", "CATCHFIRE"],
    ["cAtChFiRe", "C At Ch Fi Re"],
    ["XmlHTTPRequest", "Xml HTTP Request"]
  ]) {
    test(`"${input}" -> "${output}"`, () => {
      expect(getReadableName(input)).to.equal(output);
    });
  }
});
