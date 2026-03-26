"use strict";

const walk = require("./util/core/walk");
const getPropertyDescriptor = require("./util/core/get-property-descriptor");
const hasOwnProperty =
    require("@sinonjs/commons").prototypes.object.hasOwnProperty;
const push = require("@sinonjs/commons").prototypes.array.push;

function collectMethod(methods, object, prop, propOwner) {
    if (
        typeof getPropertyDescriptor(propOwner, prop).value === "function" &&
        hasOwnProperty(object, prop)
    ) {
        push(methods, object[prop]);
    }
}

// This function returns an array of all the own methods on the passed object
function collectOwnMethods(object) {
    const methods = [];

    walk(object, collectMethod.bind(null, methods, object));

    return methods;
}

module.exports = collectOwnMethods;
