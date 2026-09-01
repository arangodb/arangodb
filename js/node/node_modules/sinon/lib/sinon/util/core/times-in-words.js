'use strict';

const array = [null, "once", "twice", "thrice"];

/**
 * Returns a string representing the number of times.
 *
 * @param {number} count The number of times
 * @returns {string} The string representation
 */
function timesInWords(count) {
    return array[count] || `${count || 0} times`;
}

module.exports = timesInWords;
