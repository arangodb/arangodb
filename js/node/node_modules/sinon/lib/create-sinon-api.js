"use strict";

const behavior = require("./sinon/behavior");
const createSandbox = require("./sinon/create-sandbox");
const extend = require("./sinon/util/core/extend");
const fakeTimers = require("./sinon/util/fake-timers");
const Sandbox = require("./sinon/sandbox");
const stub = require("./sinon/stub");
const promise = require("./sinon/promise");

/**
 * @returns {object} a configured sandbox
 */
module.exports = function createApi() {
    const apiMethods = {
        createSandbox: createSandbox,
        match: require("@sinonjs/samsam").createMatcher,
        restoreObject: require("./sinon/restore-object"),

        expectation: require("./sinon/mock-expectation"),

        // fake timers
        timers: fakeTimers.timers,

        addBehavior: function (name, fn) {
            behavior.addBehavior(stub, name, fn);
        },

        // fake promise
        promise: promise,
    };

    const sandbox = new Sandbox();
    return extend(sandbox, apiMethods);
};
