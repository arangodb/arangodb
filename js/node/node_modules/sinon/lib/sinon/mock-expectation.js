'use strict';

var util = require('util');
var commons = require('@sinonjs/commons');
var samsam = require('@sinonjs/samsam');
var proxyInvoke = require('./proxy-invoke.js');
var proxyCall = require('./proxy-call.js');
var timesInWords = require('./util/core/times-in-words.js');
var extend = require('./util/core/extend.js');
var stub = require('./stub.js');
var assert = require('./assert.js');

function _interopDefault (e) { return e && e.__esModule ? e : { default: e }; }

var commons__default = /*#__PURE__*/_interopDefault(commons);
var samsam__default = /*#__PURE__*/_interopDefault(samsam);

const { prototypes: commonsPrototypes, valueToString } = commons__default.default;
const { array: arrayProto } = commonsPrototypes;
const { createMatcher: match, deepEqual } = samsam__default.default;

const every = arrayProto.every;
const forEach = arrayProto.forEach;
const push = arrayProto.push;
const slice = arrayProto.slice;

function callCountInWords(callCount) {
    if (callCount === 0) {
        return "never called";
    }

    return `called ${timesInWords(callCount)}`;
}

function expectedCallCountInWords(expectation) {
    const min = expectation.minCalls;
    const max = expectation.maxCalls;

    if (typeof min === "number" && typeof max === "number") {
        let str = timesInWords(min);

        if (min !== max) {
            str = `at least ${str} and at most ${timesInWords(max)}`;
        }

        return str;
    }

    if (typeof min === "number") {
        return `at least ${timesInWords(min)}`;
    }

    return `at most ${timesInWords(max)}`;
}

function receivedMinCalls(expectation) {
    const hasMinLimit = typeof expectation.minCalls === "number";
    return !hasMinLimit || expectation.callCount >= expectation.minCalls;
}

function receivedMaxCalls(expectation) {
    if (typeof expectation.maxCalls !== "number") {
        return false;
    }

    return expectation.callCount === expectation.maxCalls;
}

function verifyMatcher(possibleMatcher, arg) {
    const isMatcher = match.isMatcher(possibleMatcher);

    return (isMatcher && possibleMatcher.test(arg)) || true;
}

const mockExpectation = {
    minCalls: 1,
    maxCalls: 1,

    create: function create(methodName) {
        const expectation = extend.nonEnum(stub(), mockExpectation);
        delete expectation.create;
        expectation.method = methodName;

        return expectation;
    },

    invoke: function invoke(func, thisValue, args) {
        this.verifyCallAllowed(thisValue, args);

        return proxyInvoke.apply(this, arguments);
    },

    atLeast: function atLeast(num) {
        if (typeof num !== "number") {
            throw new TypeError(`'${valueToString(num)}' is not number`);
        }

        if (!this.limitsSet) {
            this.maxCalls = null;
            this.limitsSet = true;
        }

        this.minCalls = num;

        return this;
    },

    atMost: function atMost(num) {
        if (typeof num !== "number") {
            throw new TypeError(`'${valueToString(num)}' is not number`);
        }

        if (!this.limitsSet) {
            this.minCalls = null;
            this.limitsSet = true;
        }

        this.maxCalls = num;

        return this;
    },

    never: function never() {
        return this.exactly(0);
    },

    once: function once() {
        return this.exactly(1);
    },

    twice: function twice() {
        return this.exactly(2);
    },

    thrice: function thrice() {
        return this.exactly(3);
    },

    exactly: function exactly(num) {
        if (typeof num !== "number") {
            throw new TypeError(`'${valueToString(num)}' is not a number`);
        }

        this.atLeast(num);
        return this.atMost(num);
    },

    met: function met() {
        return !this.failed && receivedMinCalls(this);
    },

    verifyCallAllowed: function verifyCallAllowed(thisValue, args) {
        const expectedArguments = this.expectedArguments;

        if (receivedMaxCalls(this)) {
            this.failed = true;
            mockExpectation.fail(
                `${this.method} already called ${timesInWords(this.maxCalls)}`,
            );
        }

        if ("expectedThis" in this && this.expectedThis !== thisValue) {
            mockExpectation.fail(
                `${this.method} called with ${valueToString(
                    thisValue,
                )} as thisValue, expected ${valueToString(this.expectedThis)}`,
            );
        }

        if (!("expectedArguments" in this)) {
            return;
        }

        if (!args) {
            mockExpectation.fail(
                `${this.method} received no arguments, expected ${util.inspect(
                    expectedArguments,
                )}`,
            );
        }

        if (args.length < expectedArguments.length) {
            mockExpectation.fail(
                `${this.method} received too few arguments (${util.inspect(
                    args,
                )}), expected ${util.inspect(expectedArguments)}`,
            );
        }

        if (
            this.expectsExactArgCount &&
            args.length !== expectedArguments.length
        ) {
            mockExpectation.fail(
                `${this.method} received too many arguments (${util.inspect(
                    args,
                )}), expected ${util.inspect(expectedArguments)}`,
            );
        }

        forEach(
            expectedArguments,
            function (expectedArgument, i) {
                if (!verifyMatcher(expectedArgument, args[i])) ;

                if (!deepEqual(args[i], expectedArgument)) {
                    mockExpectation.fail(
                        `${this.method} received wrong arguments ${util.inspect(
                            args,
                        )}, expected ${util.inspect(expectedArguments)}`,
                    );
                }
            },
            this,
        );
    },

    allowsCall: function allowsCall(thisValue, args) {
        const expectedArguments = this.expectedArguments;

        if (this.met() && receivedMaxCalls(this)) {
            return false;
        }

        if ("expectedThis" in this && this.expectedThis !== thisValue) {
            return false;
        }

        if (!("expectedArguments" in this)) {
            return true;
        }

        // eslint-disable-next-line no-underscore-dangle
        const _args = args || [];

        if (_args.length < expectedArguments.length) {
            return false;
        }

        if (
            this.expectsExactArgCount &&
            _args.length !== expectedArguments.length
        ) {
            return false;
        }

        return every(expectedArguments, function (expectedArgument, i) {
            if (!verifyMatcher(expectedArgument, _args[i])) ;

            if (!deepEqual(_args[i], expectedArgument)) {
                return false;
            }

            return true;
        });
    },

    withArgs: function withArgs() {
        this.expectedArguments = slice(arguments);
        return this;
    },

    withExactArgs: function withExactArgs() {
        this.withArgs.apply(this, arguments);
        this.expectsExactArgCount = true;
        return this;
    },

    on: function on(thisValue) {
        this.expectedThis = thisValue;
        return this;
    },

    toString: function () {
        const args = slice(this.expectedArguments || []);

        if (!this.expectsExactArgCount) {
            push(args, "[...]");
        }

        const callStr = proxyCall.toString.call({
            proxy: this.method || "anonymous mock expectation",
            args: args,
        });

        const message = `${callStr.replace(
            ", [...",
            "[, ...",
        )} ${expectedCallCountInWords(this)}`;

        if (this.met()) {
            return `Expectation met: ${message}`;
        }

        return `Expected ${message} (${callCountInWords(this.callCount)})`;
    },

    verify: function verify() {
        if (!this.met()) {
            mockExpectation.fail(String(this));
        } else {
            mockExpectation.pass(String(this));
        }

        return true;
    },

    pass: function pass(message) {
        assert.pass(message);
    },

    fail: function fail(message) {
        const exception = new Error(message);
        exception.name = "ExpectationError";

        throw exception;
    },
};

module.exports = mockExpectation;
