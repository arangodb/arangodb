/*jslint onevar: false, eqeqeq: false, plusplus: false*/
/*globals sinon buster*/
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

buster.testCase("sinon.collection", {
    "creates fake collection": function () {
        var collection = sinon.create(sinon.collection);

        assert.isFunction(collection.verify);
        assert.isFunction(collection.restore);
        assert.isFunction(collection.verifyAndRestore);
        assert.isFunction(collection.stub);
        assert.isFunction(collection.mock);
    },

    "stub": {
        setUp: function () {
            this.stub = sinon.stub;
            this.collection = sinon.create(sinon.collection);
        },

        tearDown: function () {
            sinon.stub = this.stub;
        },

        "calls stub": function () {
            var object = { method: function () {} };
            var args;

            sinon.stub = function () {
                args = Array.prototype.slice.call(arguments);
            };

            this.collection.stub(object, "method");

            assert.equals(args, [object, "method"]);
        },

        "adds stub to fake array": function () {
            var object = { method: function () {} };

            sinon.stub = function () {
                return object;
            };

            this.collection.stub(object, "method");

            assert.equals(this.collection.fakes, [object]);
        },

        "appends stubs to fake array": function () {
            var objects = [{ id: 42 }, { id: 17 }];
            var i = 0;

            sinon.stub = function () {
                return objects[i++];
            };

            this.collection.stub({ method: function () {} }, "method");
            this.collection.stub({ method: function () {} }, "method");

            assert.equals(this.collection.fakes, objects);
        },

        "adds all object methods to fake array": function() {
            var object = { method: function () {}, method2: function() {} };

            sinon.stub = function() {
                return object;
            };

            this.collection.stub(object);

            assert.equals(this.collection.fakes, [object.method, object.method2]);
            assert.equals(this.collection.fakes.length, 2);
        },

        "returns a stubbed object": function() {
            var object = { method: function () {} };

            sinon.stub = function () {
                return object;
            };

            assert.equals(this.collection.stub(object), object);
        },

        "returns a stubbed method": function() {
            var object = { method: function () {} };

            sinon.stub = function () {
                return object.method;
            };

            assert.equals(this.collection.stub(object, "method"), object.method);
        },

        "on node": {
            requiresSupportFor: { "process": typeof process !== "undefined" },

            setUp: function () {
                process.env.HELL = "Ain't too bad";
            },

            "stubs environment property": function () {
                this.collection.stub(process.env, "HELL", "froze over");
                assert.equals(process.env.HELL, "froze over");
            }
        }
    },

    "stubAnything": {
        setUp: function () {
            this.object = { property: 42 };
            this.collection = sinon.create(sinon.collection);
        },

        "stubs number property": function () {
            this.collection.stub(this.object, "property", 1);

            assert.equals(this.object.property, 1);
        },

        "restores number property": function () {
            this.collection.stub(this.object, "property", 1);
            this.collection.restore();

            assert.equals(this.object.property, 42);
        },

        "fails if property does not exist": function () {
            var collection = this.collection;
            var object = {};

            assert.exception(function () {
                collection.stub(object, "prop", 1);
            });
        }
    },

    "mock": {
        setUp: function () {
            this.mock = sinon.mock;
            this.collection = sinon.create(sinon.collection);
        },

        tearDown: function () {
            sinon.mock = this.mock;
        },

        "calls mock": function () {
            var object = { id: 42 };
            var args;

            sinon.mock = function () {
                args = Array.prototype.slice.call(arguments);
            };

            this.collection.mock(object, "method");

            assert.equals(args, [object, "method"]);
        },

        "adds mock to fake array": function () {
            var object = { id: 42 };

            sinon.mock = function () {
                return object;
            };

            this.collection.mock(object, "method");

            assert.equals(this.collection.fakes, [object]);
        },

        "appends mocks to fake array": function () {
            var objects = [{ id: 42 }, { id: 17 }];
            var i = 0;

            sinon.mock = function () {
                return objects[i++];
            };

            this.collection.mock({}, "method");
            this.collection.mock({}, "method");

            assert.equals(this.collection.fakes, objects);
        }
    },

    "stubAndMockTest": {
        setUp: function () {
            this.mock = sinon.mock;
            this.stub = sinon.stub;
            this.collection = sinon.create(sinon.collection);
        },

        tearDown: function () {
            sinon.mock = this.mock;
            sinon.stub = this.stub;
        },

        "appends mocks and stubs to fake array": function () {
            var objects = [{ id: 42 }, { id: 17 }];
            var i = 0;

            sinon.stub = sinon.mock = function () {
                return objects[i++];
            };

            this.collection.mock({ method: function () {} }, "method");
            this.collection.stub({ method: function () {} }, "method");

            assert.equals(this.collection.fakes, objects);
        }
    },

    "verify": {
        setUp: function () {
            this.collection = sinon.create(sinon.collection);
        },

        "calls verify on all fakes": function () {
            this.collection.fakes = [{
                verify: sinon.spy.create()
            }, {
                verify: sinon.spy.create()
            }];

            this.collection.verify();

            assert(this.collection.fakes[0].verify.called);
            assert(this.collection.fakes[1].verify.called);
        }
    },

    "restore": {
        setUp: function () {
            this.collection = sinon.create(sinon.collection);
            this.collection.fakes = [{
                restore: sinon.spy.create()
            }, {
                restore: sinon.spy.create()
            }];
        },

        "calls restore on all fakes": function () {
            var fake0 = this.collection.fakes[0];
            var fake1 = this.collection.fakes[1];

            this.collection.restore();

            assert(fake0.restore.called);
            assert(fake1.restore.called);
        },

        "removes from collection when restored": function () {
            this.collection.restore();
            assert(this.collection.fakes.length == 0);
        },

        "restores functions when stubbing entire object": function () {
            var a = function () {};
            var b = function () {};
            var obj = { a: a, b: b };
            this.collection.stub(obj);

            this.collection.restore();

            assert.same(obj.a, a);
            assert.same(obj.b, b);
        }
    },

    "verifyAndRestore": {
        setUp: function () {
            this.collection = sinon.create(sinon.collection);
        },

        "calls verify and restore": function () {
            this.collection.verify = sinon.spy.create();
            this.collection.restore = sinon.spy.create();

            this.collection.verifyAndRestore();

            assert(this.collection.verify.called);
            assert(this.collection.restore.called);
        },

        "throws when restore throws": function () {
            this.collection.verify = sinon.spy.create();
            this.collection.restore = sinon.stub.create().throws();

            assert.exception(function () {
                this.collection.verifyAndRestore();
            });
        },

        "calls restore when restore throws": function () {
            this.collection.verify = sinon.spy.create();
            this.collection.restore = sinon.stub.create().throws();

            try {
                this.collection.verifyAndRestore();
            } catch (e) {}

            assert(this.collection.restore.called);
        }
    },

    "injectTest": {
        setUp: function () {
            this.collection = sinon.create(sinon.collection);
        },

        "injects fakes into object": function () {
            var obj = {};
            this.collection.inject(obj);

            assert.isFunction(obj.spy);
            assert.isFunction(obj.stub);
            assert.isFunction(obj.mock);
        },

        "returns argument": function () {
            var obj = {};

            assert.same(this.collection.inject(obj), obj);
        },

        "injects spy, stub, mock bound to collection": sinon.test(function () {
            var obj = {};
            this.collection.inject(obj);
            this.stub(this.collection, "spy");
            this.stub(this.collection, "stub");
            this.stub(this.collection, "mock");

            obj.spy();
            var fn = obj.spy;
            fn();

            obj.stub();
            fn = obj.stub;
            fn();

            obj.mock();
            fn = obj.mock;
            fn();

            assert(this.collection.spy.calledTwice);
            assert(this.collection.stub.calledTwice);
            assert(this.collection.mock.calledTwice);
        })
    }
});
