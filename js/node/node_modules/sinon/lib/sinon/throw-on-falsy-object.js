'use strict';

var commons = require('@sinonjs/commons');

function _interopDefault (e) { return e && e.__esModule ? e : { default: e }; }

var commons__default = /*#__PURE__*/_interopDefault(commons);

const { valueToString } = commons__default.default;

/**
 * Throws an error if the object is null or undefined when trying to stub a property.
 *
 * @param {unknown} object The object to check
 * @param {string} property The property name
 */
function throwOnFalsyObject(object, property) {
    if (property && !object) {
        const type = object === null ? "null" : "undefined";
        throw new Error(
            `Trying to stub property '${valueToString(property)}' of ${type}`,
        );
    }
}

module.exports = throwOnFalsyObject;
