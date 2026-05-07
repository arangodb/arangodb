'use strict';

/**
 * @typedef {object} PropertyDescriptor
 * @see https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/defineProperty#description
 * @property {boolean} configurable defaults to false
 * @property {boolean} enumerable   defaults to false
 * @property {boolean} writable     defaults to false
 * @property {unknown} value defaults to undefined
 * @property {() => unknown} get defaults to undefined
 * @property {(value: unknown) => void} set defaults to undefined
 */

/**
 * @typedef {{isOwn: boolean} & PropertyDescriptor} SinonPropertyDescriptor
 * a slightly enriched property descriptor
 * @property {boolean} isOwn true if the descriptor is owned by this object, false if it comes from the prototype
 */

/**
 * Returns a slightly modified property descriptor that one can tell is from the object or the prototype
 *
 * @param {unknown} object
 * @param {string} property
 * @returns {SinonPropertyDescriptor}
 */
function getPropertyDescriptor(object, property) {
    let proto = object;
    let descriptor;
    const isOwn = Boolean(
        object && Object.getOwnPropertyDescriptor(object, property),
    );

    while (
        proto &&
        !(descriptor = Object.getOwnPropertyDescriptor(proto, property))
    ) {
        proto = Object.getPrototypeOf(proto);
    }

    if (descriptor) {
        descriptor.isOwn = isOwn;
    }

    return descriptor;
}

module.exports = getPropertyDescriptor;
