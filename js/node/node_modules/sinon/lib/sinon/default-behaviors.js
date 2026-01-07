"use strict";

const arrayProto = require("@sinonjs/commons").prototypes.array;
const isPropertyConfigurable = require("./util/core/is-property-configurable");
const exportAsyncBehaviors = require("./util/core/export-async-behaviors");
const extend = require("./util/core/extend");

const slice = arrayProto.slice;

const useLeftMostCallback = -1;
const useRightMostCallback = -2;

function throwsException(fake, error, message) {
    if (typeof error === "function") {
        fake.exceptionCreator = error;
    } else if (typeof error === "string") {
        fake.exceptionCreator = function () {
            const newException = new Error(
                message || `Sinon-provided ${error}`,
            );
            newException.name = error;
            return newException;
        };
    } else if (!error) {
        fake.exceptionCreator = function () {
            return new Error("Error");
        };
    } else {
        fake.exception = error;
    }
}

const defaultBehaviors = {
    callsFake: function callsFake(fake, fn) {
        fake.fakeFn = fn;
        fake.exception = undefined;
        fake.exceptionCreator = undefined;
        fake.callsThrough = false;
    },

    callsArg: function callsArg(fake, index) {
        if (typeof index !== "number") {
            throw new TypeError("argument index is not number");
        }

        fake.callArgAt = index;
        fake.callbackArguments = [];
        fake.callbackContext = undefined;
        fake.callArgProp = undefined;
        fake.callbackAsync = false;
        fake.callsThrough = false;
    },

    callsArgOn: function callsArgOn(fake, index, context) {
        if (typeof index !== "number") {
            throw new TypeError("argument index is not number");
        }

        fake.callArgAt = index;
        fake.callbackArguments = [];
        fake.callbackContext = context;
        fake.callArgProp = undefined;
        fake.callbackAsync = false;
        fake.callsThrough = false;
    },

    callsArgWith: function callsArgWith(fake, index) {
        if (typeof index !== "number") {
            throw new TypeError("argument index is not number");
        }

        fake.callArgAt = index;
        fake.callbackArguments = slice(arguments, 2);
        fake.callbackContext = undefined;
        fake.callArgProp = undefined;
        fake.callbackAsync = false;
        fake.callsThrough = false;
    },

    callsArgOnWith: function callsArgWith(fake, index, context) {
        if (typeof index !== "number") {
            throw new TypeError("argument index is not number");
        }

        fake.callArgAt = index;
        fake.callbackArguments = slice(arguments, 3);
        fake.callbackContext = context;
        fake.callArgProp = undefined;
        fake.callbackAsync = false;
        fake.callsThrough = false;
    },

    yields: function (fake) {
        fake.callArgAt = useLeftMostCallback;
        fake.callbackArguments = slice(arguments, 1);
        fake.callbackContext = undefined;
        fake.callArgProp = undefined;
        fake.callbackAsync = false;
        fake.fakeFn = undefined;
        fake.callsThrough = false;
    },

    yieldsRight: function (fake) {
        fake.callArgAt = useRightMostCallback;
        fake.callbackArguments = slice(arguments, 1);
        fake.callbackContext = undefined;
        fake.callArgProp = undefined;
        fake.callbackAsync = false;
        fake.callsThrough = false;
        fake.fakeFn = undefined;
    },

    yieldsOn: function (fake, context) {
        fake.callArgAt = useLeftMostCallback;
        fake.callbackArguments = slice(arguments, 2);
        fake.callbackContext = context;
        fake.callArgProp = undefined;
        fake.callbackAsync = false;
        fake.callsThrough = false;
        fake.fakeFn = undefined;
    },

    yieldsTo: function (fake, prop) {
        fake.callArgAt = useLeftMostCallback;
        fake.callbackArguments = slice(arguments, 2);
        fake.callbackContext = undefined;
        fake.callArgProp = prop;
        fake.callbackAsync = false;
        fake.callsThrough = false;
        fake.fakeFn = undefined;
    },

    yieldsToOn: function (fake, prop, context) {
        fake.callArgAt = useLeftMostCallback;
        fake.callbackArguments = slice(arguments, 3);
        fake.callbackContext = context;
        fake.callArgProp = prop;
        fake.callbackAsync = false;
        fake.fakeFn = undefined;
    },

    throws: throwsException,
    throwsException: throwsException,

    returns: function returns(fake, value) {
        fake.callsThrough = false;
        fake.returnValue = value;
        fake.resolve = false;
        fake.reject = false;
        fake.returnValueDefined = true;
        fake.exception = undefined;
        fake.exceptionCreator = undefined;
        fake.fakeFn = undefined;
    },

    returnsArg: function returnsArg(fake, index) {
        if (typeof index !== "number") {
            throw new TypeError("argument index is not number");
        }
        fake.callsThrough = false;

        fake.returnArgAt = index;
    },

    throwsArg: function throwsArg(fake, index) {
        if (typeof index !== "number") {
            throw new TypeError("argument index is not number");
        }
        fake.callsThrough = false;

        fake.throwArgAt = index;
    },

    returnsThis: function returnsThis(fake) {
        fake.returnThis = true;
        fake.callsThrough = false;
    },

    resolves: function resolves(fake, value) {
        fake.returnValue = value;
        fake.resolve = true;
        fake.resolveThis = false;
        fake.reject = false;
        fake.returnValueDefined = true;
        fake.exception = undefined;
        fake.exceptionCreator = undefined;
        fake.fakeFn = undefined;
        fake.callsThrough = false;
    },

    resolvesArg: function resolvesArg(fake, index) {
        if (typeof index !== "number") {
            throw new TypeError("argument index is not number");
        }
        fake.resolveArgAt = index;
        fake.returnValue = undefined;
        fake.resolve = true;
        fake.resolveThis = false;
        fake.reject = false;
        fake.returnValueDefined = false;
        fake.exception = undefined;
        fake.exceptionCreator = undefined;
        fake.fakeFn = undefined;
        fake.callsThrough = false;
    },

    rejects: function rejects(fake, error, message) {
        let reason;
        if (typeof error === "string") {
            reason = new Error(message || "");
            reason.name = error;
        } else if (!error) {
            reason = new Error("Error");
        } else {
            reason = error;
        }
        fake.returnValue = reason;
        fake.resolve = false;
        fake.resolveThis = false;
        fake.reject = true;
        fake.returnValueDefined = true;
        fake.exception = undefined;
        fake.exceptionCreator = undefined;
        fake.fakeFn = undefined;
        fake.callsThrough = false;

        return fake;
    },

    resolvesThis: function resolvesThis(fake) {
        fake.returnValue = undefined;
        fake.resolve = false;
        fake.resolveThis = true;
        fake.reject = false;
        fake.returnValueDefined = false;
        fake.exception = undefined;
        fake.exceptionCreator = undefined;
        fake.fakeFn = undefined;
        fake.callsThrough = false;
    },

    callThrough: function callThrough(fake) {
        fake.callsThrough = true;
    },

    callThroughWithNew: function callThroughWithNew(fake) {
        fake.callsThroughWithNew = true;
    },

    get: function get(fake, getterFunction) {
        const rootStub = fake.stub || fake;

        Object.defineProperty(rootStub.rootObj, rootStub.propName, {
            get: getterFunction,
            configurable: isPropertyConfigurable(
                rootStub.rootObj,
                rootStub.propName,
            ),
        });

        return fake;
    },

    set: function set(fake, setterFunction) {
        const rootStub = fake.stub || fake;

        Object.defineProperty(
            rootStub.rootObj,
            rootStub.propName,
            // eslint-disable-next-line accessor-pairs
            {
                set: setterFunction,
                configurable: isPropertyConfigurable(
                    rootStub.rootObj,
                    rootStub.propName,
                ),
            },
        );

        return fake;
    },

    value: function value(fake, newVal) {
        const rootStub = fake.stub || fake;

        Object.defineProperty(rootStub.rootObj, rootStub.propName, {
            value: newVal,
            enumerable: true,
            writable: true,
            configurable:
                rootStub.shadowsPropOnPrototype ||
                isPropertyConfigurable(rootStub.rootObj, rootStub.propName),
        });

        return fake;
    },
};

const asyncBehaviors = exportAsyncBehaviors(defaultBehaviors);

module.exports = extend({}, defaultBehaviors, asyncBehaviors);
