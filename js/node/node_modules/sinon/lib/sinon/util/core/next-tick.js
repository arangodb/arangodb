'use strict';

var commons = require('@sinonjs/commons');
var getNextTick = require('./get-next-tick.js');

function _interopDefault (e) { return e && e.__esModule ? e : { default: e }; }

var commons__default = /*#__PURE__*/_interopDefault(commons);

const { global: globalObject } = commons__default.default;

/**
 * A platform-agnostic next-tick function.
 */
var nextTick = getNextTick(globalObject.process, globalObject.setImmediate);

module.exports = nextTick;
