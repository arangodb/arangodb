"use strict";

const array = [null, "once", "twice", "thrice"];

module.exports = function timesInWords(count) {
    return array[count] || `${count || 0} times`;
};
