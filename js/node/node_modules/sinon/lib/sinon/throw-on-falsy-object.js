"use strict";
const valueToString = require("@sinonjs/commons").valueToString;

function throwOnFalsyObject(object, property) {
    if (property && !object) {
        const type = object === null ? "null" : "undefined";
        throw new Error(
            `Trying to stub property '${valueToString(property)}' of ${type}`,
        );
    }
}

module.exports = throwOnFalsyObject;
