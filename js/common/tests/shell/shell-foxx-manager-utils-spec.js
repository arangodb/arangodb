/* global test */
"use strict";
const { expect } = require("chai");
const { getReadableName } = require("@arangodb/foxx/manager-utils");

test("getReadableName", () => {
  expect(getReadableName("catch-fire")).to.equal("Catch Fire");
  expect(getReadableName("catchFire")).to.equal("Catch Fire");
  expect(getReadableName("CatchFire")).to.equal("Catch Fire");
  expect(getReadableName("cAtChFiRe")).to.equal("C At Ch Fi Re");
});
