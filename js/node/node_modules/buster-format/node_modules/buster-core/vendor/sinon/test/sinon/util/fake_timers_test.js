/*jslint onevar: false, eqeqeq: false, plusplus: false*/
/*globals sinon buster setTimeout setInterval clearTimeout clearInterval*/
/**
 * @author Christian Johansen (christian@cjohansen.no)
 * @license BSD
 *
 * Copyright (c) 2010-2012 Christian Johansen
 */
"use strict";

if (typeof require == "function" && typeof module == "object") {
    var buster = require("../../runner");
    var sinon = require("../../../lib/sinon");
    sinon.extend(sinon, require("../../../lib/sinon/util/fake_timers"));
}

buster.testCase("sinon.clock", {
    setUp: function () {
        this.global = typeof global != "undefined" ? global : window;
    },

    "setTimeout": {
        setUp: function () {
            this.clock = sinon.clock.create();
            sinon.clock.evalCalled = false;
        },

        tearDown: function () {
            delete sinon.clock.evalCalled;
        },

        "throws if no arguments": function () {
            var clock = this.clock;

            assert.exception(function () { clock.setTimeout(); });
        },

        "returns numeric id": function () {
            var result = this.clock.setTimeout("");

            assert.isNumber(result);
        },

        "returns unique id": function () {
            var id1 = this.clock.setTimeout("");
            var id2 = this.clock.setTimeout("");

            refute.equals(id2, id1);
        },

        "sets timers on instance": function () {
            var clock1 = sinon.clock.create();
            var clock2 = sinon.clock.create();
            var stubs = [sinon.stub.create(), sinon.stub.create()];

            clock1.setTimeout(stubs[0], 100);
            clock2.setTimeout(stubs[1], 100);
            clock2.tick(200);

            assert.isFalse(stubs[0].called);
            assert(stubs[1].called);
        },

        "evals non-function callbacks": function () {
            this.clock.setTimeout("sinon.clock.evalCalled = true", 10);
            this.clock.tick(10);

            assert(sinon.clock.evalCalled);
        },

        "passes setTimeout parameters": function() {
            var clock = sinon.clock.create();
            var stub = sinon.stub.create();

            clock.setTimeout(stub, 2, "the first", "the second");

            clock.tick(3);

            assert.isTrue(stub.calledWithExactly("the first", "the second"));
        },

        "calls correct timeout on recursive tick": function() {
            var clock = sinon.clock.create();
            var stub = sinon.stub.create();
            var recurseCallback = function () { clock.tick(100); };

            clock.setTimeout(recurseCallback, 50);
            clock.setTimeout(stub, 100);

            clock.tick(50);
            assert(stub.called);
        }
    },

    "tick": {
        setUp: function () {
            this.clock = sinon.useFakeTimers(0);
        },

        tearDown: function () {
            this.clock.restore();
        },

        "triggers immediately without specified delay": function () {
            var stub = sinon.stub.create();
            this.clock.setTimeout(stub);

            this.clock.tick(0);

            assert(stub.called);
        },

        "does not trigger without sufficient delay": function () {
            var stub = sinon.stub.create();
            this.clock.setTimeout(stub, 100);
            this.clock.tick(10);

            assert.isFalse(stub.called);
        },

        "triggers after sufficient delay": function () {
            var stub = sinon.stub.create();
            this.clock.setTimeout(stub, 100);
            this.clock.tick(100);

            assert(stub.called);
        },

        "triggers simultaneous timers": function () {
            var spies = [sinon.spy(), sinon.spy()];
            this.clock.setTimeout(spies[0], 100);
            this.clock.setTimeout(spies[1], 100);

            this.clock.tick(100);

            assert(spies[0].called);
            assert(spies[1].called);
        },

        "triggers multiple simultaneous timers": function () {
            var spies = [sinon.spy(), sinon.spy(), sinon.spy(), sinon.spy()];
            this.clock.setTimeout(spies[0], 100);
            this.clock.setTimeout(spies[1], 100);
            this.clock.setTimeout(spies[2], 99);
            this.clock.setTimeout(spies[3], 100);

            this.clock.tick(100);

            assert(spies[0].called);
            assert(spies[1].called);
            assert(spies[2].called);
            assert(spies[3].called);
        },

        "waits after setTimeout was called": function () {
            this.clock.tick(100);
            var stub = sinon.stub.create();
            this.clock.setTimeout(stub, 150);
            this.clock.tick(50);

            assert.isFalse(stub.called);
            this.clock.tick(100);
            assert(stub.called);
        },

        "mini integration test": function () {
            var stubs = [sinon.stub.create(), sinon.stub.create(), sinon.stub.create()];
            this.clock.setTimeout(stubs[0], 100);
            this.clock.setTimeout(stubs[1], 120);
            this.clock.tick(10);
            this.clock.tick(89);
            assert.isFalse(stubs[0].called);
            assert.isFalse(stubs[1].called);
            this.clock.setTimeout(stubs[2], 20);
            this.clock.tick(1);
            assert(stubs[0].called);
            assert.isFalse(stubs[1].called);
            assert.isFalse(stubs[2].called);
            this.clock.tick(19);
            assert.isFalse(stubs[1].called);
            assert(stubs[2].called);
            this.clock.tick(1);
            assert(stubs[1].called);
        },

        "triggers even when some throw": function () {
            var clock = this.clock;
            var stubs = [sinon.stub.create().throws(), sinon.stub.create()];

            clock.setTimeout(stubs[0], 100);
            clock.setTimeout(stubs[1], 120);

            assert.exception(function() {
                clock.tick(120);
            });

            assert(stubs[0].called);
            assert(stubs[1].called);
        },

        "calls function with global object or null (strict mode) as this": function () {
            var clock = this.clock;
            var stub = sinon.stub.create().throws();
            clock.setTimeout(stub, 100);

            assert.exception(function() {
                clock.tick(100);
            });

            assert(stub.calledOn(this.global) || stub.calledOn(null));
        },

        "triggers in the order scheduled": function () {
            var spies = [sinon.spy.create(), sinon.spy.create()];
            this.clock.setTimeout(spies[0], 13);
            this.clock.setTimeout(spies[1], 11);

            this.clock.tick(15);

            assert(spies[1].calledBefore(spies[0]));
        },

        "creates updated Date while ticking": function () {
            var spy = sinon.spy.create();

            this.clock.setInterval(function () {
                spy(new Date().getTime());
            }, 10);

            this.clock.tick(100);

            assert.equals(spy.callCount, 10);
            assert(spy.calledWith(10));
            assert(spy.calledWith(20));
            assert(spy.calledWith(30));
            assert(spy.calledWith(40));
            assert(spy.calledWith(50));
            assert(spy.calledWith(60));
            assert(spy.calledWith(70));
            assert(spy.calledWith(80));
            assert(spy.calledWith(90));
            assert(spy.calledWith(100));
        },

        "fires timer in intervals of 13": function () {
            var spy = sinon.spy.create();
            this.clock.setInterval(spy, 13);

            this.clock.tick(500);

            assert.equals(spy.callCount, 38);
        },

        "fires timers in correct order": function () {
            var spy13 = sinon.spy.create();
            var spy10 = sinon.spy.create();

            this.clock.setInterval(function () {
                spy13(new Date().getTime());
            }, 13);

            this.clock.setInterval(function () {
                spy10(new Date().getTime());
            }, 10);

            this.clock.tick(500);

            assert.equals(spy13.callCount, 38);
            assert.equals(spy10.callCount, 50);

            assert(spy13.calledWith(416));
            assert(spy10.calledWith(320));

            assert(spy10.getCall(0).calledBefore(spy13.getCall(0)));
            assert(spy10.getCall(4).calledBefore(spy13.getCall(3)));
        },

        "triggers timeouts and intervals in the order scheduled": function () {
            var spies = [sinon.spy.create(), sinon.spy.create()];
            this.clock.setInterval(spies[0], 10);
            this.clock.setTimeout(spies[1], 50);

            this.clock.tick(100);

            assert(spies[0].calledBefore(spies[1]));
            assert.equals(spies[0].callCount, 10);
            assert.equals(spies[1].callCount, 1);
        },

        "does not fire canceled intervals": function () {
            var id;
            var callback = sinon.spy(function () {
                if (callback.callCount == 3) {
                    clearTimeout(id);
                }
            });

            id = this.clock.setInterval(callback, 10);
            this.clock.tick(100);

            assert.equals(callback.callCount, 3);
        },

        "passes 6 seconds": function () {
            var spy = sinon.spy.create();
            this.clock.setInterval(spy, 4000);

            this.clock.tick("08");

            assert.equals(spy.callCount, 2);
        },

        "passes 1 minute": function () {
            var spy = sinon.spy.create();
            this.clock.setInterval(spy, 6000);

            this.clock.tick("01:00");

            assert.equals(spy.callCount, 10);
        },

        "passes 2 hours, 34 minutes and 12 seconds": function () {
            var spy = sinon.spy.create();
            this.clock.setInterval(spy, 10000);

            this.clock.tick("02:34:10");

            assert.equals(spy.callCount, 925);
        },

        "throws for invalid format": function () {
            var spy = sinon.spy.create();
            this.clock.setInterval(spy, 10000);
            var test = this;

            assert.exception(function () {
                test.clock.tick("12:02:34:10");
            });

            assert.equals(spy.callCount, 0);
        },

        "throws for invalid minutes": function () {
            var spy = sinon.spy.create();
            this.clock.setInterval(spy, 10000);
            var test = this;

            assert.exception(function () {
                test.clock.tick("67:10");
            });

            assert.equals(spy.callCount, 0);
        },

        "throws for negative minutes": function () {
            var spy = sinon.spy.create();
            this.clock.setInterval(spy, 10000);
            var test = this;

            assert.exception(function () {
                test.clock.tick("-7:10");
            });

            assert.equals(spy.callCount, 0);
        },

        "treats missing argument as 0": function () {
            this.clock.tick();

            assert.equals(this.clock.now, 0);
        },

        "fires nested setTimeout calls properly": function () {
            var i = 0;
            var clock = this.clock;

            var callback = function () {
                ++i;
                clock.setTimeout(function () {
                    callback();
                }, 100);
            };

            callback();

            clock.tick(1000);

            assert.equals(i, 11);
        },

        "does not silently catch exceptions": function () {
            var clock = this.clock;

            clock.setTimeout(function() {
                throw new Exception("oh no!");
            }, 1000);

            assert.exception(function() {
                clock.tick(1000);
            });
        }
    },

    "clearTimeout": {
        setUp: function () {
            this.clock = sinon.clock.create();
        },

        "removes timeout": function () {
            var stub = sinon.stub.create();
            var id = this.clock.setTimeout(stub, 50);
            this.clock.clearTimeout(id);
            this.clock.tick(50);

            assert.isFalse(stub.called);
        }
    },

    "reset": {
        setUp: function () {
            this.clock = sinon.clock.create();
        },

        "empties timeouts queue": function () {
            var stub = sinon.stub.create();
            this.clock.setTimeout(stub);
            this.clock.reset();
            this.clock.tick(0);

            assert.isFalse(stub.called);
        }
    },

    "terval": {
        setUp: function () {
            this.clock = sinon.clock.create();
        },

        "throws if no arguments": function () {
            var clock = this.clock;

            assert.exception(function () {
                clock.setInterval();
            });
        },

        "returns numeric id": function () {
            var result = this.clock.setInterval("");

            assert.isNumber(result);
        },

        "returns unique id": function () {
            var id1 = this.clock.setInterval("");
            var id2 = this.clock.setInterval("");

            refute.equals(id2, id1);
        },

        "schedules recurring timeout": function () {
            var stub = sinon.stub.create();
            this.clock.setInterval(stub, 10);
            this.clock.tick(99);

            assert.equals(stub.callCount, 9);
        },

        "does not schedule recurring timeout when cleared": function () {
            var clock = this.clock;
            var id;
            var stub = sinon.spy.create(function () {
                if (stub.callCount == 3) {
                    clock.clearInterval(id);
                }
            });

            id = this.clock.setInterval(stub, 10);
            this.clock.tick(100);

            assert.equals(stub.callCount, 3);
        },

        "passes setTimeout parameters": function() {
            var clock = sinon.clock.create();
            var stub = sinon.stub.create();

            clock.setInterval(stub, 2, "the first", "the second");

            clock.tick(3);

            assert.isTrue(stub.calledWithExactly("the first", "the second"));
        }
    },

    "date": {
        setUp: function () {
            this.now = new Date().getTime() - 3000;
            this.clock = sinon.clock.create(this.now);
            this.Date = this.global.Date;
        },

        tearDown: function () {
            this.global.Date = this.Date;
        },

        "provides date constructor": function () {
            assert.isFunction(this.clock.Date);
        },

        "creates real Date objects": function () {
            var date = new this.clock.Date();

            assert(Date.prototype.isPrototypeOf(date));
        },

        "creates real Date objects when called as function": function () {
            var date = this.clock.Date();

            assert(Date.prototype.isPrototypeOf(date));
        },

        "creates real Date objects when Date constructor is gone": function () {
            var realDate = new Date();
            Date = function () {};
            this.global.Date = function () {};

            var date = new this.clock.Date();

            assert.same(date.constructor.prototype, realDate.constructor.prototype);
        },

        "creates Date objects representing clock time": function () {
            var date = new this.clock.Date();

            assert.equals(date.getTime(), new Date(this.now).getTime());
        },

        "returns Date object representing clock time": function () {
            var date = this.clock.Date();

            assert.equals(date.getTime(), new Date(this.now).getTime());
        },

        "listens to ticking clock": function () {
            var date1 = new this.clock.Date();
            this.clock.tick(3);
            var date2 = new this.clock.Date();

            assert.equals(date2.getTime() - date1.getTime(), 3);
        },

        "creates regular date when passing timestamp": function () {
            var date = new Date();
            var fakeDate = new this.clock.Date(date.getTime());

            assert.equals(fakeDate.getTime(), date.getTime());
        },

        "returns regular date when calling with timestamp": function () {
            var date = new Date();
            var fakeDate = this.clock.Date(date.getTime());

            assert.equals(fakeDate.getTime(), date.getTime());
        },

        "creates regular date when passing year, month": function () {
            var date = new Date(2010, 4);
            var fakeDate = new this.clock.Date(2010, 4);

            assert.equals(fakeDate.getTime(), date.getTime());
        },

        "returns regular date when calling with year, month": function () {
            var date = new Date(2010, 4);
            var fakeDate = this.clock.Date(2010, 4);

            assert.equals(fakeDate.getTime(), date.getTime());
        },

        "creates regular date when passing y, m, d": function () {
            var date = new Date(2010, 4, 2);
            var fakeDate = new this.clock.Date(2010, 4, 2);

            assert.equals(fakeDate.getTime(), date.getTime());
        },

        "returns regular date when calling with y, m, d": function () {
            var date = new Date(2010, 4, 2);
            var fakeDate = this.clock.Date(2010, 4, 2);

            assert.equals(fakeDate.getTime(), date.getTime());
        },

        "creates regular date when passing y, m, d, h": function () {
            var date = new Date(2010, 4, 2, 12);
            var fakeDate = new this.clock.Date(2010, 4, 2, 12);

            assert.equals(fakeDate.getTime(), date.getTime());
        },

        "returns regular date when calling with y, m, d, h": function () {
            var date = new Date(2010, 4, 2, 12);
            var fakeDate = this.clock.Date(2010, 4, 2, 12);

            assert.equals(fakeDate.getTime(), date.getTime());
        },

        "creates regular date when passing y, m, d, h, m": function () {
            var date = new Date(2010, 4, 2, 12, 42);
            var fakeDate = new this.clock.Date(2010, 4, 2, 12, 42);

            assert.equals(fakeDate.getTime(), date.getTime());
        },

        "returns regular date when calling with y, m, d, h, m": function () {
            var date = new Date(2010, 4, 2, 12, 42);
            var fakeDate = this.clock.Date(2010, 4, 2, 12, 42);

            assert.equals(fakeDate.getTime(), date.getTime());
        },

        "creates regular date when passing y, m, d, h, m, s": function () {
            var date = new Date(2010, 4, 2, 12, 42, 53);
            var fakeDate = new this.clock.Date(2010, 4, 2, 12, 42, 53);

            assert.equals(fakeDate.getTime(), date.getTime());
        },

        "returns regular date when calling with y, m, d, h, m, s": function () {
            var date = new Date(2010, 4, 2, 12, 42, 53);
            var fakeDate = this.clock.Date(2010, 4, 2, 12, 42, 53);

            assert.equals(fakeDate.getTime(), date.getTime());
        },

        "creates regular date when passing y, m, d, h, m, s, ms": function () {
            var date = new Date(2010, 4, 2, 12, 42, 53, 498);
            var fakeDate = new this.clock.Date(2010, 4, 2, 12, 42, 53, 498);

            assert.equals(fakeDate.getTime(), date.getTime());
        },

        "returns regular date when calling with y, m, d, h, m, s, ms": function () {
            var date = new Date(2010, 4, 2, 12, 42, 53, 498);
            var fakeDate = this.clock.Date(2010, 4, 2, 12, 42, 53, 498);

            assert.equals(fakeDate.getTime(), date.getTime());
        },

        "mirrors native Date.prototype": function () {
            assert.same(this.clock.Date.prototype, Date.prototype);
        },

        "supports now method if present": function () {
            assert.same(typeof this.clock.Date.now, typeof Date.now);
        },

        "now": {
            requiresSupportFor: { "Date.now": !!Date.now },

            "returns clock.now": function () {
                assert.equals(this.clock.Date.now(), this.now);
            }
        },

        "unsupported now": {
            requiresSupportFor: { "No Date.now implementation": !Date.now },

            "is undefined": function () {
                refute.defined(this.clock.Date.now);
            }
        },

        "mirrors parse method": function () {
            assert.same(this.clock.Date.parse, Date.parse);
        },

        "mirrors UTC method": function () {
            assert.same(this.clock.Date.UTC, Date.UTC);
        },

        "mirrors toUTCString method": function () {
            assert.same(this.clock.Date.prototype.toUTCString, Date.prototype.toUTCString);
        },


        "toSource": {
            requiresSupportFor: { "Date.toSource": !!Date.toSource },

            "is mirrored": function () {
                assert.same(this.clock.Date.toSource(), Date.toSource());
            }
        },

        "unsupported toSource": {
            requiresSupportFor: { "No Date.toSource implementation": !Date.toSource },

            "is undefined": function () {
                refute.defined(this.clock.Date.toSource);
            }
        },

        "mirrors toString": function () {
            assert.same(this.clock.Date.toString(), Date.toString());
        }
    },

    "stubTimers": {
        setUp: function () {
            this.dateNow = this.global.Date.now;
        },

        tearDown: function () {
            if (this.clock) {
                this.clock.restore();
            }

            clearTimeout(this.timer);
            if (typeof this.dateNow == "undefined") {
                delete this.global.Date.now;
            } else {
                this.global.Date.now = this.dateNow;
            }
        },

        "returns clock object": function () {
            this.clock = sinon.useFakeTimers();

            assert.isObject(this.clock);
            assert.isFunction(this.clock.tick);
        },

        "haves clock property": function () {
            this.clock = sinon.useFakeTimers();

            assert.same(setTimeout.clock, this.clock);
            assert.same(clearTimeout.clock, this.clock);
            assert.same(setInterval.clock, this.clock);
            assert.same(clearInterval.clock, this.clock);
            assert.same(Date.clock, this.clock);
        },

        "sets initial timestamp": function () {
            this.clock = sinon.useFakeTimers(1400);

            assert.equals(this.clock.now, 1400);
        },

        "replaces global setTimeout": function () {
            this.clock = sinon.useFakeTimers();
            var stub = sinon.stub.create();

            setTimeout(stub, 1000);
            this.clock.tick(1000);

            assert(stub.called);
        },

        "global fake setTimeout should return id": function () {
            this.clock = sinon.useFakeTimers();
            var stub = sinon.stub.create();

            var id = setTimeout(stub, 1000);

            assert.isNumber(id);
        },

        "replaces global clearTimeout": function () {
            this.clock = sinon.useFakeTimers();
            var stub = sinon.stub.create();

            clearTimeout(setTimeout(stub, 1000));
            this.clock.tick(1000);

            assert.isFalse(stub.called);
        },

        "restores global setTimeout": function () {
            this.clock = sinon.useFakeTimers();
            var stub = sinon.stub.create();
            this.clock.restore();

            this.timer = setTimeout(stub, 1000);
            this.clock.tick(1000);

            assert.isFalse(stub.called);
            assert.same(setTimeout, sinon.timers.setTimeout);
        },

        "restores global clearTimeout": function () {
            this.clock = sinon.useFakeTimers();
            sinon.stub.create();
            this.clock.restore();

            assert.same(clearTimeout, sinon.timers.clearTimeout);
        },

        "replaces global setInterval": function () {
            this.clock = sinon.useFakeTimers();
            var stub = sinon.stub.create();

            setInterval(stub, 500);
            this.clock.tick(1000);

            assert(stub.calledTwice);
        },

        "replaces global clearInterval": function () {
            this.clock = sinon.useFakeTimers();
            var stub = sinon.stub.create();

            clearInterval(setInterval(stub, 500));
            this.clock.tick(1000);

            assert.isFalse(stub.called);
        },

        "restores global setInterval": function () {
            this.clock = sinon.useFakeTimers();
            var stub = sinon.stub.create();
            this.clock.restore();

            this.timer = setInterval(stub, 1000);
            this.clock.tick(1000);

            assert.isFalse(stub.called);
            assert.same(setInterval, sinon.timers.setInterval);
        },

        "restores global clearInterval": function () {
            this.clock = sinon.useFakeTimers();
            sinon.stub.create();
            this.clock.restore();

            assert.same(clearInterval, sinon.timers.clearInterval);
        },

        "//deletes global property if it originally did not have own property": 
        "Not quite sure why this test was initially commented out - TODO: Fix"
        /*function () {
            // remove this properties from the global object ("hasOwnProperty" false)
            // delete this.global.clearTimeout;
            // delete this.global.setInterval;
            // // set these properties to the global object ("hasOwnProperty" true)
            // this.global.clearInterval = this.global.clearInterval;
            // this.global.setTimeout = this.global.clearInterval;

            // this.clock = sinon.useFakeTimers();
            // this.clock.restore();

            // // these properties should be removed from the global object directly.
            // assert.isFalse(this.global.hasOwnProperty("clearTimeout"));
            // assert.isFalse(this.global.hasOwnProperty("setInterval"));

            // // these properties should be added back into the global object directly.
            // assert(this.global.hasOwnProperty("clearInterval"));
            // assert(this.global.hasOwnProperty("setTimeout"));
        }*/,

        "fakes Date constructor": function () {
            this.clock = sinon.useFakeTimers(0);
            var now = new Date();

            refute.same(Date, sinon.timers.Date);
            assert.equals(now.getTime(), 0);
        },

        "fake Date constructor should mirror Date's properties": function () {
            this.clock = sinon.useFakeTimers(0);

            assert(!!Date.parse);
            assert(!!Date.UTC);
        },

        "decide on Date.now support at call-time when supported": function () {
            this.global.Date.now = function () {};
            this.clock = sinon.useFakeTimers(0);

            assert.equals(typeof Date.now, "function");
        },

        "decide on Date.now support at call-time when unsupported": function () {
            this.global.Date.now = null;
            this.clock = sinon.useFakeTimers(0);

            refute.defined(Date.now);
        },

        "restores Date constructor": function () {
            this.clock = sinon.useFakeTimers(0);
            this.clock.restore();

            assert.same(Date, sinon.timers.Date);
        },

        "fakes provided methods": function () {
            this.clock = sinon.useFakeTimers("setTimeout", "Date");

            refute.same(setTimeout, sinon.timers.setTimeout);
            refute.same(Date, sinon.timers.Date);
        },

        "resets faked methods": function () {
            this.clock = sinon.useFakeTimers("setTimeout", "Date");
            this.clock.restore();

            assert.same(setTimeout, sinon.timers.setTimeout);
            assert.same(Date, sinon.timers.Date);
        },

        "does not fake methods not provided": function () {
            this.clock = sinon.useFakeTimers("setTimeout", "Date");

            assert.same(clearTimeout, sinon.timers.clearTimeout);
            assert.same(setInterval, sinon.timers.setInterval);
            assert.same(clearInterval, sinon.timers.clearInterval);
        },

        "does not be able to use date object for now": function () {
            assert.exception(function () {
                sinon.useFakeTimers(new Date(2011, 9, 1));
            });
        }
    }
});
