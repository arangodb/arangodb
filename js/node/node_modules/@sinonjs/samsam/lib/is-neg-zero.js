"use strict";

/**
 * Returns `true` when `value` is `-0`
 * @alias module:samsam.isNegZero
 * @param {number} value A value to examine
 * @returns {boolean} Returns `true` when `value` is `-0`
 */
function isNegZero(value) {
    return value === 0 && 1 / value === -Infinity;
}

module.exports = isNegZero;
