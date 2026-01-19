"use strict";

const globalObject = require("@sinonjs/commons").global;
const getNextTick = require("./get-next-tick");

module.exports = getNextTick(globalObject.process, globalObject.setImmediate);
