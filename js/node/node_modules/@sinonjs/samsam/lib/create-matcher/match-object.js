"use strict";

var every = require("@sinonjs/commons").prototypes.array.every;
var concat = require("@sinonjs/commons").prototypes.array.concat;
var typeOf = require("@sinonjs/commons").typeOf;

var deepEqualFactory = require("../deep-equal").use;

var identical = require("../identical");
var isMatcher = require("./is-matcher");

var keys = Object.keys;
var getOwnPropertySymbols = Object.getOwnPropertySymbols;

/**
 * Matches `actual` with `expectation`
 *
 * @private
 * @param {*} actual A value to examine
 * @param {object} expectation An object with properties to match on
 * @param {object} matcher A matcher to use for comparison
 * @returns {boolean} Returns true when `actual` matches all properties in `expectation`
 */
function matchObject(actual, expectation, matcher) {
    var deepEqual = deepEqualFactory(matcher);
    if (actual === null || actual === undefined) {
        return false;
    }

    var expectedKeys = keys(expectation);
    /* istanbul ignore else: cannot collect coverage for engine that doesn't support Symbol */
    if (typeOf(getOwnPropertySymbols) === "function") {
        expectedKeys = concat(expectedKeys, getOwnPropertySymbols(expectation));
    }

    return every(expectedKeys, function (key) {
        var exp = expectation[key];
        var act = actual[key];

        if (isMatcher(exp)) {
            if (!exp.test(act)) {
                return false;
            }
        } else if (typeOf(exp) === "object") {
            if (identical(exp, act)) {
                return true;
            }
            if (!matchObject(act, exp, matcher)) {
                return false;
            }
        } else if (!deepEqual(act, exp)) {
            return false;
        }

        return true;
    });
}

module.exports = matchObject;
