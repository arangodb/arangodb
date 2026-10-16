"use strict";

var getClass = require("./get-class");

/**
 * Returns `true` when `object` is an `arguments` object, `false` otherwise
 * @alias module:samsam.isArguments
 * @param  {unknown}  object - The object to examine
 * @returns {boolean} `true` when `object` is an `arguments` object
 */
function isArguments(object) {
    return getClass(object) === "Arguments";
}

module.exports = isArguments;
