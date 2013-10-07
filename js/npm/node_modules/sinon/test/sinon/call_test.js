/*jslint onevar: false, eqeqeq: false*/
/*globals window sinon buster*/
/**
 * @author Christian Johansen (christian@cjohansen.no)
 * @author Maximilian Antoni (mail@maxantoni.de)
 * @license BSD
 *
 * Copyright (c) 2010-2013 Christian Johansen
 * Copyright (c) 2013 Maximilian Antoni
 */
"use strict";

if (typeof require === "function" && typeof module === "object") {
    var buster = require("../runner");
    var sinon = require("../../lib/sinon");
}

(function () {

    function spyCallCalledTests(method) {
        return {
            setUp: spyCallSetUp,

            "returns true if all args match": function () {
                var args = this.args;

                assert(this.call[method](args[0], args[1], args[2]));
            },

            "returns true if first args match": function () {
                var args = this.args;

                assert(this.call[method](args[0], args[1]));
            },

            "returns true if first arg match": function () {
                var args = this.args;

                assert(this.call[method](args[0]));
            },

            "returns true for no args": function () {
                assert(this.call[method]());
            },

            "returns false for too many args": function () {
                var args = this.args;

                assert.isFalse(this.call[method](args[0], args[1], args[2], args[3], {}));
            },

            "returns false for wrong arg": function () {
                var args = this.args;

                assert.isFalse(this.call[method](args[0], args[2]));
            }
        };
    }

    function spyCallNotCalledTests(method) {
        return {
            setUp: spyCallSetUp,

            "returns false if all args match": function () {
                var args = this.args;

                assert.isFalse(this.call[method](args[0], args[1], args[2]));
            },

            "returns false if first args match": function () {
                var args = this.args;

                assert.isFalse(this.call[method](args[0], args[1]));
            },

            "returns false if first arg match": function () {
                var args = this.args;

                assert.isFalse(this.call[method](args[0]));
            },

            "returns false for no args": function () {
                assert.isFalse(this.call[method]());
            },

            "returns true for too many args": function () {
                var args = this.args;

                assert(this.call[method](args[0], args[1], args[2], args[3], {}));
            },

            "returns true for wrong arg": function () {
                var args = this.args;

                assert(this.call[method](args[0], args[2]));
            }
        };
    }

    function spyCallSetUp() {
        this.thisValue = {};
        this.args = [{}, [], new Error(), 3];
        this.returnValue = function () {};
        this.call = sinon.spyCall(function () {}, this.thisValue,
            this.args, this.returnValue, null, 0);
    }

    function spyCallCallSetup() {
        this.args = [];
        this.proxy = sinon.spy();
        this.call = sinon.spyCall(this.proxy, {}, this.args, null, null, 0);
    }

    buster.testCase("sinon.spy.call", {

        "callObject": {
            setUp: spyCallSetUp,

            "gets call object": function () {
                var spy = sinon.spy.create();
                spy();
                var firstCall = spy.getCall(0);

                assert.isFunction(firstCall.calledOn);
                assert.isFunction(firstCall.calledWith);
                assert.isFunction(firstCall.returned);
            },

            "stores given call id": function () {
                var call = sinon.spyCall(function () {}, {}, [], null, null, 42);

                assert.same(call.callId, 42);
            },

            "throws if callId is undefined": function () {
                assert.exception(function () {
                    sinon.spyCall.create(function () {}, {}, []);
                });
            },

            // This is actually a spy test:
            "records ascending call id's": function () {
                var spy = sinon.spy();
                spy();

                assert(this.call.callId < spy.getCall(0).callId);
            },

            "exposes thisValue property": function () {
                var spy = sinon.spy();
                var obj = {};
                spy.call(obj);

                assert.same(spy.getCall(0).thisValue, obj);
            }
        },

        "callCalledOn": {
            setUp: spyCallSetUp,

            "calledOn should return true": function () {
                assert(this.call.calledOn(this.thisValue));
            },

            "calledOn should return false": function () {
                assert.isFalse(this.call.calledOn({}));
            }
        },

        "call.calledWith": spyCallCalledTests("calledWith"),
        "call.calledWithMatch": spyCallCalledTests("calledWithMatch"),
        "call.notCalledWith": spyCallNotCalledTests("notCalledWith"),
        "call.notCalledWithMatch": spyCallNotCalledTests("notCalledWithMatch"),

        "call.calledWithExactly": {
            setUp: spyCallSetUp,

            "returns true when all args match": function () {
                var args = this.args;

                assert(this.call.calledWithExactly(args[0], args[1], args[2], args[3]));
            },

            "returns false for too many args": function () {
                var args = this.args;

                assert.isFalse(this.call.calledWithExactly(args[0], args[1], args[2], args[3], {}));
            },

            "returns false for too few args": function () {
                var args = this.args;

                assert.isFalse(this.call.calledWithExactly(args[0], args[1]));
            },

            "returns false for unmatching args": function () {
                var args = this.args;

                assert.isFalse(this.call.calledWithExactly(args[0], args[1], args[1]));
            },

            "returns true for no arguments": function () {
                var call = sinon.spyCall(function () {}, {}, [], null, null, 0);

                assert(call.calledWithExactly());
            },

            "returns false when called with no args but matching one": function () {
                var call = sinon.spyCall(function () {}, {}, [], null, null, 0);

                assert.isFalse(call.calledWithExactly({}));
            }
        },

        "call.callArg": {
            setUp: spyCallCallSetup,

            "calls argument at specified index": function () {
                var callback = sinon.spy();
                this.args.push(1, 2, callback);

                this.call.callArg(2);

                assert(callback.called);
            },

            "throws if argument at specified index is not callable": function () {
                this.args.push(1);
                var call = this.call;

                assert.exception(function () {
                    call.callArg(0);
                }, "TypeError");
            },

            "throws if no index is specified": function () {
                var call = this.call;

                assert.exception(function () {
                    call.callArg();
                }, "TypeError");
            },

            "throws if index is not number": function () {
                var call = this.call;

                assert.exception(function () {
                    call.callArg({});
                }, "TypeError");
            }
        },

        "call.callArgOn": {
            setUp: spyCallCallSetup,

            "calls argument at specified index": function () {
                var callback = sinon.spy();
                var thisObj = { name1: "value1", name2: "value2" };
                this.args.push(1, 2, callback);

                this.call.callArgOn(2, thisObj);

                assert(callback.called);
                assert(callback.calledOn(thisObj));
            },

            "throws if argument at specified index is not callable": function () {
                var thisObj = { name1: "value1", name2: "value2" };
                this.args.push(1);
                var call = this.call;

                assert.exception(function () {
                    call.callArgOn(0, thisObj);
                }, "TypeError");
            },

            "throws if index is not number": function () {
                var thisObj = { name1: "value1", name2: "value2" };
                var call = this.call;

                assert.exception(function () {
                    call.callArgOn({}, thisObj);
                }, "TypeError");
            }
        },

        "call.callArgWith": {
            setUp: spyCallCallSetup,

            "calls argument at specified index with provided args": function () {
                var object = {};
                var callback = sinon.spy();
                this.args.push(1, callback);

                this.call.callArgWith(1, object);

                assert(callback.calledWith(object));
            },

            "calls callback without args": function () {
                var callback = sinon.spy();
                this.args.push(1, callback);

                this.call.callArgWith(1);

                assert(callback.calledWith());
            },

            "calls callback wit multiple args": function () {
                var object = {};
                var array = [];
                var callback = sinon.spy();
                this.args.push(1, 2, callback);

                this.call.callArgWith(2, object, array);

                assert(callback.calledWith(object, array));
            },

            "throws if no index is specified": function () {
                var call = this.call;

                assert.exception(function () {
                    call.callArgWith();
                }, "TypeError");
            },

            "throws if index is not number": function () {
                var call = this.call;

                assert.exception(function () {
                    call.callArgWith({});
                }, "TypeError");
            }
        },

        "call.callArgOnWith": {
            setUp: spyCallCallSetup,

            "calls argument at specified index with provided args": function () {
                var object = {};
                var thisObj = { name1: "value1", name2: "value2" };
                var callback = sinon.spy();
                this.args.push(1, callback);

                this.call.callArgOnWith(1, thisObj, object);

                assert(callback.calledWith(object));
                assert(callback.calledOn(thisObj));
            },

            "calls callback without args": function () {
                var callback = sinon.spy();
                var thisObj = { name1: "value1", name2: "value2" };
                this.args.push(1, callback);

                this.call.callArgOnWith(1, thisObj);

                assert(callback.calledWith());
                assert(callback.calledOn(thisObj));
            },

            "calls callback with multiple args": function () {
                var object = {};
                var array = [];
                var thisObj = { name1: "value1", name2: "value2" };
                var callback = sinon.spy();
                this.args.push(1, 2, callback);

                this.call.callArgOnWith(2, thisObj, object, array);

                assert(callback.calledWith(object, array));
                assert(callback.calledOn(thisObj));
            },

            "throws if index is not number": function () {
                var thisObj = { name1: "value1", name2: "value2" };
                var call = this.call;

                assert.exception(function () {
                    call.callArgOnWith({}, thisObj);
                }, "TypeError");
            }
        },

        "call.yieldTest": {
            setUp: spyCallCallSetup,

            "invokes only argument as callback": function () {
                var callback = sinon.spy();
                this.args.push(callback);

                this.call.yield();

                assert(callback.calledOnce);
                assert.equals(callback.args[0].length, 0);
            },

            "throws understandable error if no callback is passed": function () {
                var call = this.call;

                try {
                    call.yield();
                    throw new Error();
                } catch (e) {
                    assert.equals(e.message, "spy cannot yield since no callback was passed.");
                }
            },

            "includes stub name and actual arguments in error": function () {
                this.proxy.displayName = "somethingAwesome";
                this.args.push(23, 42);
                var call = this.call;

                try {
                    call.yield();
                    throw new Error();
                } catch (e) {
                    assert.equals(e.message, "somethingAwesome cannot yield since no callback was passed. " +
                                  "Received [23, 42]");
                }
            },

            "invokes last argument as callback": function () {
                var spy = sinon.spy();
                this.args.push(24, {}, spy);

                this.call.yield();

                assert(spy.calledOnce);
                assert.equals(spy.args[0].length, 0);
            },

            "invokes first of two callbacks": function () {
                var spy = sinon.spy();
                var spy2 = sinon.spy();
                this.args.push(24, {}, spy, spy2);

                this.call.yield();

                assert(spy.calledOnce);
                assert.isFalse(spy2.called);
            },

            "invokes callback with arguments": function () {
                var obj = { id: 42 };
                var spy = sinon.spy();
                this.args.push(spy);

                this.call.yield(obj, "Crazy");

                assert(spy.calledWith(obj, "Crazy"));
            },

            "throws if callback throws": function () {
                this.args.push(function () {
                    throw new Error("d'oh!");
                });
                var call = this.call;

                assert.exception(function () {
                    call.yield();
                });
            }
        },

        "call.invokeCallback": {

            "is alias for yield": function () {
                var call = sinon.spyCall(function () {}, {}, [], null, null, 0);

                assert.same(call.yield, call.invokeCallback);
            }

        },

        "call.yieldOnTest": {
            setUp: spyCallCallSetup,

            "invokes only argument as callback": function () {
                var callback = sinon.spy();
                var thisObj = { name1: "value1", name2: "value2" };
                this.args.push(callback);

                this.call.yieldOn(thisObj);

                assert(callback.calledOnce);
                assert(callback.calledOn(thisObj));
                assert.equals(callback.args[0].length, 0);
            },

            "throws understandable error if no callback is passed": function () {
                var call = this.call;
                var thisObj = { name1: "value1", name2: "value2" };

                try {
                    call.yieldOn(thisObj);
                    throw new Error();
                } catch (e) {
                    assert.equals(e.message, "spy cannot yield since no callback was passed.");
                }
            },

            "includes stub name and actual arguments in error": function () {
                this.proxy.displayName = "somethingAwesome";
                this.args.push(23, 42);
                var call = this.call;
                var thisObj = { name1: "value1", name2: "value2" };

                try {
                    call.yieldOn(thisObj);
                    throw new Error();
                } catch (e) {
                    assert.equals(e.message, "somethingAwesome cannot yield since no callback was passed. " +
                                  "Received [23, 42]");
                }
            },

            "invokes last argument as callback": function () {
                var spy = sinon.spy();
                var thisObj = { name1: "value1", name2: "value2" };
                this.args.push(24, {}, spy);

                this.call.yieldOn(thisObj);

                assert(spy.calledOnce);
                assert.equals(spy.args[0].length, 0);
                assert(spy.calledOn(thisObj));
            },

            "invokes first of two callbacks": function () {
                var spy = sinon.spy();
                var spy2 = sinon.spy();
                var thisObj = { name1: "value1", name2: "value2" };
                this.args.push(24, {}, spy, spy2);

                this.call.yieldOn(thisObj);

                assert(spy.calledOnce);
                assert(spy.calledOn(thisObj));
                assert.isFalse(spy2.called);
            },

            "invokes callback with arguments": function () {
                var obj = { id: 42 };
                var spy = sinon.spy();
                var thisObj = { name1: "value1", name2: "value2" };
                this.args.push(spy);

                this.call.yieldOn(thisObj, obj, "Crazy");

                assert(spy.calledWith(obj, "Crazy"));
                assert(spy.calledOn(thisObj));
            },

            "throws if callback throws": function () {
                this.args.push(function () {
                    throw new Error("d'oh!");
                });
                var call = this.call;
                var thisObj = { name1: "value1", name2: "value2" };

                assert.exception(function () {
                    call.yieldOn(thisObj);
                });
            }
        },

        "call.yieldTo": {
            setUp: spyCallCallSetup,

            "invokes only argument as callback": function () {
                var callback = sinon.spy();
                this.args.push({
                    success: callback
                });

                this.call.yieldTo("success");

                assert(callback.calledOnce);
                assert.equals(callback.args[0].length, 0);
            },

            "throws understandable error if no callback is passed": function () {
                var call = this.call;

                try {
                    call.yieldTo("success");
                    throw new Error();
                } catch (e) {
                    assert.equals(e.message, "spy cannot yield to 'success' since no callback was passed.");
                }
            },

            "includes stub name and actual arguments in error": function () {
                this.proxy.displayName = "somethingAwesome";
                this.args.push(23, 42);
                var call = this.call;

                try {
                    call.yieldTo("success");
                    throw new Error();
                } catch (e) {
                    assert.equals(e.message, "somethingAwesome cannot yield to 'success' since no callback was passed. " +
                                  "Received [23, 42]");
                }
            },

            "invokes property on last argument as callback": function () {
                var spy = sinon.spy();
                this.args.push(24, {}, { success: spy });

                this.call.yieldTo("success");

                assert(spy.calledOnce);
                assert.equals(spy.args[0].length, 0);
            },

            "invokes first of two possible callbacks": function () {
                var spy = sinon.spy();
                var spy2 = sinon.spy();
                this.args.push(24, {}, { error: spy }, { error: spy2 });

                this.call.yieldTo("error");

                assert(spy.calledOnce);
                assert.isFalse(spy2.called);
            },

            "invokes callback with arguments": function () {
                var obj = { id: 42 };
                var spy = sinon.spy();
                this.args.push({ success: spy });

                this.call.yieldTo("success", obj, "Crazy");

                assert(spy.calledWith(obj, "Crazy"));
            },

            "throws if callback throws": function () {
                this.args.push({
                    success: function () {
                        throw new Error("d'oh!");
                    }
                });
                var call = this.call;

                assert.exception(function () {
                    call.yieldTo("success");
                });
            }
        },

        "call.yieldToOn": {
            setUp: spyCallCallSetup,

            "invokes only argument as callback": function () {
                var callback = sinon.spy();
                var thisObj = { name1: "value1", name2: "value2" };
                this.args.push({
                    success: callback
                });

                this.call.yieldToOn("success", thisObj);

                assert(callback.calledOnce);
                assert.equals(callback.args[0].length, 0);
                assert(callback.calledOn(thisObj));
            },

            "throws understandable error if no callback is passed": function () {
                var call = this.call;
                var thisObj = { name1: "value1", name2: "value2" };

                try {
                    call.yieldToOn("success", thisObj);
                    throw new Error();
                } catch (e) {
                    assert.equals(e.message, "spy cannot yield to 'success' since no callback was passed.");
                }
            },

            "includes stub name and actual arguments in error": function () {
                this.proxy.displayName = "somethingAwesome";
                this.args.push(23, 42);
                var call = this.call;
                var thisObj = { name1: "value1", name2: "value2" };

                try {
                    call.yieldToOn("success", thisObj);
                    throw new Error();
                } catch (e) {
                    assert.equals(e.message, "somethingAwesome cannot yield to 'success' since no callback was passed. " +
                                  "Received [23, 42]");
                }
            },

            "invokes property on last argument as callback": function () {
                var spy = sinon.spy();
                var thisObj = { name1: "value1", name2: "value2" };
                this.args.push(24, {}, { success: spy });

                this.call.yieldToOn("success", thisObj);

                assert(spy.calledOnce);
                assert(spy.calledOn(thisObj));
                assert.equals(spy.args[0].length, 0);
            },

            "invokes first of two possible callbacks": function () {
                var spy = sinon.spy();
                var spy2 = sinon.spy();
                var thisObj = { name1: "value1", name2: "value2" };
                this.args.push(24, {}, { error: spy }, { error: spy2 });

                this.call.yieldToOn("error", thisObj);

                assert(spy.calledOnce);
                assert(spy.calledOn(thisObj));
                assert.isFalse(spy2.called);
            },

            "invokes callback with arguments": function () {
                var obj = { id: 42 };
                var spy = sinon.spy();
                var thisObj = { name1: "value1", name2: "value2" };
                this.args.push({ success: spy });

                this.call.yieldToOn("success", thisObj, obj, "Crazy");

                assert(spy.calledWith(obj, "Crazy"));
                assert(spy.calledOn(thisObj));
            },

            "throws if callback throws": function () {
                this.args.push({
                    success: function () {
                        throw new Error("d'oh!");
                    }
                });
                var call = this.call;
                var thisObj = { name1: "value1", name2: "value2" };

                assert.exception(function () {
                    call.yieldToOn("success", thisObj);
                });
            }
        },

        "call.toString": {
            setUp: function () {
                this.format = sinon.format;
            },

            tearDown: function () {
                sinon.format = this.format;
            },

            "includes spy name": function () {
                var object = { doIt: sinon.spy() };
                object.doIt();

                assert.equals(object.doIt.getCall(0).toString(), "doIt()");
            },

            "includes single argument": function () {
                var object = { doIt: sinon.spy() };
                object.doIt(42);

                assert.equals(object.doIt.getCall(0).toString(), "doIt(42)");
            },

            "includes all arguments": function () {
                var object = { doIt: sinon.spy() };
                object.doIt(42, "Hey");

                assert.equals(object.doIt.getCall(0).toString(), "doIt(42, Hey)");
            },

            "includes explicit return value": function () {
                var object = { doIt: sinon.stub().returns(42) };
                object.doIt(42, "Hey");

                assert.equals(object.doIt.getCall(0).toString(), "doIt(42, Hey) => 42");
            },

            "includes empty string return value": function () {
                var object = { doIt: sinon.stub().returns("") };
                object.doIt(42, "Hey");

                assert.equals(object.doIt.getCall(0).toString(), "doIt(42, Hey) => ");
            },

            "includes exception": function () {
                var object = { doIt: sinon.stub().throws("TypeError") };

                try {
                    object.doIt();
                } catch (e) {}

                assert.equals(object.doIt.getCall(0).toString(), "doIt() !TypeError");
            },

            "includes exception message if any": function () {
                var object = { doIt: sinon.stub().throws("TypeError", "Oh noes!") };

                try {
                    object.doIt();
                } catch (e) {}

                assert.equals(object.doIt.getCall(0).toString(), "doIt() !TypeError(Oh noes!)");
            },

            "formats arguments with sinon.format": function () {
                sinon.format = sinon.stub().returns("Forty-two");
                var object = { doIt: sinon.spy() };

                object.doIt(42);

                assert.equals(object.doIt.getCall(0).toString(), "doIt(Forty-two)");
                assert(sinon.format.calledWith(42));
            },

            "formats return value with sinon.format": function () {
                sinon.format = sinon.stub().returns("Forty-two");
                var object = { doIt: sinon.stub().returns(42) };

                object.doIt();

                assert.equals(object.doIt.getCall(0).toString(), "doIt() => Forty-two");
                assert(sinon.format.calledWith(42));
            }
        },

        "constructor": {
            setUp: function () {
                this.CustomConstructor = function () {};
                this.customPrototype = this.CustomConstructor.prototype;
                this.StubConstructor = sinon.spy(this, "CustomConstructor");
            },

            "creates original object": function () {
                var myInstance = new this.CustomConstructor();

                assert(this.customPrototype.isPrototypeOf(myInstance));
            },

            "does not interfere with instanceof": function () {
                var myInstance = new this.CustomConstructor();

                assert(myInstance instanceof this.CustomConstructor);
            },

            "records usage": function () {
                var myInstance = new this.CustomConstructor();

                assert(this.CustomConstructor.called);
            }
        },

        "function": {
            "throws if spying on non-existent property": function () {
                var myObj = {};

                assert.exception(function () {
                    sinon.spy(myObj, "ouch");
                });

                refute.defined(myObj.ouch);
            },

            "throws if spying on non-existent object": function () {
                assert.exception(function () {
                    sinon.spy(undefined, "ouch");
                });
            },

            "haves toString method": function () {
                var obj = { meth: function () {} };
                sinon.spy(obj, "meth");

                assert.equals(obj.meth.toString(), "meth");
            },

            "toString should say 'spy' when unable to infer name": function () {
                var spy = sinon.spy();

                assert.equals(spy.toString(), "spy");
            },

            "toString should report name of spied function": function () {
                function myTestFunc() {}
                var spy = sinon.spy(myTestFunc);

                assert.equals(spy.toString(), "myTestFunc");
            },

            "toString should prefer displayName property if available": function () {
                function myTestFunc() {}
                myTestFunc.displayName = "My custom method";
                var spy = sinon.spy(myTestFunc);

                assert.equals(spy.toString(), "My custom method");
            },

            "toString should prefer property name if possible": function () {
                var obj = {};
                obj.meth = sinon.spy();
                obj.meth();

                assert.equals(obj.meth.toString(), "meth");
            }
        },

        "reset": (function () {
            function assertReset(spy) {
                assert(!spy.called);
                assert(!spy.calledOnce);
                assert.equals(spy.args.length, 0);
                assert.equals(spy.returnValues.length, 0);
                assert.equals(spy.exceptions.length, 0);
                assert.equals(spy.thisValues.length, 0);
                assert.isNull(spy.firstCall);
                assert.isNull(spy.secondCall);
                assert.isNull(spy.thirdCall);
                assert.isNull(spy.lastCall);
            }

            return {
                "resets spy state": function () {
                    var spy = sinon.spy();
                    spy();

                    spy.reset();

                    assertReset(spy);
                },

                "resets call order state": function () {
                    var spies = [sinon.spy(), sinon.spy()];
                    spies[0]();
                    spies[1]();

                    spies[0].reset();

                    assert(!spies[0].calledBefore(spies[1]));
                },

                "resets fakes returned by withArgs": function () {
                    var spy = sinon.spy();
                    var fakeA = spy.withArgs("a");
                    var fakeB = spy.withArgs("b");
                    spy("a");
                    spy("b");
                    spy("c");
                    var fakeC = spy.withArgs("c");

                    spy.reset();

                    assertReset(fakeA);
                    assertReset(fakeB);
                    assertReset(fakeC);
                }
            }
        }()),

        "withArgs": {
            "defines withArgs method": function () {
                var spy = sinon.spy();

                assert.isFunction(spy.withArgs);
            },

            "records single call": function () {
                var spy = sinon.spy().withArgs(1);
                spy(1);

                assert.equals(spy.callCount, 1);
            },

            "records non-matching call on original spy": function () {
                var spy = sinon.spy();
                var argSpy = spy.withArgs(1);
                spy(1);
                spy(2);

                assert.equals(spy.callCount, 2);
                assert.equals(argSpy.callCount, 1);
            },

            "records non-matching call with several arguments separately": function () {
                var spy = sinon.spy();
                var argSpy = spy.withArgs(1, "str", {});
                spy(1);
                spy(1, "str", {});

                assert.equals(spy.callCount, 2);
                assert.equals(argSpy.callCount, 1);
            },

            "records for partial argument match": function () {
                var spy = sinon.spy();
                var argSpy = spy.withArgs(1, "str", {});
                spy(1);
                spy(1, "str", {});
                spy(1, "str", {}, []);

                assert.equals(spy.callCount, 3);
                assert.equals(argSpy.callCount, 2);
            },

            "records filtered spy when original throws": function () {
                var spy = sinon.spy(function () {
                    throw new Error("Oops");
                });

                var argSpy = spy.withArgs({}, []);

                assert.exception(function () {
                    spy(1);
                });

                assert.exception(function () {
                    spy({}, []);
                });

                assert.equals(spy.callCount, 2);
                assert.equals(argSpy.callCount, 1);
            },

            "returns existing override for arguments": function () {
                var spy = sinon.spy();
                var argSpy = spy.withArgs({}, []);
                var another = spy.withArgs({}, []);
                spy();
                spy({}, []);
                spy({}, [], 2);

                assert.same(another, argSpy);
                refute.same(another, spy);
                assert.equals(spy.callCount, 3);
                assert.equals(spy.withArgs({}, []).callCount, 2);
            },

            "chains withArgs calls on original spy": function () {
                var spy = sinon.spy();
                var numArgSpy = spy.withArgs({}, []).withArgs(3);
                spy();
                spy({}, []);
                spy(3);

                assert.equals(spy.callCount, 3);
                assert.equals(numArgSpy.callCount, 1);
                assert.equals(spy.withArgs({}, []).callCount, 1);
            },

            "initializes filtered spy with callCount": function () {
                var spy = sinon.spy();
                spy("a");
                spy("b");
                spy("b");
                spy("c");
                spy("c");
                spy("c");

                var argSpy1 = spy.withArgs("a");
                var argSpy2 = spy.withArgs("b");
                var argSpy3 = spy.withArgs("c");

                assert.equals(argSpy1.callCount, 1);
                assert.equals(argSpy2.callCount, 2);
                assert.equals(argSpy3.callCount, 3);
                assert(argSpy1.called);
                assert(argSpy2.called);
                assert(argSpy3.called);
                assert(argSpy1.calledOnce);
                assert(argSpy2.calledTwice);
                assert(argSpy3.calledThrice);
            },

            "initializes filtered spy with first, second, third and last call": function () {
                var spy = sinon.spy();
                spy("a", 1);
                spy("b", 2);
                spy("b", 3);
                spy("b", 4);

                var argSpy1 = spy.withArgs("a");
                var argSpy2 = spy.withArgs("b");

                assert(argSpy1.firstCall.calledWithExactly("a", 1));
                assert(argSpy1.lastCall.calledWithExactly("a", 1));
                assert(argSpy2.firstCall.calledWithExactly("b", 2));
                assert(argSpy2.secondCall.calledWithExactly("b", 3));
                assert(argSpy2.thirdCall.calledWithExactly("b", 4));
                assert(argSpy2.lastCall.calledWithExactly("b", 4));
            },

            "initializes filtered spy with arguments": function () {
                var spy = sinon.spy();
                spy("a");
                spy("b");
                spy("b", "c", "d");

                var argSpy1 = spy.withArgs("a");
                var argSpy2 = spy.withArgs("b");

                assert(argSpy1.getCall(0).calledWithExactly("a"));
                assert(argSpy2.getCall(0).calledWithExactly("b"));
                assert(argSpy2.getCall(1).calledWithExactly("b", "c", "d"));
            },

            "initializes filtered spy with thisValues": function () {
                var spy = sinon.spy();
                var thisValue1 = {};
                var thisValue2 = {};
                var thisValue3 = {};
                spy.call(thisValue1, "a");
                spy.call(thisValue2, "b");
                spy.call(thisValue3, "b");

                var argSpy1 = spy.withArgs("a");
                var argSpy2 = spy.withArgs("b");

                assert(argSpy1.getCall(0).calledOn(thisValue1));
                assert(argSpy2.getCall(0).calledOn(thisValue2));
                assert(argSpy2.getCall(1).calledOn(thisValue3));
            },

            "initializes filtered spy with return values": function () {
                var spy = sinon.spy(function (value) { return value; });
                spy("a");
                spy("b");
                spy("b");

                var argSpy1 = spy.withArgs("a");
                var argSpy2 = spy.withArgs("b");

                assert(argSpy1.getCall(0).returned("a"));
                assert(argSpy2.getCall(0).returned("b"));
                assert(argSpy2.getCall(1).returned("b"));
            },

            "initializes filtered spy with call order": function () {
                var spy = sinon.spy();
                spy("a");
                spy("b");
                spy("b");

                var argSpy1 = spy.withArgs("a");
                var argSpy2 = spy.withArgs("b");

                assert(argSpy2.getCall(0).calledAfter(argSpy1.getCall(0)));
                assert(argSpy2.getCall(1).calledAfter(argSpy1.getCall(0)));
            },

            "initializes filtered spy with exceptions": function () {
                var spy = sinon.spy(function (x, y) {
                    var error = new Error();
                    error.name = y;
                    throw error;
                });
                try {
                    spy("a", "1");
                } catch (ignored) {}
                try {
                    spy("b", "2");
                } catch (ignored) {}
                try {
                    spy("b", "3");
                } catch (ignored) {}

                var argSpy1 = spy.withArgs("a");
                var argSpy2 = spy.withArgs("b");

                assert(argSpy1.getCall(0).threw("1"));
                assert(argSpy2.getCall(0).threw("2"));
                assert(argSpy2.getCall(1).threw("3"));
            }
        },
        "printf" : {
            "name" : {
                "named" : function () {
                    var named = sinon.spy(function cool() { });
                    assert.equals(named.printf('%n'), 'cool');
                },
                "anon" : function () {
                    var anon = sinon.spy(function () {});
                    assert.equals(anon.printf('%n'), 'spy');

                    var noFn = sinon.spy();
                    assert.equals(noFn.printf('%n'), 'spy');
                },
            },
            "count" : function () {
                // Throwing just to make sure it has no effect.
                var spy = sinon.spy(sinon.stub().throws());
                function call() {
                    try {
                        spy();
                    } catch (e) {}
                }

                call();
                assert.equals(spy.printf('%c'), 'once');
                call();
                assert.equals(spy.printf('%c'), 'twice');
                call();
                assert.equals(spy.printf('%c'), 'thrice');
                call();
                assert.equals(spy.printf('%c'), '4 times');
            },
            "calls" : {
                oneLine : function () {
                    function test(arg, expected) {
                        var spy = sinon.spy();
                        spy(arg);
                        assert.equals(spy.printf('%C'), '\n    ' + expected);
                    }

                    test(true, 'spy(true)');
                    test(false, 'spy(false)');
                    test(undefined, 'spy(undefined)');
                    test(1, 'spy(1)');
                    test(0, 'spy(0)');
                    test(-1, 'spy(-1)');
                    test(-1.1, 'spy(-1.1)');
                    test(Infinity, 'spy(Infinity)');
                    test(['a'], 'spy(["a"])');
                    test({ a : 'a' }, 'spy({ a: "a" })');
                },
                multiline : function () {
                    var str = 'spy\ntest';
                    var spy = sinon.spy();

                    spy(str);
                    spy(str);
                    spy(str);

                    assert.equals(spy.printf('%C'), 
                        '\n    spy(' + str + ')' +
                        '\n\n    spy(' + str + ')' +
                        '\n\n    spy(' + str + ')');

                    spy.reset();

                    spy('test');
                    spy('spy\ntest');
                    spy('spy\ntest');

                    assert.equals(spy.printf('%C'),
                        '\n    spy(test)' +
                        '\n    spy(' + str + ')' +
                        '\n\n    spy(' + str + ')');
                }
            },
            "thisValues" : function () {
                var spy = sinon.spy();
                spy();
                assert.equals(spy.printf('%t'), 'undefined');

                spy.reset();
                spy.call(true);
                assert.equals(spy.printf('%t'), 'true');
            }
        }
    });
}());

