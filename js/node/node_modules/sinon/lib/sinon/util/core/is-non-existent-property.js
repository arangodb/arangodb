"use strict";

/**
 * @param {*} object
 * @param {string} property
 * @returns {boolean} whether a prop exists in the prototype chain
 */
function isNonExistentProperty(object, property) {
    return Boolean(
        object && typeof property !== "undefined" && !(property in object),
    );
}

module.exports = isNonExistentProperty;
