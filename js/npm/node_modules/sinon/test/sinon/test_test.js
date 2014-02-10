/*jslint onevar: false, eqeqeq: false, browser: true*/
/*globals sinon, buster*/
/**
 * @author Christian Johansen (christian@cjohansen.no)
 * @license BSD
 *
 * Copyright (c) 2010-2012 Christian Johansen
 */
"use strict";

if (typeof require == "function" && typeof module == "object") {
    var buster = require("../runner");
    var sinon = require("../../lib/sinon");
}

buster.testCase("sinon.test", {
    setUp: function () {
        this.boundTestCase = function () {
            var properties = {
                fn: function () {
                    properties.self = this;
                    properties.args = arguments;
                    properties.spy = this.spy;
                    properties.stub = this.stub;
                    properties.mock = this.mock;
                    properties.clock = this.clock;
                    properties.server = this.server;
                    properties.requests = this.requests;
                    properties.sandbox = this.sandbox;
                }
            };

            return properties;
        };
    },

    tearDown: function () {
        sinon.config = {};
    },

    "throws if argument is not a function": function () {
        assert.exception(function () {
            sinon.test({});
        });
    },

    "proxys return value": function () {
        var object = {};

        var result = sinon.test(function () {
            return object;
        })();

        assert.same(result, object);
    },

    "stubs inside sandbox": function () {
        var method = function () {};
        var object = { method: method };

        sinon.test(function () {
            this.stub(object, "method").returns(object);

            assert.same(object.method(), object);
        }).call({});
    },

    "restores stubs": function () {
        var method = function () {};
        var object = { method: method };

        sinon.test(function () {
            this.stub(object, "method");
        }).call({});

        assert.same(object.method, method);
    },

    "restores stubs on all object methods": function() {
        var method = function () {};
        var method2 = function () {};
        var object = { method: method, method2: method2 };

        sinon.test(function () {
            this.stub(object);
        }).call({});

        assert.same(object.method, method);
        assert.same(object.method2, method2);
    },

    "throws when method throws": function () {
        var method = function () {};
        var object = { method: method };

        assert.exception(function () {
            sinon.test(function () {
                this.stub(object, "method");
                throw new Error();
            }).call({});
        }, "Error");
    },

    "restores stub when method throws": function () {
        var method = function () {};
        var object = { method: method };

        try {
            sinon.test(function () {
                this.stub(object, "method");
                throw new Error();
            }).call({});
        } catch (e) {}

        assert.same(object.method, method);
    },

    "mocks inside sandbox": function () {
        var method = function () {};
        var object = { method: method };

        sinon.test(function () {
            this.mock(object).expects("method").returns(object);

            assert.same(object.method(), object);
        }).call({});
    },

    "verifies mocks": function () {
        var method = function () {};
        var object = { method: method };
        var exception;

        try {
            sinon.test(function () {
                this.mock(object).expects("method").withExactArgs(1).once();
                object.method(42);
            }).call({});
        } catch (e) {
          exception = e;
        }

        assert.same(exception.name, "ExpectationError");
        assert.equals(exception.message,
                      "Unexpected call: method(42)\n" +
                      "    Expected method(1) once (never called)");
        assert.same(object.method, method);
    },

    "restores mocks": function () {
        var method = function () {};
        var object = { method: method };

        try {
            sinon.test(function () {
                this.mock(object).expects("method");
            }).call({});
        } catch (e) {}

        assert.same(object.method, method);
    },

    "restores mock when method throws": function () {
        var method = function () {};
        var object = { method: method };

        try {
            sinon.test(function () {
                this.mock(object).expects("method").never();
                object.method();
            }).call({});
        } catch (e) {}

        assert.same(object.method, method);
    },

    "appends helpers after normal arguments": function () {
        var object = { method: function () {} };

        sinon.config = {
            injectIntoThis: false,
            properties: ["stub", "mock"]
        };

        sinon.test(function (obj, stub, mock) {
            mock(object).expects("method").once();
            object.method();

            assert.same(obj, object);
        })(object);
    },

    "maintains the this value": function () {
        var testCase = {
            someTest: sinon.test(function (obj, stub, mock) {
                return this;
            })
        };

        assert.same(testCase.someTest(), testCase);
    },

    "configurable test with sandbox": {
        tearDown: function () {
            sinon.config = {};
        },

        "yields stub, mock as arguments": function () {
            var stubbed, mocked;
            var obj = { meth: function () {} };

            sinon.config = {
                injectIntoThis: false,
                properties: ["stub", "mock"]
            };

            sinon.test(function (stub, mock) {
                stubbed = stub(obj, "meth");
                mocked = mock(obj);

                assert.equals(arguments.length, 2);
            })();

            assert.stub(stubbed);
            assert.mock(mocked);
        },

        "yields spy, stub, mock as arguments": function () {
            var spied, stubbed, mocked;
            var obj = { meth: function () {} };

            sinon.config = {
                injectIntoThis: false,
                properties: ["spy", "stub", "mock"]
            };

            sinon.test(function (spy, stub, mock) {
                spied = spy(obj, "meth");
                spied.restore();
                stubbed = stub(obj, "meth");
                mocked = mock(obj);

                assert.equals(arguments.length, 3);
            })();

            assert.spy(spied);
            assert.stub(stubbed);
            assert.mock(mocked);
        },

        "does not yield server when not faking xhr": function () {
            var stubbed, mocked;
            var obj = { meth: function () {} };

            sinon.config = {
                injectIntoThis: false,
                properties: ["server", "stub", "mock"],
                useFakeServer: false
            };

            sinon.test(function (stub, mock) {
                stubbed = stub(obj, "meth");
                mocked = mock(obj);

                assert.equals(arguments.length, 2);
            })();

            assert.stub(stubbed);
            assert.mock(mocked);
        },

        "browser options": {
            requiresSupportFor: {
                "ajax/browser": typeof XMLHttpRequest != "undefined" || typeof ActiveXObject != "undefined"
            },

            "yields server when faking xhr": function () {
                var stubbed, mocked, server;
                var obj = { meth: function () {} };

                sinon.config = {
                    injectIntoThis: false,
                    properties: ["server", "stub", "mock"]
                };

                sinon.test(function (serv, stub, mock) {
                    server = serv;
                    stubbed = stub(obj, "meth");
                    mocked = mock(obj);

                    assert.equals(arguments.length, 3);
                })();

                assert.fakeServer(server);
                assert.stub(stubbed);
                assert.mock(mocked);
            },

            "uses serverWithClock when faking xhr": function () {
                var server;

                sinon.config = {
                    injectIntoThis: false,
                    properties: ["server"],
                    useFakeServer: sinon.fakeServerWithClock
                };

                sinon.test(function (serv) {
                    server = serv;
                })();

                assert.fakeServer(server);
                assert(sinon.fakeServerWithClock.isPrototypeOf(server));
            },

            "injects properties into object": function () {
                var testCase = this.boundTestCase();
                var obj = {};

                sinon.config = {
                    injectIntoThis: false,
                    injectInto: obj,
                    properties: ["server", "clock", "spy", "stub", "mock", "requests"]
                };

                sinon.test(testCase.fn).call(testCase);

                assert.equals(testCase.args.length, 0);
                refute.defined(testCase.server);
                refute.defined(testCase.clock);
                refute.defined(testCase.spy);
                refute.defined(testCase.stub);
                refute.defined(testCase.mock);
                refute.defined(testCase.requests);
                assert.fakeServer(obj.server);
                assert.clock(obj.clock);
                assert.isFunction(obj.spy);
                assert.isFunction(obj.stub);
                assert.isFunction(obj.mock);
                assert.isArray(obj.requests);
            },

            "injects server and clock when only enabling them": function () {
                var testCase = this.boundTestCase();

                sinon.config = {
                    useFakeTimers: true,
                    useFakeServer: true
                };

                sinon.test(testCase.fn).call(testCase);

                assert.equals(testCase.args.length, 0);
                assert.isFunction(testCase.spy);
                assert.isFunction(testCase.stub);
                assert.isFunction(testCase.mock);
                assert.fakeServer(testCase.server);
                assert.isArray(testCase.requests);
                assert.clock(testCase.clock);
                refute.defined(testCase.sandbox);
            }
        },

        "yields clock when faking timers": function () {
            var clock;

            sinon.config = {
                injectIntoThis: false,
                properties: ["clock"]
            };

            sinon.test(function (c) {
                clock = c;
                assert.equals(arguments.length, 1);
            })();

            assert.clock(clock);
        },

        "fakes specified timers": function () {
            var props;

            sinon.config = {
                injectIntoThis: false,
                properties: ["clock"],
                useFakeTimers: ["Date", "setTimeout"]
            };

            sinon.test(function (c) {
                props = {
                    clock: c,
                    Date: Date,
                    setTimeout: setTimeout,
                    clearTimeout: clearTimeout,
                    // clear & setImmediate are not yet available in all environments
                    setImmediate: (typeof setImmediate !== "undefined" ? setImmediate : undefined),
                    clearImmediate: (typeof clearImmediate !== "undefined" ? clearImmediate : undefined),
                    setInterval: setInterval,
                    clearInterval: clearInterval
                };
            })();

            refute.same(props.Date, sinon.timers.Date);
            refute.same(props.setTimeout, sinon.timers.setTimeout);
            assert.same(props.clearTimeout, sinon.timers.clearTimeout);
            assert.same(props.setImmediate, sinon.timers.setImmediate);
            assert.same(props.clearImmediate, sinon.timers.clearImmediate);
            assert.same(props.setInterval, sinon.timers.setInterval);
            assert.same(props.clearInterval, sinon.timers.clearInterval);
        },

        "injects properties into test case": function () {
            var testCase = this.boundTestCase();

            sinon.config = {
                properties: ["clock"]
            };

            sinon.test(testCase.fn).call(testCase);

            assert.same(testCase.self, testCase);
            assert.equals(testCase.args.length, 0);
            assert.clock(testCase.clock);
            refute.defined(testCase.spy);
            refute.defined(testCase.stub);
            refute.defined(testCase.mock);
        },

        "injects functions into test case by default": function () {
            var testCase = this.boundTestCase();

            sinon.test(testCase.fn).call(testCase);

            assert.equals(testCase.args.length, 0);
            assert.isFunction(testCase.spy);
            assert.isFunction(testCase.stub);
            assert.isFunction(testCase.mock);
            assert.isObject(testCase.clock);
        },

        "injects sandbox": function () {
            var testCase = this.boundTestCase();

            sinon.config = {
                properties: ["sandbox", "spy"]
            };

            sinon.test(testCase.fn).call(testCase);

            assert.equals(testCase.args.length, 0);
            assert.isFunction(testCase.spy);
            assert.isObject(testCase.sandbox);
        },

        "uses sinon.test to fake time": function () {
            sinon.config = {
                useFakeTimers: true
            };

            var called;

            var testCase = {
                test: sinon.test(function () {
                    var spy = this.spy();
                    setTimeout(spy, 19);
                    this.clock.tick(19);

                    called = spy.called;
                })
            };

            testCase.test();

            assert(called);
        }
    }
});
