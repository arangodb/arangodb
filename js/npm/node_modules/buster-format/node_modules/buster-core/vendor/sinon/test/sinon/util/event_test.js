/*jslint onevar: false, eqeqeq: false, browser: true*/
/*globals window sinon buster*/
/**
 * @author Christian Johansen (christian@cjohansen.no)
 * @license BSD
 *
 * Copyright (c) 2011-2012 Christian Johansen
 */
"use strict";

buster.testCase("sinon.EventTarget", {
    setUp: function () {
        this.target = sinon.extend({}, sinon.EventTarget);
    },

    "notifies event listener": function () {
        var listener = sinon.spy();
        this.target.addEventListener("dummy", listener);

        var event = new sinon.Event("dummy");
        this.target.dispatchEvent(event);

        assert(listener.calledOnce);
        assert(listener.calledWith(event));
    },

    "notifies event listener with target as this": function () {
        var listener = sinon.spy();
        this.target.addEventListener("dummy", listener);

        var event = new sinon.Event("dummy");
        this.target.dispatchEvent(event);

        assert(listener.calledOn(this.target));
    },

    "notifies all event listeners": function () {
        var listeners = [sinon.spy(), sinon.spy()];
        this.target.addEventListener("dummy", listeners[0]);
        this.target.addEventListener("dummy", listeners[1]);

        var event = new sinon.Event("dummy");
        this.target.dispatchEvent(event);

        assert(listeners[0].calledOnce);
        assert(listeners[0].calledOnce);
    },

    "notifies event listener of type listener": function () {
        var listener = { handleEvent: sinon.spy() };
        this.target.addEventListener("dummy", listener);

        this.target.dispatchEvent(new sinon.Event("dummy"));

        assert(listener.handleEvent.calledOnce);
    },

    "does not notify listeners of other events": function () {
        var listeners = [sinon.spy(), sinon.spy()];
        this.target.addEventListener("dummy", listeners[0]);
        this.target.addEventListener("other", listeners[1]);

        this.target.dispatchEvent(new sinon.Event("dummy"));

        assert.isFalse(listeners[1].called);
    },

    "does not notify unregistered listeners": function () {
        var listener = sinon.spy();
        this.target.addEventListener("dummy", listener);
        this.target.removeEventListener("dummy", listener);

        this.target.dispatchEvent(new sinon.Event("dummy"));

        assert.isFalse(listener.called);
    },

    "notifies existing listeners after removing one": function () {
        var listeners = [sinon.spy(), sinon.spy(), sinon.spy()];
        this.target.addEventListener("dummy", listeners[0]);
        this.target.addEventListener("dummy", listeners[1]);
        this.target.addEventListener("dummy", listeners[2]);
        this.target.removeEventListener("dummy", listeners[1]);

        this.target.dispatchEvent(new sinon.Event("dummy"));

        assert(listeners[0].calledOnce);
        assert(listeners[2].calledOnce);
    },

    "returns false when event.preventDefault is not called": function () {
        this.target.addEventListener("dummy", sinon.spy());

        var event = new sinon.Event("dummy");
        var result = this.target.dispatchEvent(event);

        assert.isFalse(result);
    },

    "returns true when event.preventDefault is called": function () {
        this.target.addEventListener("dummy", function (e) {
            e.preventDefault();
        });

        var result = this.target.dispatchEvent(new sinon.Event("dummy"));

        assert.isTrue(result);
    }
});
