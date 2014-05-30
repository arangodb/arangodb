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

buster.testCase("sinon.assert", {
    setUp: function () {
        this.global = typeof window !== "undefined" ? window : global;

        this.setUpStubs = function () {
            this.stub = sinon.stub.create();
            sinon.stub(sinon.assert, "fail").throws();
            sinon.stub(sinon.assert, "pass");
        };

        this.tearDownStubs = function () {
            sinon.assert.fail.restore();
            sinon.assert.pass.restore();
        };
    },

    "is object": function () {
        assert.isObject(sinon.assert);
    },

    "fail": {
        setUp: function () {
            this.exceptionName = sinon.assert.failException;
        },

        tearDown: function () {
            sinon.assert.failException = this.exceptionName;
        },

        "throws exception": function () {
            var failed = false;
            var exception;

            try {
                sinon.assert.fail("Some message");
                failed = true;
            } catch (e) {
                exception = e;
            }

            assert.isFalse(failed);
            assert.equals(exception.name, "AssertError");
        },

        "throws configured exception type": function () {
            sinon.assert.failException = "RetardError";

            assert.exception(function () {
                sinon.assert.fail("Some message");
            }, "RetardError");
        }
    },

    "match": {
        setUp: function () { this.setUpStubs(); },
        tearDown: function () { this.tearDownStubs(); },

        "fails when arguments to not match": function () {
            assert.exception(function () {
                sinon.assert.match("foo", "bar");
            });

            assert(sinon.assert.fail.calledOnce);
        },

        "passes when argumens match": function () {
            sinon.assert.match("foo", "foo");
            assert(sinon.assert.pass.calledOnce);
        }
    },

    "called": {
        setUp: function () { this.setUpStubs(); },
        tearDown: function () { this.tearDownStubs(); },

        "fails when method does not exist": function () {
            assert.exception(function () {
                sinon.assert.called();
            });

            assert(sinon.assert.fail.called);
        },

        "fails when method is not stub": function () {
            assert.exception(function () {
                sinon.assert.called(function () {});
            });

            assert(sinon.assert.fail.called);
        },

        "fails when method was not called": function () {
            var stub = this.stub;

            assert.exception(function () {
                sinon.assert.called(stub);
            });

            assert(sinon.assert.fail.called);
        },

        "does not fail when method was called": function () {
            var stub = this.stub;
            stub();

            refute.exception(function () {
                sinon.assert.called(stub);
            });

            assert.isFalse(sinon.assert.fail.called);
        },

        "calls pass callback": function () {
            var stub = this.stub;
            stub();

            refute.exception(function () {
                sinon.assert.called(stub);
            });

            assert(sinon.assert.pass.calledOnce);
            assert(sinon.assert.pass.calledWith("called"));
        }
    },

    "notCalled": {
        setUp: function () { this.setUpStubs(); },
        tearDown: function () { this.tearDownStubs(); },

        "fails when method does not exist": function () {
            assert.exception(function () {
                sinon.assert.notCalled();
            });

            assert(sinon.assert.fail.called);
        },

        "fails when method is not stub": function () {
            assert.exception(function () {
                sinon.assert.notCalled(function () {});
            });

            assert(sinon.assert.fail.called);
        },

        "fails when method was called": function () {
            var stub = this.stub;
            stub();

            assert.exception(function () {
                sinon.assert.notCalled(stub);
            });

            assert(sinon.assert.fail.called);
        },

        "passes when method was not called": function () {
            var stub = this.stub;

            refute.exception(function () {
                sinon.assert.notCalled(stub);
            });

            assert.isFalse(sinon.assert.fail.called);
        },

        "should call pass callback": function () {
            var stub = this.stub;
            sinon.assert.notCalled(stub);

            assert(sinon.assert.pass.calledOnce);
            assert(sinon.assert.pass.calledWith("notCalled"));
        }
    },

    "calledOnce": {
        setUp: function () { this.setUpStubs(); },
        tearDown: function () { this.tearDownStubs(); },

        "fails when method does not exist": function () {
            assert.exception(function () {
                sinon.assert.calledOnce();
            });

            assert(sinon.assert.fail.called);
        },

        "fails when method is not stub": function () {
            assert.exception(function () {
                sinon.assert.calledOnce(function () {});
            });

            assert(sinon.assert.fail.called);
        },

        "fails when method was not called": function () {
            var stub = this.stub;

            assert.exception(function () {
                sinon.assert.calledOnce(stub);
            });

            assert(sinon.assert.fail.called);
        },

        "passes when method was called": function () {
            var stub = this.stub;
            stub();

            refute.exception(function () {
                sinon.assert.calledOnce(stub);
            });

            assert.isFalse(sinon.assert.fail.called);
        },

        "fails when method was called more than once": function () {
            var stub = this.stub;
            stub();
            stub();

            assert.exception(function () {
                sinon.assert.calledOnce(stub);
            });

            assert(sinon.assert.fail.called);
        },

        "calls pass callback": function () {
            var stub = this.stub;
            stub();
            sinon.assert.calledOnce(stub);

            assert(sinon.assert.pass.calledOnce);
            assert(sinon.assert.pass.calledWith("calledOnce"));
        }
    },

    "calledTwice": {
        setUp: function () { this.setUpStubs(); },
        tearDown: function () { this.tearDownStubs(); },

        "fails if called once": function () {
            var stub = this.stub;
            this.stub();

            assert.exception(function () {
                sinon.assert.calledTwice(stub);
            });
        },

        "passes if called twice": function () {
            var stub = this.stub;
            this.stub();
            this.stub();

            refute.exception(function () {
                sinon.assert.calledTwice(stub);
            });
        },

        "calls pass callback": function () {
            var stub = this.stub;
            stub();
            stub();
            sinon.assert.calledTwice(stub);

            assert(sinon.assert.pass.calledOnce);
            assert(sinon.assert.pass.calledWith("calledTwice"));
        }
    },

    "calledThrice": {
        setUp: function () { this.setUpStubs(); },
        tearDown: function () { this.tearDownStubs(); },

        "fails if called once": function () {
            var stub = this.stub;
            this.stub();

            assert.exception(function () {
                sinon.assert.calledThrice(stub);
            });
        },

        "passes if called thrice": function () {
            var stub = this.stub;
            this.stub();
            this.stub();
            this.stub();

            refute.exception(function () {
                sinon.assert.calledThrice(stub);
            });
        },

        "calls pass callback": function () {
            var stub = this.stub;
            stub();
            stub();
            stub();
            sinon.assert.calledThrice(stub);

            assert(sinon.assert.pass.calledOnce);
            assert(sinon.assert.pass.calledWith("calledThrice"));
        }
    },

    "callOrder": {
        setUp: function () { this.setUpStubs(); },
        tearDown: function () { this.tearDownStubs(); },

        "passes when calls where done in right order": function () {
            var spy1 = sinon.spy();
            var spy2 = sinon.spy();
            spy1();
            spy2();

            refute.exception(function () {
                sinon.assert.callOrder(spy1, spy2);
            });
        },

        "fails when calls where done in wrong order": function () {
            var spy1 = sinon.spy();
            var spy2 = sinon.spy();
            spy2();
            spy1();

            assert.exception(function () {
                sinon.assert.callOrder(spy1, spy2);
            });

            assert(sinon.assert.fail.called);
        },

        "passes when many calls where done in right order": function () {
            var spy1 = sinon.spy();
            var spy2 = sinon.spy();
            var spy3 = sinon.spy();
            var spy4 = sinon.spy();
            spy1();
            spy2();
            spy3();
            spy4();

            refute.exception(function () {
                sinon.assert.callOrder(spy1, spy2, spy3, spy4);
            });
        },

        "fails when one of many calls where done in wrong order": function () {
            var spy1 = sinon.spy();
            var spy2 = sinon.spy();
            var spy3 = sinon.spy();
            var spy4 = sinon.spy();
            spy1();
            spy2();
            spy4();
            spy3();

            assert.exception(function () {
                sinon.assert.callOrder(spy1, spy2, spy3, spy4);
            });

            assert(sinon.assert.fail.called);
        },

        "calls pass callback": function () {
            var stubs = [sinon.spy(), sinon.spy()];
            stubs[0]();
            stubs[1]();
            sinon.assert.callOrder(stubs[0], stubs[1]);

            assert(sinon.assert.pass.calledOnce);
            assert(sinon.assert.pass.calledWith("callOrder"));
        },

        "passes for multiple calls to same spy": function () {
            var first = sinon.spy();
            var second = sinon.spy();

            first();
            second();
            first();

            refute.exception(function () {
                sinon.assert.callOrder(first, second, first);
            });
        },

        "fails if first spy was not called": function () {
            var first = sinon.spy();
            var second = sinon.spy();

            second();

            assert.exception(function () {
                sinon.assert.callOrder(first, second);
            });
        },

        "fails if second spy was not called": function () {
            var first = sinon.spy();
            var second = sinon.spy();

            first();

            assert.exception(function () {
                sinon.assert.callOrder(first, second);
            });
        }
    },

    "calledOn": {
        setUp: function () { this.setUpStubs(); },
        tearDown: function () { this.tearDownStubs(); },

        "fails when method does not exist": function () {
            var object = {};
            sinon.stub(this.stub, "calledOn");

            assert.exception(function () {
                sinon.assert.calledOn(null, object);
            });

            assert.isFalse(this.stub.calledOn.calledWith(object));
            assert(sinon.assert.fail.called);
        },

        "fails when method is not stub": function () {
            var object = {};
            sinon.stub(this.stub, "calledOn");

            assert.exception(function () {
                sinon.assert.calledOn(function () {}, object);
            });

            assert.isFalse(this.stub.calledOn.calledWith(object));
            assert(sinon.assert.fail.called);
        },

        "fails when method fails": function () {
            var object = {};
            sinon.stub(this.stub, "calledOn").returns(false);
            var stub = this.stub;

            assert.exception(function () {
                sinon.assert.calledOn(stub, object);
            });

            assert(sinon.assert.fail.called);
        },

        "passes when method doesn't fail": function () {
            var object = {};
            sinon.stub(this.stub, "calledOn").returns(true);
            var stub = this.stub;

            sinon.assert.calledOn(stub, object);

            assert.isFalse(sinon.assert.fail.called);
        },

        "calls pass callback": function () {
            var obj = {};
            this.stub.call(obj);
            sinon.assert.calledOn(this.stub, obj);

            assert(sinon.assert.pass.calledOnce);
            assert(sinon.assert.pass.calledWith("calledOn"));
        }
    },

    "calledWithNew": {
        setUp: function () { this.setUpStubs(); },
        tearDown: function () { this.tearDownStubs(); },

        "fails when method does not exist": function () {
            sinon.stub(this.stub, "calledWithNew");

            assert.exception(function () {
                sinon.assert.calledWithNew(null);
            });

            assert.isFalse(this.stub.calledWithNew.called);
            assert(sinon.assert.fail.called);
        },

        "fails when method is not stub": function () {
            sinon.stub(this.stub, "calledWithNew");

            assert.exception(function () {
                sinon.assert.calledWithNew(function () {});
            });

            assert.isFalse(this.stub.calledWithNew.called);
            assert(sinon.assert.fail.called);
        },

        "fails when method fails": function () {
            sinon.stub(this.stub, "calledWithNew").returns(false);
            var stub = this.stub;

            assert.exception(function () {
                sinon.assert.calledWithNew(stub);
            });

            assert(sinon.assert.fail.called);
        },

        "passes when method doesn't fail": function () {
            sinon.stub(this.stub, "calledWithNew").returns(true);
            var stub = this.stub;

            sinon.assert.calledWithNew(stub);

            assert.isFalse(sinon.assert.fail.called);
        },

        "calls pass callback": function () {
            var a = new this.stub();
            sinon.assert.calledWithNew(this.stub);

            assert(sinon.assert.pass.calledOnce);
            assert(sinon.assert.pass.calledWith("calledWithNew"));
        }
    },

    "alwaysCalledWithNew": {
        setUp: function () { this.setUpStubs(); },
        tearDown: function () { this.tearDownStubs(); },

        "fails when method does not exist": function () {
            sinon.stub(this.stub, "alwaysCalledWithNew");

            assert.exception(function () {
                sinon.assert.alwaysCalledWithNew(null);
            });

            assert.isFalse(this.stub.alwaysCalledWithNew.called);
            assert(sinon.assert.fail.called);
        },

        "fails when method is not stub": function () {
            sinon.stub(this.stub, "alwaysCalledWithNew");

            assert.exception(function () {
                sinon.assert.alwaysCalledWithNew(function () {});
            });

            assert.isFalse(this.stub.alwaysCalledWithNew.called);
            assert(sinon.assert.fail.called);
        },

        "fails when method fails": function () {
            sinon.stub(this.stub, "alwaysCalledWithNew").returns(false);
            var stub = this.stub;

            assert.exception(function () {
                sinon.assert.alwaysCalledWithNew(stub);
            });

            assert(sinon.assert.fail.called);
        },

        "passes when method doesn't fail": function () {
            sinon.stub(this.stub, "alwaysCalledWithNew").returns(true);
            var stub = this.stub;

            sinon.assert.alwaysCalledWithNew(stub);

            assert.isFalse(sinon.assert.fail.called);
        },

        "calls pass callback": function () {
            var a = new this.stub();
            sinon.assert.alwaysCalledWithNew(this.stub);

            assert(sinon.assert.pass.calledOnce);
            assert(sinon.assert.pass.calledWith("alwaysCalledWithNew"));
        }
    },

    "calledWith": {
        setUp: function () { this.setUpStubs(); },
        tearDown: function () { this.tearDownStubs(); },

        "fails when method fails": function () {
            var object = {};
            sinon.stub(this.stub, "calledWith").returns(false);
            var stub = this.stub;

            assert.exception(function () {
                sinon.assert.calledWith(stub, object, 1);
            });

            assert(this.stub.calledWith.calledWith(object, 1));
            assert(sinon.assert.fail.called);
        },

        "passes when method doesn't fail": function () {
            var object = {};
            sinon.stub(this.stub, "calledWith").returns(true);
            var stub = this.stub;

            refute.exception(function () {
                sinon.assert.calledWith(stub, object, 1);
            });

            assert(this.stub.calledWith.calledWith(object, 1));
            assert.isFalse(sinon.assert.fail.called);
        },

        "calls pass callback": function () {
            this.stub("yeah");
            sinon.assert.calledWith(this.stub, "yeah");

            assert(sinon.assert.pass.calledOnce);
            assert(sinon.assert.pass.calledWith("calledWith"));
        }
    },

    "calledWithExactly": {
        setUp: function () { this.setUpStubs(); },
        tearDown: function () { this.tearDownStubs(); },

        "fails when method fails": function () {
            var object = {};
            sinon.stub(this.stub, "calledWithExactly").returns(false);
            var stub = this.stub;

            assert.exception(function () {
                sinon.assert.calledWithExactly(stub, object, 1);
            });

            assert(this.stub.calledWithExactly.calledWithExactly(object, 1));
            assert(sinon.assert.fail.called);
        },

        "passes when method doesn't fail": function () {
            var object = {};
            sinon.stub(this.stub, "calledWithExactly").returns(true);
            var stub = this.stub;

            refute.exception(function () {
                sinon.assert.calledWithExactly(stub, object, 1);
            });

            assert(this.stub.calledWithExactly.calledWithExactly(object, 1));
            assert.isFalse(sinon.assert.fail.called);
        },

        "calls pass callback": function () {
            this.stub("yeah");
            sinon.assert.calledWithExactly(this.stub, "yeah");

            assert(sinon.assert.pass.calledOnce);
            assert(sinon.assert.pass.calledWith("calledWithExactly"));
        }
    },

    "neverCalledWith": {
        setUp: function () { this.setUpStubs(); },
        tearDown: function () { this.tearDownStubs(); },

        "fails when method fails": function () {
            var object = {};
            sinon.stub(this.stub, "neverCalledWith").returns(false);
            var stub = this.stub;

            assert.exception(function () {
                sinon.assert.neverCalledWith(stub, object, 1);
            });

            assert(this.stub.neverCalledWith.calledWith(object, 1));
            assert(sinon.assert.fail.called);
        },

        "passes when method doesn't fail": function () {
            var object = {};
            sinon.stub(this.stub, "neverCalledWith").returns(true);
            var stub = this.stub;

            refute.exception(function () {
                sinon.assert.neverCalledWith(stub, object, 1);
            });

            assert(this.stub.neverCalledWith.calledWith(object, 1));
            assert.isFalse(sinon.assert.fail.called);
        },

        "calls pass callback": function () {
            this.stub("yeah");
            sinon.assert.neverCalledWith(this.stub, "nah!");

            assert(sinon.assert.pass.calledOnce);
            assert(sinon.assert.pass.calledWith("neverCalledWith"));
        }
    },

    "threwTest": {
        setUp: function () { this.setUpStubs(); },
        tearDown: function () { this.tearDownStubs(); },

        "fails when method fails": function () {
            sinon.stub(this.stub, "threw").returns(false);
            var stub = this.stub;

            assert.exception(function () {
                sinon.assert.threw(stub, 1, 2);
            });

            assert(this.stub.threw.calledWithExactly(1, 2));
            assert(sinon.assert.fail.called);
        },

        "passes when method doesn't fail": function () {
            sinon.stub(this.stub, "threw").returns(true);
            var stub = this.stub;

            refute.exception(function () {
                sinon.assert.threw(stub, 1, 2);
            });

            assert(this.stub.threw.calledWithExactly(1, 2));
            assert.isFalse(sinon.assert.fail.called);
        },

        "calls pass callback": function () {
            sinon.stub(this.stub, "threw").returns(true);
            this.stub();
            sinon.assert.threw(this.stub);

            assert(sinon.assert.pass.calledOnce);
            assert(sinon.assert.pass.calledWith("threw"));
        }
    },

    "callCount": {
        setUp: function () { this.setUpStubs(); },
        tearDown: function () { this.tearDownStubs(); },

        "fails when method fails": function () {
            this.stub();
            this.stub();
            var stub = this.stub;

            assert.exception(function () {
                sinon.assert.callCount(stub, 3);
            });

            assert(sinon.assert.fail.called);
        },

        "passes when method doesn't fail": function () {
            var stub = this.stub;
            this.stub.callCount = 3;

            refute.exception(function () {
                sinon.assert.callCount(stub, 3);
            });

            assert.isFalse(sinon.assert.fail.called);
        },

        "calls pass callback": function () {
            this.stub();
            sinon.assert.callCount(this.stub, 1);

            assert(sinon.assert.pass.calledOnce);
            assert(sinon.assert.pass.calledWith("callCount"));
        }
    },

    "alwaysCalledOn": {
        setUp: function () { this.setUpStubs(); },
        tearDown: function () { this.tearDownStubs(); },

        "fails if method is missing": function () {
            assert.exception(function () {
                sinon.assert.alwaysCalledOn();
            });
        },

        "fails if method is not fake": function () {
            assert.exception(function () {
                sinon.assert.alwaysCalledOn(function () {}, {});
            });
        },

        "fails if stub returns false": function () {
            var stub = sinon.stub();
            sinon.stub(stub, "alwaysCalledOn").returns(false);

            assert.exception(function () {
                sinon.assert.alwaysCalledOn(stub, {});
            });

            assert(sinon.assert.fail.called);
        },

        "passes if stub returns true": function () {
            var stub = sinon.stub.create();
            sinon.stub(stub, "alwaysCalledOn").returns(true);

            sinon.assert.alwaysCalledOn(stub, {});

            assert.isFalse(sinon.assert.fail.called);
        },

        "calls pass callback": function () {
            this.stub();
            sinon.assert.alwaysCalledOn(this.stub, this);

            assert(sinon.assert.pass.calledOnce);
            assert(sinon.assert.pass.calledWith("alwaysCalledOn"));
        }
    },

    "alwaysCalledWith": {
        setUp: function () {
            sinon.stub(sinon.assert, "fail").throws();
            sinon.stub(sinon.assert, "pass");
        },

        tearDown: function () {
            sinon.assert.fail.restore();
            sinon.assert.pass.restore();
        },

        "fails if method is missing": function () {
            assert.exception(function () {
                sinon.assert.alwaysCalledWith();
            });
        },

        "fails if method is not fake": function () {
            assert.exception(function () {
                sinon.assert.alwaysCalledWith(function () {});
            });
        },

        "fails if stub returns false": function () {
            var stub = sinon.stub.create();
            sinon.stub(stub, "alwaysCalledWith").returns(false);

            assert.exception(function () {
                sinon.assert.alwaysCalledWith(stub, {}, []);
            });

            assert(sinon.assert.fail.called);
        },

        "passes if stub returns true": function () {
            var stub = sinon.stub.create();
            sinon.stub(stub, "alwaysCalledWith").returns(true);

            sinon.assert.alwaysCalledWith(stub, {}, []);

            assert.isFalse(sinon.assert.fail.called);
        },

        "calls pass callback": function () {
            var spy = sinon.spy();
            spy("Hello");
            sinon.assert.alwaysCalledWith(spy, "Hello");

            assert(sinon.assert.pass.calledOnce);
            assert(sinon.assert.pass.calledWith("alwaysCalledWith"));
        }
    },

    "alwaysCalledWithExactly": {
        setUp: function () {
            sinon.stub(sinon.assert, "fail");
            sinon.stub(sinon.assert, "pass");
        },

        tearDown: function () {
            sinon.assert.fail.restore();
            sinon.assert.pass.restore();
        },

        "fails if stub returns false": function () {
            var stub = sinon.stub.create();
            sinon.stub(stub, "alwaysCalledWithExactly").returns(false);

            sinon.assert.alwaysCalledWithExactly(stub, {}, []);

            assert(sinon.assert.fail.called);
        },

        "passes if stub returns true": function () {
            var stub = sinon.stub.create();
            sinon.stub(stub, "alwaysCalledWithExactly").returns(true);

            sinon.assert.alwaysCalledWithExactly(stub, {}, []);

            assert.isFalse(sinon.assert.fail.called);
        },

        "calls pass callback": function () {
            var spy = sinon.spy();
            spy("Hello");
            sinon.assert.alwaysCalledWithExactly(spy, "Hello");

            assert(sinon.assert.pass.calledOnce);
            assert(sinon.assert.pass.calledWith("alwaysCalledWithExactly"));
        }
    },

    "expose": {
        "exposes asserts into object": function () {
            var test = {};
            sinon.assert.expose(test);

            assert.isFunction(test.fail);
            assert.isString(test.failException);
            assert.isFunction(test.assertCalled);
            assert.isFunction(test.assertCalledOn);
            assert.isFunction(test.assertCalledWith);
            assert.isFunction(test.assertCalledWithExactly);
            assert.isFunction(test.assertThrew);
            assert.isFunction(test.assertCallCount);
        },

        "exposes asserts into global": function () {
            sinon.assert.expose(this.global, {
                includeFail: false
            });

            assert.equals(typeof failException, "undefined");
            assert.isFunction(assertCalled);
            assert.isFunction(assertCalledOn);
            assert.isFunction(assertCalledWith);
            assert.isFunction(assertCalledWithExactly);
            assert.isFunction(assertThrew);
            assert.isFunction(assertCallCount);
        },

        "fails exposed asserts without errors": function () {
            sinon.assert.expose(this.global, {
                includeFail: false
            });

            try {
                assertCalled(sinon.spy());
            } catch (e) {
                assert.equals(e.message, "expected spy to have been called at least once but was never called");
            }
        },

        "exposes asserts into object without prefixes": function () {
            var test = {};

            sinon.assert.expose(test, { prefix: "" });

            assert.isFunction(test.fail);
            assert.isString(test.failException);
            assert.isFunction(test.called);
            assert.isFunction(test.calledOn);
            assert.isFunction(test.calledWith);
            assert.isFunction(test.calledWithExactly);
            assert.isFunction(test.threw);
            assert.isFunction(test.callCount);
        },

        "throws if target is undefined": function () {
            assert.exception(function () {
                sinon.assert.expose();
            }, "TypeError");
        },

        "throws if target is null": function () {
            assert.exception(function () {
                sinon.assert.expose(null);
            }, "TypeError");
        }
    },

    "message": {
        setUp: function () {
            this.obj = {
                doSomething: function () {}
            };

            sinon.spy(this.obj, "doSomething");

            this.message = function (method) {
                try {
                    sinon.assert[method].apply(sinon.assert, [].slice.call(arguments, 1));
                } catch (e) {
                    return e.message;
                }
            };
        },

        "assert.called exception message": function () {
            assert.equals(this.message("called", this.obj.doSomething),
                          "expected doSomething to have been called at " +
                          "least once but was never called");
        },

        "assert.notCalled exception message one call": function () {
            this.obj.doSomething();

            assert.equals(this.message("notCalled", this.obj.doSomething),
                          "expected doSomething to not have been called " +
                          "but was called once\n    doSomething()");
        },

        "assert.notCalled exception message four calls": function () {
            this.obj.doSomething();
            this.obj.doSomething();
            this.obj.doSomething();
            this.obj.doSomething();

            assert.equals(this.message("notCalled", this.obj.doSomething),
                          "expected doSomething to not have been called " +
                          "but was called 4 times\n    doSomething()\n    " +
                          "doSomething()\n    doSomething()\n    doSomething()");
        },

        "assert.notCalled exception message with calls with arguments": function () {
            this.obj.doSomething();
            this.obj.doSomething(3);
            this.obj.doSomething(42, 1);
            this.obj.doSomething();

            assert.equals(this.message("notCalled", this.obj.doSomething),
                          "expected doSomething to not have been called " +
                          "but was called 4 times\n    doSomething()\n    " +
                          "doSomething(3)\n    doSomething(42, 1)\n    doSomething()");
        },

        "assert.callOrder exception message": function () {
            var obj = { doop: function () {}, foo: function () {} };
            sinon.spy(obj, "doop");
            sinon.spy(obj, "foo");

            obj.doop();
            this.obj.doSomething();
            obj.foo();

            var message = this.message("callOrder", this.obj.doSomething, obj.doop, obj.foo);

            assert.equals(message,
                          "expected doSomething, doop, foo to be called in " +
                          "order but were called as doop, doSomething, foo");
        },

        "assert.callOrder with missing first call exception message": function () {
            var obj = { doop: function () {}, foo: function () {} };
            sinon.spy(obj, "doop");
            sinon.spy(obj, "foo");

            obj.foo();

            var message = this.message("callOrder", obj.doop, obj.foo);

            assert.equals(message,
                          "expected doop, foo to be called in " +
                          "order but were called as foo");
        },

        "assert.callOrder with missing last call exception message": function () {
            var obj = { doop: function () {}, foo: function () {} };
            sinon.spy(obj, "doop");
            sinon.spy(obj, "foo");

            obj.doop();

            var message = this.message("callOrder", obj.doop, obj.foo);

            assert.equals(message,
                          "expected doop, foo to be called in " +
                          "order but were called as doop");
        },

        "assert.callCount exception message": function () {
            this.obj.doSomething();

            assert.equals(this.message("callCount", this.obj.doSomething, 3),
                          "expected doSomething to be called thrice but was called " +
                          "once\n    doSomething()");
        },

        "assert.calledOnce exception message": function () {
            this.obj.doSomething();
            this.obj.doSomething();

            assert.equals(this.message("calledOnce", this.obj.doSomething),
                          "expected doSomething to be called once but was called " +
                          "twice\n    doSomething()\n    doSomething()");

            this.obj.doSomething();

            assert.equals(this.message("calledOnce", this.obj.doSomething),
                          "expected doSomething to be called once but was called " +
                          "thrice\n    doSomething()\n    doSomething()\n    doSomething()");
        },

        "assert.calledTwice exception message": function () {
            this.obj.doSomething();

            assert.equals(this.message("calledTwice", this.obj.doSomething),
                          "expected doSomething to be called twice but was called " +
                          "once\n    doSomething()");
        },

        "assert.calledThrice exception message": function () {
            this.obj.doSomething();
            this.obj.doSomething();
            this.obj.doSomething();
            this.obj.doSomething();

            assert.equals(this.message("calledThrice", this.obj.doSomething),
                          "expected doSomething to be called thrice but was called 4 times\n    doSomething()\n    doSomething()\n    doSomething()\n    doSomething()");
        },

        "assert.calledOn exception message": function () {
            this.obj.toString = function () {
                return "[Oh yeah]";
            };

            var obj = { toString: function () { return "[Oh no]"; } };
            var obj2 = { toString: function () { return "[Oh well]"; } };

            this.obj.doSomething.call(obj);
            this.obj.doSomething.call(obj2);

            assert.equals(this.message("calledOn", this.obj.doSomething, this.obj),
                          "expected doSomething to be called with [Oh yeah] as this but was called with [Oh no], [Oh well]");
        },

        "assert.alwaysCalledOn exception message": function () {
            this.obj.toString = function () {
                return "[Oh yeah]";
            };

            var obj = { toString: function () { return "[Oh no]"; } };
            var obj2 = { toString: function () { return "[Oh well]"; } };

            this.obj.doSomething.call(obj);
            this.obj.doSomething.call(obj2);
            this.obj.doSomething();

            assert.equals(this.message("alwaysCalledOn", this.obj.doSomething, this.obj),
                          "expected doSomething to always be called with [Oh yeah] as this but was called with [Oh no], [Oh well], [Oh yeah]");
        },

        "assert.calledWithNew exception message": function () {
            this.obj.doSomething();

            assert.equals(this.message("calledWithNew", this.obj.doSomething),
                          "expected doSomething to be called with new");
        },

        "assert.alwaysCalledWithNew exception message": function () {
            var a = new this.obj.doSomething();
            this.obj.doSomething();

            assert.equals(this.message("alwaysCalledWithNew", this.obj.doSomething),
                          "expected doSomething to always be called with new");
        },

        "assert.calledWith exception message": function () {
            this.obj.doSomething(1, 3, "hey");

            assert.equals(this.message("calledWith", this.obj.doSomething, 4, 3, "hey"),
                          "expected doSomething to be called with arguments 4, 3, " +
                          "hey\n    doSomething(1, 3, hey)");
        },

        "assert.calledWith match.any exception message": function () {
            this.obj.doSomething(true, true);

            assert.equals(this.message("calledWith", this.obj.doSomething, sinon.match.any, false),
                          "expected doSomething to be called with arguments " +
                          "any, false\n    doSomething(true, true)");
        },

        "assert.calledWith match.defined exception message": function () {
            this.obj.doSomething();

            assert.equals(this.message("calledWith", this.obj.doSomething, sinon.match.defined),
                          "expected doSomething to be called with arguments " +
                          "defined\n    doSomething()");
        },

        "assert.calledWith match.truthy exception message": function () {
            this.obj.doSomething();

            assert.equals(this.message("calledWith", this.obj.doSomething, sinon.match.truthy),
                         "expected doSomething to be called with arguments " +
                         "truthy\n    doSomething()");
        },

        "assert.calledWith match.falsy exception message": function () {
            this.obj.doSomething(true);

            assert.equals(this.message("calledWith", this.obj.doSomething, sinon.match.falsy),
                          "expected doSomething to be called with arguments " +
                          "falsy\n    doSomething(true)");
        },

        "assert.calledWith match.same exception message": function () {
            this.obj.doSomething();

            assert.equals(this.message("calledWith", this.obj.doSomething, sinon.match.same(1)),
                          "expected doSomething to be called with arguments " +
                          "same(1)\n    doSomething()");
        },

        "assert.calledWith match.typeOf exception message": function () {
            this.obj.doSomething();

            assert.equals(this.message("calledWith", this.obj.doSomething, sinon.match.typeOf("string")),
                          "expected doSomething to be called with arguments " +
                          "typeOf(\"string\")\n    doSomething()");
        },

        "assert.calledWith match.instanceOf exception message": function () {
            this.obj.doSomething();

            assert.equals(this.message("calledWith", this.obj.doSomething, sinon.match.instanceOf(function CustomType() {})),
                          "expected doSomething to be called with arguments " +
                          "instanceOf(CustomType)\n    doSomething()");
        },

        "assert.calledWith match object exception message": function () {
            this.obj.doSomething();

            assert.equals(this.message("calledWith", this.obj.doSomething, sinon.match({ some: "value", and: 123 })),
                          "expected doSomething to be called with arguments " +
                          "match(some: value, and: 123)\n    doSomething()");
        },

        "assert.calledWith match boolean exception message": function () {
            this.obj.doSomething();

            assert.equals(this.message("calledWith", this.obj.doSomething, sinon.match(true)),
                          "expected doSomething to be called with arguments " +
                          "match(true)\n    doSomething()");
        },

        "assert.calledWith match number exception message": function () {
            this.obj.doSomething();

            assert.equals(this.message("calledWith", this.obj.doSomething, sinon.match(123)),
                          "expected doSomething to be called with arguments " +
                          "match(123)\n    doSomething()");
        },

        "assert.calledWith match string exception message": function () {
            this.obj.doSomething();

            assert.equals(this.message("calledWith", this.obj.doSomething, sinon.match("Sinon")),
                          "expected doSomething to be called with arguments " +
                          "match(\"Sinon\")\n    doSomething()");
        },

        "assert.calledWith match regexp exception message": function () {
            this.obj.doSomething();

            assert.equals(this.message("calledWith", this.obj.doSomething, sinon.match(/[a-z]+/)),
                          "expected doSomething to be called with arguments " +
                          "match(/[a-z]+/)\n    doSomething()");
        },

        "assert.calledWith match test function exception message": function () {
            this.obj.doSomething();

            assert.equals(this.message("calledWith", this.obj.doSomething, sinon.match({ test: function custom() {} })),
                          "expected doSomething to be called with arguments " +
                          "match(custom)\n    doSomething()");
        },

        "assert.calledWithMatch exception message": function () {
            this.obj.doSomething(1, 3, "hey");

            assert.equals(this.message("calledWithMatch", this.obj.doSomething, 4, 3, "hey"),
                          "expected doSomething to be called with match 4, 3, " +
                          "hey\n    doSomething(1, 3, hey)");
        },

        "assert.alwaysCalledWith exception message": function () {
            this.obj.doSomething(1, 3, "hey");
            this.obj.doSomething(1, "hey");

            assert.equals(this.message("alwaysCalledWith", this.obj.doSomething, 1, "hey"),
                          "expected doSomething to always be called with arguments 1" +
                          ", hey\n    doSomething(1, 3, hey)\n    doSomething(1, hey)");
        },

        "assert.alwaysCalledWithMatch exception message": function () {
            this.obj.doSomething(1, 3, "hey");
            this.obj.doSomething(1, "hey");

            assert.equals(this.message("alwaysCalledWithMatch", this.obj.doSomething, 1, "hey"),
                          "expected doSomething to always be called with match 1" +
                          ", hey\n    doSomething(1, 3, hey)\n    doSomething(1, hey)");
        },

        "assert.calledWithExactly exception message": function () {
            this.obj.doSomething(1, 3, "hey");

            assert.equals(this.message("calledWithExactly", this.obj.doSomething, 1, 3),
                          "expected doSomething to be called with exact arguments 1" +
                          ", 3\n    doSomething(1, 3, hey)");
        },

        "assert.alwaysCalledWithExactly exception message": function () {
            this.obj.doSomething(1, 3, "hey");
            this.obj.doSomething(1, 3);

            assert.equals(this.message("alwaysCalledWithExactly", this.obj.doSomething, 1, 3),
                          "expected doSomething to always be called with exact " +
                          "arguments 1, 3\n    doSomething(1, 3, hey)\n    " +
                          "doSomething(1, 3)");
        },

        "assert.neverCalledWith exception message": function () {
            this.obj.doSomething(1, 2, 3);

            assert.equals(this.message("neverCalledWith", this.obj.doSomething, 1, 2),
                          "expected doSomething to never be called with " +
                          "arguments 1, 2\n    doSomething(1, 2, 3)");
        },

        "assert.neverCalledWithMatch exception message": function () {
            this.obj.doSomething(1, 2, 3);

            assert.equals(this.message("neverCalledWithMatch", this.obj.doSomething, 1, 2),
                          "expected doSomething to never be called with match " +
                          "1, 2\n    doSomething(1, 2, 3)");
        },

        "assert.threw exception message": function () {
            this.obj.doSomething(1, 3, "hey");
            this.obj.doSomething(1, 3);

            assert.equals(this.message("threw", this.obj.doSomething),
                          "doSomething did not throw exception\n" +
                          "    doSomething(1, 3, hey)\n    doSomething(1, 3)");
        },

        "assert.alwaysThrew exception message": function () {
            this.obj.doSomething(1, 3, "hey");
            this.obj.doSomething(1, 3);

            assert.equals(this.message("alwaysThrew", this.obj.doSomething),
                          "doSomething did not always throw exception\n" +
                          "    doSomething(1, 3, hey)\n    doSomething(1, 3)");
        },

        "assert.match exception message": function () {
            assert.equals(this.message("match", { foo: 1 }, [1, 3]),
                          "expected value to match\n" +
                          "    expected = [1, 3]\n" +
                          "    actual = { foo: 1 }");
        }
    }
});
