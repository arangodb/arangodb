/*jslint onevar: false*/
/*globals sinon buster*/
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

buster.testCase("sinon.stub", {
    "is spy": function () {
        var stub = sinon.stub.create();

        assert.isFalse(stub.called);
        assert.isFunction(stub.calledWith);
        assert.isFunction(stub.calledOn);
    },

    "returns": {
        "returns specified value": function () {
            var stub = sinon.stub.create();
            var object = {};
            stub.returns(object);

            assert.same(stub(), object);
        },

        "returns should return stub": function () {
            var stub = sinon.stub.create();

            assert.same(stub.returns(""), stub);
        },

        "returns undefined": function () {
            var stub = sinon.stub.create();

            refute.defined(stub());
        }
    },

    "returnsArg": {
        "returns argument at specified index": function() {
            var stub = sinon.stub.create();
            stub.returnsArg(0);
            var object = {};

            assert.same(stub(object), object);
        },

        "returns stub": function () {
            var stub = sinon.stub.create();

            assert.same(stub.returnsArg(0), stub);
        },

        "throws if no index is specified": function () {
            var stub = sinon.stub.create();

            assert.exception(function () {
                stub.returnsArg();
            }, "TypeError");
        },

        "throws if index is not number": function () {
            var stub = sinon.stub.create();

            assert.exception(function () {
                stub.returnsArg({});
            }, "TypeError");
        }
    },

    "returnsThis": {
        "stub returns this": function () {
            var instance = {};
            instance.stub = sinon.stub.create();
            instance.stub.returnsThis();

            assert.same(instance.stub(), instance);
        },

        "stub returns undefined when detached": function () {
            var stub = sinon.stub.create();
            stub.returnsThis();

            // Due to strict mode, would be `global` otherwise
            assert.same(stub(), undefined);
        },

        "stub respects call/apply": function() {
            var stub = sinon.stub.create();
            stub.returnsThis();
            var object = {};

            assert.same(stub.call(object), object);
            assert.same(stub.apply(object), object);
        },

        "returns stub": function () {
            var stub = sinon.stub.create();

            assert.same(stub.returnsThis(), stub);
        }
    },

    "throws": {
        "throws specified exception": function () {
            var stub = sinon.stub.create();
            var error = new Error();
            stub.throws(error);

            try {
                stub();
                fail("Expected stub to throw");
            } catch (e) {
                assert.same(e, error);
            }
        },

        "returns stub": function () {
            var stub = sinon.stub.create();

            assert.same(stub.throws({}), stub);
        },

        "sets type of exception to throw": function () {
            var stub = sinon.stub.create();
            var exceptionType = "TypeError";
            stub.throws(exceptionType);

            assert.exception(function () {
                stub();
            }, exceptionType);
        },

        "specifies exception message": function () {
            var stub = sinon.stub.create();
            var message = "Oh no!";
            stub.throws("Error", message);

            try {
                stub();
                buster.assertions.fail("Expected stub to throw");
            } catch (e) {
                assert.equals(e.message, message);
            }
        },

        "does not specify exception message if not provided": function () {
            var stub = sinon.stub.create();
            stub.throws("Error");

            try {
                stub();
                buster.assertions.fail("Expected stub to throw");
            } catch (e) {
                assert.equals(e.message, "");
            }
        },

        "throws generic error": function () {
            var stub = sinon.stub.create();
            stub.throws();

            assert.exception(function () {
                stub();
            }, "Error");
        }
    },

    "callsArg": {
        setUp: function () {
            this.stub = sinon.stub.create();
        },

        "calls argument at specified index": function () {
            this.stub.callsArg(2);
            var callback = sinon.stub.create();

            this.stub(1, 2, callback);

            assert(callback.called);
        },

        "returns stub": function () {
            assert.isFunction(this.stub.callsArg(2));
        },

        "throws if argument at specified index is not callable": function () {
            this.stub.callsArg(0);

            assert.exception(function () {
                this.stub(1);
            }, "TypeError");
        },

        "throws if no index is specified": function () {
            var stub = this.stub;

            assert.exception(function () {
                stub.callsArg();
            }, "TypeError");
        },

        "throws if index is not number": function () {
            var stub = this.stub;

            assert.exception(function () {
                stub.callsArg({});
            }, "TypeError");
        }
    },

    "callsArgWith": {
        setUp: function () {
            this.stub = sinon.stub.create();
        },

        "calls argument at specified index with provided args": function () {
            var object = {};
            this.stub.callsArgWith(1, object);
            var callback = sinon.stub.create();

            this.stub(1, callback);

            assert(callback.calledWith(object));
        },

        "returns function": function () {
            var stub = this.stub.callsArgWith(2, 3);

            assert.isFunction(stub);
        },

        "calls callback without args": function () {
            this.stub.callsArgWith(1);
            var callback = sinon.stub.create();

            this.stub(1, callback);

            assert(callback.calledWith());
        },

        "calls callback with multiple args": function () {
            var object = {};
            var array = [];
            this.stub.callsArgWith(1, object, array);
            var callback = sinon.stub.create();

            this.stub(1, callback);

            assert(callback.calledWith(object, array));
        },

        "throws if no index is specified": function () {
            var stub = this.stub;

            assert.exception(function () {
                stub.callsArgWith();
            }, "TypeError");
        },

        "throws if index is not number": function () {
            var stub = this.stub;

            assert.exception(function () {
                stub.callsArgWith({});
            }, "TypeError");
        }
    },

    "callsArgOn": {
        setUp: function () {
            this.stub = sinon.stub.create();
            this.fakeContext = {
                foo: "bar"
            };
        },

        "calls argument at specified index": function () {
            this.stub.callsArgOn(2, this.fakeContext);
            var callback = sinon.stub.create();

            this.stub(1, 2, callback);

            assert(callback.called);
            assert(callback.calledOn(this.fakeContext));
        },

        "returns stub": function () {
            var stub = this.stub.callsArgOn(2, this.fakeContext);

            assert.isFunction(stub);
        },

        "throws if argument at specified index is not callable": function () {
            this.stub.callsArgOn(0, this.fakeContext);

            assert.exception(function () {
                this.stub(1);
            }, "TypeError");
        },

        "throws if no index is specified": function () {
            var stub = this.stub;

            assert.exception(function () {
                stub.callsArgOn();
            }, "TypeError");
        },

        "throws if no context is specified": function () {
            var stub = this.stub;

            assert.exception(function () {
                stub.callsArgOn(3);
            }, "TypeError");
        },

        "throws if index is not number": function () {
            var stub = this.stub;

            assert.exception(function () {
                stub.callsArgOn(this.fakeContext, 2);
            }, "TypeError");
        },

        "throws if context is not an object": function () {
            var stub = this.stub;

            assert.exception(function () {
                stub.callsArgOn(2, 2);
            }, "TypeError");
        }
    },

    "callsArgOnWith": {
        setUp: function () {
            this.stub = sinon.stub.create();
            this.fakeContext = { foo: "bar" };
        },

        "calls argument at specified index with provided args": function () {
            var object = {};
            this.stub.callsArgOnWith(1, this.fakeContext, object);
            var callback = sinon.stub.create();

            this.stub(1, callback);

            assert(callback.calledWith(object));
            assert(callback.calledOn(this.fakeContext));
        },

        "returns function": function () {
            var stub = this.stub.callsArgOnWith(2, this.fakeContext, 3);

            assert.isFunction(stub);
        },

        "calls callback without args": function () {
            this.stub.callsArgOnWith(1, this.fakeContext);
            var callback = sinon.stub.create();

            this.stub(1, callback);

            assert(callback.calledWith());
            assert(callback.calledOn(this.fakeContext));
        },

        "calls callback with multiple args": function () {
            var object = {};
            var array = [];
            this.stub.callsArgOnWith(1, this.fakeContext, object, array);
            var callback = sinon.stub.create();

            this.stub(1, callback);

            assert(callback.calledWith(object, array));
            assert(callback.calledOn(this.fakeContext));
        },

        "throws if no index is specified": function () {
            var stub = this.stub;

            assert.exception(function () {
                stub.callsArgOnWith();
            }, "TypeError");
        },

        "throws if no context is specified": function () {
            var stub = this.stub;

            assert.exception(function () {
                stub.callsArgOnWith(3);
            }, "TypeError");
        },

        "throws if index is not number": function () {
            var stub = this.stub;

            assert.exception(function () {
                stub.callsArgOnWith({});
            }, "TypeError");
        },

        "throws if context is not an object": function () {
            var stub = this.stub;

            assert.exception(function () {
                stub.callsArgOnWith(2, 2);
            }, "TypeError");
        }
    },

    "objectMethod": {
        setUp: function () {
            this.method = function () {};
            this.object = { method: this.method };
            this.wrapMethod = sinon.wrapMethod;
        },

        tearDown: function () {
            sinon.wrapMethod = this.wrapMethod;
        },

        "returns function from wrapMethod": function () {
            var wrapper = function () {};
            sinon.wrapMethod = function () {
                return wrapper;
            };

            var result = sinon.stub(this.object, "method");

            assert.same(result, wrapper);
        },

        "passes object and method to wrapMethod": function () {
            var wrapper = function () {};
            var args;

            sinon.wrapMethod = function () {
                args = arguments;
                return wrapper;
            };

            var result = sinon.stub(this.object, "method");

            assert.same(args[0], this.object);
            assert.same(args[1], "method");
        },

        "uses provided function as stub": function () {
            var called = false;
            var stub = sinon.stub(this.object, "method", function () {
                called = true;
            });

            stub();

            assert(called);
        },

        "wraps provided function": function () {
            var customStub = function () {};
            var stub = sinon.stub(this.object, "method", customStub);

            refute.same(stub, customStub);
            assert.isFunction(stub.restore);
        },

        "throws if third argument is provided but not function": function () {
            var object = this.object;

            assert.exception(function () {
                sinon.stub(object, "method", {});
            }, "TypeError");
        },

        "stubbed method should be proper stub": function () {
            var stub = sinon.stub(this.object, "method");

            assert.isFunction(stub.returns);
            assert.isFunction(stub.throws);
        },

        "custom stubbed method should not be proper stub": function () {
            var stub = sinon.stub(this.object, "method", function () {});

            refute.defined(stub.returns);
            refute.defined(stub.throws);
        },

        "stub should be spy": function () {
            var stub = sinon.stub(this.object, "method");
            this.object.method();

            assert(stub.called);
            assert(stub.calledOn(this.object));
        },

        "custom stubbed method should be spy": function () {
            var stub = sinon.stub(this.object, "method", function () {});
            this.object.method();

            assert(stub.called);
            assert(stub.calledOn(this.object));
        },

        "stub should affect spy": function () {
            var stub = sinon.stub(this.object, "method");
            var someObj = {};
            stub.throws("TypeError");

            try {
                this.object.method();
            } catch (e) {}

            assert(stub.threw("TypeError"));
        },

        "returns standalone stub without arguments": function () {
            var stub = sinon.stub();

            assert.isFunction(stub);
            assert.isFalse(stub.called);
        },

        "throws if property is not a function": function () {
            var obj = { someProp: 42 };

            assert.exception(function () {
                sinon.stub(obj, "someProp");
            });

            assert.equals(obj.someProp, 42);
        },

        "does not stub function object": function () {
            assert.exception(function () {
                sinon.stub(function () {});
            });
        }
    },

    "everything": {
        "stubs all methods of object without property": function () {
            var obj = {
                func1: function () {},
                func2: function () {},
                func3: function () {}
            };

            var stub = sinon.stub(obj);

            assert.isFunction(obj.func1.restore);
            assert.isFunction(obj.func2.restore);
            assert.isFunction(obj.func3.restore);
        },

        "stubs prototype methods": function () {
            function Obj() {}
            Obj.prototype.func1 = function() {};
            var obj = new Obj();

            var stub = sinon.stub(obj);

            assert.isFunction(obj.func1.restore);
        },

        "returns object": function () {
            var object = {};

            assert.same(sinon.stub(object), object);
        },

        "only stubs functions": function () {
            var object = { foo: "bar" };
            sinon.stub(object);

            assert.equals(object.foo, "bar");
        }
    },

    "function": {
        "throws if stubbing non-existent property": function () {
            var myObj = {};

            assert.exception(function () {
                sinon.stub(myObj, "ouch");
            });

            refute.defined(myObj.ouch);
        },

        "has toString method": function () {
            var obj = { meth: function () {} };
            sinon.stub(obj, "meth");

            assert.equals(obj.meth.toString(), "meth");
        },

        "toString should say 'stub' when unable to infer name": function () {
            var stub = sinon.stub();

            assert.equals(stub.toString(), "stub");
        },

        "toString should prefer property name if possible": function () {
            var obj = {};
            obj.meth = sinon.stub();
            obj.meth();

            assert.equals(obj.meth.toString(), "meth");
        }
    },

    "yields": {
        "invokes only argument as callback": function () {
            var stub = sinon.stub().yields();
            var spy = sinon.spy();
            stub(spy);

            assert(spy.calledOnce);
            assert.equals(spy.args[0].length, 0);
        },

        "throws understandable error if no callback is passed": function () {
            var stub = sinon.stub().yields();

            try {
                stub();
                throw new Error();
            } catch (e) {
                assert.equals(e.message, "stub expected to yield, but no callback was passed.");
            }
        },

        "includes stub name and actual arguments in error": function () {
            var myObj = { somethingAwesome: function () {} };
            var stub = sinon.stub(myObj, "somethingAwesome").yields();

            try {
                stub(23, 42);
                throw new Error();
            } catch (e) {
                assert.equals(e.message, "somethingAwesome expected to yield, but no callback " +
                              "was passed. Received [23, 42]");
            }
        },

        "invokes last argument as callback": function () {
            var stub = sinon.stub().yields();
            var spy = sinon.spy();
            stub(24, {}, spy);

            assert(spy.calledOnce);
            assert.equals(spy.args[0].length, 0);
        },

        "invokes first of two callbacks": function () {
            var stub = sinon.stub().yields();
            var spy = sinon.spy();
            var spy2 = sinon.spy();
            stub(24, {}, spy, spy2);

            assert(spy.calledOnce);
            assert(!spy2.called);
        },

        "invokes callback with arguments": function () {
            var obj = { id: 42 };
            var stub = sinon.stub().yields(obj, "Crazy");
            var spy = sinon.spy();
            stub(spy);

            assert(spy.calledWith(obj, "Crazy"));
        },

        "throws if callback throws": function () {
            var obj = { id: 42 };
            var stub = sinon.stub().yields(obj, "Crazy");
            var callback = sinon.stub().throws();

            assert.exception(function () {
                stub(callback);
            });
        },

        "plays nice with throws": function () {
            var stub = sinon.stub().throws().yields();
            var spy = sinon.spy();
            assert.exception(function () {
                stub(spy);
            })
            assert(spy.calledOnce);
        },

        "plays nice with returns": function () {
            var obj = {};
            var stub = sinon.stub().returns(obj).yields();
            var spy = sinon.spy();
            assert.same(stub(spy), obj);
            assert(spy.calledOnce);
        },

        "plays nice with returnsArg": function () {
            var stub = sinon.stub().returnsArg(0).yields();
            var spy = sinon.spy();
            assert.same(stub(spy), spy);
            assert(spy.calledOnce);
        },

        "plays nice with returnsThis": function () {
            var obj = {};
            var stub = sinon.stub().returnsThis().yields();
            var spy = sinon.spy();
            assert.same(stub.call(obj, spy), obj);
            assert(spy.calledOnce);
        }
    },

    "yieldsOn": {
        setUp: function () {
            this.stub = sinon.stub.create();
            this.fakeContext = { foo: "bar" };
        },

        "invokes only argument as callback": function () {
            var spy = sinon.spy();

            this.stub.yieldsOn(this.fakeContext);
            this.stub(spy);

            assert(spy.calledOnce);
            assert(spy.calledOn(this.fakeContext));
            assert.equals(spy.args[0].length, 0);
        },

        "throws if no context is specified": function () {
            assert.exception(function () {
                this.stub.yieldsOn();
            }, "TypeError");
        },

        "throws understandable error if no callback is passed": function () {
            this.stub.yieldsOn(this.fakeContext);

            try {
                this.stub();
                throw new Error();
            } catch (e) {
                assert.equals(e.message, "stub expected to yield, but no callback was passed.");
            }
        },

        "includes stub name and actual arguments in error": function () {
            var myObj = { somethingAwesome: function () {} };
            var stub = sinon.stub(myObj, "somethingAwesome").yieldsOn(this.fakeContext);

            try {
                stub(23, 42);
                throw new Error();
            } catch (e) {
                assert.equals(e.message, "somethingAwesome expected to yield, but no callback " +
                              "was passed. Received [23, 42]");
            }
        },

        "invokes last argument as callback": function () {
            var spy = sinon.spy();
            this.stub.yieldsOn(this.fakeContext);

            this.stub(24, {}, spy);

            assert(spy.calledOnce);
            assert(spy.calledOn(this.fakeContext));
            assert.equals(spy.args[0].length, 0);
        },

        "invokes first of two callbacks": function () {
            var spy = sinon.spy();
            var spy2 = sinon.spy();

            this.stub.yieldsOn(this.fakeContext);
            this.stub(24, {}, spy, spy2);

            assert(spy.calledOnce);
            assert(spy.calledOn(this.fakeContext));
            assert(!spy2.called);
        },

        "invokes callback with arguments": function () {
            var obj = { id: 42 };
            var spy = sinon.spy();

            this.stub.yieldsOn(this.fakeContext, obj, "Crazy");
            this.stub(spy);

            assert(spy.calledWith(obj, "Crazy"));
            assert(spy.calledOn(this.fakeContext));
        },

        "throws if callback throws": function () {
            var obj = { id: 42 };
            var callback = sinon.stub().throws();

            this.stub.yieldsOn(this.fakeContext, obj, "Crazy");

            assert.exception(function () {
                this.stub(callback);
            });
        }
    },

    "yieldsTo": {
        "yields to property of object argument": function () {
            var stub = sinon.stub().yieldsTo("success");
            var callback = sinon.spy();

            stub({ success: callback });

            assert(callback.calledOnce);
            assert.equals(callback.args[0].length, 0);
        },

        "throws understandable error if no object with callback is passed": function () {
            var stub = sinon.stub().yieldsTo("success");

            try {
                stub();
                throw new Error();
            } catch (e) {
                assert.equals(e.message, "stub expected to yield to 'success', but no object "+
                              "with such a property was passed.");
            }
        },

        "includes stub name and actual arguments in error": function () {
            var myObj = { somethingAwesome: function () {} };
            var stub = sinon.stub(myObj, "somethingAwesome").yieldsTo("success");

            try {
                stub(23, 42);
                throw new Error();
            } catch (e) {
                assert.equals(e.message, "somethingAwesome expected to yield to 'success', but " +
                              "no object with such a property was passed. " +
                              "Received [23, 42]");
            }
        },

        "invokes property on last argument as callback": function () {
            var stub = sinon.stub().yieldsTo("success");
            var callback = sinon.spy();
            stub(24, {}, { success: callback });

            assert(callback.calledOnce);
            assert.equals(callback.args[0].length, 0);
        },

        "invokes first of two possible callbacks": function () {
            var stub = sinon.stub().yieldsTo("error");
            var callback = sinon.spy();
            var callback2 = sinon.spy();
            stub(24, {}, { error: callback }, { error: callback2 });

            assert(callback.calledOnce);
            assert(!callback2.called);
        },

        "invokes callback with arguments": function () {
            var obj = { id: 42 };
            var stub = sinon.stub().yieldsTo("success", obj, "Crazy");
            var callback = sinon.spy();
            stub({ success: callback });

            assert(callback.calledWith(obj, "Crazy"));
        },

        "throws if callback throws": function () {
            var obj = { id: 42 };
            var stub = sinon.stub().yieldsTo("error", obj, "Crazy");
            var callback = sinon.stub().throws();

            assert.exception(function () {
                stub({ error: callback });
            });
        }
    },

    "yieldsToOn": {
        setUp: function () {
            this.stub = sinon.stub.create();
            this.fakeContext = { foo: "bar" };
        },

        "yields to property of object argument": function () {
            this.stub.yieldsToOn("success", this.fakeContext);
            var callback = sinon.spy();

            this.stub({ success: callback });

            assert(callback.calledOnce);
            assert(callback.calledOn(this.fakeContext));
            assert.equals(callback.args[0].length, 0);
        },

        "throws if no context is specified": function () {
            assert.exception(function () {
                this.stub.yieldsToOn("success");
            }, "TypeError");
        },

        "throws understandable error if no object with callback is passed": function () {
            this.stub.yieldsToOn("success", this.fakeContext);

            try {
                this.stub();
                throw new Error();
            } catch (e) {
                assert.equals(e.message, "stub expected to yield to 'success', but no object "+
                              "with such a property was passed.");
            }
        },

        "includes stub name and actual arguments in error": function () {
            var myObj = { somethingAwesome: function () {} };
            var stub = sinon.stub(myObj, "somethingAwesome").yieldsToOn("success", this.fakeContext);

            try {
                stub(23, 42);
                throw new Error();
            } catch (e) {
                assert.equals(e.message, "somethingAwesome expected to yield to 'success', but " +
                              "no object with such a property was passed. " +
                              "Received [23, 42]");
            }
        },

        "invokes property on last argument as callback": function () {
            var callback = sinon.spy();

            this.stub.yieldsToOn("success", this.fakeContext);
            this.stub(24, {}, { success: callback });

            assert(callback.calledOnce);
            assert(callback.calledOn(this.fakeContext));
            assert.equals(callback.args[0].length, 0);
        },

        "invokes first of two possible callbacks": function () {
            var callback = sinon.spy();
            var callback2 = sinon.spy();

            this.stub.yieldsToOn("error", this.fakeContext);
            this.stub(24, {}, { error: callback }, { error: callback2 });

            assert(callback.calledOnce);
            assert(callback.calledOn(this.fakeContext));
            assert(!callback2.called);
        },

        "invokes callback with arguments": function () {
            var obj = { id: 42 };
            var callback = sinon.spy();

            this.stub.yieldsToOn("success", this.fakeContext, obj, "Crazy");
            this.stub({ success: callback });

            assert(callback.calledOn(this.fakeContext));
            assert(callback.calledWith(obj, "Crazy"));
        },

        "throws if callback throws": function () {
            var obj = { id: 42 };
            var callback = sinon.stub().throws();

            this.stub.yieldsToOn("error", this.fakeContext, obj, "Crazy");

            assert.exception(function () {
                this.stub({ error: callback });
            });
        }
    },

    "withArgs": {
        "defines withArgs method": function () {
            var stub = sinon.stub();

            assert.isFunction(stub.withArgs);
        },

        "creates filtered stub": function () {
            var stub = sinon.stub();
            var other = stub.withArgs(23);

            refute.same(other, stub);
            assert.isFunction(stub.returns);
            assert.isFunction(other.returns);
        },

        "filters return values based on arguments": function () {
            var stub = sinon.stub().returns(23);
            stub.withArgs(42).returns(99);

            assert.equals(stub(), 23);
            assert.equals(stub(42), 99);
        },

        "filters exceptions based on arguments": function () {
            var stub = sinon.stub().returns(23);
            stub.withArgs(42).throws();

            refute.exception(stub);
            assert.exception(function () { stub(42); });
        }
    },

    "callsArgAsync": {
        setUp: function () {
            this.stub = sinon.stub.create();
        },

        "passes call to callsArg": function () {
            var spy = sinon.spy(this.stub, "callsArg");

            this.stub.callsArgAsync(2);

            assert(spy.calledWith(2));
        },

        "asynchronously calls argument at specified index": function (done) {
            this.stub.callsArgAsync(2);
            var callback = sinon.spy(done);

            this.stub(1, 2, callback);

            assert(!callback.called);
        }
    },

    "callsArgWithAsync": {
        setUp: function () {
            this.stub = sinon.stub.create();
        },
        
        "passes call to callsArgWith": function () {
            var object = {};
            sinon.spy(this.stub, "callsArgWith");
            
            this.stub.callsArgWithAsync(1, object);
            
            assert(this.stub.callsArgWith.calledWith(1, object));
        },

        "asynchronously calls callback at specified index with multiple args": function (done) {
            var object = {};
            var array = [];
            this.stub.callsArgWithAsync(1, object, array);
            
            var callback = sinon.spy(done(function () {
                assert(callback.calledWith(object, array));
            }));

            this.stub(1, callback);

            assert(!callback.called);
        }
    },

    "callsArgOnAsync": {
        setUp: function () {
            this.stub = sinon.stub.create();
            this.fakeContext = {
                foo: "bar"
            };
        },
        
        "passes call to callsArgOn": function () {
            sinon.spy(this.stub, "callsArgOn");

            this.stub.callsArgOnAsync(2, this.fakeContext);

            assert(this.stub.callsArgOn.calledWith(2, this.fakeContext));
        },

        "asynchronously calls argument at specified index with specified context": function (done) {
            var context = this.fakeContext;
            this.stub.callsArgOnAsync(2, context);
            
            var callback = sinon.spy(done(function () {
                assert(callback.calledOn(context));
            }));

            this.stub(1, 2, callback);
            
            assert(!callback.called);
        }
    },

    "callsArgOnWithAsync": {
        setUp: function () {
            this.stub = sinon.stub.create();
            this.fakeContext = { foo: "bar" };
        },
        
        "passes call to callsArgOnWith": function () {
            var object = {};
            sinon.spy(this.stub, "callsArgOnWith");

            this.stub.callsArgOnWithAsync(1, this.fakeContext, object);

            assert(this.stub.callsArgOnWith.calledWith(1, this.fakeContext, object));
        },

        "asynchronously calls argument at specified index with provided context and args": function (done) {
            var object = {};
            var context = this.fakeContext;
            this.stub.callsArgOnWithAsync(1, context, object);
            
            var callback = sinon.spy(done(function () {
                assert(callback.calledOn(context))
                assert(callback.calledWith(object));
            }));

            this.stub(1, callback);
            
            assert(!callback.called);
        }
    },

    "yieldsAsync": {
        "passes call to yields": function () {
            var stub = sinon.stub();
            sinon.spy(stub, "yields");

            stub.yieldsAsync();

            assert(stub.yields.calledWith());
        },

        "asynchronously invokes only argument as callback": function (done) {
            var stub = sinon.stub().yieldsAsync();

            var spy = sinon.spy(done);
            
            stub(spy);

            assert(!spy.called);
        }
    },

    "yieldsOnAsync": {
        setUp: function () {
            this.stub = sinon.stub.create();
            this.fakeContext = { foo: "bar" };
        },

        "passes call to yieldsOn": function () {
            var stub = sinon.stub();
            sinon.spy(stub, "yieldsOn");

            stub.yieldsOnAsync(this.fakeContext);

            assert(stub.yieldsOn.calledWith(this.fakeContext));
        },

        "asynchronously invokes only argument as callback with given context": function (done) {
            var context = this.fakeContext;
            this.stub.yieldsOnAsync(context);

            var spy = sinon.spy(done(function () {
                assert(spy.calledOnce);
                assert(spy.calledOn(context));
                assert.equals(spy.args[0].length, 0);
            }));
            
            this.stub(spy);
            
            assert(!spy.called);
        }
    },

    "yieldsToAsync": {
        "passes call to yieldsTo": function () {
            var stub = sinon.stub();
            sinon.spy(stub, "yieldsTo");

            stub.yieldsToAsync("success");

            assert(stub.yieldsTo.calledWith("success"));
        },
        
        "asynchronously yields to property of object argument": function (done) {
            var stub = sinon.stub().yieldsToAsync("success");

            var callback = sinon.spy(done(function () {
                assert(callback.calledOnce);
                assert.equals(callback.args[0].length, 0);
            }));

            stub({ success: callback });

            assert(!callback.called);
        }
    },

    "yieldsToOnAsync": {
        setUp: function () {
            this.stub = sinon.stub.create();
            this.fakeContext = { foo: "bar" };
        },

        "passes call to yieldsToOn": function () {
            var stub = sinon.stub();
            sinon.spy(stub, "yieldsToOn");

            stub.yieldsToOnAsync("success", this.fakeContext);

            assert(stub.yieldsToOn.calledWith("success", this.fakeContext));
        },

        "asynchronously yields to property of object argument with given context": function (done) {
            var context = this.fakeContext;
            this.stub.yieldsToOnAsync("success", context);
            
            var callback = sinon.spy(done(function () {
                assert(callback.calledOnce);
                assert(callback.calledOn(context));
                assert.equals(callback.args[0].length, 0);
            }));

            this.stub({ success: callback });

            assert(!callback.called);
        }
    }
});
