"use strict";

const arrayProto = require("@sinonjs/commons").prototypes.array;
const createProxy = require("./proxy");
const nextTick = require("./util/core/next-tick");

const slice = arrayProto.slice;

module.exports = fake;

/**
 * Returns a `fake` that records all calls, arguments and return values.
 *
 * When an `f` argument is supplied, this implementation will be used.
 *
 * @example
 * // create an empty fake
 * var f1 = sinon.fake();
 *
 * f1();
 *
 * f1.calledOnce()
 * // true
 *
 * @example
 * function greet(greeting) {
 *   console.log(`Hello ${greeting}`);
 * }
 *
 * // create a fake with implementation
 * var f2 = sinon.fake(greet);
 *
 * // Hello world
 * f2("world");
 *
 * f2.calledWith("world");
 * // true
 *
 * @param {Function|undefined} f
 * @returns {Function}
 * @namespace
 */
function fake(f) {
    if (arguments.length > 0 && typeof f !== "function") {
        throw new TypeError("Expected f argument to be a Function");
    }

    return wrapFunc(f);
}

/**
 * Creates a `fake` that returns the provided `value`, as well as recording all
 * calls, arguments and return values.
 *
 * @example
 * var f1 = sinon.fake.returns(42);
 *
 * f1();
 * // 42
 *
 * @memberof fake
 * @param {*} value
 * @returns {Function}
 */
fake.returns = function returns(value) {
    // eslint-disable-next-line jsdoc/require-jsdoc
    function f() {
        return value;
    }

    return wrapFunc(f);
};

/**
 * Creates a `fake` that throws an Error.
 * If the `value` argument does not have Error in its prototype chain, it will
 * be used for creating a new error.
 *
 * @example
 * var f1 = sinon.fake.throws("hello");
 *
 * f1();
 * // Uncaught Error: hello
 *
 * @example
 * var f2 = sinon.fake.throws(new TypeError("Invalid argument"));
 *
 * f2();
 * // Uncaught TypeError: Invalid argument
 *
 * @memberof fake
 * @param {*|Error} value
 * @returns {Function}
 */
fake.throws = function throws(value) {
    // eslint-disable-next-line jsdoc/require-jsdoc
    function f() {
        throw getError(value);
    }

    return wrapFunc(f);
};

/**
 * Creates a `fake` that returns a promise that resolves to the passed `value`
 * argument.
 *
 * @example
 * var f1 = sinon.fake.resolves("apple pie");
 *
 * await f1();
 * // "apple pie"
 *
 * @memberof fake
 * @param {*} value
 * @returns {Function}
 */
fake.resolves = function resolves(value) {
    // eslint-disable-next-line jsdoc/require-jsdoc
    function f() {
        return Promise.resolve(value);
    }

    return wrapFunc(f);
};

/**
 * Creates a `fake` that returns a promise that rejects to the passed `value`
 * argument. When `value` does not have Error in its prototype chain, it will be
 * wrapped in an Error.
 *
 * @example
 * var f1 = sinon.fake.rejects(":(");
 *
 * try {
 *   await f1();
 * } catch (error) {
 *   console.log(error);
 *   // ":("
 * }
 *
 * @memberof fake
 * @param {*} value
 * @returns {Function}
 */
fake.rejects = function rejects(value) {
    // eslint-disable-next-line jsdoc/require-jsdoc
    function f() {
        return Promise.reject(getError(value));
    }

    return wrapFunc(f);
};

/**
 * Returns a `fake` that calls the callback with the defined arguments.
 *
 * @example
 * function callback() {
 *   console.log(arguments.join("*"));
 * }
 *
 * const f1 = sinon.fake.yields("apple", "pie");
 *
 * f1(callback);
 * // "apple*pie"
 *
 * @memberof fake
 * @returns {Function}
 */
fake.yields = function yields() {
    const values = slice(arguments);

    // eslint-disable-next-line jsdoc/require-jsdoc
    function f() {
        const callback = arguments[arguments.length - 1];
        if (typeof callback !== "function") {
            throw new TypeError("Expected last argument to be a function");
        }

        callback.apply(null, values);
    }

    return wrapFunc(f);
};

/**
 * Returns a `fake` that calls the callback **asynchronously** with the
 * defined arguments.
 *
 * @example
 * function callback() {
 *   console.log(arguments.join("*"));
 * }
 *
 * const f1 = sinon.fake.yields("apple", "pie");
 *
 * f1(callback);
 *
 * setTimeout(() => {
 *   // "apple*pie"
 * });
 *
 * @memberof fake
 * @returns {Function}
 */
fake.yieldsAsync = function yieldsAsync() {
    const values = slice(arguments);

    // eslint-disable-next-line jsdoc/require-jsdoc
    function f() {
        const callback = arguments[arguments.length - 1];
        if (typeof callback !== "function") {
            throw new TypeError("Expected last argument to be a function");
        }
        nextTick(function () {
            callback.apply(null, values);
        });
    }

    return wrapFunc(f);
};

let uuid = 0;
/**
 * Creates a proxy (sinon concept) from the passed function.
 *
 * @private
 * @param  {Function} f
 * @returns {Function}
 */
function wrapFunc(f) {
    const fakeInstance = function () {
        let firstArg, lastArg;

        if (arguments.length > 0) {
            firstArg = arguments[0];
            lastArg = arguments[arguments.length - 1];
        }

        const callback =
            lastArg && typeof lastArg === "function" ? lastArg : undefined;

        /* eslint-disable no-use-before-define */
        proxy.firstArg = firstArg;
        proxy.lastArg = lastArg;
        proxy.callback = callback;

        return f && f.apply(this, arguments);
    };
    const proxy = createProxy(fakeInstance, f || fakeInstance);

    proxy.displayName = "fake";
    proxy.id = `fake#${uuid++}`;

    return proxy;
}

/**
 * Returns an Error instance from the passed value, if the value is not
 * already an Error instance.
 *
 * @private
 * @param  {*} value [description]
 * @returns {Error}       [description]
 */
function getError(value) {
    return value instanceof Error ? value : new Error(value);
}
