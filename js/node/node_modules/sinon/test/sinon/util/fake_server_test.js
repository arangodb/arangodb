/*jslint onevar: false, browser: false, regexp: false, browser: true*/
/*globals sinon window buster*/
/**
 * @author Christian Johansen (christian@cjohansen.no)
 * @license BSD
 *
 * Copyright (c) 2010-2012 Christian Johansen
 */
"use strict";

buster.testCase("sinon.fakeServer", {
    tearDown: function () {
        if (this.server) {
            this.server.restore();
        }
    },

    "provides restore method": function () {
        this.server = sinon.fakeServer.create();

        assert.isFunction(this.server.restore);
    },

    "fakes XMLHttpRequest": sinon.test(function () {
        this.stub(sinon, "useFakeXMLHttpRequest").returns({
            restore: this.stub()
        });

        this.server = sinon.fakeServer.create();

        assert(sinon.useFakeXMLHttpRequest.called);
    }),

    "mirrors FakeXMLHttpRequest restore method": sinon.test(function () {
        this.server = sinon.fakeServer.create();
        var restore = this.stub(sinon.FakeXMLHttpRequest, "restore");
        this.server.restore();

        assert(restore.called);
    }),

    "requests": {
        setUp: function () {
            this.server = sinon.fakeServer.create();
        },

        tearDown: function () {
            this.server.restore();
        },

        "collects objects created with fake XHR": function () {
            var xhrs = [new sinon.FakeXMLHttpRequest(), new sinon.FakeXMLHttpRequest()];

            assert.equals(this.server.requests, xhrs);
        },

        "collects xhr objects through addRequest": function () {
            this.server.addRequest = sinon.spy();
            var xhr = new sinon.FakeXMLHttpRequest();

            assert(this.server.addRequest.calledWith(xhr));
        },

        "observes onSend on requests": function () {
            var xhrs = [new sinon.FakeXMLHttpRequest(), new sinon.FakeXMLHttpRequest()];

            assert.isFunction(xhrs[0].onSend);
            assert.isFunction(xhrs[1].onSend);
        },

        "onSend should call handleRequest with request object": function () {
            var xhr = new sinon.FakeXMLHttpRequest();
            xhr.open("GET", "/");
            sinon.spy(this.server, "handleRequest");

            xhr.send();

            assert(this.server.handleRequest.called);
            assert(this.server.handleRequest.calledWith(xhr));
        }
    },

    "handleRequest": {
        setUp: function () {
            this.server = sinon.fakeServer.create();
        },

        tearDown: function () {
            this.server.restore();
        },

        "responds to synchronous requests": function () {
            var xhr = new sinon.FakeXMLHttpRequest();
            xhr.open("GET", "/", false);
            sinon.spy(xhr, "respond");

            xhr.send();

            assert(xhr.respond.called);
        },

        "does not respond to async requests": function () {
            var xhr = new sinon.FakeXMLHttpRequest();
            xhr.open("GET", "/", true);
            sinon.spy(xhr, "respond");

            xhr.send();

            assert.isFalse(xhr.respond.called);
        }
    },

    "respondWith": {
        setUp: function () {
            this.server = sinon.fakeServer.create();

            this.getRootAsync = new sinon.FakeXMLHttpRequest();
            this.getRootAsync.open("GET", "/", true);
            this.getRootAsync.send();
            sinon.spy(this.getRootAsync, "respond");

            this.postRootAsync = new sinon.FakeXMLHttpRequest();
            this.postRootAsync.open("POST", "/", true);
            this.postRootAsync.send();
            sinon.spy(this.postRootAsync, "respond");

            this.getRootSync = new sinon.FakeXMLHttpRequest();
            this.getRootSync.open("GET", "/", false);

            this.getPathAsync = new sinon.FakeXMLHttpRequest();
            this.getPathAsync.open("GET", "/path", true);
            this.getPathAsync.send();
            sinon.spy(this.getPathAsync, "respond");

            this.postPathAsync = new sinon.FakeXMLHttpRequest();
            this.postPathAsync.open("POST", "/path", true);
            this.postPathAsync.send();
            sinon.spy(this.postPathAsync, "respond");
        },

        tearDown: function () {
            this.server.restore();
        },

        "responds to queued async requests": function () {
            this.server.respondWith("Oh yeah! Duffman!");

            this.server.respond();

            assert(this.getRootAsync.respond.called);
            assert.equals(this.getRootAsync.respond.args[0], [200, {}, "Oh yeah! Duffman!"]);
        },

        "responds to all queued async requests": function () {
            this.server.respondWith("Oh yeah! Duffman!");

            this.server.respond();

            assert(this.getRootAsync.respond.called);
            assert(this.getPathAsync.respond.called);
        },

        "does not respond to requests queued after respond() (eg from callbacks)": function () {
            var xhr;
            this.getRootAsync.addEventListener("load", function () {
                xhr = new sinon.FakeXMLHttpRequest();
                xhr.open("GET", "/", true);
                xhr.send();
                sinon.spy(xhr, "respond");
            });

            this.server.respondWith("Oh yeah! Duffman!");

            this.server.respond();

            assert(this.getRootAsync.respond.called);
            assert(this.getPathAsync.respond.called);
            assert(!xhr.respond.called);

            this.server.respond();

            assert(xhr.respond.called);
        },

        "responds with status, headers, and body": function () {
            var headers = { "Content-Type": "X-test" };
            this.server.respondWith([201, headers, "Oh yeah!"]);

            this.server.respond();

            assert.equals(this.getRootAsync.respond.args[0], [201, headers, "Oh yeah!"]);
        },

        "handles responding with empty queue": function () {
            delete this.server.queue;
            var server = this.server;

            refute.exception(function () {
                server.respond();
            });
        },

        "responds to sync request with canned answers": function () {
            this.server.respondWith([210, { "X-Ops": "Yeah" }, "Body, man"]);

            this.getRootSync.send();

            assert.equals(this.getRootSync.status, 210);
            assert.equals(this.getRootSync.getAllResponseHeaders(), "X-Ops: Yeah\r\n");
            assert.equals(this.getRootSync.responseText, "Body, man");
        },

        "responds to sync request with 404 if no response is set": function () {
            this.getRootSync.send();

            assert.equals(this.getRootSync.status, 404);
            assert.equals(this.getRootSync.getAllResponseHeaders(), "");
            assert.equals(this.getRootSync.responseText, "");
        },

        "responds to async request with 404 if no response is set": function () {
            this.server.respond();

            assert.equals(this.getRootAsync.respond.args[0], [404, {}, ""]);
        },

        "responds to specific URL": function () {
            this.server.respondWith("/path", "Duffman likes Duff beer");

            this.server.respond();

            assert.equals(this.getRootAsync.respond.args[0], [404, {}, ""]);
            assert.equals(this.getPathAsync.respond.args[0], [200, {}, "Duffman likes Duff beer"]);
        },

        "responds to URL matched by regexp": function () {
            this.server.respondWith(/^\/p.*/, "Regexp");

            this.server.respond();

            assert.equals(this.getPathAsync.respond.args[0], [200, {}, "Regexp"]);
        },

        "does not respond to URL not matched by regexp": function () {
            this.server.respondWith(/^\/p.*/, "No regexp match");

            this.server.respond();

            assert.equals(this.getRootAsync.respond.args[0], [404, {}, ""]);
        },

        "responds to all URLs matched by regexp": function () {
            this.server.respondWith(/^\/.*/, "Match all URLs");

            this.server.respond();

            assert.equals(this.getRootAsync.respond.args[0], [200, {}, "Match all URLs"]);
            assert.equals(this.getPathAsync.respond.args[0], [200, {}, "Match all URLs"]);
        },

        "responds to all requests when match URL is falsy": function () {
            this.server.respondWith("", "Falsy URL");

            this.server.respond();

            assert.equals(this.getRootAsync.respond.args[0], [200, {}, "Falsy URL"]);
            assert.equals(this.getPathAsync.respond.args[0], [200, {}, "Falsy URL"]);
        },

        "responds to all GET requests": function () {
            this.server.respondWith("GET", "", "All GETs");

            this.server.respond();

            assert.equals(this.getRootAsync.respond.args[0], [200, {}, "All GETs"]);
            assert.equals(this.getPathAsync.respond.args[0], [200, {}, "All GETs"]);
            assert.equals(this.postRootAsync.respond.args[0], [404, {}, ""]);
            assert.equals(this.postPathAsync.respond.args[0], [404, {}, ""]);
        },

        "responds to all 'get' requests (case-insensitivity)": function () {
            this.server.respondWith("get", "", "All GETs");

            this.server.respond();

            assert.equals(this.getRootAsync.respond.args[0], [200, {}, "All GETs"]);
            assert.equals(this.getPathAsync.respond.args[0], [200, {}, "All GETs"]);
            assert.equals(this.postRootAsync.respond.args[0], [404, {}, ""]);
            assert.equals(this.postPathAsync.respond.args[0], [404, {}, ""]);
        },

        "responds to all PUT requests": function () {
            this.server.respondWith("PUT", "", "All PUTs");

            this.server.respond();

            assert.equals(this.getRootAsync.respond.args[0], [404, {}, ""]);
            assert.equals(this.getPathAsync.respond.args[0], [404, {}, ""]);
            assert.equals(this.postRootAsync.respond.args[0], [404, {}, ""]);
            assert.equals(this.postPathAsync.respond.args[0], [404, {}, ""]);
        },

        "responds to all POST requests": function () {
            this.server.respondWith("POST", "", "All POSTs");

            this.server.respond();

            assert.equals(this.getRootAsync.respond.args[0], [404, {}, ""]);
            assert.equals(this.getPathAsync.respond.args[0], [404, {}, ""]);
            assert.equals(this.postRootAsync.respond.args[0], [200, {}, "All POSTs"]);
            assert.equals(this.postPathAsync.respond.args[0], [200, {}, "All POSTs"]);
        },

        "responds to all POST requests to /path": function () {
            this.server.respondWith("POST", "/path", "All POSTs");

            this.server.respond();

            assert.equals(this.getRootAsync.respond.args[0], [404, {}, ""]);
            assert.equals(this.getPathAsync.respond.args[0], [404, {}, ""]);
            assert.equals(this.postRootAsync.respond.args[0], [404, {}, ""]);
            assert.equals(this.postPathAsync.respond.args[0], [200, {}, "All POSTs"]);
        },

        "responds to all POST requests matching regexp": function () {
            this.server.respondWith("POST", /^\/path(\?.*)?/, "All POSTs");

            this.server.respond();

            assert.equals(this.getRootAsync.respond.args[0], [404, {}, ""]);
            assert.equals(this.getPathAsync.respond.args[0], [404, {}, ""]);
            assert.equals(this.postRootAsync.respond.args[0], [404, {}, ""]);
            assert.equals(this.postPathAsync.respond.args[0], [200, {}, "All POSTs"]);
        },

        "does not respond to aborted requests": function () {
            this.server.respondWith("/", "That's my homepage!");
            this.getRootAsync.aborted = true;

            this.server.respond();

            assert.isFalse(this.getRootAsync.respond.called);
        },

        "resets requests": function () {
            this.server.respondWith("/", "That's my homepage!");

            this.server.respond();

            assert.equals(this.server.queue, []);
        },

        "notifies all requests when some throw": function () {
            this.getRootAsync.respond = function () {
                throw new Error("Oops!");
            };

            this.server.respondWith("");
            this.server.respond();

            assert.equals(this.getPathAsync.respond.args[0], [200, {}, ""]);
            assert.equals(this.postRootAsync.respond.args[0], [200, {}, ""]);
            assert.equals(this.postPathAsync.respond.args[0], [200, {}, ""]);
        },

        "recognizes request with hostname": function () {
            this.server.respondWith("/", [200, {}, "Yep"]);
            var xhr = new sinon.FakeXMLHttpRequest();
            var loc = window.location;
            xhr.open("GET", loc.protocol + "//" + loc.host + "/", true);
            xhr.send();
            sinon.spy(xhr, "respond");

            this.server.respond();

            assert.equals(xhr.respond.args[0], [200, {}, "Yep"]);
        },

        "throws understandable error if response is not a string": function () {
            var error;

            try {
                this.server.respondWith("/", {});
            } catch (e) {
                error = e;
            }

            assert.isObject(error);
            assert.equals(error.message, "Fake server response body should be string, but was object");
        },

        "throws understandable error if response in array is not a string": function () {
            var error;

            try {
                this.server.respondWith("/", [200, {}]);
            } catch (e) {
                error = e;
            }

            assert.isObject(error);
            assert.equals(error.message, "Fake server response body should be string, but was undefined");
        },

        "is able to pass the same args to respond directly": function () {
            this.server.respond("Oh yeah! Duffman!");

            assert.equals(this.getRootAsync.respond.args[0], [200, {}, "Oh yeah! Duffman!"]);
            assert.equals(this.getPathAsync.respond.args[0], [200, {}, "Oh yeah! Duffman!"]);
            assert.equals(this.postRootAsync.respond.args[0], [200, {}, "Oh yeah! Duffman!"]);
            assert.equals(this.postPathAsync.respond.args[0], [200, {}, "Oh yeah! Duffman!"]);
        },

        "responds to most recently defined match": function () {
            this.server.respondWith("POST", "", "All POSTs");
            this.server.respondWith("POST", "/path", "Particular POST");

            this.server.respond();

            assert.equals(this.postRootAsync.respond.args[0], [200, {}, "All POSTs"]);
            assert.equals(this.postPathAsync.respond.args[0], [200, {}, "Particular POST"]);
        }
    },

    "respondWithFunctionHandler": {
        setUp: function () {
            this.server = sinon.fakeServer.create();
        },

        tearDown: function () {
            this.server.restore();
        },

        "yields response to request function handler": function () {
            var handler = sinon.spy();
            this.server.respondWith("/hello", handler);
            var xhr = new sinon.FakeXMLHttpRequest();
            xhr.open("GET", "/hello");
            xhr.send();

            this.server.respond();

            assert(handler.calledOnce);
            assert(handler.calledWith(xhr));
        },

        "responds to request from function handler": function () {
            this.server.respondWith("/hello", function (xhr) {
                xhr.respond(200, { "Content-Type": "application/json" }, '{"id":42}');
            });

            var request = new sinon.FakeXMLHttpRequest();
            request.open("GET", "/hello");
            request.send();

            this.server.respond();

            assert.equals(request.status, 200);
            assert.equals(request.responseHeaders, { "Content-Type": "application/json" });
            assert.equals(request.responseText, '{"id":42}');
        },

        "yields response to request function handler when method matches": function () {
            var handler = sinon.spy();
            this.server.respondWith("GET", "/hello", handler);
            var xhr = new sinon.FakeXMLHttpRequest();
            xhr.open("GET", "/hello");
            xhr.send();

            this.server.respond();

            assert(handler.calledOnce);
        },

        "yields response to request function handler when url contains RegExp characters": function () {
            var handler = sinon.spy();
            this.server.respondWith("GET", "/hello?world", handler);
            var xhr = new sinon.FakeXMLHttpRequest();
            xhr.open("GET", "/hello?world");
            xhr.send();

            this.server.respond();

            assert(handler.calledOnce);
        },

        "does not yield response to request function handler when method does not match": function () {
            var handler = sinon.spy();
            this.server.respondWith("GET", "/hello", handler);
            var xhr = new sinon.FakeXMLHttpRequest();
            xhr.open("POST", "/hello");
            xhr.send();

            this.server.respond();

            assert(!handler.called);
        },

        "yields response to request function handler when regexp url matches": function () {
            var handler = sinon.spy();
            this.server.respondWith("GET", /\/.*/, handler);
            var xhr = new sinon.FakeXMLHttpRequest();
            xhr.open("GET", "/hello");
            xhr.send();

            this.server.respond();

            assert(handler.calledOnce);
        },

        "does not yield response to request function handler when regexp url does not match": function () {
            var handler = sinon.spy();
            this.server.respondWith("GET", /\/a.*/, handler);
            var xhr = new sinon.FakeXMLHttpRequest();
            xhr.open("GET", "/hello");
            xhr.send();

            this.server.respond();

            assert(!handler.called);
        },

        "adds function handler without method or url filter": function () {
            this.server.respondWith(function (xhr) {
                xhr.respond(200, { "Content-Type": "application/json" }, '{"id":42}');
            });

            var request = new sinon.FakeXMLHttpRequest();
            request.open("GET", "/whatever");
            request.send();

            this.server.respond();

            assert.equals(request.status, 200);
            assert.equals(request.responseHeaders, { "Content-Type": "application/json" });
            assert.equals(request.responseText, '{"id":42}');
        },

        "does not process request further if processed by function": function () {
            var handler = sinon.spy();
            this.server.respondWith("GET", "/aloha", [200, {}, "Oh hi"]);
            this.server.respondWith("GET", /\/a.*/, handler);
            var xhr = new sinon.FakeXMLHttpRequest();
            xhr.respond = sinon.spy();
            xhr.open("GET", "/aloha");
            xhr.send();

            this.server.respond();

            assert(handler.called);
            assert(xhr.respond.calledOnce);
        },

        "yields URL capture groups to response handler": function () {
            var handler = sinon.spy();
            this.server.respondWith("GET", /\/people\/(\d+)/, handler);
            var xhr = new sinon.FakeXMLHttpRequest();
            xhr.respond = sinon.spy();
            xhr.open("GET", "/people/3");
            xhr.send();

            this.server.respond();

            assert(handler.called);
            assert.equals(handler.args[0], [xhr, 3]);
        }
    },

    "respondFakeHTTPVerb": {
        setUp: function () {
            this.server = sinon.fakeServer.create();

            this.request = new sinon.FakeXMLHttpRequest();
            this.request.open("post", "/path", true);
            this.request.send("_method=delete");
            sinon.spy(this.request, "respond");
        },

        tearDown: function () {
            this.server.restore();
        },

        "does not respond to DELETE request with _method parameter": function () {
            this.server.respondWith("DELETE", "", "");

            this.server.respond();

            assert.equals(this.request.respond.args[0], [404, {}, ""]);
        },

        "responds to 'fake' DELETE request": function () {
            this.server.fakeHTTPMethods = true;
            this.server.respondWith("DELETE", "", "OK");

            this.server.respond();

            assert.equals(this.request.respond.args[0], [200, {}, "OK"]);
        },

        "does not respond to POST when faking DELETE": function () {
            this.server.fakeHTTPMethods = true;
            this.server.respondWith("POST", "", "OK");

            this.server.respond();

            assert.equals(this.request.respond.args[0], [404, {}, ""]);
        },

        "does not fake method when not POSTing": function () {
            this.server.fakeHTTPMethods = true;
            this.server.respondWith("DELETE", "", "OK");

            var request = new sinon.FakeXMLHttpRequest();
            request.open("GET", "/");
            request.send();
            request.respond = sinon.spy();
            this.server.respond();

            assert.equals(request.respond.args[0], [404, {}, ""]);
        },

        "customizes HTTP method extraction": function () {
            this.server.getHTTPMethod = function (request) {
                return "PUT";
            };

            this.server.respondWith("PUT", "", "OK");

            this.server.respond();

            assert.equals(this.request.respond.args[0], [200, {}, "OK"]);
        },

        "does not fail when getting the HTTP method from a request with no body": function () {
            var server = this.server;
            server.fakeHTTPMethods = true;

            assert.equals(server.getHTTPMethod({ method: "POST" }), "POST");
        }
    },

    "autoResponse": {
        setUp: function () {
            this.get = function get(url) {
                var request = new sinon.FakeXMLHttpRequest();
                sinon.spy(request, "respond");
                request.open("get", url, true);
                request.send();
                return request;
            };

            this.server = sinon.fakeServer.create();
            this.clock = sinon.useFakeTimers();
        },

        tearDown: function () {
            this.server.restore();
            this.clock.restore();
        },

        "responds async automatically after 10ms": function () {
            this.server.autoRespond = true;
            var request = this.get("/path");

            this.clock.tick(10);

            assert.isTrue(request.respond.calledOnce);
        },

        "normal server does not respond automatically": function () {
            var request = this.get("/path");

            this.clock.tick(100);

            assert.isTrue(!request.respond.called);
        },

        "auto-responds only once": function () {
            this.server.autoRespond = true;
            var requests = [this.get("/path")];
            this.clock.tick(5);
            requests.push(this.get("/other"));
            this.clock.tick(5);

            assert.isTrue(requests[0].respond.calledOnce);
            assert.isTrue(requests[1].respond.calledOnce);
        },

        "auto-responds after having already responded": function () {
            this.server.autoRespond = true;
            var requests = [this.get("/path")];
            this.clock.tick(10);
            requests.push(this.get("/other"));
            this.clock.tick(10);

            assert.isTrue(requests[0].respond.calledOnce);
            assert.isTrue(requests[1].respond.calledOnce);
        },

        "sets auto-respond timeout to 50ms": function () {
            this.server.autoRespond = true;
            this.server.autoRespondAfter = 50;

            var request = this.get("/path");
            this.clock.tick(49);
            assert.isFalse(request.respond.called);

            this.clock.tick(1);
            assert.isTrue(request.respond.calledOnce);
        },

        "auto-responds if two successive requests are made with a single XHR": function () {
            this.server.autoRespond = true;

            var request = this.get("/path");

            this.clock.tick(10);

            assert.isTrue(request.respond.calledOnce);

            request.open("get", "/other", true);
            request.send();

            this.clock.tick(10);

            assert.isTrue(request.respond.calledTwice);
        },

        "auto-responds if timeout elapses between creating a XHR object and sending a request with it": function () {
            this.server.autoRespond = true;

            var request = new sinon.FakeXMLHttpRequest();
            sinon.spy(request, "respond");

            this.clock.tick(100);

            request.open("get", "/path", true);
            request.send();

            this.clock.tick(10);

            assert.isTrue(request.respond.calledOnce);
        }
    }
});
