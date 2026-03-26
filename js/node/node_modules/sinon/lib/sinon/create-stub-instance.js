"use strict";

const stub = require("./stub");
const sinonType = require("./util/core/sinon-type");
const forEach = require("@sinonjs/commons").prototypes.array.forEach;

function isStub(value) {
    return sinonType.get(value) === "stub";
}

module.exports = function createStubInstance(constructor, overrides) {
    if (typeof constructor !== "function") {
        throw new TypeError("The constructor should be a function.");
    }

    const stubInstance = Object.create(constructor.prototype);
    sinonType.set(stubInstance, "stub-instance");

    const stubbedObject = stub(stubInstance);

    forEach(Object.keys(overrides || {}), function (propertyName) {
        if (propertyName in stubbedObject) {
            const value = overrides[propertyName];
            if (isStub(value)) {
                stubbedObject[propertyName] = value;
            } else {
                stubbedObject[propertyName].returns(value);
            }
        } else {
            throw new Error(
                `Cannot stub ${propertyName}. Property does not exist!`,
            );
        }
    });
    return stubbedObject;
};
