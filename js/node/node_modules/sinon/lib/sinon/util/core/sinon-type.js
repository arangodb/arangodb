'use strict';

const sinonTypeSymbolProperty = Symbol("SinonType");

/**
 * @callback SinonFunction
 * @param {...unknown} args
 * @returns {unknown}
 */

var sinonType = {
    /**
     * Set the type of a Sinon object to make it possible to identify it later at runtime
     *
     * @param {object|((...args: unknown[]) => unknown)} object  object/function to set the type on
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

module.exports = sinonType;
