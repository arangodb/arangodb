"use strict";

/**
 * Returns `true` when the argument is an iterable, `false` otherwise
 *
 * @alias module:samsam.isIterable
 * @param  {*}  val - A value to examine
 * @returns {boolean} Returns `true` when the argument is an iterable, `false` otherwise
 */
function isIterable(val) {
    // checks for null and undefined
    if (typeof val !== "object") {
        return false;
    }
    return typeof val[Symbol.iterator] === "function";
}

module.exports = isIterable;
