'use strict';

var walk = require('./util/core/walk.js');
var getPropertyDescriptor = require('./util/core/get-property-descriptor.js');
var commons = require('@sinonjs/commons');

function _interopDefault (e) { return e && e.__esModule ? e : { default: e }; }

var commons__default = /*#__PURE__*/_interopDefault(commons);

const { prototypes } = commons__default.default;
const { hasOwnProperty } = prototypes.object;
const { push } = prototypes.array;

function collectMethod(methods, object, prop, propOwner) {
    const descriptor = getPropertyDescriptor(propOwner, prop);
    const value = descriptor.value;

    if (
        typeof value === "function" &&
        hasOwnProperty(object, prop) &&
        value &&
        value.restore &&
        value.restore.sinon
    ) {
        push(methods, value);
    }
}

/**
 * Returns an array of all the own methods on the passed object.
 *
 * @param {object} object The object to collect methods from
 * @returns {Array} Array of methods
 */
function collectOwnMethods(object) {
    const methods = [];

    walk(object, collectMethod.bind(null, methods, object));

    return methods;
}

module.exports = collectOwnMethods;
