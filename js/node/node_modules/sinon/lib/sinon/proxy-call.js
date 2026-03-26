"use strict";

const arrayProto = require("@sinonjs/commons").prototypes.array;
const match = require("@sinonjs/samsam").createMatcher;
const deepEqual = require("@sinonjs/samsam").deepEqual;
const functionName = require("@sinonjs/commons").functionName;
const inspect = require("util").inspect;
const valueToString = require("@sinonjs/commons").valueToString;

const concat = arrayProto.concat;
const filter = arrayProto.filter;
const join = arrayProto.join;
const map = arrayProto.map;
const reduce = arrayProto.reduce;
const slice = arrayProto.slice;

/**
 * @param proxy
 * @param text
 * @param args
 */
function throwYieldError(proxy, text, args) {
    let msg = functionName(proxy) + text;
    if (args.length) {
        msg += ` Received [${join(slice(args), ", ")}]`;
    }
    throw new Error(msg);
}

const callProto = {
    calledOn: function calledOn(thisValue) {
        if (match.isMatcher(thisValue)) {
            return thisValue.test(this.thisValue);
        }
        return this.thisValue === thisValue;
    },

    calledWith: function calledWith() {
        const self = this;
        const calledWithArgs = slice(arguments);

        if (calledWithArgs.length > self.args.length) {
            return false;
        }

        return reduce(
            calledWithArgs,
            function (prev, arg, i) {
                return prev && deepEqual(self.args[i], arg);
            },
            true,
        );
    },

    calledWithMatch: function calledWithMatch() {
        const self = this;
        const calledWithMatchArgs = slice(arguments);

        if (calledWithMatchArgs.length > self.args.length) {
            return false;
        }

        return reduce(
            calledWithMatchArgs,
            function (prev, expectation, i) {
                const actual = self.args[i];

                return prev && match(expectation).test(actual);
            },
            true,
        );
    },

    calledWithExactly: function calledWithExactly() {
        return (
            arguments.length === this.args.length &&
            this.calledWith.apply(this, arguments)
        );
    },

    notCalledWith: function notCalledWith() {
        return !this.calledWith.apply(this, arguments);
    },

    notCalledWithMatch: function notCalledWithMatch() {
        return !this.calledWithMatch.apply(this, arguments);
    },

    returned: function returned(value) {
        return deepEqual(this.returnValue, value);
    },

    threw: function threw(error) {
        if (typeof error === "undefined" || !this.exception) {
            return Boolean(this.exception);
        }

        return this.exception === error || this.exception.name === error;
    },

    calledWithNew: function calledWithNew() {
        return this.proxy.prototype && this.thisValue instanceof this.proxy;
    },

    calledBefore: function (other) {
        return this.callId < other.callId;
    },

    calledAfter: function (other) {
        return this.callId > other.callId;
    },

    calledImmediatelyBefore: function (other) {
        return this.callId === other.callId - 1;
    },

    calledImmediatelyAfter: function (other) {
        return this.callId === other.callId + 1;
    },

    callArg: function (pos) {
        this.ensureArgIsAFunction(pos);
        return this.args[pos]();
    },

    callArgOn: function (pos, thisValue) {
        this.ensureArgIsAFunction(pos);
        return this.args[pos].apply(thisValue);
    },

    callArgWith: function (pos) {
        return this.callArgOnWith.apply(
            this,
            concat([pos, null], slice(arguments, 1)),
        );
    },

    callArgOnWith: function (pos, thisValue) {
        this.ensureArgIsAFunction(pos);
        const args = slice(arguments, 2);
        return this.args[pos].apply(thisValue, args);
    },

    throwArg: function (pos) {
        if (pos > this.args.length) {
            throw new TypeError(
                `Not enough arguments: ${pos} required but only ${this.args.length} present`,
            );
        }

        throw this.args[pos];
    },

    yield: function () {
        return this.yieldOn.apply(this, concat([null], slice(arguments, 0)));
    },

    yieldOn: function (thisValue) {
        const args = slice(this.args);
        const yieldFn = filter(args, function (arg) {
            return typeof arg === "function";
        })[0];

        if (!yieldFn) {
            throwYieldError(
                this.proxy,
                " cannot yield since no callback was passed.",
                args,
            );
        }

        return yieldFn.apply(thisValue, slice(arguments, 1));
    },

    yieldTo: function (prop) {
        return this.yieldToOn.apply(
            this,
            concat([prop, null], slice(arguments, 1)),
        );
    },

    yieldToOn: function (prop, thisValue) {
        const args = slice(this.args);
        const yieldArg = filter(args, function (arg) {
            return arg && typeof arg[prop] === "function";
        })[0];
        const yieldFn = yieldArg && yieldArg[prop];

        if (!yieldFn) {
            throwYieldError(
                this.proxy,
                ` cannot yield to '${valueToString(
                    prop,
                )}' since no callback was passed.`,
                args,
            );
        }

        return yieldFn.apply(thisValue, slice(arguments, 2));
    },

    toString: function () {
        if (!this.args) {
            return ":(";
        }

        let callStr = this.proxy ? `${String(this.proxy)}(` : "";
        const formattedArgs = map(this.args, function (arg) {
            return inspect(arg);
        });

        callStr = `${callStr + join(formattedArgs, ", ")})`;

        if (typeof this.returnValue !== "undefined") {
            callStr += ` => ${inspect(this.returnValue)}`;
        }

        if (this.exception) {
            callStr += ` !${this.exception.name}`;

            if (this.exception.message) {
                callStr += `(${this.exception.message})`;
            }
        }
        if (this.stack) {
            // If we have a stack, add the first frame that's in end-user code
            // Skip the first two frames because they will refer to Sinon code
            callStr += (this.stack.split("\n")[3] || "unknown").replace(
                /^\s*(?:at\s+|@)?/,
                " at ",
            );
        }

        return callStr;
    },

    ensureArgIsAFunction: function (pos) {
        if (typeof this.args[pos] !== "function") {
            throw new TypeError(
                `Expected argument at position ${pos} to be a Function, but was ${typeof this
                    .args[pos]}`,
            );
        }
    },
};
Object.defineProperty(callProto, "stack", {
    enumerable: true,
    configurable: true,
    get: function () {
        return (this.errorWithCallStack && this.errorWithCallStack.stack) || "";
    },
});

callProto.invokeCallback = callProto.yield;

/**
 * @param proxy
 * @param thisValue
 * @param args
 * @param returnValue
 * @param exception
 * @param id
 * @param errorWithCallStack
 *
 * @returns {object} proxyCall
 */
function createProxyCall(
    proxy,
    thisValue,
    args,
    returnValue,
    exception,
    id,
    errorWithCallStack,
) {
    if (typeof id !== "number") {
        throw new TypeError("Call id is not a number");
    }

    let firstArg, lastArg;

    if (args.length > 0) {
        firstArg = args[0];
        lastArg = args[args.length - 1];
    }

    const proxyCall = Object.create(callProto);
    const callback =
        lastArg && typeof lastArg === "function" ? lastArg : undefined;

    proxyCall.proxy = proxy;
    proxyCall.thisValue = thisValue;
    proxyCall.args = args;
    proxyCall.firstArg = firstArg;
    proxyCall.lastArg = lastArg;
    proxyCall.callback = callback;
    proxyCall.returnValue = returnValue;
    proxyCall.exception = exception;
    proxyCall.callId = id;
    proxyCall.errorWithCallStack = errorWithCallStack;

    return proxyCall;
}
createProxyCall.toString = callProto.toString; // used by mocks

module.exports = createProxyCall;
