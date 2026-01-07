"use strict";

const sinonTypeSymbolProperty = Symbol("SinonType");

module.exports = {
    /**
     * Set the type of a Sinon object to make it possible to identify it later at runtime
     *
     * @param {object|Function} object  object/function to set the type on
     * @param {string} type the named type of the object/function
     */
    set(object, type) {
        Object.defineProperty(object, sinonTypeSymbolProperty, {
            value: type,
            configurable: false,
            enumerable: false,
        });
    },
    get(object) {
        return object && object[sinonTypeSymbolProperty];
    },
};
