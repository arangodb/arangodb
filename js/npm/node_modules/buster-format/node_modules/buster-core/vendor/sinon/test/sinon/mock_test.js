/*jslint onevar: false*/
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

buster.testCase("sinon.mock", {
    "create": {
        "returns function with expects method": function () {
            var mock = sinon.mock.create({});

            assert.isFunction(mock.expects);
        },

        "throws without object": function () {
            assert.exception(function () {
                sinon.mock.create();
            }, "TypeError");
        }
    },

    "expects": {
        setUp: function () {
            this.mock = sinon.mock.create({ someMethod: function () {} });
        },

        "throws without method": function () {
            var mock = this.mock;

            assert.exception(function () {
                mock.expects();
            }, "TypeError");
        },

        "returns expectation": function () {
            var result = this.mock.expects("someMethod");

            assert.isFunction(result);
            assert.equals(result.method, "someMethod");
        },

        "throws if expecting a non-existent method": function () {
            var mock = this.mock;

            assert.exception(function () {
                mock.expects("someMethod2");
            });
        }
    },

    "expectation": {
        setUp: function () {
            this.method = "myMeth";
            this.expectation = sinon.expectation.create(this.method);
        },

        "call expectation": function () {
            this.expectation();

            assert.isFunction(this.expectation.invoke);
            assert(this.expectation.called);
        },

        "is invokable": function () {
            var expectation = this.expectation;

            refute.exception(function () {
                expectation();
            });
        },

        "returns": {
            "returns configured return value": function () {
                var object = {};
                this.expectation.returns(object);

                assert.same(this.expectation(), object);
            }
        },

        "call": {
            "is called with correct this value": function () {
                var object = { method: this.expectation };
                object.method();

                assert(this.expectation.calledOn(object));
            }
        },

        "callCount": {
            "onlys be invokable once by default": function () {
                var expectation = this.expectation;
                expectation();

                assert.exception(function () {
                    expectation();
                }, "ExpectationError");
            },

            "throw readable error": function () {
                var expectation = this.expectation;
                expectation();

                try {
                    expectation();
                    buster.assertions.fail("Expected to throw");
                } catch (e) {
                    assert.equals(e.message, "myMeth already called once");
                }
            }
        },

        "callCountNever": {
            "is not callable": function () {
                var expectation = this.expectation;
                expectation.never();

                assert.exception(function () {
                    expectation();
                }, "ExpectationError");
            },

            "returns expectation for chaining": function () {
                assert.same(this.expectation.never(), this.expectation);
            }
        },

        "callCountOnce": {
            "allows one call": function () {
                var expectation = this.expectation;
                expectation.once();
                expectation();

                assert.exception(function () {
                    expectation();
                }, "ExpectationError");
            },

            "returns expectation for chaining": function () {
                assert.same(this.expectation.once(), this.expectation);
            }
        },

        "callCountTwice": {
            "allows two calls": function () {
                var expectation = this.expectation;
                expectation.twice();
                expectation();
                expectation();

                assert.exception(function () {
                    expectation();
                }, "ExpectationError");
            },

            "returns expectation for chaining": function () {
                assert.same(this.expectation.twice(), this.expectation);
            }
        },

        "callCountThrice": {
            "allows three calls": function () {
                var expectation = this.expectation;
                expectation.thrice();
                expectation();
                expectation();
                expectation();

                assert.exception(function () {
                    expectation();
                }, "ExpectationError");
            },

            "returns expectation for chaining": function () {
                assert.same(this.expectation.thrice(), this.expectation);
            }
        },

        "callCountExactly": {
            "allows specified number of calls": function () {
                var expectation = this.expectation;
                expectation.exactly(2);
                expectation();
                expectation();

                assert.exception(function () {
                    expectation();
                }, "ExpectationError");
            },

            "returns expectation for chaining": function () {
                assert.same(this.expectation.exactly(2), this.expectation);
            },

            "throws without argument": function () {
                var expectation = this.expectation;

                assert.exception(function () {
                    expectation.exactly();
                }, "TypeError");
            },

            "throws without number": function () {
                var expectation = this.expectation;

                assert.exception(function () {
                    expectation.exactly("12");
                }, "TypeError");
            }
        },

        "atLeast": {
            "throws without argument": function () {
                var expectation = this.expectation;

                assert.exception(function () {
                    expectation.atLeast();
                }, "TypeError");
            },

            "throws without number": function () {
                var expectation = this.expectation;

                assert.exception(function () {
                    expectation.atLeast({});
                }, "TypeError");
            },

            "returns expectation for chaining": function () {
                assert.same(this.expectation.atLeast(2), this.expectation);
            },

            "allows any number of calls": function () {
                var expectation = this.expectation;
                expectation.atLeast(2);
                expectation();
                expectation();

                refute.exception(function () {
                    expectation();
                    expectation();
                });
            },

            "nots be met with too few calls": function () {
                this.expectation.atLeast(2);
                this.expectation();

                assert.isFalse(this.expectation.met());
            },

            "is met with exact calls": function () {
                this.expectation.atLeast(2);
                this.expectation();
                this.expectation();

                assert(this.expectation.met());
            },

            "is met with excessive calls": function () {
                this.expectation.atLeast(2);
                this.expectation();
                this.expectation();
                this.expectation();

                assert(this.expectation.met());
            },

            "nots throw when exceeding at least expectation": function () {
                var obj = { foobar: function () {} };
                var mock = sinon.mock(obj);
                mock.expects("foobar").atLeast(1);

                obj.foobar();

                refute.exception(function () {
                    obj.foobar();
                    mock.verify();
                });
            }
        },

        "atMost": {
            "throws without argument": function () {
                var expectation = this.expectation;

                assert.exception(function () {
                    expectation.atMost();
                }, "TypeError");
            },

            "throws without number": function () {
                var expectation = this.expectation;

                assert.exception(function () {
                    expectation.atMost({});
                }, "TypeError");
            },

            "returns expectation for chaining": function () {
                assert.same(this.expectation.atMost(2), this.expectation);
            },

            "allows fewer calls": function () {
                var expectation = this.expectation;
                expectation.atMost(2);

                refute.exception(function () {
                    expectation();
                });
            },

            "is met with fewer calls": function () {
                this.expectation.atMost(2);
                this.expectation();

                assert(this.expectation.met());
            },

            "is met with exact calls": function () {
                this.expectation.atMost(2);
                this.expectation();
                this.expectation();

                assert(this.expectation.met());
            },

            "nots be met with excessive calls": function () {
                var expectation = this.expectation;
                this.expectation.atMost(2);
                this.expectation();
                this.expectation();

                assert.exception(function () {
                    expectation();
                }, "ExpectationError");

                assert.isFalse(this.expectation.met());
            }
        },

        "atMostAndAtLeast": {
            setUp: function () {
                this.expectation.atLeast(2);
                this.expectation.atMost(3);
            },

            "nots be met with too few calls": function () {
                this.expectation();

                assert.isFalse(this.expectation.met());
            },

            "is met with minimum calls": function () {
                this.expectation();
                this.expectation();

                assert(this.expectation.met());
            },

            "is met with maximum calls": function () {
                this.expectation();
                this.expectation();
                this.expectation();

                assert(this.expectation.met());
            },

            "throws with excessive calls": function () {
                var expectation = this.expectation;
                expectation();
                expectation();
                expectation();

                assert.exception(function () {
                    expectation();
                }, "ExpectationError");
            }
        },

        "met": {
            "nots be met when not called enough times": function () {
                assert.isFalse(this.expectation.met());
            },

            "is met when called enough times": function () {
                this.expectation();

                assert(this.expectation.met());
            },

            "nots be met when called too many times": function () {
                this.expectation();

                try {
                    this.expectation();
                } catch (e) {}

                assert.isFalse(this.expectation.met());
            }
        },

        "withArgs": {
            "returns expectation for chaining": function () {
                assert.same(this.expectation.withArgs(1), this.expectation);
            },

            "accepts call with expected args": function () {
                this.expectation.withArgs(1, 2, 3);
                this.expectation(1, 2, 3);

                assert(this.expectation.met());
            },

            "throws when called without args": function () {
                var expectation = this.expectation;
                expectation.withArgs(1, 2, 3);

                assert.exception(function () {
                    expectation();
                }, "ExpectationError");
            },

            "throws when called with too few args": function () {
                var expectation = this.expectation;
                expectation.withArgs(1, 2, 3);

                assert.exception(function () {
                    expectation(1, 2);
                }, "ExpectationError");
            },

            "throws when called with wrong args": function () {
                var expectation = this.expectation;
                expectation.withArgs(1, 2, 3);

                assert.exception(function () {
                    expectation(2, 2, 3);
                }, "ExpectationError");
            },

            "allows excessive args": function () {
                var expectation = this.expectation;
                expectation.withArgs(1, 2, 3);

                refute.exception(function () {
                    expectation(1, 2, 3, 4);
                });
            },

            "calls accept with no args": function () {
                this.expectation.withArgs();
                this.expectation();

                assert(this.expectation.met());
            },

            "allows no args called with excessive args": function () {
                var expectation = this.expectation;
                expectation.withArgs();

                refute.exception(function () {
                    expectation(1, 2, 3);
                });
            }
        },

        "withExactArgs": {
            "returns expectation for chaining": function () {
                assert.same(this.expectation.withExactArgs(1), this.expectation);
            },

            "accepts call with expected args": function () {
                this.expectation.withExactArgs(1, 2, 3);
                this.expectation(1, 2, 3);

                assert(this.expectation.met());
            },

            "throws when called without args": function () {
                var expectation = this.expectation;
                expectation.withExactArgs(1, 2, 3);

                assert.exception(function () {
                    expectation();
                }, "ExpectationError");
            },

            "throws when called with too few args": function () {
                var expectation = this.expectation;
                expectation.withExactArgs(1, 2, 3);

                assert.exception(function () {
                    expectation(1, 2);
                }, "ExpectationError");
            },

            "throws when called with wrong args": function () {
                var expectation = this.expectation;
                expectation.withExactArgs(1, 2, 3);

                assert.exception(function () {
                    expectation(2, 2, 3);
                }, "ExpectationError");
            },

            "nots allow excessive args": function () {
                var expectation = this.expectation;
                expectation.withExactArgs(1, 2, 3);

                assert.exception(function () {
                    expectation(1, 2, 3, 4);
                }, "ExpectationError");
            },

            "accepts call with no expected args": function () {
                this.expectation.withExactArgs();
                this.expectation();

                assert(this.expectation.met());
            },

            "nots allow excessive args with no expected args": function () {
                var expectation = this.expectation;
                expectation.withExactArgs();

                assert.exception(function () {
                    expectation(1, 2, 3);
                }, "ExpectationError");
            }
        },

        "on": {
            "returns expectation for chaining": function () {
                assert.same(this.expectation.on({}), this.expectation);
            },

            "allows calls on object": function () {
                this.expectation.on(this);
                this.expectation();

                assert(this.expectation.met());
            },

            "throws if called on wrong object": function () {
                var expectation = this.expectation;
                expectation.on({});

                assert.exception(function () {
                    expectation();
                }, "ExpectationError");
            }
        },

        "verify": {
            "passs if met": function () {
                sinon.stub(sinon.expectation, "pass");
                var expectation = this.expectation;

                expectation();
                expectation.verify();

                assert.equals(sinon.expectation.pass.callCount, 1);
                sinon.expectation.pass.restore();
            },

            "throws if not called enough times": function () {
                var expectation = this.expectation;

                assert.exception(function () {
                    expectation.verify();
                }, "ExpectationError");
            },

            "throws readable error": function () {
                var expectation = this.expectation;

                try {
                    expectation.verify();
                    buster.assertions.fail("Expected to throw");
                } catch (e) {
                    assert.equals(e.message,
                                  "Expected myMeth([...]) once (never called)");
                }
            }
        }
    },

    "verify": {
        setUp: function () {
            this.method = function () {};
            this.object = { method: this.method };
            this.mock = sinon.mock.create(this.object);
        },

        "restores mocks": function () {
            this.object.method();
            this.object.method.call(this.thisValue);
            this.mock.verify();

            assert.same(this.object.method, this.method);
        },

        "passes verified mocks": function () {
            sinon.stub(sinon.expectation, "pass");

            this.mock.expects("method").once();
            this.object.method();
            this.mock.verify();

            assert.equals(sinon.expectation.pass.callCount, 1);
            sinon.expectation.pass.restore();
        },

        "restores if not met": function () {
            var mock = this.mock;
            mock.expects("method");

            assert.exception(function () {
                mock.verify();
            }, "ExpectationError");

            assert.same(this.object.method, this.method);
        },

        "includes all calls in error message": function () {
            var mock = this.mock;
            mock.expects("method").thrice();
            mock.expects("method").once().withArgs(42);
            var message;

            try {
                mock.verify();
            } catch (e) {
                message = e.message;
            }

            assert.equals(message,
                          "Expected method([...]) thrice (never called)\nExpected method(42[, ...]) once (never called)");
        },

        "includes exact expected arguments in error message": function () {
            var mock = this.mock;
            mock.expects("method").once().withExactArgs(42);
            var message;

            try {
                mock.verify();
            } catch (e) {
                message = e.message;
            }

            assert.equals(message, "Expected method(42) once (never called)");
        },

        "includes received call count in error message": function () {
            var mock = this.mock;
            mock.expects("method").thrice().withExactArgs(42);
            this.object.method(42);
            var message;

            try {
                mock.verify();
            } catch (e) {
                message = e.message;
            }

            assert.equals(message, "Expected method(42) thrice (called once)");
        },

        "includes unexpected calls in error message": function () {
            var mock = this.mock;
            mock.expects("method").thrice().withExactArgs(42);
            var message;

            try {
                this.object.method();
            } catch (e) {
                message = e.message;
            }

            assert.equals(message,
                          "Unexpected call: method()\n" +
                          "    Expected method(42) thrice (never called)");
        },

        "includes met expectations in error message": function () {
            var mock = this.mock;
            mock.expects("method").once().withArgs(1);
            mock.expects("method").thrice().withExactArgs(42);
            this.object.method(1);
            var message;

            try {
                this.object.method();
            } catch (e) {
                message = e.message;
            }

            assert.equals(message, "Unexpected call: method()\n" +
                          "    Expectation met: method(1[, ...]) once\n" +
                          "    Expected method(42) thrice (never called)");
        },

        "includes met expectations in error message from verify": function () {
            var mock = this.mock;
            mock.expects("method").once().withArgs(1);
            mock.expects("method").thrice().withExactArgs(42);
            this.object.method(1);
            var message;

            try {
                mock.verify();
            } catch (e) {
                message = e.message;
            }

            assert.equals(message, "Expected method(42) thrice (never called)\n" +
                          "Expectation met: method(1[, ...]) once");
        },

        "reports min calls in error message": function () {
            var mock = this.mock;
            mock.expects("method").atLeast(1);
            var message;

            try {
                mock.verify();
            } catch (e) {
                message = e.message;
            }

            assert.equals(message, "Expected method([...]) at least once (never called)");
        },

        "reports max calls in error message": function () {
            var mock = this.mock;
            mock.expects("method").atMost(2);
            var message;

            try {
                this.object.method();
                this.object.method();
                this.object.method();
            } catch (e) {
                message = e.message;
            }

            assert.equals(message, "Unexpected call: method()\n" +
                          "    Expectation met: method([...]) at most twice");
        },

        "reports min calls in met expectation": function () {
            var mock = this.mock;
            mock.expects("method").atLeast(1);
            mock.expects("method").withArgs(2).once();
            var message;

            try {
                this.object.method();
                this.object.method(2);
                this.object.method(2);
            } catch (e) {
                message = e.message;
            }

            assert.equals(message, "Unexpected call: method(2)\n" +
                          "    Expectation met: method([...]) at least once\n" +
                          "    Expectation met: method(2[, ...]) once");
        },

        "reports max and min calls in error messages": function () {
            var mock = this.mock;
            mock.expects("method").atLeast(1).atMost(2);
            var message;

            try {
                mock.verify();
            } catch (e) {
                message = e.message;
            }

            assert.equals(message, "Expected method([...]) at least once and at most twice " +
                          "(never called)");
        }
    },

    "mockObject": {
        setUp: function () {
            this.method = function () {};
            this.object = { method: this.method };
            this.mock = sinon.mock.create(this.object);
        },

        "mocks object method": function () {
            this.mock.expects("method");

            assert.isFunction(this.object.method);
            refute.same(this.object.method, this.method);
        },

        "reverts mocked method": function () {
            this.mock.expects("method");
            this.object.method.restore();

            assert.same(this.object.method, this.method);
        },

        "reverts expectation": function () {
            var method = this.mock.expects("method");
            this.object.method.restore();

            assert.same(this.object.method, this.method);
        },

        "reverts mock": function () {
            var method = this.mock.expects("method");
            this.mock.restore();

            assert.same(this.object.method, this.method);
        },

        "verifies mock": function () {
            var method = this.mock.expects("method");
            this.object.method();
            var mock = this.mock;

            refute.exception(function () {
                assert(mock.verify());
            });
        },

        "verifies mock with unmet expectations": function () {
            var method = this.mock.expects("method");
            var mock = this.mock;

            assert.exception(function () {
                assert(mock.verify());
            }, "ExpectationError");
        }
    },

    "mock method multiple times": {
        setUp: function () {
            this.thisValue = {};
            this.method = function () {};
            this.object = { method: this.method };
            this.mock = sinon.mock.create(this.object);
            this.mock1 = this.mock.expects("method");
            this.mock2 = this.mock.expects("method").on(this.thisValue);
        },

        "queues expectations": function () {
            var object = this.object;

            refute.exception(function () {
                object.method();
            });
        },

        "starts on next expectation when first is met": function () {
            var object = this.object;
            object.method();

            assert.exception(function () {
                object.method();
            }, "ExpectationError");
        },

        "fails on last expectation": function () {
            var object = this.object;
            object.method();
            object.method.call(this.thisValue);

            assert.exception(function () {
                object.method();
            }, "ExpectationError");
        },

        "allows mock calls in any order": function () {
            var object = { method: function () {} };
            var mock = sinon.mock(object);
            mock.expects("method").once().withArgs(42);
            mock.expects("method").twice().withArgs("Yeah");

            refute.exception(function () {
                object.method("Yeah");
            });

            refute.exception(function () {
                object.method(42);
            });

            assert.exception(function () {
                object.method(1);
            });

            refute.exception(function () {
                object.method("Yeah");
            });

            assert.exception(function () {
                object.method(42);
            });
        }
    },

    "mock function": {
        "returns mock method": function () {
            var mock = sinon.mock();

            assert.isFunction(mock);
            assert.isFunction(mock.atLeast);
            assert.isFunction(mock.verify);
        },

        "returns mock object": function () {
            var mock = sinon.mock({});

            assert.isObject(mock);
            assert.isFunction(mock.expects);
            assert.isFunction(mock.verify);
        }
    },

    "yields": {
        "invokes only argument as callback": function () {
            var mock = sinon.mock().yields();
            var spy = sinon.spy();
            mock(spy);

            assert(spy.calledOnce);
            assert.equals(spy.args[0].length, 0);
        },

        "throws understandable error if no callback is passed": function () {
            var mock = sinon.mock().yields();
            var spy = sinon.spy();

            try {
                mock();
                throw new Error();
            } catch (e) {
                assert.equals(e.message, "stub expected to yield, but no callback was passed.");
            }
        }
    }
});
