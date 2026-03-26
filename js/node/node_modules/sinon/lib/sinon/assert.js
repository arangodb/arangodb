"use strict";
/** @module */

const arrayProto = require("@sinonjs/commons").prototypes.array;
const calledInOrder = require("@sinonjs/commons").calledInOrder;
const createMatcher = require("@sinonjs/samsam").createMatcher;
const orderByFirstCall = require("@sinonjs/commons").orderByFirstCall;
const timesInWords = require("./util/core/times-in-words");
const inspect = require("util").inspect;
const stringSlice = require("@sinonjs/commons").prototypes.string.slice;
const globalObject = require("@sinonjs/commons").global;

const arraySlice = arrayProto.slice;
const concat = arrayProto.concat;
const forEach = arrayProto.forEach;
const join = arrayProto.join;
const splice = arrayProto.splice;

function applyDefaults(obj, defaults) {
    for (const key of Object.keys(defaults)) {
        const val = obj[key];
        if (val === null || typeof val === "undefined") {
            obj[key] = defaults[key];
        }
    }
}

/**
 * @typedef {object} CreateAssertOptions
 * @global
 *
 * @property {boolean} [shouldLimitAssertionLogs] default is false
 * @property {number}  [assertionLogLimit] default is 10K
 */

/**
 * Create an assertion object that exposes several methods to invoke
 *
 * @param {CreateAssertOptions}  [opts] options bag
 * @returns {object} object with multiple assertion methods
 */
function createAssertObject(opts) {
    const cleanedAssertOptions = opts || {};
    applyDefaults(cleanedAssertOptions, {
        shouldLimitAssertionLogs: false,
        assertionLogLimit: 1e4,
    });

    const assert = {
        fail: function fail(message) {
            let msg = message;
            if (cleanedAssertOptions.shouldLimitAssertionLogs) {
                msg = message.substring(
                    0,
                    cleanedAssertOptions.assertionLogLimit,
                );
            }
            const error = new Error(msg);
            error.name = "AssertError";

            throw error;
        },

        pass: function pass() {
            return;
        },

        callOrder: function assertCallOrder() {
            verifyIsStub.apply(null, arguments);
            let expected = "";
            let actual = "";

            if (!calledInOrder(arguments)) {
                try {
                    expected = join(arguments, ", ");
                    const calls = arraySlice(arguments);
                    let i = calls.length;
                    while (i) {
                        if (!calls[--i].called) {
                            splice(calls, i, 1);
                        }
                    }
                    actual = join(orderByFirstCall(calls), ", ");
                } catch (e) {
                    // If this fails, we'll just fall back to the blank string
                }

                failAssertion(
                    this,
                    `expected ${expected} to be called in order but were called as ${actual}`,
                );
            } else {
                assert.pass("callOrder");
            }
        },

        callCount: function assertCallCount(method, count) {
            verifyIsStub(method);

            let msg;
            if (typeof count !== "number") {
                msg =
                    `expected ${inspect(count)} to be a number ` +
                    `but was of type ${typeof count}`;
                failAssertion(this, msg);
            } else if (method.callCount !== count) {
                msg =
                    `expected %n to be called ${timesInWords(count)} ` +
                    `but was called %c%C`;
                failAssertion(this, method.printf(msg));
            } else {
                assert.pass("callCount");
            }
        },

        expose: function expose(target, options) {
            if (!target) {
                throw new TypeError("target is null or undefined");
            }

            const o = options || {};
            const prefix =
                (typeof o.prefix === "undefined" && "assert") || o.prefix;
            const includeFail =
                typeof o.includeFail === "undefined" || Boolean(o.includeFail);
            const instance = this;

            forEach(Object.keys(instance), function (method) {
                if (
                    method !== "expose" &&
                    (includeFail || !/^(fail)/.test(method))
                ) {
                    target[exposedName(prefix, method)] = instance[method];
                }
            });

            return target;
        },

        match: function match(actual, expectation) {
            const matcher = createMatcher(expectation);
            if (matcher.test(actual)) {
                assert.pass("match");
            } else {
                const formatted = [
                    "expected value to match",
                    `    expected = ${inspect(expectation)}`,
                    `    actual = ${inspect(actual)}`,
                ];

                failAssertion(this, join(formatted, "\n"));
            }
        },
    };

    function verifyIsStub() {
        const args = arraySlice(arguments);

        forEach(args, function (method) {
            if (!method) {
                assert.fail("fake is not a spy");
            }

            if (method.proxy && method.proxy.isSinonProxy) {
                verifyIsStub(method.proxy);
            } else {
                if (typeof method !== "function") {
                    assert.fail(`${method} is not a function`);
                }

                if (typeof method.getCall !== "function") {
                    assert.fail(`${method} is not stubbed`);
                }
            }
        });
    }

    function verifyIsValidAssertion(assertionMethod, assertionArgs) {
        switch (assertionMethod) {
            case "notCalled":
            case "called":
            case "calledOnce":
            case "calledTwice":
            case "calledThrice":
                if (assertionArgs.length !== 0) {
                    assert.fail(
                        `${assertionMethod} takes 1 argument but was called with ${
                            assertionArgs.length + 1
                        } arguments`,
                    );
                }
                break;
            default:
                break;
        }
    }

    function failAssertion(object, msg) {
        const obj = object || globalObject;
        const failMethod = obj.fail || assert.fail;
        failMethod.call(obj, msg);
    }

    function mirrorPropAsAssertion(name, method, message) {
        let msg = message;
        let meth = method;
        if (arguments.length === 2) {
            msg = method;
            meth = name;
        }

        assert[name] = function (fake) {
            verifyIsStub(fake);

            const args = arraySlice(arguments, 1);
            let failed = false;

            verifyIsValidAssertion(name, args);

            if (typeof meth === "function") {
                failed = !meth(fake);
            } else {
                failed =
                    typeof fake[meth] === "function"
                        ? !fake[meth].apply(fake, args)
                        : !fake[meth];
            }

            if (failed) {
                failAssertion(
                    this,
                    (fake.printf || fake.proxy.printf).apply(
                        fake,
                        concat([msg], args),
                    ),
                );
            } else {
                assert.pass(name);
            }
        };
    }

    function exposedName(prefix, prop) {
        return !prefix || /^fail/.test(prop)
            ? prop
            : prefix +
                  stringSlice(prop, 0, 1).toUpperCase() +
                  stringSlice(prop, 1);
    }

    mirrorPropAsAssertion(
        "called",
        "expected %n to have been called at least once but was never called",
    );
    mirrorPropAsAssertion(
        "notCalled",
        function (spy) {
            return !spy.called;
        },
        "expected %n to not have been called but was called %c%C",
    );
    mirrorPropAsAssertion(
        "calledOnce",
        "expected %n to be called once but was called %c%C",
    );
    mirrorPropAsAssertion(
        "calledTwice",
        "expected %n to be called twice but was called %c%C",
    );
    mirrorPropAsAssertion(
        "calledThrice",
        "expected %n to be called thrice but was called %c%C",
    );
    mirrorPropAsAssertion(
        "calledOn",
        "expected %n to be called with %1 as this but was called with %t",
    );
    mirrorPropAsAssertion(
        "alwaysCalledOn",
        "expected %n to always be called with %1 as this but was called with %t",
    );
    mirrorPropAsAssertion("calledWithNew", "expected %n to be called with new");
    mirrorPropAsAssertion(
        "alwaysCalledWithNew",
        "expected %n to always be called with new",
    );
    mirrorPropAsAssertion(
        "calledWith",
        "expected %n to be called with arguments %D",
    );
    mirrorPropAsAssertion(
        "calledWithMatch",
        "expected %n to be called with match %D",
    );
    mirrorPropAsAssertion(
        "alwaysCalledWith",
        "expected %n to always be called with arguments %D",
    );
    mirrorPropAsAssertion(
        "alwaysCalledWithMatch",
        "expected %n to always be called with match %D",
    );
    mirrorPropAsAssertion(
        "calledWithExactly",
        "expected %n to be called with exact arguments %D",
    );
    mirrorPropAsAssertion(
        "calledOnceWith",
        "expected %n to be called once and with arguments %D",
    );
    mirrorPropAsAssertion(
        "calledOnceWithExactly",
        "expected %n to be called once and with exact arguments %D",
    );
    mirrorPropAsAssertion(
        "calledOnceWithMatch",
        "expected %n to be called once and with match %D",
    );
    mirrorPropAsAssertion(
        "alwaysCalledWithExactly",
        "expected %n to always be called with exact arguments %D",
    );
    mirrorPropAsAssertion(
        "neverCalledWith",
        "expected %n to never be called with arguments %*%C",
    );
    mirrorPropAsAssertion(
        "neverCalledWithMatch",
        "expected %n to never be called with match %*%C",
    );
    mirrorPropAsAssertion("threw", "%n did not throw exception%C");
    mirrorPropAsAssertion("alwaysThrew", "%n did not always throw exception%C");

    return assert;
}

module.exports = createAssertObject();
module.exports.createAssertObject = createAssertObject;
