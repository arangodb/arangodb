if (typeof module === "object" && typeof require === "function") {
    var sinon = require("sinon");
    var buster = require("../lib/buster-core");
    buster.util = require("buster-util");
    var assert = require("assert");

    buster.extend(buster, {
        eventEmitter: require("../lib/buster-event-emitter")
    });
}

buster.util.testCase("EmitterCreateTest", {
    "creates event emitter": function () {
        var emitter = buster.eventEmitter.create();

        assert.ok(buster.eventEmitter.isPrototypeOf(emitter));
    }
});

buster.util.testCase("EmitterAddListenerTest", {
    "stores functions": function () {
        var emitter = buster.create(buster.eventEmitter);
        var listeners = [sinon.spy(), sinon.spy()];

        emitter.addListener("event", listeners[0]);
        emitter.addListener("event", listeners[1]);

        assert.ok(emitter.hasListener("event", listeners[0]));
        assert.ok(emitter.hasListener("event", listeners[1]));
    },

    "throws for uncallable listener": function () {
        var emitter = buster.create(buster.eventEmitter);

        assert.throws(function () {
            emitter.addListener("event", {});
        }, "TypeError");
    },

    "stores functions with this context": function () {
        var emitter = buster.create(buster.eventEmitter);
        var listener = sinon.spy();
        var obj = {};

        emitter.addListener("event", listener, obj);

        assert.ok(emitter.hasListener("event", listener, obj));
        assert.ok(!emitter.hasListener("event", listener));
    }
});

buster.util.testCase("EmitterHasListenerTest", {
    "returns false when no listeners": function () {
        var emitter = buster.create(buster.eventEmitter);

        assert.ok(!emitter.hasListener(function () {}));
    },

    "compares this objects strictly": function () {
        var emitter = buster.create(buster.eventEmitter);
        var listener = sinon.spy();
        var obj = {};

        emitter.addListener("event", listener, obj);

        assert.ok(!emitter.hasListener("event", listener, {}));
    },

    "finds listener without this object": function () {
        var emitter = buster.create(buster.eventEmitter);
        var listener = sinon.spy();

        emitter.addListener("event", listener);

        assert.ok(emitter.hasListener("event", listener));
    },

    "should not find listener with wrong this object": function () {
        var emitter = buster.create(buster.eventEmitter);
        var listener = sinon.spy();

        emitter.addListener("event", listener);

        assert.ok(!emitter.hasListener("event", listener, {}));
    },

    "should not find bound listener without this object": function () {
        var emitter = buster.create(buster.eventEmitter);
        var listener = sinon.spy();

        emitter.addListener("event", listener, {});

        assert.ok(!emitter.hasListener("event", listener));
    }
});

buster.util.testCase("EmitterEmitTest", {
    "calls all listeners": function () {
        var emitter = buster.create(buster.eventEmitter);
        var listeners = [sinon.spy(), sinon.spy()];
        emitter.addListener("event", listeners[0]);
        emitter.addListener("event", listeners[1]);

        emitter.emit("event");

        assert.ok(listeners[0].called);
        assert.ok(listeners[1].called);
    },

    "calls all listeners with correct this object": function () {
        var emitter = buster.create(buster.eventEmitter);
        var listeners = [sinon.spy(), sinon.spy()];
        var obj = {};
        emitter.addListener("event", listeners[0], obj);
        emitter.addListener("event", listeners[1]);

        emitter.emit("event");

        assert.ok(listeners[0].calledOn(obj));
        assert.ok(listeners[1].calledOn(emitter));
    },

    "calls all listeners added with on": function () {
        var emitter = buster.create(buster.eventEmitter);
        var listeners = [sinon.spy(), sinon.spy()];
        emitter.on("event", listeners[0]);
        emitter.on("event", listeners[1]);

        emitter.emit("event");

        assert.ok(listeners[0].called);
        assert.ok(listeners[1].called);
    },

    "passes through arguments": function () {
        var emitter = buster.create(buster.eventEmitter);
        var listener = sinon.spy();

        emitter.addListener("event", listener);
        emitter.emit("event", "String", 1, 32);

        assert.ok(listener.calledWith("String", 1, 32));
    },

    "emits all even when some fail": function () {
        var emitter = buster.create(buster.eventEmitter);
        var listeners = [sinon.stub().throws("I'm thrown on purpose"), sinon.spy()];

        emitter.addListener("event", listeners[0]);
        emitter.addListener("event", listeners[1]);

        emitter.emit("event");

        assert.ok(listeners[1].called);
    },

    "calls listeners in the order they were added": function () {
        var emitter = buster.create(buster.eventEmitter);
        var listeners = [sinon.spy(), sinon.spy()];

        emitter.addListener("event", listeners[0]);
        emitter.addListener("event", listeners[1]);

        emitter.emit("event");

        assert.ok(listeners[0].calledBefore(listeners[1]));
    },

    "does not fail if no listeners": function () {
        var emitter = buster.create(buster.eventEmitter);

        assert.doesNotThrow(function () {
            emitter.emit("event");
        });
    },

    "only notifies relevant listeners": function () {
        var emitter = buster.create(buster.eventEmitter);
        var listeners = [sinon.spy(), sinon.spy()];

        emitter.addListener("event", listeners[0]);
        emitter.addListener("other", listeners[1]);

        emitter.emit("other");

        assert.ok(listeners[1].called);
        assert.ok(!listeners[0].called);
    }
});

buster.util.testCase("EventEmitterBindTest", {
    "returns object bound to": function () {
        var listener = { doIt: function () {} };
        var result = buster.create(buster.eventEmitter).bind(listener, "doIt");

        assert.equal(result, listener);
    },

    "binds to method named after event": function () {
        var emitter = buster.create(buster.eventEmitter);
        var listener = { doIt: function() {} };

        emitter.bind(listener, "doIt");

        assert.ok(emitter.hasListener("doIt", listener.doIt, listener));
    },

    "binds all methods as event listeners to correspondingly named event": function () {
        var emitter = buster.create(buster.eventEmitter);
        var listener = { complete: function() {},
                         failure: function () {},
                         success: function () {} };

        emitter.bind(listener);

        assert.ok(emitter.hasListener("complete", listener.complete, listener));
        assert.ok(emitter.hasListener("failure", listener.failure, listener));
        assert.ok(emitter.hasListener("success", listener.success, listener));
    },

    "does not bind non-function properties": function () {
        var emitter = buster.create(buster.eventEmitter);
        var listener = { id: 42, failure: function () {} };

        emitter.bind(listener);

        assert.ok(!emitter.hasListener("id", listener.id, listener));
        assert.ok(emitter.hasListener("failure", listener.failure, listener));
    },

    "does not bind inherited methods": function () {
        var emitter = buster.create(buster.eventEmitter);
        var proto = { something: function () {} };
        var listener = buster.create(proto);
        listener.failure = function () {};

        emitter.bind(listener);

        assert.ok(!emitter.hasListener("something", listener.something, listener));
        assert.ok(emitter.hasListener("failure", listener.failure, listener));
    },

    "maps event names to method names": function () {
        var emitter = buster.create(buster.eventEmitter);
        var listener = { one: function () {}, two: function () {} };

        emitter.bind(listener, { event1: "one", event2: "two" });

        assert.ok(emitter.hasListener("event1", listener.one, listener));
        assert.ok(emitter.hasListener("event2", listener.two, listener));
    },

    "throws when mapping events to non-existing methods": function () {
        var emitter = buster.create(buster.eventEmitter);
        var listener = { one: function () {}, two: function () {} };

        assert.throws(function () {
            emitter.bind(listener, { event1: "oops", event2: "two" });
        });
    },

    "binds array of methods/events": function () {
        var emitter = buster.create(buster.eventEmitter);
        var listener = { one: function () {},
                         two: function () {},
                         three: function () {} };

        emitter.bind(listener, ["one", "three"]);

        assert.ok(emitter.hasListener("one", listener.one, listener));
        assert.ok(!emitter.hasListener("two", listener.two, listener));
        assert.ok(emitter.hasListener("three", listener.three, listener));
    },

    "fails array when binding non-existent method": function () {
        var emitter = buster.create(buster.eventEmitter);
        var listener = {};

        assert.throws(function () {
            emitter.bind(listener, "one");
        });
    },

    "fails array when binding non-existent method in array": function () {
        var emitter = buster.create(buster.eventEmitter);
        var listener = { one: function () {} };

        assert.throws(function () {
            emitter.bind(listener, ["one", "two"]);
        });
    },

    "adds function as method and bind it to event": function () {
        var method = function () {};
        var listener = {};
        var emitter = buster.create(buster.eventEmitter);

        emitter.bind(listener, { "someEvent": method });

        assert.equal(listener.someEvent, method);
        assert.ok(emitter.hasListener("someEvent", listener.someEvent, listener));
    },

    "uses function names for property names if present": function () {
        function myHandler() {};
        var listener = {};
        var emitter = buster.create(buster.eventEmitter);

        emitter.bind(listener, { "someEvent": myHandler });

        assert.equal(listener.myHandler, myHandler);
        assert.ok(emitter.hasListener("someEvent", listener.myHandler, listener));
    }
});

buster.util.testCase("EmitterBindAllTest", {
    "is notified of all events": function () {
        var listener = sinon.spy();
        var emitter = buster.eventEmitter.create();

        emitter.on(listener);
        emitter.emit("something", 42);
        emitter.emit("stuff", { id: 13 });

        assert.ok(listener.calledTwice);
        assert.ok(listener.calledWith("something", 42));
    }
});

buster.util.testCase("EmitterRemoveListenerTest", {
    "removes listener": function () {
        var listener = sinon.spy();
        var emitter = buster.create(buster.eventEmitter);

        emitter.on("event", listener);
        emitter.removeListener("event", listener);

        assert.ok(!emitter.hasListener("event", listener));
    },

    "should not remove listener for other event": function () {
        var listener = sinon.spy();
        var emitter = buster.create(buster.eventEmitter);

        emitter.on("event", listener);
        emitter.removeListener("event2", listener);

        assert.ok(emitter.hasListener("event", listener));
    },

    "should not remove other listeners": function () {
        var listeners = [sinon.spy(), sinon.spy(), sinon.spy()];
        var emitter = buster.create(buster.eventEmitter);

        emitter.on("event", listeners[0]);
        emitter.on("event", listeners[1]);
        emitter.on("event", listeners[2]);
        emitter.removeListener("event", listeners[1]);

        assert.ok(emitter.hasListener("event", listeners[0]));
        assert.ok(emitter.hasListener("event", listeners[2]));
    },

    "should remove listener in other listener for same event": function () {
        var emitter = buster.eventEmitter.create();
        var fooListener = sinon.spy();

        emitter.on("foo", function () {
            emitter.removeListener("foo", fooListener);
        });
        emitter.on("foo", fooListener);
        emitter.emit("foo");
    }
});

buster.util.testCase("EmitterOnceTest", {
    "is only called once": function () {
        var listener = sinon.spy();
        var emitter = buster.create(buster.eventEmitter);

        emitter.once("event", listener);
        emitter.emit("event");
        assert.ok(listener.calledOnce);

        emitter.emit("event");
        assert.ok(listener.calledOnce);
    },

    "is called with emitted arguments": function () {
        var listener = sinon.spy();
        var emitter = buster.create(buster.eventEmitter);
        emitter.once("event", listener);
        emitter.emit("event", "foo", 1);
        assert.ok(listener.calledWithExactly("foo", 1));
    },

    "is called with context": function () {
        var emitter = buster.create(buster.eventEmitter);
        var listener = function () { this.foo = "bar"; };
        var obj = {};

        emitter.addListener("event", listener, obj);
        emitter.emit("event");
        assert.equal("bar", obj.foo);
    },

    "looks up with hasListener": function () {
        var listener = sinon.spy();
        var emitter = buster.create(buster.eventEmitter);

        emitter.once("event", listener);
        assert.ok(emitter.hasListener("event", listener));

        emitter.emit("event");
        assert.ok(!emitter.hasListener("event", listener));
    }
});
