"use strict";

const arrayProto = require("@sinonjs/commons").prototypes.array;
const hasOwnProperty =
    require("@sinonjs/commons").prototypes.object.hasOwnProperty;

const join = arrayProto.join;
const push = arrayProto.push;

// Adapted from https://developer.mozilla.org/en/docs/ECMAScript_DontEnum_attribute#JScript_DontEnum_Bug
const hasDontEnumBug = (function () {
    const obj = {
        constructor: function () {
            return "0";
        },
        toString: function () {
            return "1";
        },
        valueOf: function () {
            return "2";
        },
        toLocaleString: function () {
            return "3";
        },
        prototype: function () {
            return "4";
        },
        isPrototypeOf: function () {
            return "5";
        },
        propertyIsEnumerable: function () {
            return "6";
        },
        hasOwnProperty: function () {
            return "7";
        },
        length: function () {
            return "8";
        },
        unique: function () {
            return "9";
        },
    };

    const result = [];
    for (const prop in obj) {
        if (hasOwnProperty(obj, prop)) {
            push(result, obj[prop]());
        }
    }
    return join(result, "") !== "0123456789";
})();

/**
 *
 * @param target
 * @param sources
 * @param doCopy
 * @returns {*} target
 */
function extendCommon(target, sources, doCopy) {
    let source, i, prop;

    for (i = 0; i < sources.length; i++) {
        source = sources[i];

        for (prop in source) {
            if (hasOwnProperty(source, prop)) {
                doCopy(target, source, prop);
            }
        }

        // Make sure we copy (own) toString method even when in JScript with DontEnum bug
        // See https://developer.mozilla.org/en/docs/ECMAScript_DontEnum_attribute#JScript_DontEnum_Bug
        if (
            hasDontEnumBug &&
            hasOwnProperty(source, "toString") &&
            source.toString !== target.toString
        ) {
            target.toString = source.toString;
        }
    }

    return target;
}

/**
 * Public: Extend target in place with all (own) properties, except 'name' when [[writable]] is false,
 *         from sources in-order. Thus, last source will override properties in previous sources.
 *
 * @param {object} target - The Object to extend
 * @param {object[]} sources - Objects to copy properties from.
 * @returns {object} the extended target
 */
module.exports = function extend(target, ...sources) {
    return extendCommon(
        target,
        sources,
        function copyValue(dest, source, prop) {
            const destOwnPropertyDescriptor = Object.getOwnPropertyDescriptor(
                dest,
                prop,
            );
            const sourceOwnPropertyDescriptor = Object.getOwnPropertyDescriptor(
                source,
                prop,
            );

            if (prop === "name" && !destOwnPropertyDescriptor.writable) {
                return;
            }
            const descriptors = {
                configurable: sourceOwnPropertyDescriptor.configurable,
                enumerable: sourceOwnPropertyDescriptor.enumerable,
            };
            /*
                if the source has an Accessor property copy over the accessor functions (get and set)
                data properties has writable attribute where as accessor property don't
                REF: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Data_structures#properties
            */

            if (hasOwnProperty(sourceOwnPropertyDescriptor, "writable")) {
                descriptors.writable = sourceOwnPropertyDescriptor.writable;
                descriptors.value = sourceOwnPropertyDescriptor.value;
            } else {
                if (sourceOwnPropertyDescriptor.get) {
                    descriptors.get =
                        sourceOwnPropertyDescriptor.get.bind(dest);
                }
                if (sourceOwnPropertyDescriptor.set) {
                    descriptors.set =
                        sourceOwnPropertyDescriptor.set.bind(dest);
                }
            }
            Object.defineProperty(dest, prop, descriptors);
        },
    );
};

/**
 * Public: Extend target in place with all (own) properties from sources in-order. Thus, last source will
 *         override properties in previous sources. Define the properties as non enumerable.
 *
 * @param {object} target - The Object to extend
 * @param {object[]} sources - Objects to copy properties from.
 * @returns {object} the extended target
 */
module.exports.nonEnum = function extendNonEnum(target, ...sources) {
    return extendCommon(
        target,
        sources,
        function copyProperty(dest, source, prop) {
            Object.defineProperty(dest, prop, {
                value: source[prop],
                enumerable: false,
                configurable: true,
                writable: true,
            });
        },
    );
};
