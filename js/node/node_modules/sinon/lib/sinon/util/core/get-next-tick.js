'use strict';

/* istanbul ignore next : not testing that setTimeout works */
function nextTick(callback) {
    setTimeout(callback, 0);
}

/**
 * @callback NextTickFunction
 * @param {...unknown} args
 * @returns {unknown}
 */

/**
 * Returns a next-tick function.
 *
 * @param {object} [process] The process object
 * @param {NextTickFunction} [setImmediate] The setImmediate function
 * @returns {NextTickFunction} The next-tick function
 */
function getNextTick(process, setImmediate) {
    if (typeof process === "object" && typeof process.nextTick === "function") {
        return process.nextTick;
    }

    if (typeof setImmediate === "function") {
        return setImmediate;
    }

    return nextTick;
}

module.exports = getNextTick;
