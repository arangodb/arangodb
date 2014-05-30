/*jslint onevar: false, eqeqeq: false, browser: true*/
/*globals window XMLHttpRequest ActiveXObject sinon buster*/
/**
 * @author Christian Johansen (christian@cjohansen.no)
 * @license BSD
 *
 * Copyright (c) 2010-2012 Christian Johansen
 */
"use strict";

(function (global) {
    var globalXMLHttpRequest = global.XMLHttpRequest;
    var globalActiveXObject = global.ActiveXObject;

    var fakeXhrSetUp = function () {
        this.fakeXhr = sinon.useFakeXMLHttpRequest();
    };

    var fakeXhrTearDown = function () {
        if (typeof this.fakeXhr.restore == "function") {
            this.fakeXhr.restore();
        }
    };

    var runWithWorkingXHROveride = function(workingXHR,test) {
        try {
            var original = sinon.xhr.workingXHR;
            sinon.xhr.workingXHR = workingXHR;
            test();
        } finally {
            sinon.xhr.workingXHR = original;
        }
    };

    buster.testCase("sinon.FakeXMLHttpRequest", {
        tearDown: function () {
            delete sinon.FakeXMLHttpRequest.onCreate;
        },

        "is constructor": function () {
            assert.isFunction(sinon.FakeXMLHttpRequest);
            assert.same(sinon.FakeXMLHttpRequest.prototype.constructor, sinon.FakeXMLHttpRequest);
        },

        "implements readyState constants": function () {
            assert.same(sinon.FakeXMLHttpRequest.OPENED, 1);
            assert.same(sinon.FakeXMLHttpRequest.HEADERS_RECEIVED, 2);
            assert.same(sinon.FakeXMLHttpRequest.LOADING, 3);
            assert.same(sinon.FakeXMLHttpRequest.DONE, 4);
        },

        "calls onCreate if listener is set": function () {
            var onCreate = sinon.spy();
            sinon.FakeXMLHttpRequest.onCreate = onCreate;

            var xhr = new sinon.FakeXMLHttpRequest();

            assert(onCreate.called);
        },

        "passes new object to onCreate if set": function () {
            var onCreate = sinon.spy();
            sinon.FakeXMLHttpRequest.onCreate = onCreate;

            var xhr = new sinon.FakeXMLHttpRequest();

            assert.same(onCreate.getCall(0).args[0], xhr);
        },

        "withCredentials": {
            setUp: function () {
                this.xhr = new sinon.FakeXMLHttpRequest();
            },

            "property is set if we support standards CORS": function () {
                assert.equals(sinon.xhr.supportsCORS, "withCredentials" in this.xhr);
            }

        },

        "open": {
            setUp: function () {
                this.xhr = new sinon.FakeXMLHttpRequest();
            },

            "is method": function () {
                assert.isFunction(this.xhr.open);
            },

            "sets properties on object": function () {
                this.xhr.open("GET", "/my/url", true, "cjno", "pass");

                assert.equals(this.xhr.method, "GET");
                assert.equals(this.xhr.url, "/my/url");
                assert.isTrue(this.xhr.async);
                assert.equals(this.xhr.username, "cjno");
                assert.equals(this.xhr.password, "pass");

            },

            "is async by default": function () {
                this.xhr.open("GET", "/my/url");

                assert.isTrue(this.xhr.async);
            },

            "sets async to false": function () {
                this.xhr.open("GET", "/my/url", false);

                assert.isFalse(this.xhr.async);
            },

            "sets responseText to null": function () {
                this.xhr.open("GET", "/my/url");

                assert.isNull(this.xhr.responseText);
            },

            "sets requestHeaders to blank object": function () {
                this.xhr.open("GET", "/my/url");

                assert.isObject(this.xhr.requestHeaders);
                assert.equals(this.xhr.requestHeaders, {});
            },

            "sets readyState to OPENED": function () {
                this.xhr.open("GET", "/my/url");

                assert.same(this.xhr.readyState, sinon.FakeXMLHttpRequest.OPENED);
            },

            "sets send flag to false": function () {
                this.xhr.open("GET", "/my/url");

                assert.isFalse(this.xhr.sendFlag);
            },

            "dispatches onreadystatechange with reset state": function () {
                var state = {};

                this.xhr.onreadystatechange = function () {
                    sinon.extend(state, this);
                };

                this.xhr.open("GET", "/my/url");

                assert.equals(state.method, "GET");
                assert.equals(state.url, "/my/url");
                assert.isTrue(state.async);
                refute.defined(state.username);
                refute.defined(state.password);
                assert.isNull(state.responseText);
                refute.defined(state.responseHeaders);
                assert.equals(state.readyState, sinon.FakeXMLHttpRequest.OPENED);
                assert.isFalse(state.sendFlag);
            }
        },

        "setRequestHeader": {
            setUp: function () {
                this.xhr = new sinon.FakeXMLHttpRequest();
                this.xhr.open("GET", "/");
            },

            "throws exception if readyState is not OPENED": function () {
                var xhr = new sinon.FakeXMLHttpRequest();

                assert.exception(function () {
                    xhr.setRequestHeader("X-EY", "No-no");
                });
            },

            "throws exception if send flag is true": function () {
                var xhr = this.xhr;
                xhr.sendFlag = true;

                assert.exception(function () {
                    xhr.setRequestHeader("X-EY", "No-no");
                });
            },

            "disallows unsafe headers": function () {
                var xhr = this.xhr;

                assert.exception(function () {
                    xhr.setRequestHeader("Accept-Charset", "");
                });

                assert.exception(function () {
                    xhr.setRequestHeader("Accept-Encoding", "");
                });

                assert.exception(function () {
                    xhr.setRequestHeader("Connection", "");
                });

                assert.exception(function () {
                    xhr.setRequestHeader("Content-Length", "");
                });

                assert.exception(function () {
                    xhr.setRequestHeader("Cookie", "");
                });

                assert.exception(function () {
                    xhr.setRequestHeader("Cookie2", "");
                });

                assert.exception(function () {
                    xhr.setRequestHeader("Content-Transfer-Encoding", "");
                });

                assert.exception(function () {
                    xhr.setRequestHeader("Date", "");
                });

                assert.exception(function () {
                    xhr.setRequestHeader("Expect", "");
                });

                assert.exception(function () {
                    xhr.setRequestHeader("Host", "");
                });

                assert.exception(function () {
                    xhr.setRequestHeader("Keep-Alive", "");
                });

                assert.exception(function () {
                    xhr.setRequestHeader("Referer", "");
                });

                assert.exception(function () {
                    xhr.setRequestHeader("TE", "");
                });

                assert.exception(function () {
                    xhr.setRequestHeader("Trailer", "");
                });

                assert.exception(function () {
                    xhr.setRequestHeader("Transfer-Encoding", "");
                });

                assert.exception(function () {
                    xhr.setRequestHeader("Upgrade", "");
                });

                assert.exception(function () {
                    xhr.setRequestHeader("User-Agent", "");
                });

                assert.exception(function () {
                    xhr.setRequestHeader("Via", "");
                });

                assert.exception(function () {
                    xhr.setRequestHeader("Proxy-Oops", "");
                });

                assert.exception(function () {
                    xhr.setRequestHeader("Sec-Oops", "");
                });
            },

            "sets header and value": function () {
                this.xhr.setRequestHeader("X-Fake", "Yeah!");

                assert.equals(this.xhr.requestHeaders, { "X-Fake": "Yeah!" });
            },

            "appends same-named header values": function () {
                this.xhr.setRequestHeader("X-Fake", "Oh");
                this.xhr.setRequestHeader("X-Fake", "yeah!");

                assert.equals(this.xhr.requestHeaders, { "X-Fake": "Oh,yeah!" });
            }
        },

        "send": {
            setUp: function () {
                this.xhr = new sinon.FakeXMLHttpRequest();
            },

            "throws if request is not open": function () {
                var xhr = new sinon.FakeXMLHttpRequest();

                assert.exception(function () {
                    xhr.send();
                });
            },

            "throws if send flag is true": function () {
                var xhr = this.xhr;
                xhr.open("GET", "/");
                xhr.sendFlag = true;

                assert.exception(function () {
                    xhr.send();
                });
            },

            "sets GET body to null": function () {
                this.xhr.open("GET", "/");
                this.xhr.send("Data");

                assert.isNull(this.xhr.requestBody);
            },

            "sets HEAD body to null": function () {
                this.xhr.open("HEAD", "/");
                this.xhr.send("Data");

                assert.isNull(this.xhr.requestBody);
            },

            "sets mime to text/plain": function () {
                this.xhr.open("POST", "/");
                this.xhr.send("Data");

                assert.equals(this.xhr.requestHeaders["Content-Type"], "text/plain;charset=utf-8");
            },

            "does not override mime": function () {
                this.xhr.open("POST", "/");
                this.xhr.setRequestHeader("Content-Type", "text/html");
                this.xhr.send("Data");

                assert.equals(this.xhr.requestHeaders["Content-Type"], "text/html;charset=utf-8");
            },

            "sets request body to string data": function () {
                this.xhr.open("POST", "/");
                this.xhr.send("Data");

                assert.equals(this.xhr.requestBody, "Data");
            },

            "sets error flag to false": function () {
                this.xhr.open("POST", "/");
                this.xhr.send("Data");

                assert.isFalse(this.xhr.errorFlag);
            },

            "sets send flag to true": function () {
                this.xhr.open("POST", "/");
                this.xhr.send("Data");

                assert.isTrue(this.xhr.sendFlag);
            },

            "does not set send flag to true if sync": function () {
                this.xhr.open("POST", "/", false);
                this.xhr.send("Data");

                assert.isFalse(this.xhr.sendFlag);
            },

            "dispatches onreadystatechange": function () {
                var state;
                this.xhr.open("POST", "/", false);

                this.xhr.onreadystatechange = function () {
                    state = this.readyState;
                };

                this.xhr.send("Data");

                assert.equals(state, sinon.FakeXMLHttpRequest.OPENED);
            },

            "dispatches event using DOM Event interface": function () {
                var listener = sinon.spy();
                this.xhr.open("POST", "/", false);
                this.xhr.addEventListener("readystatechange", listener);

                this.xhr.send("Data");

                assert(listener.calledOnce);
                assert.equals(listener.args[0][0].type, "readystatechange");
            },

            "dispatches onSend callback if set": function () {
                this.xhr.open("POST", "/", true);
                var callback = sinon.spy();
                this.xhr.onSend = callback;

                this.xhr.send("Data");

                assert(callback.called);
            },

            "dispatches onSend with request as argument": function () {
                this.xhr.open("POST", "/", true);
                var callback = sinon.spy();
                this.xhr.onSend = callback;

                this.xhr.send("Data");

                assert(callback.calledWith(this.xhr));
            },

            "dispatches onSend when async": function () {
                this.xhr.open("POST", "/", false);
                var callback = sinon.spy();
                this.xhr.onSend = callback;

                this.xhr.send("Data");

                assert(callback.calledWith(this.xhr));
            }
        },

        "setResponseHeaders": {
            setUp: function () {
                this.xhr = new sinon.FakeXMLHttpRequest();
            },

            "sets request headers": function () {
                var object = { id: 42 };
                this.xhr.open("GET", "/");
                this.xhr.send();
                this.xhr.setResponseHeaders(object);

                assert.equals(this.xhr.responseHeaders, object);
            },

            "calls readyStateChange with HEADERS_RECEIVED": function () {
                var object = { id: 42 };
                this.xhr.open("GET", "/");
                this.xhr.send();
                var spy = this.xhr.readyStateChange = sinon.spy();

                this.xhr.setResponseHeaders(object);

                assert(spy.calledWith(sinon.FakeXMLHttpRequest.HEADERS_RECEIVED));
            },

            "does not call readyStateChange if sync": function () {
                var object = { id: 42 };
                this.xhr.open("GET", "/", false);
                this.xhr.send();
                var spy = this.xhr.readyStateChange = sinon.spy();

                this.xhr.setResponseHeaders(object);

                assert.isFalse(spy.called);
            },

            "changes readyState to HEADERS_RECEIVED if sync": function () {
                var object = { id: 42 };
                this.xhr.open("GET", "/", false);
                this.xhr.send();

                this.xhr.setResponseHeaders(object);

                assert.equals(this.xhr.readyState, sinon.FakeXMLHttpRequest.HEADERS_RECEIVED);
            },

            "throws if headers were already set": function () {
                var xhr = this.xhr;

                xhr.open("GET", "/", false);
                xhr.send();
                xhr.setResponseHeaders({});

                assert.exception(function () {
                    xhr.setResponseHeaders({});
                });
            }
        },

        "setResponseBodyAsync": {
            setUp: function () {
                this.xhr = new sinon.FakeXMLHttpRequest();
                this.xhr.open("GET", "/");
                this.xhr.send();
                this.xhr.setResponseHeaders({});
            },

            "invokes onreadystatechange handler with LOADING state": function () {
                var spy = sinon.spy();
                this.xhr.readyStateChange = spy;

                this.xhr.setResponseBody("Some text goes in here ok?");

                assert(spy.calledWith(sinon.FakeXMLHttpRequest.LOADING));
            },

            "invokes onreadystatechange handler for each 10 byte chunk": function () {
                var spy = sinon.spy();
                this.xhr.readyStateChange = spy;
                this.xhr.chunkSize = 10;

                this.xhr.setResponseBody("Some text goes in here ok?");

                assert.equals(spy.callCount, 4);
            },

            "invokes onreadystatechange handler for each x byte chunk": function () {
                var spy = sinon.spy();
                this.xhr.readyStateChange = spy;
                this.xhr.chunkSize = 20;

                this.xhr.setResponseBody("Some text goes in here ok?");

                assert.equals(spy.callCount, 3);
            },

            "invokes onreadystatechange handler with partial data": function () {
                var pieces = [];

                var spy = sinon.spy(function () {
                    pieces.push(this.responseText);
                });

                this.xhr.readyStateChange = spy;
                this.xhr.chunkSize = 9;

                this.xhr.setResponseBody("Some text goes in here ok?");

                assert.equals(pieces[1], "Some text");
            },

            "calls onreadystatechange with DONE state": function () {
                var spy = sinon.spy();
                this.xhr.readyStateChange = spy;

                this.xhr.setResponseBody("Some text goes in here ok?");

                assert(spy.calledWith(sinon.FakeXMLHttpRequest.DONE));
            },

            "throws if not open": function () {
                var xhr = new sinon.FakeXMLHttpRequest();

                assert.exception(function () {
                    xhr.setResponseBody("");
                });
            },

            "throws if no headers received": function () {
                var xhr = new sinon.FakeXMLHttpRequest();
                xhr.open("GET", "/");
                xhr.send();

                assert.exception(function () {
                    xhr.setResponseBody("");
                });
            },

            "throws if body was already sent": function () {
                var xhr = new sinon.FakeXMLHttpRequest();
                xhr.open("GET", "/");
                xhr.send();
                xhr.setResponseHeaders({});
                xhr.setResponseBody("");

                assert.exception(function () {
                    xhr.setResponseBody("");
                });
            },

            "throws if body is not a string": function () {
                var xhr = new sinon.FakeXMLHttpRequest();
                xhr.open("GET", "/");
                xhr.send();
                xhr.setResponseHeaders({});

                assert.exception(function () {
                    xhr.setResponseBody({});
                }, "InvalidBodyException");
            }
        },

        "setResponseBodySync": {
            setUp: function () {
                this.xhr = new sinon.FakeXMLHttpRequest();
                this.xhr.open("GET", "/", false);
                this.xhr.send();
                this.xhr.setResponseHeaders({});
            },

            "does not throw": function () {
                var xhr = this.xhr;

                refute.exception(function () {
                    xhr.setResponseBody("");
                });
            },

            "sets readyState to DONE": function () {
                this.xhr.setResponseBody("");

                assert.equals(this.xhr.readyState, sinon.FakeXMLHttpRequest.DONE);
            },

            "throws if responding to request twice": function () {
                var xhr = this.xhr;
                this.xhr.setResponseBody("");

                assert.exception(function () {
                    xhr.setResponseBody("");
                });
            },

            "does not call onreadystatechange for sync request": function () {
                var xhr = new sinon.FakeXMLHttpRequest();
                var spy = sinon.spy();
                xhr.onreadystatechange = spy;
                xhr.open("GET", "/", false);
                xhr.send();
                var callCount = spy.callCount;

                xhr.setResponseHeaders({});
                xhr.setResponseBody("");

                assert.equals(callCount - spy.callCount, 0);
            },

            "simulates synchronous request": function () {
                var xhr = new sinon.FakeXMLHttpRequest();

                xhr.onSend = function () {
                    this.setResponseHeaders({});
                    this.setResponseBody("Oh yeah");
                };

                xhr.open("GET", "/", false);
                xhr.send();

                assert.equals(xhr.responseText, "Oh yeah");
            }
        },

        "respond": {
            setUp: function () {
                this.xhr = new sinon.FakeXMLHttpRequest();
                this.xhr.open("GET", "/");
                var spy = this.spy = sinon.spy();

                this.xhr.onreadystatechange = function () {
                    if (this.readyState == 4) {
                        spy.call(this);
                    }
                };

                this.xhr.send();
            },

            "fire onload event": function () {
                this.onload = this.spy;
                this.xhr.respond(200, {}, "");
                assert.equals(this.spy.callCount, 1);
            },

            "fire onload event with this set to the XHR object": function (done) {
                var xhr = new sinon.FakeXMLHttpRequest();
                xhr.open("GET", "/");

                xhr.onload = function() {
                    assert.same(this, xhr);

                    done();
                };

                xhr.send();
                xhr.respond(200, {}, "");
            },

            "calls readystate handler with readyState DONE once": function () {
                this.xhr.respond(200, {}, "");

                assert.equals(this.spy.callCount, 1);
            },

            "defaults to status 200, no headers, and blank body": function () {
                this.xhr.respond();

                assert.equals(this.xhr.status, 200);
                assert.equals(this.xhr.getAllResponseHeaders(), "");
                assert.equals(this.xhr.responseText, "");
            },

            "sets status": function () {
                this.xhr.respond(201);

                assert.equals(this.xhr.status, 201);
            },

            "sets status text": function () {
                this.xhr.respond(201);

                assert.equals(this.xhr.statusText, "Created");
            },

            "sets headers": function () {
                sinon.spy(this.xhr, "setResponseHeaders");
                var responseHeaders = { some: "header", value: "over here" };
                this.xhr.respond(200, responseHeaders);

                assert.equals(this.xhr.setResponseHeaders.args[0][0], responseHeaders);
            },

            "sets response text": function () {
                this.xhr.respond(200, {}, "'tis some body text");

                assert.equals(this.xhr.responseText, "'tis some body text");
            },

            "completes request when onreadystatechange fails": function () {
                this.xhr.onreadystatechange = sinon.stub().throws();
                this.xhr.respond(200, {}, "'tis some body text");

                assert.equals(this.xhr.onreadystatechange.callCount, 4);
            },

            "sets status before transitioning to readyState HEADERS_RECEIVED": function () {
                var status, statusText;
                this.xhr.onreadystatechange = function () {
                    if (this.readyState == 2) {
                        status = this.status;
                        statusText = this.statusText;
                    }
                };
                this.xhr.respond(204);

                assert.equals(status, 204);
                assert.equals(statusText, "No Content");
            }
        },

        "getResponseHeader": {
            setUp: function () {
                this.xhr = new sinon.FakeXMLHttpRequest();
            },

            "returns null if request is not finished": function () {
                this.xhr.open("GET", "/");
                assert.isNull(this.xhr.getResponseHeader("Content-Type"));
            },

            "returns null if header is Set-Cookie": function () {
                this.xhr.open("GET", "/");
                this.xhr.send();

                assert.isNull(this.xhr.getResponseHeader("Set-Cookie"));
            },

            "returns null if header is Set-Cookie2": function () {
                this.xhr.open("GET", "/");
                this.xhr.send();

                assert.isNull(this.xhr.getResponseHeader("Set-Cookie2"));
            },

            "returns header value": function () {
                this.xhr.open("GET", "/");
                this.xhr.send();
                this.xhr.setResponseHeaders({ "Content-Type": "text/html" });

                assert.equals(this.xhr.getResponseHeader("Content-Type"), "text/html");
            },

            "returns header value if sync": function () {
                this.xhr.open("GET", "/", false);
                this.xhr.send();
                this.xhr.setResponseHeaders({ "Content-Type": "text/html" });

                assert.equals(this.xhr.getResponseHeader("Content-Type"), "text/html");
            },

            "returns null if header is not set": function () {
                this.xhr.open("GET", "/");
                this.xhr.send();

                assert.isNull(this.xhr.getResponseHeader("Content-Type"));
            },

            "returns headers case insensitive": function () {
                this.xhr.open("GET", "/");
                this.xhr.send();
                this.xhr.setResponseHeaders({ "Content-Type": "text/html" });

                assert.equals(this.xhr.getResponseHeader("content-type"), "text/html");
            }
        },

        "getAllResponseHeaders": {
            setUp: function () {
                this.xhr = new sinon.FakeXMLHttpRequest();
            },

            "returns empty string if request is not finished": function () {
                this.xhr.open("GET", "/");
                assert.equals(this.xhr.getAllResponseHeaders(), "");
            },

            "does not return Set-Cookie and Set-Cookie2 headers": function () {
                this.xhr.open("GET", "/");
                this.xhr.send();
                this.xhr.setResponseHeaders({
                    "Set-Cookie": "Hey",
                    "Set-Cookie2": "There"
                });

                assert.equals(this.xhr.getAllResponseHeaders(), "");
            },

            "returns headers": function () {
                this.xhr.open("GET", "/");
                this.xhr.send();
                this.xhr.setResponseHeaders({
                    "Content-Type": "text/html",
                    "Set-Cookie2": "There",
                    "Content-Length": "32"
                });

                assert.equals(this.xhr.getAllResponseHeaders(), "Content-Type: text/html\r\nContent-Length: 32\r\n");
            },

            "returns headers if sync": function () {
                this.xhr.open("GET", "/", false);
                this.xhr.send();
                this.xhr.setResponseHeaders({
                    "Content-Type": "text/html",
                    "Set-Cookie2": "There",
                    "Content-Length": "32"
                });

                assert.equals(this.xhr.getAllResponseHeaders(), "Content-Type: text/html\r\nContent-Length: 32\r\n");
            }
        },

        "abort": {
            setUp: function () {
                this.xhr = new sinon.FakeXMLHttpRequest();
            },

            "sets aborted flag to true": function () {
                this.xhr.aborted = true;

                this.xhr.abort();

                assert.isTrue(this.xhr.aborted);
            },


            "sets responseText to null": function () {
                this.xhr.responseText = "Partial data";

                this.xhr.abort();

                assert.isNull(this.xhr.responseText);
            },

            "sets errorFlag to true": function () {
                this.xhr.errorFlag = true;

                this.xhr.abort();

                assert.isTrue(this.xhr.errorFlag);
            },

            "fire onerror event": function () {
                var spy = sinon.spy();
                this.xhr.onerror = spy;
                this.xhr.abort();
                assert.equals(spy.callCount, 1);
            },

            "nulls request headers": function () {
                this.xhr.open("GET", "/");
                this.xhr.setRequestHeader("X-Test", "Sumptn");

                this.xhr.abort();

                assert.equals(this.xhr.requestHeaders, {});
            },

            "sets state to DONE if sent before": function () {
                var readyState;
                this.xhr.open("GET", "/");
                this.xhr.send();

                this.xhr.onreadystatechange = function () {
                    readyState = this.readyState;
                };

                this.xhr.abort();

                assert.equals(readyState, sinon.FakeXMLHttpRequest.DONE);
            },

            "sets send flag to false if sent before": function () {
                this.xhr.open("GET", "/");
                this.xhr.send();

                this.xhr.abort();

                assert.isFalse(this.xhr.sendFlag);
            },

            "dispatches readystatechange event if sent before": function () {
                this.xhr.open("GET", "/");
                this.xhr.send();
                this.xhr.onreadystatechange = sinon.stub();

                this.xhr.abort();

                assert(this.xhr.onreadystatechange.called);
            },

            "sets readyState to unsent if sent before": function () {
                this.xhr.open("GET", "/");
                this.xhr.send();

                this.xhr.abort();

                assert.equals(this.xhr.readyState, sinon.FakeXMLHttpRequest.UNSENT);
            },

            "does not dispatch readystatechange event if readyState is unsent": function () {
                this.xhr.onreadystatechange = sinon.stub();

                this.xhr.abort();

                assert.isFalse(this.xhr.onreadystatechange.called);
            },

            "does not dispatch readystatechange event if readyState is opened but not sent": function () {
                this.xhr.open("GET", "/");
                this.xhr.onreadystatechange = sinon.stub();

                this.xhr.abort();

                assert.isFalse(this.xhr.onreadystatechange.called);
            }
        },

        "responseXML": {
            setUp: function () {
                this.xhr = new sinon.FakeXMLHttpRequest();
            },

            "is initially null": function () {
                this.xhr.open("GET", "/");
                assert.isNull(this.xhr.responseXML);
            },

            "is null when the response body is empty": function () {
                this.xhr.open("GET", "/");
                this.xhr.send();

                this.xhr.respond(200, {}, "");

                assert.isNull(this.xhr.responseXML);
            },

            "parses XML for application/xml": function () {
                this.xhr.open("GET", "/");
                this.xhr.send();

                this.xhr.respond(200, { "Content-Type": "application/xml" },
                                 "<div><h1>Hola!</h1></div>");

                var doc = this.xhr.responseXML;
                var elements = doc.documentElement.getElementsByTagName("h1");
                assert.equals(elements.length, 1);
                assert.equals(elements[0].tagName, "h1");
            },

            "parses XML for text/xml": function () {
                this.xhr.open("GET", "/");
                this.xhr.send();

                this.xhr.respond(200, { "Content-Type": "text/xml" },
                                 "<div><h1>Hola!</h1></div>");

                refute.isNull(this.xhr.responseXML);
            },

            "parses XML for custom xml content type": function () {
                this.xhr.open("GET", "/");
                this.xhr.send();

                this.xhr.respond(200, { "Content-Type": "application/text+xml" },
                                 "<div><h1>Hola!</h1></div>");

                refute.isNull(this.xhr.responseXML);
            },

            "parses XML with no Content-Type": function () {
                this.xhr.open("GET", "/");
                this.xhr.send();

                this.xhr.respond(200, {}, "<div><h1>Hola!</h1></div>");

                var doc = this.xhr.responseXML;
                var elements = doc.documentElement.getElementsByTagName("h1");
                assert.equals(elements.length, 1);
                assert.equals(elements[0].tagName, "h1");
            },

            "does not parse XML with Content-Type text/plain": function () {
                this.xhr.open("GET", "/");
                this.xhr.send();

                this.xhr.respond(200, { "Content-Type": "text/plain" }, "<div></div>");

                assert.isNull(this.xhr.responseXML);
            },

            "does not parse XML with Content-Type text/plain if sync": function () {
                this.xhr.open("GET", "/", false);
                this.xhr.send();

                this.xhr.respond(200, { "Content-Type": "text/plain" }, "<div></div>");

                assert.isNull(this.xhr.responseXML);
            }
        },

        "stub XHR": {
            setUp: fakeXhrSetUp,
            tearDown: fakeXhrTearDown,

            "returns FakeXMLHttpRequest constructor": function () {
                assert.same(this.fakeXhr, sinon.FakeXMLHttpRequest);
            },

            "temporarily blesses FakeXMLHttpRequest with restore method": function () {
                assert.isFunction(this.fakeXhr.restore);
            },

            "calling restore removes temporary method": function () {
                this.fakeXhr.restore();

                refute.defined(this.fakeXhr.restore);
            },

            "removes XMLHttpRequest onCreate listener": function () {
                sinon.FakeXMLHttpRequest.onCreate = function () {};

                this.fakeXhr.restore();

                refute.defined(sinon.FakeXMLHttpRequest.onCreate);
            },

            "optionally keeps XMLHttpRequest onCreate listener": function () {
                var onCreate = function () {};
                sinon.FakeXMLHttpRequest.onCreate = onCreate;

                this.fakeXhr.restore(true);

                assert.same(sinon.FakeXMLHttpRequest.onCreate, onCreate);
            }
        },

        "filtering": {
            setUp: function () {
                sinon.FakeXMLHttpRequest.useFilters = true;
                sinon.FakeXMLHttpRequest.filters = [];
                sinon.useFakeXMLHttpRequest();
            },

            tearDown: function () {
                sinon.FakeXMLHttpRequest.useFilters = false;
                sinon.FakeXMLHttpRequest.restore();
                if (sinon.FakeXMLHttpRequest.defake.restore) {
                    sinon.FakeXMLHttpRequest.defake.restore();
                }
            },

            "does not defake XHR requests that don't match a filter": function () {
                sinon.stub(sinon.FakeXMLHttpRequest, "defake");

                sinon.FakeXMLHttpRequest.addFilter(function () { return false; });
                new XMLHttpRequest().open("GET", "http://example.com");

                refute(sinon.FakeXMLHttpRequest.defake.called);
            },

            "defakes XHR requests that match a filter": function () {
                sinon.stub(sinon.FakeXMLHttpRequest, "defake");

                sinon.FakeXMLHttpRequest.addFilter(function () { return true; });
                new XMLHttpRequest().open("GET", "http://example.com");

                assert(sinon.FakeXMLHttpRequest.defake.calledOnce);
            }
        },

        "defaked XHR": {
            setUp: function () {
                this.fakeXhr = new sinon.FakeXMLHttpRequest();
            },

            "updates attributes from working XHR object when ready state changes": function () {
                var workingXHRInstance;
                var readyStateCb;
                var workingXHROverride = function () {
                    workingXHRInstance = this;
                    this.addEventListener = function (str, fn) {
                        readyStateCb = fn;
                    };
                    this.open = function () {};
                };
                var fakeXhr = this.fakeXhr;
                runWithWorkingXHROveride(workingXHROverride, function () {
                    sinon.FakeXMLHttpRequest.defake(fakeXhr, []);
                    workingXHRInstance.statusText = "This is the status text of the real XHR";
                    workingXHRInstance.readyState = 4;
                    readyStateCb();
                    assert.equals(fakeXhr.statusText, "This is the status text of the real XHR");
                });
            },

            "passes on methods to working XHR object": function () {
                var workingXHRInstance;
                var spy;
                var workingXHROverride = function () {
                    workingXHRInstance = this;
                    this.addEventListener = this.open = function () {};
                };
                var fakeXhr = this.fakeXhr;
                runWithWorkingXHROveride(workingXHROverride, function () {
                    sinon.FakeXMLHttpRequest.defake(fakeXhr, []);
                    workingXHRInstance.getResponseHeader = spy = sinon.spy();
                    fakeXhr.getResponseHeader();
                    assert(spy.calledOnce);
                });
            },

            "calls legacy onreadystatechange handlers with target set to fakeXHR": function () {
                var workingXHRInstance;
                var spy;
                var readyStateCb;
                var workingXHROverride = function () {
                    workingXHRInstance = this;
                    this.addEventListener = function (str, fn) {
                        readyStateCb = fn;
                    };
                    this.open = function () {};
                };
                var fakeXhr = this.fakeXhr;

                runWithWorkingXHROveride(workingXHROverride, function () {
                    sinon.FakeXMLHttpRequest.defake(fakeXhr, []);
                    fakeXhr.onreadystatechange = spy = sinon.spy();
                    readyStateCb();
                    assert(spy.calledOnce);

                    // Fix to make weinre work
                    assert.isObject(spy.args[0][0]);
                    assert.equals(spy.args[0][0].target, fakeXhr);
                });
            },

            "performs initial readystatechange on opening when filters are being used, but don't match": function () {
                try {
                    sinon.FakeXMLHttpRequest.useFilters = true;
                    var spy = sinon.spy();
                    this.fakeXhr.addEventListener("readystatechange", spy);
                    this.fakeXhr.open("GET","http://example.com", true);
                    assert(spy.calledOnce);
                } finally {
                    sinon.FakeXMLHttpRequest.useFilters = false;
                }
            }
        },

        "defaked XHR filters": {
            setUp: function () {
                sinon.FakeXMLHttpRequest.useFilters = true;
                sinon.FakeXMLHttpRequest.filters = [];
                sinon.useFakeXMLHttpRequest();
                sinon.FakeXMLHttpRequest.addFilter(function () { return true; });
            },

            tearDown: function () {
                sinon.FakeXMLHttpRequest.useFilters = false;
                sinon.FakeXMLHttpRequest.filters = [];
                sinon.FakeXMLHttpRequest.restore();
            },

            "loads resource asynchronously": function (done) {
                var req = new XMLHttpRequest();

                req.onreadystatechange = function () {
                    if (this.readyState == 4) {
                        assert.match(this.responseText, /loaded successfully/);
                        done();
                    }
                };

                req.open("GET", "/test/resources/xhr_target.txt", true);
                req.send();
            },

            "loads resource synchronously": function () {
                var req = new XMLHttpRequest();
                req.open("GET", "/test/resources/xhr_target.txt", false);
                req.send();

                assert.match(req.responseText, /loaded successfully/);
            }
        },

        "missing ActiveXObject": {
            requiresSupportFor: {
                "no ActiveXObject": typeof ActiveXObject === "undefined"
            },
            setUp: fakeXhrSetUp,
            tearDown: fakeXhrTearDown,

            "does not expose ActiveXObject": function () {
                assert.equals(typeof ActiveXObject, "undefined");
            },

            "does not expose ActiveXObject when restored": function () {
                this.fakeXhr.restore();

                assert.equals(typeof ActiveXObject, "undefined");
            }
        },

        "native ActiveXObject": {
            requiresSupportFor: {
                "ActiveXObject": typeof ActiveXObject !== "undefined"
            },
            setUp: fakeXhrSetUp,
            tearDown: fakeXhrTearDown,

            "hijacks ActiveXObject": function () {
                refute.same(global.ActiveXObject, globalActiveXObject);
                refute.same(window.ActiveXObject, globalActiveXObject);
                refute.same(ActiveXObject, globalActiveXObject);
            },

            "restores global ActiveXObject": function () {
                this.fakeXhr.restore();

                assert.same(global.ActiveXObject, globalActiveXObject);
                assert.same(window.ActiveXObject, globalActiveXObject);
                assert.same(ActiveXObject, globalActiveXObject);
            },

            "creates FakeXHR object with ActiveX Microsoft.XMLHTTP": function () {
                var xhr = new ActiveXObject("Microsoft.XMLHTTP");

                assert(xhr instanceof sinon.FakeXMLHttpRequest);
            },

            "creates FakeXHR object with ActiveX Msxml2.XMLHTTP": function () {
                var xhr = new ActiveXObject("Msxml2.XMLHTTP");

                assert(xhr instanceof sinon.FakeXMLHttpRequest);
            },

            "creates FakeXHR object with ActiveX Msxml2.XMLHTTP.3.0": function () {
                var xhr = new ActiveXObject("Msxml2.XMLHTTP.3.0");

                assert(xhr instanceof sinon.FakeXMLHttpRequest);
            },

            "creates FakeXHR object with ActiveX Msxml2.XMLHTTP.6.0": function () {
                var xhr = new ActiveXObject("Msxml2.XMLHTTP.6.0");

                assert(xhr instanceof sinon.FakeXMLHttpRequest);
            }
        },

        "missing native XHR": {
            requiresSupportFor: { "no native XHR": typeof XMLHttpRequest == "undefined" },
            setUp: fakeXhrSetUp,
            tearDown: fakeXhrTearDown,

            "does not expose XMLHttpRequest": function () {
                assert.equals(typeof XMLHttpRequest, "undefined");
            },

            "does not expose XMLHttpRequest after restore": function () {
                this.fakeXhr.restore();

                assert.equals(typeof XMLHttpRequest, "undefined");
            }
        },

        "native XHR": {
            requiresSupportFor: { "XHR": typeof XMLHttpRequest !== "undefined" },
            setUp: fakeXhrSetUp,
            tearDown: fakeXhrTearDown,

            "replaces global XMLHttpRequest": function () {
                refute.same(XMLHttpRequest, globalXMLHttpRequest);
                assert.same(XMLHttpRequest, sinon.FakeXMLHttpRequest);
            },

            "restores global XMLHttpRequest": function () {
                this.fakeXhr.restore();

                assert.same(XMLHttpRequest, globalXMLHttpRequest);
            }
        },

        "progress events": {
            setUp: function () {
                this.xhr = new sinon.FakeXMLHttpRequest();
                this.xhr.open("GET", "/some/url")
            },

            "triggers 'loadstart' event on #send": function (done) {
                this.xhr.addEventListener("loadstart", function () {
                    assert(true);

                    done();
                });

                this.xhr.send();
            },

            "triggers 'loadstart' with event target set to the XHR object": function (done) {
                var xhr = this.xhr;

                this.xhr.addEventListener("loadstart", function (event) {
                    assert.same(xhr, event.target);

                    done();
                });

                this.xhr.send();
            },

            "calls #onloadstart on #send": function (done) {
                this.xhr.onloadstart = function () {
                    assert(true);

                    done();
                };

                this.xhr.send();
            },

            "triggers 'load' event on success": function (done) {
                var xhr = this.xhr;

                this.xhr.addEventListener("load", function () {
                    assert.equals(xhr.readyState, sinon.FakeXMLHttpRequest.DONE);
                    refute.equals(xhr.status, 0);

                    done();
                });

                this.xhr.send();
                this.xhr.respond(200, {}, "");
            },

            "triggers 'load' with event target set to the XHR object": function (done) {
                var xhr = this.xhr;

                this.xhr.addEventListener("load", function (event) {
                    assert.same(xhr, event.target);

                    done();
                });

                this.xhr.send();
                this.xhr.respond(200, {}, "");
            },

            "calls #onload on success": function (done) {
                var xhr = this.xhr;

                this.xhr.onload = function () {
                    assert.equals(xhr.readyState, sinon.FakeXMLHttpRequest.DONE);
                    refute.equals(xhr.status, 0);

                    done();
                };

                this.xhr.send();
                this.xhr.respond(200, {}, "");
            },

            "triggers 'abort' event on cancel": function (done) {
                var xhr = this.xhr;

                this.xhr.addEventListener("abort", function () {
                    assert.equals(xhr.readyState, sinon.FakeXMLHttpRequest.UNSENT);
                    assert.equals(xhr.status, 0);

                    done();
                });

                this.xhr.send();
                this.xhr.abort();
            },

            "triggers 'abort' with event target set to the XHR object": function (done) {
                var xhr = this.xhr;

                this.xhr.addEventListener("abort", function (event) {
                    assert.same(xhr, event.target);

                    done();
                });

                this.xhr.send();
                this.xhr.abort();
            },

            "calls #onabort on cancel": function (done) {
                var xhr = this.xhr;

                this.xhr.onabort = function () {
                    assert.equals(xhr.readyState, sinon.FakeXMLHttpRequest.UNSENT);
                    assert.equals(xhr.status, 0);

                    done();
                };

                this.xhr.send();
                this.xhr.abort();
            },

            "triggers 'loadend' event at the end": function (done) {
                this.xhr.addEventListener("loadend", function () {
                    assert(true);

                    done();
                });

                this.xhr.send();
                this.xhr.respond(403, {}, "");
            },

            "triggers 'loadend' with event target set to the XHR object": function (done) {
                var xhr = this.xhr;

                this.xhr.addEventListener("loadend", function (event) {
                    assert.same(xhr, event.target);

                    done();
                });

                this.xhr.send();
                this.xhr.respond(200, {}, "");
            },

            "calls #onloadend at the end": function (done) {
                this.xhr.onloadend = function () {
                    assert(true);

                    done();
                };

                this.xhr.send();
                this.xhr.respond(403, {}, "");
            }
        },

        "xhr.upload": {
            setUp: function () {
                this.xhr = new sinon.FakeXMLHttpRequest();
                this.xhr.open("POST", "/some/url", true);
            },

            "progress event is triggered with xhr.uploadProgress({loaded: 20, total: 100})": function (done) {
                this.xhr.upload.addEventListener("progress", function(e) {
                    assert.equals(e.total, 100);
                    assert.equals(e.loaded, 20);
                    done();
                });
                this.xhr.uploadProgress({
                    total: 100,
                    loaded: 20
                });
            },

            "triggers 'load' event on success": function (done) {
                var xhr = this.xhr;

                this.xhr.upload.addEventListener("load", function () {
                    assert.equals(xhr.readyState, sinon.FakeXMLHttpRequest.DONE);
                    refute.equals(xhr.status, 0);
                    done();
                });

                this.xhr.send();
                this.xhr.respond(200, {}, "");
            },

            "fires event with 100% progress on 'load'": function(done) {
                this.xhr.upload.addEventListener("progress", function(e) {
                    assert.equals(e.total, 100);
                    assert.equals(e.loaded, 100);
                    done();
                });

                this.xhr.send();
                this.xhr.respond(200, {}, "");
            },

            "calls 'abort' on cancel": function (done) {
                var xhr = this.xhr;

                this.xhr.upload.addEventListener("abort", function () {
                    assert.equals(xhr.readyState, sinon.FakeXMLHttpRequest.UNSENT);
                    assert.equals(xhr.status, 0);

                    done();
                });

                this.xhr.send();
                this.xhr.abort();
            },

            "error event is triggered with xhr.uploadError(new Error('foobar'))": function(done) {
                this.xhr.upload.addEventListener("error", function(e) {
                    assert.equals(e.detail.message, "foobar");

                    done();
                });
                this.xhr.uploadError(new Error("foobar"));
            },

            "event listeners can be removed": function() {
                var callback = function() {};
                this.xhr.upload.addEventListener("load", callback);
                this.xhr.upload.removeEventListener("load", callback);
                assert.equals(this.xhr.upload.eventListeners["load"].length, 0);
            }
        }
    });
}(this));
