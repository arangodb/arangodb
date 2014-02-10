/*jslint onevar: false, eqeqeq: false*/
/*globals window sinon buster*/
/**
 * @author Christian Johansen (christian@cjohansen.no)
 * @license BSD
 *
 * Copyright (c) 2010-2012 Christian Johansen
 */
"use strict";

if (typeof require === "function" && typeof module === "object") {
    var buster = require("../runner");
    var sinon = require("../../lib/sinon");
}

(function () {
    function spyCalledTests(method) {
        return {
            setUp: function () {
                this.spy = sinon.spy.create();
            },

            "returns false if spy was not called": function () {
                assert.isFalse(this.spy[method](1, 2, 3));
            },

            "returns true if spy was called with args": function () {
                this.spy(1, 2, 3);

                assert(this.spy[method](1, 2, 3));
            },

            "returns true if called with args at least once": function () {
                this.spy(1, 3, 3);
                this.spy(1, 2, 3);
                this.spy(3, 2, 3);

                assert(this.spy[method](1, 2, 3));
            },

            "returns false if not called with args": function () {
                this.spy(1, 3, 3);
                this.spy(2);
                this.spy();

                assert.isFalse(this.spy[method](1, 2, 3));
            },

            "returns true for partial match": function () {
                this.spy(1, 3, 3);
                this.spy(2);
                this.spy();

                assert(this.spy[method](1, 3));
            },

            "matchs all arguments individually, not as array": function () {
                this.spy([1, 2, 3]);

                assert.isFalse(this.spy[method](1, 2, 3));
            },

            "uses matcher": function () {
                this.spy("abc");

                assert(this.spy[method](sinon.match.typeOf("string")));
            },

            "uses matcher in object": function () {
                this.spy({ some: "abc" });

                assert(this.spy[method]({ some: sinon.match.typeOf("string") }));
            }
        };
    }

    function spyAlwaysCalledTests(method) {
        return {
            setUp: function () {
                this.spy = sinon.spy.create();
            },

            "returns false if spy was not called": function () {
                assert.isFalse(this.spy[method](1, 2, 3));
            },

            "returns true if spy was called with args": function () {
                this.spy(1, 2, 3);

                assert(this.spy[method](1, 2, 3));
            },

            "returns false if called with args only once": function () {
                this.spy(1, 3, 3);
                this.spy(1, 2, 3);
                this.spy(3, 2, 3);

                assert.isFalse(this.spy[method](1, 2, 3));
            },

            "returns false if not called with args": function () {
                this.spy(1, 3, 3);
                this.spy(2);
                this.spy();

                assert.isFalse(this.spy[method](1, 2, 3));
            },

            "returns true for partial match": function () {
                this.spy(1, 3, 3);

                assert(this.spy[method](1, 3));
            },

            "returns true for partial match on many calls": function () {
                this.spy(1, 3, 3);
                this.spy(1, 3);
                this.spy(1, 3, 4, 5);
                this.spy(1, 3, 1);

                assert(this.spy[method](1, 3));
            },

            "matchs all arguments individually, not as array": function () {
                this.spy([1, 2, 3]);

                assert.isFalse(this.spy[method](1, 2, 3));
            }
        };
    }

    function spyNeverCalledTests(method) {
        return {
            setUp: function () {
                this.spy = sinon.spy.create();
            },

            "returns true if spy was not called": function () {
                assert(this.spy[method](1, 2, 3));
            },

            "returns false if spy was called with args": function () {
                this.spy(1, 2, 3);

                assert.isFalse(this.spy[method](1, 2, 3));
            },

            "returns false if called with args at least once": function () {
                this.spy(1, 3, 3);
                this.spy(1, 2, 3);
                this.spy(3, 2, 3);

                assert.isFalse(this.spy[method](1, 2, 3));
            },

            "returns true if not called with args": function () {
                this.spy(1, 3, 3);
                this.spy(2);
                this.spy();

                assert(this.spy[method](1, 2, 3));
            },

            "returns false for partial match": function () {
                this.spy(1, 3, 3);
                this.spy(2);
                this.spy();

                assert.isFalse(this.spy[method](1, 3));
            },

            "matchs all arguments individually, not as array": function () {
                this.spy([1, 2, 3]);

                assert(this.spy[method](1, 2, 3));
            }
        };
    }

    buster.testCase("sinon.spy", {
        "does not throw if called without function": function () {
            refute.exception(function () {
                sinon.spy.create();
            });
        },

        "does not throw when calling anonymous spy": function () {
            var spy = sinon.spy.create();

            refute.exception(function () {
                spy();
            });

            assert(spy.called);
        },

        "returns spy function": function () {
            var func = function () {};
            var spy = sinon.spy.create(func);

            assert.isFunction(spy);
            refute.same(func, spy);
        },

        "mirrors custom properties on function": function () {
            var func = function () {};
            func.myProp = 42;
            var spy = sinon.spy.create(func);

            assert.equals(spy.myProp, func.myProp);
        },

        "does not define create method": function () {
            var spy = sinon.spy.create();

            refute.defined(spy.create);
        },

        "does not overwrite original create property": function () {
            var func = function () {};
            var object = func.create = {};
            var spy = sinon.spy.create(func);

            assert.same(spy.create, object);
        },

        "setups logging arrays": function () {
            var spy = sinon.spy.create();

            assert.isArray(spy.args);
            assert.isArray(spy.returnValues);
            assert.isArray(spy.thisValues);
            assert.isArray(spy.exceptions);
        },

        "call": {
            "calls underlying function": function () {
                var called = false;

                var spy = sinon.spy.create(function () {
                    called = true;
                });

                spy();

                assert(called);
            },

            "passs arguments to function": function () {
                var actualArgs;

                var func = function (a, b, c, d) {
                    actualArgs = [a, b, c, d];
                };

                var args = [1, {}, [], ""];
                var spy = sinon.spy.create(func);
                spy(args[0], args[1], args[2], args[3]);

                assert.equals(actualArgs, args);
            },

            "maintains this binding": function () {
                var actualThis;

                var func = function () {
                    actualThis = this;
                };

                var object = {};
                var spy = sinon.spy.create(func);
                spy.call(object);

                assert.same(actualThis, object);
            },

            "returns function's return value": function () {
                var object = {};

                var func = function () {
                    return object;
                };

                var spy = sinon.spy.create(func);
                var actualReturn = spy();

                assert.same(actualReturn, object);
            },

            "throws if function throws": function () {
                var err = new Error();
                var spy = sinon.spy.create(function () {
                    throw err;
                });

                try {
                    spy();
                    buster.assertions.fail("Expected spy to throw exception");
                } catch (e) {
                    assert.same(e, err);
                }
            },

            "retains function length 0": function () {
                var spy = sinon.spy.create(function () {});

                assert.equals(spy.length, 0);
            },

            "retains function length 1": function () {
                var spy = sinon.spy.create(function (a) {});

                assert.equals(spy.length, 1);
            },

            "retains function length 2": function () {
                var spy = sinon.spy.create(function (a, b) {});

                assert.equals(spy.length, 2);
            },

            "retains function length 3": function () {
                var spy = sinon.spy.create(function (a, b, c) {});

                assert.equals(spy.length, 3);
            },

            "retains function length 4": function () {
                var spy = sinon.spy.create(function (a, b, c, d) {});

                assert.equals(spy.length, 4);
            },

            "retains function length 12": function () {
                var spy = sinon.spy.create(function (a, b, c, d, e, f, g, h, i, j,k,l) {});

                assert.equals(spy.length, 12);
            }
        },

        "called": {
            setUp: function () {
                this.spy = sinon.spy.create();
            },

            "is false prior to calling the spy": function () {
                assert.isFalse(this.spy.called);
            },

            "is true after calling the spy once": function () {
                this.spy();

                assert(this.spy.called);
            },

            "is true after calling the spy twice": function () {
                this.spy();
                this.spy();

                assert(this.spy.called);
            }
        },

        "notCalled": {
            setUp: function () {
                this.spy = sinon.spy.create();
            },

            "is true prior to calling the spy": function () {
                assert.isTrue(this.spy.notCalled);
            },

            "is false after calling the spy once": function () {
                this.spy();

                assert.isFalse(this.spy.notCalled);
            }
        },

        "calledOnce": {
            setUp: function () {
                this.spy = sinon.spy.create();
            },

            "is false prior to calling the spy": function () {
                assert.isFalse(this.spy.calledOnce);
            },

            "is true after calling the spy once": function () {
                this.spy();

                assert(this.spy.calledOnce);
            },

            "is false after calling the spy twice": function () {
                this.spy();
                this.spy();

                assert.isFalse(this.spy.calledOnce);
            }
        },

        "calledTwice": {
            setUp: function () {
                this.spy = sinon.spy.create();
            },

            "is false prior to calling the spy": function () {
                assert.isFalse(this.spy.calledTwice);
            },

            "is false after calling the spy once": function () {
                this.spy();

                assert.isFalse(this.spy.calledTwice);
            },

            "is true after calling the spy twice": function () {
                this.spy();
                this.spy();

                assert(this.spy.calledTwice);
            },

            "is false after calling the spy thrice": function () {
                this.spy();
                this.spy();
                this.spy();

                assert.isFalse(this.spy.calledTwice);
            }
        },

        "calledThrice": {
            setUp: function () {
                this.spy = sinon.spy.create();
            },

            "is false prior to calling the spy": function () {
                assert.isFalse(this.spy.calledThrice);
            },

            "is false after calling the spy twice": function () {
                this.spy();
                this.spy();

                assert.isFalse(this.spy.calledThrice);
            },

            "is true after calling the spy thrice": function () {
                this.spy();
                this.spy();
                this.spy();

                assert(this.spy.calledThrice);
            },

            "is false after calling the spy four times": function () {
                this.spy();
                this.spy();
                this.spy();
                this.spy();

                assert.isFalse(this.spy.calledThrice);
            }
        },

        "callCount": {
            setUp: function () {
                this.spy = sinon.spy.create();
            },

            "reports 0 calls": function () {
                assert.equals(this.spy.callCount, 0);
            },

            "records one call": function () {
                this.spy();

                assert.equals(this.spy.callCount, 1);
            },

            "records two calls": function () {
                this.spy();
                this.spy();

                assert.equals(this.spy.callCount, 2);
            },

            "increases call count for each call": function () {
                this.spy();
                this.spy();
                assert.equals(this.spy.callCount, 2);

                this.spy();
                assert.equals(this.spy.callCount, 3);
            }
        },

        "calledOn": {
            setUp: function () {
                this.spy = sinon.spy.create();
            },

            "is false if spy wasn't called": function () {
                assert.isFalse(this.spy.calledOn({}));
            },

            "is true if called with thisValue": function () {
                var object = {};
                this.spy.call(object);

                assert(this.spy.calledOn(object));
            },

            "browser": {
                requiresSupportFor: { "browser": typeof window !== "undefined" },

                "is true if called on object at least once": function () {
                    var object = {};
                    this.spy();
                    this.spy.call({});
                    this.spy.call(object);
                    this.spy.call(window);

                    assert(this.spy.calledOn(object));
                }
            },

            "returns false if not called on object": function () {
                var object = {};
                this.spy.call(object);
                this.spy();

                assert.isFalse(this.spy.calledOn({}));
            },

            "is true if called with matcher that returns true": function () {
                var matcher = sinon.match(function () { return true; });
                this.spy();

                assert(this.spy.calledOn(matcher));
            },

            "is false if called with matcher that returns false": function () {
                var matcher = sinon.match(function () { return false; });
                this.spy();

                assert.isFalse(this.spy.calledOn(matcher));
            },

            "invokes matcher.test with given object": function () {
                var expected = {};
                var actual;
                this.spy.call(expected);

                this.spy.calledOn(sinon.match(function (value) {
                    actual = value;
                }));

                assert.same(actual, expected);
            }
        },

        "alwaysCalledOn": {
            setUp: function () {
                this.spy = sinon.spy.create();
            },

            "is false prior to calling the spy": function () {
                assert.isFalse(this.spy.alwaysCalledOn({}));
            },

            "is true if called with thisValue once": function () {
                var object = {};
                this.spy.call(object);

                assert(this.spy.alwaysCalledOn(object));
            },

            "is true if called with thisValue many times": function () {
                var object = {};
                this.spy.call(object);
                this.spy.call(object);
                this.spy.call(object);
                this.spy.call(object);

                assert(this.spy.alwaysCalledOn(object));
            },

            "is false if called with another object atleast once": function () {
                var object = {};
                this.spy.call(object);
                this.spy.call(object);
                this.spy.call(object);
                this.spy();
                this.spy.call(object);

                assert.isFalse(this.spy.alwaysCalledOn(object));
            },

            "is false if never called with expected object": function () {
                var object = {};
                this.spy();
                this.spy();
                this.spy();

                assert.isFalse(this.spy.alwaysCalledOn(object));
            }
        },

        "calledWithNew": {
            setUp: function () {
                this.spy = sinon.spy.create();
            },

            "is false if spy wasn't called": function () {
                assert.isFalse(this.spy.calledWithNew());
            },

            "is true if called with new": function () {
                var result = new this.spy();

                assert(this.spy.calledWithNew());
            },

            "is true if called with new on custom constructor": function () {
                function MyThing() {}
                MyThing.prototype = {};
                var ns = { MyThing: MyThing };
                sinon.spy(ns, "MyThing");

                var result = new ns.MyThing();
                assert(ns.MyThing.calledWithNew());
            },

            "is false if called as function": function () {
                this.spy();

                assert.isFalse(this.spy.calledWithNew());
            },

            "browser": {
                requiresSupportFor: { "browser": typeof window !== "undefined" },

                "is true if called with new at least once": function () {
                    var object = {};
                    this.spy();
                    var a = new this.spy();
                    this.spy(object);
                    this.spy(window);

                    assert(this.spy.calledWithNew());
                }
            },

            "is true newed constructor returns object": function () {
                function MyThing() { return {}; }
                var object = { MyThing: MyThing };
                sinon.spy(object, "MyThing");

                var result = new object.MyThing;

                assert(object.MyThing.calledWithNew());
            }
        },

        "alwaysCalledWithNew": {
            setUp: function () {
                this.spy = sinon.spy.create();
            },

            "is false if spy wasn't called": function () {
                assert.isFalse(this.spy.alwaysCalledWithNew());
            },

            "is true if always called with new": function () {
                var result = new this.spy();
                var result2 = new this.spy();
                var result3 = new this.spy();

                assert(this.spy.alwaysCalledWithNew());
            },

            "is false if called as function once": function () {
                var result = new this.spy();
                var result2 = new this.spy();
                this.spy();

                assert.isFalse(this.spy.alwaysCalledWithNew());
            }
        },

        "thisValue": {
            setUp: function () {
                this.spy = sinon.spy.create();
            },

            "contains one object": function () {
                var object = {};
                this.spy.call(object);

                assert.equals(this.spy.thisValues, [object]);
            },

            "stacks up objects": function () {
                function MyConstructor() {}
                var objects = [{}, [], new MyConstructor(), { id: 243 }];
                this.spy();
                this.spy.call(objects[0]);
                this.spy.call(objects[1]);
                this.spy.call(objects[2]);
                this.spy.call(objects[3]);

                assert.equals(this.spy.thisValues, [this].concat(objects));
            }
        },

        "calledWith": spyCalledTests("calledWith"),
        "calledWithMatch": spyCalledTests("calledWithMatch"),

        "calledWithMatchSpecial": {
            setUp: function () {
                this.spy = sinon.spy.create();
            },

            "checks substring match": function () {
                this.spy("I like it");

                assert(this.spy.calledWithMatch("like"));
                assert.isFalse(this.spy.calledWithMatch("nope"));
            },

            "checks for regexp match": function () {
                this.spy("I like it");

                assert(this.spy.calledWithMatch(/[a-z ]+/i));
                assert.isFalse(this.spy.calledWithMatch(/[0-9]+/));
            },

            "checks for partial object match": function () {
                this.spy({ foo: "foo", bar: "bar" });

                assert(this.spy.calledWithMatch({ bar: "bar" }));
                assert.isFalse(this.spy.calledWithMatch({ same: "same" }));
            }
        },

        "alwaysCalledWith": spyAlwaysCalledTests("alwaysCalledWith"),
        "alwaysCalledWithMatch": spyAlwaysCalledTests("alwaysCalledWithMatch"),

        "alwaysCalledWithMatchSpecial": {
            setUp: function () {
                this.spy = sinon.spy.create();
            },

            "checks true": function () {
                this.spy(true);

                assert(this.spy.alwaysCalledWithMatch(true));
                assert.isFalse(this.spy.alwaysCalledWithMatch(false));
            },

            "checks false": function () {
                this.spy(false);

                assert(this.spy.alwaysCalledWithMatch(false));
                assert.isFalse(this.spy.alwaysCalledWithMatch(true));
            },

            "checks substring match": function () {
                this.spy("test case");
                this.spy("some test");
                this.spy("all tests");

                assert(this.spy.alwaysCalledWithMatch("test"));
                assert.isFalse(this.spy.alwaysCalledWithMatch("case"));
            },

            "checks regexp match": function () {
                this.spy("1");
                this.spy("2");
                this.spy("3");

                assert(this.spy.alwaysCalledWithMatch(/[123]/));
                assert.isFalse(this.spy.alwaysCalledWithMatch(/[12]/));
            },

            "checks partial object match": function () {
                this.spy({ a: "a", b: "b" });
                this.spy({ c: "c", b: "b" });
                this.spy({ b: "b", d: "d" });

                assert(this.spy.alwaysCalledWithMatch({ b: "b" }));
                assert.isFalse(this.spy.alwaysCalledWithMatch({ a: "a" }));
            }
        },

        "neverCalledWith": spyNeverCalledTests("neverCalledWith"),
        "neverCalledWithMatch": spyNeverCalledTests("neverCalledWithMatch"),

        "neverCalledWithMatchSpecial": {
            setUp: function () {
                this.spy = sinon.spy.create();
            },

            "checks substring match": function () {
                this.spy("a test", "b test");

                assert(this.spy.neverCalledWithMatch("a", "a"));
                assert(this.spy.neverCalledWithMatch("b", "b"));
                assert(this.spy.neverCalledWithMatch("b", "a"));
                assert.isFalse(this.spy.neverCalledWithMatch("a", "b"));
            },

            "checks regexp match": function () {
                this.spy("a test", "b test");

                assert(this.spy.neverCalledWithMatch(/a/, /a/));
                assert(this.spy.neverCalledWithMatch(/b/, /b/));
                assert(this.spy.neverCalledWithMatch(/b/, /a/));
                assert.isFalse(this.spy.neverCalledWithMatch(/a/, /b/));
            },

            "checks partial object match": function () {
                this.spy({ a: "test", b: "test" });

                assert(this.spy.neverCalledWithMatch({ a: "nope" }));
                assert(this.spy.neverCalledWithMatch({ c: "test" }));
                assert.isFalse(this.spy.neverCalledWithMatch({ b: "test" }));
            }
        },

        "args": {
            setUp: function () {
                this.spy = sinon.spy.create();
            },

            "contains real arrays": function () {
                this.spy();

                assert.isArray(this.spy.args[0]);
            },

            "contains empty array when no arguments": function () {
                this.spy();

                assert.equals(this.spy.args, [[]]);
            },

            "contains array with first call's arguments": function () {
                this.spy(1, 2, 3);

                assert.equals(this.spy.args, [[1, 2, 3]]);
            },

            "stacks up arguments in nested array": function () {
                var objects = [{}, [], { id: 324 }];
                this.spy(1, objects[0], 3);
                this.spy(1, 2, objects[1]);
                this.spy(objects[2], 2, 3);

                assert.equals(this.spy.args,
                              [[1, objects[0], 3],
                               [1, 2, objects[1]],
                               [objects[2], 2, 3]]);
            }
        },

        "calledWithExactly": {
            setUp: function () {
                this.spy = sinon.spy.create();
            },

            "returns false for partial match": function () {
                this.spy(1, 2, 3);

                assert.isFalse(this.spy.calledWithExactly(1, 2));
            },

            "returns false for missing arguments": function () {
                this.spy(1, 2, 3);

                assert.isFalse(this.spy.calledWithExactly(1, 2, 3, 4));
            },

            "returns true for exact match": function () {
                this.spy(1, 2, 3);

                assert(this.spy.calledWithExactly(1, 2, 3));
            },

            "matchs by strict comparison": function () {
                this.spy({}, []);

                assert.isFalse(this.spy.calledWithExactly({}, [], null));
            },

            "returns true for one exact match": function () {
                var object = {};
                var array = [];
                this.spy({}, []);
                this.spy(object, []);
                this.spy(object, array);

                assert(this.spy.calledWithExactly(object, array));
            }
        },

        "alwaysCalledWithExactly": {
            setUp: function () {
                this.spy = sinon.spy.create();
            },

            "returns false for partial match": function () {
                this.spy(1, 2, 3);

                assert.isFalse(this.spy.alwaysCalledWithExactly(1, 2));
            },

            "returns false for missing arguments": function () {
                this.spy(1, 2, 3);

                assert.isFalse(this.spy.alwaysCalledWithExactly(1, 2, 3, 4));
            },

            "returns true for exact match": function () {
                this.spy(1, 2, 3);

                assert(this.spy.alwaysCalledWithExactly(1, 2, 3));
            },

            "returns false for excess arguments": function () {
                this.spy({}, []);

                assert.isFalse(this.spy.alwaysCalledWithExactly({}, [], null));
            },

            "returns false for one exact match": function () {
                var object = {};
                var array = [];
                this.spy({}, []);
                this.spy(object, []);
                this.spy(object, array);

                assert(this.spy.alwaysCalledWithExactly(object, array));
            },

            "returns true for only exact matches": function () {
                var object = {};
                var array = [];

                this.spy(object, array);
                this.spy(object, array);
                this.spy(object, array);

                assert(this.spy.alwaysCalledWithExactly(object, array));
            },

            "returns false for no exact matches": function () {
                var object = {};
                var array = [];

                this.spy(object, array, null);
                this.spy(object, array, undefined);
                this.spy(object, array, "");

                assert.isFalse(this.spy.alwaysCalledWithExactly(object, array));
            }
        },

        "threw": {
            setUp: function () {
                this.spy = sinon.spy.create();

                this.spyWithTypeError = sinon.spy.create(function () {
                    throw new TypeError();
                });
                
                this.spyWithStringError = sinon.spy.create(function() {
                	throw "error";
                });
            },

            "returns exception thrown by function": function () {
                var err = new Error();

                var spy = sinon.spy.create(function () {
                    throw err;
                });

                try {
                    spy();
                } catch (e) {}

                assert(spy.threw(err));
            },

            "returns false if spy did not throw": function () {
                this.spy();

                assert.isFalse(this.spy.threw());
            },

            "returns true if spy threw": function () {
                try {
                    this.spyWithTypeError();
                } catch (e) {}

                assert(this.spyWithTypeError.threw());
            },

            "returns true if string type matches": function () {
                try {
                    this.spyWithTypeError();
                } catch (e) {}

                assert(this.spyWithTypeError.threw("TypeError"));
            },

            "returns false if string did not match": function () {
                try {
                    this.spyWithTypeError();
                } catch (e) {}

                assert.isFalse(this.spyWithTypeError.threw("Error"));
            },

            "returns false if spy did not throw specified error": function () {
                this.spy();

                assert.isFalse(this.spy.threw("Error"));
            },
            
            "returns true if string matches": function () {
            	try {
            		this.spyWithStringError();
            	} catch (e) {}
            	
            	assert(this.spyWithStringError.threw("error"));
            },
            
            "returns false if strings do not match": function() {
            	try {
            		this.spyWithStringError();
            	} catch (e) {}
            	
            	assert.isFalse(this.spyWithStringError.threw("not the error"));
            }
        },

        "alwaysThrew": {
            setUp: function () {
                this.spy = sinon.spy.create();

                this.spyWithTypeError = sinon.spy.create(function () {
                    throw new TypeError();
                });
            },

            "returns true when spy threw": function () {
                var err = new Error();

                var spy = sinon.spy.create(function () {
                    throw err;
                });

                try {
                    spy();
                } catch (e) {}

                assert(spy.alwaysThrew(err));
            },

            "returns false if spy did not throw": function () {
                this.spy();

                assert.isFalse(this.spy.alwaysThrew());
            },

            "returns true if spy threw": function () {
                try {
                    this.spyWithTypeError();
                } catch (e) {}

                assert(this.spyWithTypeError.alwaysThrew());
            },

            "returns true if string type matches": function () {
                try {
                    this.spyWithTypeError();
                } catch (e) {}

                assert(this.spyWithTypeError.alwaysThrew("TypeError"));
            },

            "returns false if string did not match": function () {
                try {
                    this.spyWithTypeError();
                } catch (e) {}

                assert.isFalse(this.spyWithTypeError.alwaysThrew("Error"));
            },

            "returns false if spy did not throw specified error": function () {
                this.spy();

                assert.isFalse(this.spy.alwaysThrew("Error"));
            },

            "returns false if some calls did not throw": function () {
                var spy = sinon.stub.create(function () {
                    if (spy.callCount === 0) {
                        throw new Error();
                    }
                });

                try {
                    this.spy();
                } catch (e) {}

                this.spy();

                assert.isFalse(this.spy.alwaysThrew());
            },

            "returns true if all calls threw": function () {
                try {
                    this.spyWithTypeError();
                } catch (e1) {}

                try {
                    this.spyWithTypeError();
                } catch (e2) {}

                assert(this.spyWithTypeError.alwaysThrew());
            },

            "returns true if all calls threw same type": function () {
                try {
                    this.spyWithTypeError();
                } catch (e1) {}

                try {
                    this.spyWithTypeError();
                } catch (e2) {}

                assert(this.spyWithTypeError.alwaysThrew("TypeError"));
            }
        },

        "exceptions": {
            setUp: function () {
                this.spy = sinon.spy.create();
                var error = this.error = {};

                this.spyWithTypeError = sinon.spy.create(function () {
                    throw error;
                });
            },

            "contains exception thrown by function": function () {
                try {
                    this.spyWithTypeError();
                } catch (e) {}

                assert.equals(this.spyWithTypeError.exceptions, [this.error]);
            },

            "contains undefined entry when function did not throw": function () {
                this.spy();

                assert.equals(this.spy.exceptions.length, 1);
                refute.defined(this.spy.exceptions[0]);
            },

            "stacks up exceptions and undefined": function () {
                var calls = 0;
                var err = this.error;

                var spy = sinon.spy.create(function () {
                    calls += 1;

                    if (calls % 2 === 0) {
                        throw err;
                    }
                });

                spy();

                try {
                    spy();
                } catch (e1) {}

                spy();

                try {
                    spy();
                } catch (e2) {}

                spy();

                assert.equals(spy.exceptions.length, 5);
                refute.defined(spy.exceptions[0]);
                assert.equals(spy.exceptions[1], err);
                refute.defined(spy.exceptions[2]);
                assert.equals(spy.exceptions[3], err);
                refute.defined(spy.exceptions[4]);
            }
        },

        "returned": {
            "returns true when no argument": function () {
                var spy = sinon.spy.create();
                spy();

                assert(spy.returned());
            },

            "returns true for undefined when no explicit return": function () {
                var spy = sinon.spy.create();
                spy();

                assert(spy.returned(undefined));
            },

            "returns true when returned value once": function () {
                var values = [{}, 2, "hey", function () {}];
                var spy = sinon.spy.create(function () {
                    return values[spy.callCount];
                });

                spy();
                spy();
                spy();
                spy();

                assert(spy.returned(values[3]));
            },

            "returns false when value is never returned": function () {
                var values = [{}, 2, "hey", function () {}];
                var spy = sinon.spy.create(function () {
                    return values[spy.callCount];
                });

                spy();
                spy();
                spy();
                spy();

                assert.isFalse(spy.returned({ id: 42 }));
            },

            "returns true when value is returned several times": function () {
                var object = { id: 42 };
                var spy = sinon.spy.create(function () {
                    return object;
                });

                spy();
                spy();
                spy();

                assert(spy.returned(object));
            },

            "compares values deeply": function () {
                var object = { deep: { id: 42 } };
                var spy = sinon.spy.create(function () {
                    return object;
                });

                spy();

                assert(spy.returned({ deep: { id: 42 } }));
            },

            "compares values strictly using match.same": function () {
                var object = { id: 42 };
                var spy = sinon.spy.create(function () {
                    return object;
                });

                spy();

                assert.isFalse(spy.returned(sinon.match.same({ id: 42 })));
                assert(spy.returned(sinon.match.same(object)));
            }
        },

        "returnValues": {
            "contains undefined when function does not return explicitly": function () {
                var spy = sinon.spy.create();
                spy();

                assert.equals(spy.returnValues.length, 1);
                refute.defined(spy.returnValues[0]);
            },

            "contains return value": function () {
                var object = { id: 42 };

                var spy = sinon.spy.create(function () {
                    return object;
                });

                spy();

                assert.equals(spy.returnValues, [object]);
            },

            "contains undefined when function throws": function () {
                var spy = sinon.spy.create(function () {
                    throw new Error();
                });

                try {
                    spy();
                } catch (e) {
                }

                assert.equals(spy.returnValues.length, 1);
                refute.defined(spy.returnValues[0]);
            },

            "contains the created object for spied constructors": function () {
                var spy = sinon.spy.create(function() { });

                var result = new spy();

                assert.equals(spy.returnValues[0], result);
            },

            "contains the return value for spied constructors that explicitly return objects": function () {
                var spy = sinon.spy.create(function() {
                    return { isExplicitlyCreatedValue: true };
                });

                var result = new spy();

                assert.isTrue(result.isExplicitlyCreatedValue);
                assert.equals(spy.returnValues[0], result);
            },

            "contains the created object for spied constructors that explicitly return primitive values": function () {
                var spy = sinon.spy.create(function() {
                    return 10;
                });

                var result = new spy();

                refute.equals(result, 10);
                assert.equals(spy.returnValues[0], result);
            },

            "stacks up return values": function () {
                var calls = 0;

                var spy = sinon.spy.create(function () {
                    calls += 1;

                    if (calls % 2 === 0) {
                        return calls;
                    }
                });

                spy();
                spy();
                spy();
                spy();
                spy();

                assert.equals(spy.returnValues.length, 5);
                refute.defined(spy.returnValues[0]);
                assert.equals(spy.returnValues[1], 2);
                refute.defined(spy.returnValues[2]);
                assert.equals(spy.returnValues[3], 4);
                refute.defined(spy.returnValues[4]);
            }
        },

        "calledBefore": {
            setUp: function () {
                this.spy1 = sinon.spy();
                this.spy2 = sinon.spy();
            },

            "is function": function () {
                assert.isFunction(this.spy1.calledBefore);
            },

            "returns true if first call to A was before first to B": function () {
                this.spy1();
                this.spy2();

                assert(this.spy1.calledBefore(this.spy2));
            },

            "compares call order of calls directly": function () {
                this.spy1();
                this.spy2();

                assert(this.spy1.getCall(0).calledBefore(this.spy2.getCall(0)));
            },

            "returns false if not called": function () {
                this.spy2();

                assert.isFalse(this.spy1.calledBefore(this.spy2));
            },

            "returns true if other not called": function () {
                this.spy1();

                assert(this.spy1.calledBefore(this.spy2));
            },

            "returns false if other called first": function () {
                this.spy2();
                this.spy1();
                this.spy2();

                assert(this.spy1.calledBefore(this.spy2));
            }
        },

        "calledAfter": {
            setUp: function () {
                this.spy1 = sinon.spy();
                this.spy2 = sinon.spy();
            },

            "is function": function () {
                assert.isFunction(this.spy1.calledAfter);
            },

            "returns true if first call to A was after first to B": function () {
                this.spy2();
                this.spy1();

                assert(this.spy1.calledAfter(this.spy2));
            },

            "compares calls directly": function () {
                this.spy2();
                this.spy1();

                assert(this.spy1.getCall(0).calledAfter(this.spy2.getCall(0)));
            },

            "returns false if not called": function () {
                this.spy2();

                assert.isFalse(this.spy1.calledAfter(this.spy2));
            },

            "returns false if other not called": function () {
                this.spy1();

                assert.isFalse(this.spy1.calledAfter(this.spy2));
            },

            "returns false if other called last": function () {
                this.spy2();
                this.spy1();
                this.spy2();

                assert.isFalse(this.spy1.calledAfter(this.spy2));
            }
        },

        "firstCall": {
            "is undefined by default": function () {
                var spy = sinon.spy();

                assert.isNull(spy.firstCall);
            },

            "is equal to getCall(0) result after first call": function () {
                var spy = sinon.spy();

                spy();

                var call0 = spy.getCall(0);
                assert.equals(spy.firstCall.callId, call0.callId);
                assert.same(spy.firstCall.spy, call0.spy);
            },

            "is tracked even if exceptions are thrown": function () {
                var spy = sinon.spy(function () { throw "an exception"; });

                try {
                    spy();
                } catch (e) { }

                refute.isNull(spy.firstCall);
            }

        },

        "secondCall": {
            "is null by default": function () {
                var spy = sinon.spy();

                assert.isNull(spy.secondCall);
            },

            "stills be null after first call": function () {
                var spy = sinon.spy();
                spy();

                assert.isNull(spy.secondCall);
            },

            "is equal to getCall(1) result after second call": function () {
                var spy = sinon.spy();

                spy();
                spy();

                var call1 = spy.getCall(1);
                assert.equals(spy.secondCall.callId, call1.callId);
                assert.same(spy.secondCall.spy, call1.spy);
            }
        },

        "thirdCall": {
            "is undefined by default": function () {
                var spy = sinon.spy();

                assert.isNull(spy.thirdCall);
            },

            "stills be undefined after second call": function () {
                var spy = sinon.spy();
                spy();
                spy();

                assert.isNull(spy.thirdCall);
            },

            "is equal to getCall(1) result after second call": function () {
                var spy = sinon.spy();

                spy();
                spy();
                spy();

                var call2 = spy.getCall(2);
                assert.equals(spy.thirdCall.callId, call2.callId);
                assert.same(spy.thirdCall.spy, call2.spy);
            }
        },

        "lastCall": {
            "is undefined by default": function () {
                var spy = sinon.spy();

                assert.isNull(spy.lastCall);
            },

            "is same as firstCall after first call": function () {
                var spy = sinon.spy();

                spy();

                assert.same(spy.lastCall.callId, spy.firstCall.callId);
                assert.same(spy.lastCall.spy, spy.firstCall.spy);
            },

            "is same as secondCall after second call": function () {
                var spy = sinon.spy();

                spy();
                spy();

                assert.same(spy.lastCall.callId, spy.secondCall.callId);
                assert.same(spy.lastCall.spy, spy.secondCall.spy);
            },

            "is same as thirdCall after third call": function () {
                var spy = sinon.spy();

                spy();
                spy();
                spy();

                assert.same(spy.lastCall.callId, spy.thirdCall.callId);
                assert.same(spy.lastCall.spy, spy.thirdCall.spy);
            },

            "is equal to getCall(3) result after fourth call": function () {
                var spy = sinon.spy();

                spy();
                spy();
                spy();
                spy();

                var call3 = spy.getCall(3);
                assert.equals(spy.lastCall.callId, call3.callId);
                assert.same(spy.lastCall.spy, call3.spy);
            },

            "is equal to getCall(4) result after fifth call": function () {
                var spy = sinon.spy();

                spy();
                spy();
                spy();
                spy();
                spy();

                var call4 = spy.getCall(4);
                assert.equals(spy.lastCall.callId, call4.callId);
                assert.same(spy.lastCall.spy, call4.spy);
            }
        },

        "getCalls": {
            "returns an empty Array by default": function () {
                var spy = sinon.spy();

                assert.isArray(spy.getCalls());
                assert.equals(spy.getCalls().length, 0);
            },

            "is analogous to using getCall(n)": function () {
                var spy = sinon.spy();

                spy();
                spy();

                assert.equals(spy.getCalls(), [ spy.getCall(0), spy.getCall(1) ]);
            }
        },

        "callArg": {
            "is function": function () {
                var spy = sinon.spy();

                assert.isFunction(spy.callArg);
            },

            "invokes argument at index for all calls": function () {
                var spy = sinon.spy();
                var callback = sinon.spy();
                spy(1, 2, callback);
                spy(3, 4, callback);

                spy.callArg(2);

                assert(callback.calledTwice);
                assert(callback.alwaysCalledWith());
            },

            "throws if argument at index is not a function": function () {
                var spy = sinon.spy();
                spy();

                assert.exception(function () {
                    spy.callArg(1);
                }, "TypeError");
            },

            "throws if spy was not yet invoked": function () {
                var spy = sinon.spy();

                try {
                    spy.callArg(0);
                    throw new Error();
                } catch (e) {
                    assert.equals(e.message, "spy cannot call arg since it was not yet invoked.");
                }
            },

            "includes spy name in error message": function () {
                var api = { someMethod: function () {} };
                var spy = sinon.spy(api, "someMethod");

                try {
                    spy.callArg(0);
                    throw new Error();
                } catch (e) {
                    assert.equals(e.message, "someMethod cannot call arg since it was not yet invoked.");
                }
            },

            "throws if index is not a number": function () {
                var spy = sinon.spy();
                spy();

                assert.exception(function () {
                    spy.callArg("");
                }, "TypeError");
            },

            "passs additional arguments": function () {
                var spy = sinon.spy();
                var callback = sinon.spy();
                var array = [];
                var object = {};
                spy(callback);

                spy.callArg(0, "abc", 123, array, object);

                assert(callback.calledWith("abc", 123, array, object));
            }
        },

        "callArgOn": {
            "is function": function () {
                var spy = sinon.spy();

                assert.isFunction(spy.callArgOn);
            },

            "invokes argument at index for all calls": function () {
                var spy = sinon.spy();
                var callback = sinon.spy();
                var thisObj = { name1: "value1", name2: "value2" };
                spy(1, 2, callback);
                spy(3, 4, callback);

                spy.callArgOn(2, thisObj);

                assert(callback.calledTwice);
                assert(callback.alwaysCalledWith());
                assert(callback.alwaysCalledOn(thisObj));
            },

            "throws if argument at index is not a function": function () {
                var spy = sinon.spy();
                var thisObj = { name1: "value1", name2: "value2" };
                spy();

                assert.exception(function () {
                    spy.callArgOn(1, thisObj);
                }, "TypeError");
            },

            "throws if spy was not yet invoked": function () {
                var spy = sinon.spy();
                var thisObj = { name1: "value1", name2: "value2" };

                try {
                    spy.callArgOn(0, thisObj);
                    throw new Error();
                } catch (e) {
                    assert.equals(e.message, "spy cannot call arg since it was not yet invoked.");
                }
            },

            "includes spy name in error message": function () {
                var api = { someMethod: function () {} };
                var spy = sinon.spy(api, "someMethod");
                var thisObj = { name1: "value1", name2: "value2" };

                try {
                    spy.callArgOn(0, thisObj);
                    throw new Error();
                } catch (e) {
                    assert.equals(e.message, "someMethod cannot call arg since it was not yet invoked.");
                }
            },

            "throws if index is not a number": function () {
                var spy = sinon.spy();
                var thisObj = { name1: "value1", name2: "value2" };
                spy();

                assert.exception(function () {
                    spy.callArg("", thisObj);
                }, "TypeError");
            },

            "pass additional arguments": function () {
                var spy = sinon.spy();
                var callback = sinon.spy();
                var array = [];
                var object = {};
                var thisObj = { name1: "value1", name2: "value2" };
                spy(callback);

                spy.callArgOn(0, thisObj, "abc", 123, array, object);

                assert(callback.calledWith("abc", 123, array, object));
                assert(callback.calledOn(thisObj));
            }
        },

        "callArgWith": {
            "is alias for callArg": function () {
                var spy = sinon.spy();

                assert.same(spy.callArgWith, spy.callArg);
            }
        },

        "callArgOnWith": {
            "is alias for callArgOn": function () {
                var spy = sinon.spy();

                assert.same(spy.callArgOnWith, spy.callArgOn);
            }
        },

        "yield": {
            "is function": function () {
                var spy = sinon.spy();

                assert.isFunction(spy.yield);
            },

            "invokes first function arg for all calls": function () {
                var spy = sinon.spy();
                var callback = sinon.spy();
                spy(1, 2, callback);
                spy(3, 4, callback);

                spy.yield();

                assert(callback.calledTwice);
                assert(callback.alwaysCalledWith());
            },

            "throws if spy was not yet invoked": function () {
                var spy = sinon.spy();

                try {
                    spy.yield();
                    throw new Error();
                } catch (e) {
                    assert.equals(e.message, "spy cannot yield since it was not yet invoked.");
                }
            },

            "includes spy name in error message": function () {
                var api = { someMethod: function () {} };
                var spy = sinon.spy(api, "someMethod");

                try {
                    spy.yield();
                    throw new Error();
                } catch (e) {
                    assert.equals(e.message, "someMethod cannot yield since it was not yet invoked.");
                }
            },

            "passs additional arguments": function () {
                var spy = sinon.spy();
                var callback = sinon.spy();
                var array = [];
                var object = {};
                spy(callback);

                spy.yield("abc", 123, array, object);

                assert(callback.calledWith("abc", 123, array, object));
            }
        },

        "invokeCallback": {
            "is alias for yield": function () {
                var spy = sinon.spy();

                assert.same(spy.invokeCallback, spy.yield);
            }
        },

        "yieldOn": {
            "is function": function () {
                var spy = sinon.spy();

                assert.isFunction(spy.yieldOn);
            },

            "invokes first function arg for all calls": function () {
                var spy = sinon.spy();
                var callback = sinon.spy();
                var thisObj = { name1: "value1", name2: "value2" };
                spy(1, 2, callback);
                spy(3, 4, callback);

                spy.yieldOn(thisObj);

                assert(callback.calledTwice);
                assert(callback.alwaysCalledWith());
                assert(callback.alwaysCalledOn(thisObj));
            },

            "throws if spy was not yet invoked": function () {
                var spy = sinon.spy();
                var thisObj = { name1: "value1", name2: "value2" };

                try {
                    spy.yieldOn(thisObj);
                    throw new Error();
                } catch (e) {
                    assert.equals(e.message, "spy cannot yield since it was not yet invoked.");
                }
            },

            "includes spy name in error message": function () {
                var api = { someMethod: function () {} };
                var spy = sinon.spy(api, "someMethod");
                var thisObj = { name1: "value1", name2: "value2" };

                try {
                    spy.yieldOn(thisObj);
                    throw new Error();
                } catch (e) {
                    assert.equals(e.message, "someMethod cannot yield since it was not yet invoked.");
                }
            },

            "pass additional arguments": function () {
                var spy = sinon.spy();
                var callback = sinon.spy();
                var array = [];
                var object = {};
                var thisObj = { name1: "value1", name2: "value2" };
                spy(callback);

                spy.yieldOn(thisObj, "abc", 123, array, object);

                assert(callback.calledWith("abc", 123, array, object));
                assert(callback.calledOn(thisObj));
            }
        },

        "yieldTo": {
            "is function": function () {
                var spy = sinon.spy();

                assert.isFunction(spy.yieldTo);
            },

            "invokes first function arg for all calls": function () {
                var spy = sinon.spy();
                var callback = sinon.spy();
                spy(1, 2, { success: callback });
                spy(3, 4, { success: callback });

                spy.yieldTo("success");

                assert(callback.calledTwice);
                assert(callback.alwaysCalledWith());
            },

            "throws if spy was not yet invoked": function () {
                var spy = sinon.spy();

                try {
                    spy.yieldTo("success");
                    throw new Error();
                } catch (e) {
                    assert.equals(e.message, "spy cannot yield to 'success' since it was not yet invoked.");
                }
            },

            "includes spy name in error message": function () {
                var api = { someMethod: function () {} };
                var spy = sinon.spy(api, "someMethod");

                try {
                    spy.yieldTo("success");
                    throw new Error();
                } catch (e) {
                    assert.equals(e.message, "someMethod cannot yield to 'success' since it was not yet invoked.");
                }
            },

        "pass additional arguments": function () {
                var spy = sinon.spy();
                var callback = sinon.spy();
                var array = [];
                var object = {};
                spy({ test: callback });

                spy.yieldTo("test", "abc", 123, array, object);

                assert(callback.calledWith("abc", 123, array, object));
            }
        },

        "yieldToOn": {
            "is function": function () {
                var spy = sinon.spy();

                assert.isFunction(spy.yieldToOn);
            },

            "invokes first function arg for all calls": function () {
                var spy = sinon.spy();
                var callback = sinon.spy();
                var thisObj = { name1: "value1", name2: "value2" };
                spy(1, 2, { success: callback });
                spy(3, 4, { success: callback });

                spy.yieldToOn("success", thisObj);

                assert(callback.calledTwice);
                assert(callback.alwaysCalledWith());
                assert(callback.alwaysCalledOn(thisObj));
            },

            "throws if spy was not yet invoked": function () {
                var spy = sinon.spy();
                var thisObj = { name1: "value1", name2: "value2" };

                try {
                    spy.yieldToOn("success", thisObj);
                    throw new Error();
                } catch (e) {
                    assert.equals(e.message, "spy cannot yield to 'success' since it was not yet invoked.");
                }
            },

            "includes spy name in error message": function () {
                var api = { someMethod: function () {} };
                var spy = sinon.spy(api, "someMethod");
                var thisObj = { name1: "value1", name2: "value2" };

                try {
                    spy.yieldToOn("success", thisObj);
                    throw new Error();
                } catch (e) {
                    assert.equals(e.message, "someMethod cannot yield to 'success' since it was not yet invoked.");
                }
            },

            "pass additional arguments": function () {
                var spy = sinon.spy();
                var callback = sinon.spy();
                var array = [];
                var object = {};
                var thisObj = { name1: "value1", name2: "value2" };
                spy({ test: callback });

                spy.yieldToOn("test", thisObj, "abc", 123, array, object);

                assert(callback.calledWith("abc", 123, array, object));
                assert(callback.calledOn(thisObj));
            }
        }
    });
}());
