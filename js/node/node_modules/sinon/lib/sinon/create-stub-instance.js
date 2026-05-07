'use strict';

var stub = require('./stub.js');
var sinonType = require('./util/core/sinon-type.js');
var commons = require('@sinonjs/commons');

function _interopDefault (e) { return e && e.__esModule ? e : { default: e }; }

var commons__default = /*#__PURE__*/_interopDefault(commons);

const { prototypes } = commons__default.default;
const { forEach } = prototypes.array;

function isStub(value) {
    return sinonType.get(value) === "stub";
}

/**
 * Creates a stub instance of a constructor.
 *
 * @param {new (...args: unknown[]) => object} constructor The constructor function
 * @param {object} [overrides] Optional overrides for the stubbed methods
 * @returns {object} The stubbed instance
 */
function createStubInstance(constructor, overrides) {
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
}

module.exports = createStubInstance;
