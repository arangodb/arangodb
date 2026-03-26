"use strict";

const functionName = require("@sinonjs/commons").functionName;

const getPropertyDescriptor = require("./get-property-descriptor");
const walk = require("./walk");

/**
 * A utility that allows traversing an object, applying mutating functions on the properties
 *
 * @param {Function} mutator called on each property
 * @param {object} object the object we are walking over
 * @param {Function} filter a predicate (boolean function) that will decide whether or not to apply the mutator to the current property
 * @returns {void} nothing
 */
function walkObject(mutator, object, filter) {
    let called = false;
    const name = functionName(mutator);

    if (!object) {
        throw new Error(
            `Trying to ${name} object but received ${String(object)}`,
        );
    }

    walk(object, function (prop, propOwner) {
        // we don't want to stub things like toString(), valueOf(), etc. so we only stub if the object
        // is not Object.prototype
        if (
            propOwner !== Object.prototype &&
            prop !== "constructor" &&
            typeof getPropertyDescriptor(propOwner, prop).value === "function"
        ) {
            if (filter) {
                if (filter(object, prop)) {
                    called = true;
                    mutator(object, prop);
                }
            } else {
                called = true;
                mutator(object, prop);
            }
        }
    });

    if (!called) {
        throw new Error(
            `Found no methods on object to which we could apply mutations`,
        );
    }

    return object;
}

module.exports = walkObject;
