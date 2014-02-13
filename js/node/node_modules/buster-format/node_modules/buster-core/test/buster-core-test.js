(function (B, sinon) {
    var isNode = typeof require == "function" && typeof module == "object";
    var bu, assert;

    if (isNode) {
        B = require("../lib/buster-core");
        sinon = require("sinon");
        bu = require("buster-util");
        assert = require("assert");
    } else {
        bu = buster.util;
    }

    bu.testCase("BusterBindTest", {
        "should call function with bound this object": function () {
            var func = sinon.spy();
            var obj = {};
            var bound = B.bind(obj, func);

            bound();
            assert.equal(func.thisValues[0], obj);

            bound.call({});
            assert.equal(func.thisValues[1], obj);

            bound.apply({});
            assert.equal(func.thisValues[2], obj);
        },

        "should call method with bound this object": function () {
            var obj = { meth: sinon.spy() };
            var bound = B.bind(obj, "meth");

            bound();
            assert.equal(obj.meth.thisValues[0], obj);

            bound.call({});
            assert.equal(obj.meth.thisValues[1], obj);

            bound.apply({});
            assert.equal(obj.meth.thisValues[2], obj);
        },

        "should call function with bound arguments": function () {
            var func = sinon.spy();
            var obj = {};
            var bound = B.bind(obj, func, 42, "Hey");

            bound();

            assert.ok(func.calledWith(42, "Hey"));
        },

        "should call function with bound arguments and passed arguments": function () {
            var func = sinon.spy();
            var obj = {};
            var bound = B.bind(obj, func, 42, "Hey");

            bound("Bound", []);
            assert.ok(func.calledWith(42, "Hey", "Bound", []));

            bound.call(null, ".call", []);
            assert.ok(func.calledWith(42, "Hey", ".call", []));

            bound.apply(null, [".apply", []]);
            assert.ok(func.calledWith(42, "Hey", ".apply", []));
        }
    });

    bu.testCase("BusterPartialTest", {
        "should return function": function () {
            assert.equal(typeof B.partial(function () {}), "function");
        },

        "should call original function": function () {
            var fn = sinon.spy();
            var func = B.partial(fn);
            func();
            assert.ok(fn.calledOnce);
        },

        "should pass bound arguments to function": function () {
            var fn = sinon.spy();
            var func = B.partial(fn, 1, 2);
            func();
            assert.ok(fn.calledWithExactly(1, 2));
        },

        "should pass bound and passed arguments to function": function () {
            var fn = sinon.spy();
            var func = B.partial(fn, 1, 2);
            func(3, 4);
            assert.ok(fn.calledWithExactly(1, 2, 3, 4));
        },

        "should call with provided this object": function () {
            var fn = sinon.spy();
            var func = B.partial(fn);
            var obj = { id: 42 };
            func.call(obj);
            assert.ok(fn.calledOn(obj));
        }
    });

    bu.testCase("BusterCreateTest", {
        "should create object inheriting from other object": function () {
            var obj = {};

            assert.ok(obj.isPrototypeOf(B.create(obj)));
        }
    });

    bu.testCase("BusterExtendTest", {
        "should copy properties from right to left": function () {
            var obj = { name: "Mr" };
            var obj2 = { id: 42 };
            var result = B.extend(obj, obj2);

            assert.equal(42, result.id);
            assert.equal(obj, result);
        },

        "should copy properties from all sources": function () {
            var obj = { name: "Mr" };
            var result = B.extend(obj, { a: 42 }, { b: 43 }, { c: 44 });

            assert.equal(result.a, 42);
            assert.equal(result.b, 43);
            assert.equal(result.c, 44);
        },

        "should prefer right-most properties on collisions": function () {
            var obj = { name: "Mr" };
            var result = B.extend(obj, { a: 42 }, { b: 43 }, { a: 44 });

            assert.equal(result.a, 44);
        }
    });

    bu.testCase("BusterFunctionNameTest", {
        "should get name from function declaration": function () {
            function myFunc() {}

            assert.equal(B.functionName(myFunc), "myFunc");
        },

        "should get name from named function expression": function () {
            var myFunc = function myFuncExpr() {}

            assert.equal(B.functionName(myFunc), "myFuncExpr");
        }
    });

    bu.testCase("BusterIsArrayTest", {
        "should pass for array": function () {
            assert.ok(B.isArray([]));
        },

        "should fail for array-like": function () {
            assert.ok(!B.isArray({ length: 1, 0: 1 }));
        },

        "should fail for arguments": function () {
            assert.ok(!B.isArray(arguments));
        }
    });

    bu.testCase("BusterFlattenTest", {
        "should not modify flat array": function () {
            assert.equal(B.flatten([1, 2, 3, 4]).length, 4);
        },

        "should flatten nested array": function () {
            assert.equal(B.flatten([[1, 2, 3, 4]]).length, 4);
        },

        "should flatten deeply nested array": function () {
            assert.equal(B.flatten([[[[1], 2], [3, 4]]]).length, 4);
        }
    });

    bu.testCase("BusterEachTest", {
        "should loop over all elements": function () {
            var fn = sinon.spy();
            B.each([1, 2, 3, 4], fn);

            assert.equal(fn.callCount, 4);
            assert.ok(fn.calledWith(1));
            assert.ok(fn.calledWith(2));
            assert.ok(fn.calledWith(3));
            assert.ok(fn.calledWith(4));
        }
    });

    bu.testCase("BusterMapTest", {
        "should loop over all elements": function () {
            var fn = sinon.spy();
            B.map([1, 2, 3, 4], fn);

            assert.equal(fn.callCount, 4);
            assert.ok(fn.calledWith(1));
            assert.ok(fn.calledWith(2));
            assert.ok(fn.calledWith(3));
            assert.ok(fn.calledWith(4));
        },

        "should return new array": function () {
            var fn = sinon.stub().returnsArg(0);
            var result = B.map([1, 2, 3, 4], fn);

            assert.equal(result.join(", "), "1, 2, 3, 4");
        },

        "should return new array with corresponding results": function () {
            var fn = sinon.stub().returns(0);
            var result = B.map([1, 2, 3, 4], fn);

            assert.equal(result.join(", "), "0, 0, 0, 0");
        }
    });

    bu.testCase("BusterSomeTest", {
        "should loop over all elements": function () {
            var fn = sinon.spy();
            B.some([1, 2, 3, 4], fn);

            assert.equal(fn.callCount, 4);
            assert.ok(fn.calledWith(1));
            assert.ok(fn.calledWith(2));
            assert.ok(fn.calledWith(3));
            assert.ok(fn.calledWith(4));
        },

        "should return true if one call returns true": function () {
            var fn = sinon.stub().returnsArg(0);
            var result = B.some([false, false, true, false], fn);

            assert.ok(result);
        },

        "should return false if no calls return true": function () {
            var fn = sinon.stub().returns(false);
            var result = B.some([1, 2, 3, 4], fn);

            assert.ok(!result);
        }
    });

    bu.testCase("BusterFilterTest", {
        "should filter items": function () {
            function isBigEnough(element) { return (element >= 10); }
            var filtered = B.filter([12, 5, 8, 130, 44], isBigEnough);

            assert.deepEqual(filtered, [12, 130, 44]);
        }
    });

    bu.testCase("BusterParallelTest", {
        setUp: function () {
            this.fns = [sinon.stub(), sinon.stub(), sinon.stub()];
        },

        "should call all functions": function () {
            B.parallel(this.fns);

            assert.ok(this.fns[0].calledOnce);
            assert.ok(this.fns[1].calledOnce);
            assert.ok(this.fns[2].calledOnce);
        },

        "should call callback immediately when no functions": function () {
            var callback = sinon.spy();
            B.parallel([], callback);

            assert.ok(callback.calledOnce);
        },

        "should do nothing when no functions and no callback": function () {
            assert.doesNotThrow(function () {
                B.parallel([]);
            });
        },

        "should not fail with no callback": function () {
            assert.doesNotThrow(function () {
                B.parallel([sinon.stub().yields()]);
            });
        },

        "should not call callback immediately": function () {
            var callback = sinon.spy();
            B.parallel(this.fns, callback);

            assert.ok(!callback.calledOnce);
        },

        "should call callback when single function completes": function () {
            var callback = sinon.spy();
            B.parallel([sinon.stub().yields()], callback);

            assert.ok(callback.calledOnce);
        },

        "should not call callback when first function completes": function () {
            var callback = sinon.spy();
            B.parallel([sinon.spy(), sinon.stub().yields()], callback);

            assert.ok(!callback.calledOnce);
        },

        "should call callback when all functions complete": function () {
            var callback = sinon.spy();
            B.parallel([sinon.stub().yields(), sinon.stub().yields()], callback);

            assert.ok(callback.calledOnce);
        },

        "should pass results to callback": function () {
            var callback = sinon.spy();
            B.parallel([sinon.stub().yields(null, 2),
                             sinon.stub().yields(null, 3)], callback);

            assert.ok(callback.calledWith(null, [2, 3]));
        },

        "should immediately pass error to callback": function () {
            var callback = sinon.spy();
            B.parallel([sinon.stub().yields({ message: "Ooops" }),
                             sinon.stub().yields(null, 3)], callback);

            assert.ok(callback.calledWith({ message: "Ooops" }));
        },

        "should only call callback once": function () {
            var callback = sinon.spy();
            B.parallel([sinon.stub().yields({ message: "Ooops" }),
                             sinon.stub().yields({ message: "Nooo!" })], callback);

            assert.ok(callback.calledOnce);
        },

        "should only call callback once with one failing and one passing": function () {
            var callback = sinon.spy();
            B.parallel([sinon.stub().yields(null), sinon.stub().yields({})], callback);

            assert.ok(callback.calledOnce);
        }
    });

    bu.testCase("BusterSeriesTest", {
        setUp: function () {
            this.fns = [sinon.stub(), sinon.stub(), sinon.stub()];
        },

        "calls first function": function () {
            B.series(this.fns);

            assert.ok(this.fns[0].calledOnce);
            assert.ok(!this.fns[1].called);
            assert.ok(!this.fns[2].called);
        },

        "calls callback immediately when no functions": function () {
            var callback = sinon.spy();
            B.series([], callback);

            assert.ok(callback.calledOnce);
        },

        "does nothing when no functions and no callback": function () {
            assert.doesNotThrow(function () {
                B.series([]);
            });
        },

        "does not fail with no callback": function () {
            assert.doesNotThrow(function () {
                B.series([sinon.stub().yields()]);
            });
        },

        "does not call callback immediately": function () {
            var callback = sinon.spy();
            B.series(this.fns, callback);

            assert.ok(!callback.calledOnce);
        },

        "calls callback when single function completes": function () {
            var callback = sinon.spy();
            B.series([sinon.stub().yields()], callback);

            assert.ok(callback.calledOnce);
        },

        "calls callback when single promise resolves": function () {
            var callback = sinon.spy();
            var promise = { then: sinon.stub().yields() };
            B.series([sinon.stub().returns(promise)], callback);

            assert.ok(callback.calledOnce);
        },

        "does not call callback when first function completes": function () {
            var callback = sinon.spy();
            B.series([sinon.stub().yields(), sinon.spy()], callback);

            assert.ok(!callback.calledOnce);
        },

        "calls next function when first function completes": function () {
            var callback = sinon.spy();
            var fns = [sinon.stub().yields(), sinon.spy()];
            B.series(fns, callback);

            assert.ok(fns[1].calledOnce);
        },

        "calls next function when first promise resolves": function () {
            var callback = sinon.spy();
            var promise = { then: sinon.stub().yields() };
            var fns = [sinon.stub().returns(promise), sinon.spy()];
            B.series(fns, callback);

            assert.ok(fns[1].calledOnce);
        },

        "calls callback when all functions complete": function () {
            var callback = sinon.spy();
            B.series([sinon.stub().yields(), sinon.stub().yields()], callback);

            assert.ok(callback.calledOnce);
        },

        "passes results to callback": function () {
            var callback = sinon.spy();
            B.series([sinon.stub().yields(null, 2),
                           sinon.stub().yields(null, 3)], callback);

            assert.ok(callback.calledWith(null, [2, 3]));
        },

        "immediately passes error to callback": function () {
            var callback = sinon.spy();
            B.series([sinon.stub().yields({ message: "Ooops" }),
                           sinon.stub().yields(null, 3)], callback);

            assert.ok(callback.calledWith({ message: "Ooops" }));
        },

        "immediately passes promise rejection to callback": function () {
            var callback = sinon.spy();
            var promise = { then: sinon.stub().callsArgWith(1, { message: "Ooops" }) };
            B.series([sinon.stub().returns(promise),
                           sinon.stub().yields(null, 3)], callback);

            assert.ok(callback.calledWith({ message: "Ooops" }));
        },

        "does not use promise resolution as error": function () {
            var callback = sinon.spy();
            var promise = { then: sinon.stub().yields({ id: 42 }) };
            var second = sinon.spy();
            B.series([sinon.stub().returns(promise), second], callback);

            assert.ok(second.calledOnce);
        },

        "collects results from promises and callbacks": function () {
            var callback = sinon.spy();
            var promise = { then: sinon.stub().yields({ id: 42 }) };

            B.series([sinon.stub().returns(promise),
                           sinon.stub().yields(null, { id: 12 })], callback);

            assert.ok(callback.calledWith(null, [{ id: 42 }, { id: 12 }]));
        },

        "does not call next when function errors": function () {
            var callback = sinon.spy();
            var fns = [sinon.stub().yields({ message: "Ooops" }),
                       sinon.stub().yields(null, 3)];
            B.series(fns, callback);

            assert.ok(!fns[1].called);
        },

        "only calls callback once": function () {
            var callback = sinon.spy();
            B.series([sinon.stub().yields({ message: "Ooops" }),
                           sinon.stub().yields({ message: "Nooo!" })], callback);

            assert.ok(callback.calledOnce);
        },

        "does not fail if error and no callback": function () {
            assert.doesNotThrow(function () {
                B.series([sinon.stub().yields({ message: "Ooops" }),
                               sinon.stub().yields({ message: "Nooo!" })]);
            });
        },

        "only calls callback once with one failing and one passing": function () {
            var callback = sinon.spy();
            B.series([sinon.stub().yields(null), sinon.stub().yields({})], callback);

            assert.ok(callback.calledOnce);
        }
    });


    bu.testCase("BusterCountDownTest", {
        "should call callback after one call": function () {
            var callback = sinon.spy();
            var cd = B.countdown(1, callback);
            cd();

            assert.ok(callback.calledOnce);
        },

        "should not call callback before reaching enough counts": function () {
            var callback = sinon.spy();
            var cd = B.countdown(2, callback);

            cd();
            assert.ok(!callback.called);

            cd();
            assert.ok(callback.calledOnce);
        },

        "should only call callback once": function () {
            var callback = sinon.spy();
            var cd = B.countdown(2, callback);

            cd();
            cd();
            cd();
            assert.ok(callback.calledOnce);
        }
    });

    if (typeof process === "undefined") { return; }

    var path = require("path");

    if (process.platform != "win32") {
        bu.testCase("TmpFile", {
            "returns pathname": function () {
                var file = "/home/christian/projects/myproject/buster.cache";
                assert.equal(B.tmpFile(file),
                             "/tmp/2f4a2c82aed0d4748c03818f69f2a26c8e49bfff");
            }
        });
    } else {
        bu.testCase("TmpFile", {
            "returns pathname": function () {
                var file = "c:\\Foo\\Bar\\buster.cache";
                assert.equal(B.tmpFile(file),
                             path.join(process.env["TEMP"], "2c1b809e8e3ae0dae467a80f19bfe712b67593f8"));
            }
        });
    }
}(typeof buster == "object" ? buster : null, typeof sinon == "object" ? sinon : null));