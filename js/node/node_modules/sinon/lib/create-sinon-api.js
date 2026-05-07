'use strict';

var behavior = require('./sinon/behavior.js');
var createSandbox = require('./sinon/create-sandbox.js');
var createStubInstance = require('./sinon/create-stub-instance.js');
var collectOwnMethods = require('./sinon/collect-own-methods.js');
var extend = require('./sinon/util/core/extend.js');
var fakeTimers = require('./sinon/util/fake-timers.js');
var sandbox = require('./sinon/sandbox.js');
var stub = require('./sinon/stub.js');
var promise = require('./sinon/promise.js');
var samsam = require('@sinonjs/samsam');
var restoreObject = require('./sinon/restore-object.js');
var mockExpectation = require('./sinon/mock-expectation.js');

function _interopDefault (e) { return e && e.__esModule ? e : { default: e }; }

var samsam__default = /*#__PURE__*/_interopDefault(samsam);

/**
 * Creates the Sinon API.
 *
 * @returns {object} The Sinon API object
 */
function createApi() {
    const sandbox$1 = new sandbox();

    const apiMethods = {
        createSandbox: function createSandbox$1(config) {
            const s = createSandbox(config);
            sandbox$1.getFakes().push(s);
            return s;
        },
        match: samsam__default.default.createMatcher,
        restoreObject: restoreObject,
        expectation: mockExpectation,
        timers: fakeTimers.timers,
        createStubInstance: function createStubInstance$1() {
            const stubbed = createStubInstance.apply(null, arguments);

            for (const method of collectOwnMethods(stubbed)) {
                sandbox$1.getFakes().push(method);
            }

            return stubbed;
        },

        addBehavior: function (name, fn) {
            behavior.addBehavior(stub, name, fn);
        },

        promise: promise,
    };

    Object.defineProperty(apiMethods.createSandbox, "name", {
        value: "createSandbox",
        configurable: true,
    });
    Object.defineProperty(apiMethods.createSandbox, "length", {
        value: 1,
        configurable: true,
    });

    Object.defineProperty(apiMethods.createStubInstance, "name", {
        value: "createStubInstance",
        configurable: true,
    });
    Object.defineProperty(apiMethods.createStubInstance, "length", {
        value: 1,
        configurable: true,
    });

    return extend(sandbox$1, apiMethods);
}

module.exports = createApi;
