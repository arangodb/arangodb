"use strict";

var valueToString = require("@sinonjs/commons").valueToString;
var className = require("@sinonjs/commons").className;
var typeOf = require("@sinonjs/commons").typeOf;
var arrayProto = require("@sinonjs/commons").prototypes.array;
var objectProto = require("@sinonjs/commons").prototypes.object;
var mapForEach = require("@sinonjs/commons").prototypes.map.forEach;

var getClass = require("./get-class");
var identical = require("./identical");
var isArguments = require("./is-arguments");
var isArrayType = require("./is-array-type");
var isDate = require("./is-date");
var isElement = require("./is-element");
var isIterable = require("./is-iterable");
var isMap = require("./is-map");
var isNaN = require("./is-nan");
var isObject = require("./is-object");
var isSet = require("./is-set");
var isSubset = require("./is-subset");

var concat = arrayProto.concat;
var every = arrayProto.every;
var push = arrayProto.push;

var getTime = Date.prototype.getTime;
var hasOwnProperty = objectProto.hasOwnProperty;
var indexOf = arrayProto.indexOf;
var keys = Object.keys;
var getOwnPropertySymbols = Object.getOwnPropertySymbols;

/**
 * Deep equal comparison. Two values are "deep equal" when:
 *
 *   - They are equal, according to samsam.identical
 *   - They are both date objects representing the same time
 *   - They are both arrays containing elements that are all deepEqual
 *   - They are objects with the same set of properties, and each property
 *     in ``actual`` is deepEqual to the corresponding property in ``expectation``
 *
 * Supports cyclic objects.
 *
 * @alias module:samsam.deepEqual
 * @param {*} actual The object to examine
 * @param {*} expectation The object actual is expected to be equal to
 * @param {object} match A value to match on
 * @returns {boolean} Returns true when actual and expectation are considered equal
 */
function deepEqualCyclic(actual, expectation, match) {
    // used for cyclic comparison
    // contain already visited objects
    var actualObjects = [];
    var expectationObjects = [];
    // contain pathes (position in the object structure)
    // of the already visited objects
    // indexes same as in objects arrays
    var actualPaths = [];
    var expectationPaths = [];
    // contains combinations of already compared objects
    // in the manner: { "$1['ref']$2['ref']": true }
    var compared = {};

    // does the recursion for the deep equal check
    // eslint-disable-next-line complexity
    return (function deepEqual(
        actualObj,
        expectationObj,
        actualPath,
        expectationPath,
    ) {
        // If both are matchers they must be the same instance in order to be
        // considered equal If we didn't do that we would end up running one
        // matcher against the other
        if (match && match.isMatcher(expectationObj)) {
            if (match.isMatcher(actualObj)) {
                return actualObj === expectationObj;
            }
            return expectationObj.test(actualObj);
        }

        var actualType = typeof actualObj;
        var expectationType = typeof expectationObj;

        if (
            actualObj === expectationObj ||
            isNaN(actualObj) ||
            isNaN(expectationObj) ||
            actualObj === null ||
            expectationObj === null ||
            actualObj === undefined ||
            expectationObj === undefined ||
            actualType !== "object" ||
            expectationType !== "object"
        ) {
            return identical(actualObj, expectationObj);
        }

        // Elements are only equal if identical(expected, actual)
        if (isElement(actualObj) || isElement(expectationObj)) {
            return false;
        }

        var isActualDate = isDate(actualObj);
        var isExpectationDate = isDate(expectationObj);
        if (isActualDate || isExpectationDate) {
            if (
                !isActualDate ||
                !isExpectationDate ||
                getTime.call(actualObj) !== getTime.call(expectationObj)
            ) {
                return false;
            }
        }

        if (actualObj instanceof RegExp && expectationObj instanceof RegExp) {
            if (valueToString(actualObj) !== valueToString(expectationObj)) {
                return false;
            }
        }

        if (actualObj instanceof Promise && expectationObj instanceof Promise) {
            return actualObj === expectationObj;
        }

        if (actualObj instanceof Error && expectationObj instanceof Error) {
            return actualObj === expectationObj;
        }

        var actualClass = getClass(actualObj);
        var expectationClass = getClass(expectationObj);
        var actualKeys = keys(actualObj);
        var expectationKeys = keys(expectationObj);
        var actualName = className(actualObj);
        var expectationName = className(expectationObj);
        var expectationSymbols =
            typeOf(getOwnPropertySymbols) === "function"
                ? getOwnPropertySymbols(expectationObj)
                : /* istanbul ignore next: cannot collect coverage for engine that doesn't support Symbol */
                  [];
        var expectationKeysAndSymbols = concat(
            expectationKeys,
            expectationSymbols,
        );

        if (isArguments(actualObj) || isArguments(expectationObj)) {
            if (actualObj.length !== expectationObj.length) {
                return false;
            }
        } else {
            if (
                actualType !== expectationType ||
                actualClass !== expectationClass ||
                actualKeys.length !== expectationKeys.length ||
                (actualName &&
                    expectationName &&
                    actualName !== expectationName)
            ) {
                return false;
            }
        }

        if (isSet(actualObj) || isSet(expectationObj)) {
            if (
                !isSet(actualObj) ||
                !isSet(expectationObj) ||
                actualObj.size !== expectationObj.size
            ) {
                return false;
            }

            return isSubset(actualObj, expectationObj, deepEqual);
        }

        if (isMap(actualObj) || isMap(expectationObj)) {
            if (
                !isMap(actualObj) ||
                !isMap(expectationObj) ||
                actualObj.size !== expectationObj.size
            ) {
                return false;
            }

            var mapsDeeplyEqual = true;
            mapForEach(actualObj, function (value, key) {
                mapsDeeplyEqual =
                    mapsDeeplyEqual &&
                    deepEqualCyclic(value, expectationObj.get(key));
            });

            return mapsDeeplyEqual;
        }

        // jQuery objects have iteration protocols
        // see: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Iteration_protocols
        // But, they don't work well with the implementation concerning iterables below,
        // so we will detect them and use jQuery's own equality function
        /* istanbul ignore next -- this can only be tested in the `test-headless` script */
        if (
            actualObj.constructor &&
            actualObj.constructor.name === "jQuery" &&
            typeof actualObj.is === "function"
        ) {
            return actualObj.is(expectationObj);
        }

        var isActualNonArrayIterable =
            isIterable(actualObj) &&
            !isArrayType(actualObj) &&
            !isArguments(actualObj);
        var isExpectationNonArrayIterable =
            isIterable(expectationObj) &&
            !isArrayType(expectationObj) &&
            !isArguments(expectationObj);
        if (isActualNonArrayIterable || isExpectationNonArrayIterable) {
            var actualArray = Array.from(actualObj);
            var expectationArray = Array.from(expectationObj);
            if (actualArray.length !== expectationArray.length) {
                return false;
            }

            var arrayDeeplyEquals = true;
            every(actualArray, function (key) {
                arrayDeeplyEquals =
                    arrayDeeplyEquals &&
                    deepEqualCyclic(actualArray[key], expectationArray[key]);
            });

            return arrayDeeplyEquals;
        }

        return every(expectationKeysAndSymbols, function (key) {
            if (!hasOwnProperty(actualObj, key)) {
                return false;
            }

            var actualValue = actualObj[key];
            var expectationValue = expectationObj[key];
            var actualObject = isObject(actualValue);
            var expectationObject = isObject(expectationValue);
            // determines, if the objects were already visited
            // (it's faster to check for isObject first, than to
            // get -1 from getIndex for non objects)
            var actualIndex = actualObject
                ? indexOf(actualObjects, actualValue)
                : -1;
            var expectationIndex = expectationObject
                ? indexOf(expectationObjects, expectationValue)
                : -1;
            // determines the new paths of the objects
            // - for non cyclic objects the current path will be extended
            //   by current property name
            // - for cyclic objects the stored path is taken
            var newActualPath =
                actualIndex !== -1
                    ? actualPaths[actualIndex]
                    : `${actualPath}[${JSON.stringify(key)}]`;
            var newExpectationPath =
                expectationIndex !== -1
                    ? expectationPaths[expectationIndex]
                    : `${expectationPath}[${JSON.stringify(key)}]`;
            var combinedPath = newActualPath + newExpectationPath;

            // stop recursion if current objects are already compared
            if (compared[combinedPath]) {
                return true;
            }

            // remember the current objects and their paths
            if (actualIndex === -1 && actualObject) {
                push(actualObjects, actualValue);
                push(actualPaths, newActualPath);
            }
            if (expectationIndex === -1 && expectationObject) {
                push(expectationObjects, expectationValue);
                push(expectationPaths, newExpectationPath);
            }

            // remember that the current objects are already compared
            if (actualObject && expectationObject) {
                compared[combinedPath] = true;
            }

            // End of cyclic logic

            // neither actualValue nor expectationValue is a cycle
            // continue with next level
            return deepEqual(
                actualValue,
                expectationValue,
                newActualPath,
                newExpectationPath,
            );
        });
    })(actual, expectation, "$1", "$2");
}

deepEqualCyclic.use = function (match) {
    return function deepEqual(a, b) {
        return deepEqualCyclic(a, b, match);
    };
};

module.exports = deepEqualCyclic;
