/*jslint onevar: false, eqeqeq: false*/
/*globals window sinon buster*/
/**
* @author Maximilian Antoni (mail@maxantoni.de)
* @license BSD
*
* Copyright (c) 2012 Maximilian Antoni
* Copyright (c) 2012 Christian Johansen
 */
"use strict";

if (typeof require == "function" && typeof module == "object") {
    var testCase = require("../runner");
    var sinon = require("../../lib/sinon");
}

function propertyMatcherTests(matcher) {
    return {
        "returns matcher": function () {
            var has = matcher("foo");

            assert(sinon.match.isMatcher(has));
        },

        "throws if first argument is not string": function () {
            assert.exception(function () {
                matcher();
            }, "TypeError");
            assert.exception(function () {
                matcher(123);
            }, "TypeError");
        },

        "returns false if value is undefined or null": function () {
            var has = matcher("foo");

            assert.isFalse(has.test(undefined));
            assert.isFalse(has.test(null));
        },

        "returns true if object has property": function () {
            var has = matcher("foo");

            assert(has.test({ foo: null }));
        },

        "returns false if object value is not equal to given value": function () {
            var has = matcher("foo", 1);

            assert.isFalse(has.test({ foo: null }));
        },

        "returns true if object value is equal to given value": function () {
            var has = matcher("message", "sinon rocks");

            assert(has.test({ message: "sinon rocks" }));
            assert(has.test(new Error("sinon rocks")));
        },

        "returns true if string property matches": function () {
            var has = matcher("length", 5);

            assert(has.test("sinon"));
        },

        "allows to expect undefined": function () {
            var has = matcher("foo", undefined);

            assert.isFalse(has.test({ foo: 1 }));
        },

        "compares value deeply": function () {
            var has = matcher("foo", { bar: "doo", test: 42 });

            assert(has.test({ foo: { bar: "doo", test: 42 } }));
        },

        "compares with matcher": function () {
            var has = matcher("callback", sinon.match.typeOf("function"));

            assert(has.test({ callback: function () {} }));
        }
    };
}

buster.testCase("sinon.match", {
    "returns matcher": function () {
        var match = sinon.match(function () {});

        assert(sinon.match.isMatcher(match));
    },

    "exposes test function": function () {
        var test = function () {};

        var match = sinon.match(test);

        assert.same(match.test, test);
    },

    "returns true if properties are equal": function () {
        var match = sinon.match({ str: "sinon", nr: 1 });

        assert(match.test({ str: "sinon", nr: 1, other: "ignored" }));
    },

    "returns true if properties are deep equal": function () {
        var match = sinon.match({ deep: { str: "sinon" } });

        assert(match.test({ deep: { str: "sinon", ignored: "value" } }));
    },

    "returns false if a property is not equal": function () {
        var match = sinon.match({ str: "sinon", nr: 1 });

        assert.isFalse(match.test({ str: "sinon", nr: 2 }));
    },

    "returns false if a property is missing": function () {
        var match = sinon.match({ str: "sinon", nr: 1 });

        assert.isFalse(match.test({ nr: 1 }));
    },

    "returns true if array is equal": function () {
        var match = sinon.match({ arr: ["a", "b"] });

        assert(match.test({ arr: ["a", "b"] }));
    },

    "returns false if array is not equal": function () {
        var match = sinon.match({ arr: ["b", "a"] });

        assert.isFalse(match.test({ arr: ["a", "b"] }));
    },

    "returns true if number objects are equal": function () {
        var match = sinon.match({ one : new Number(1) });

        assert(match.test({ one : new Number(1) }));
    },

    "returns true if test matches": function () {
        var match = sinon.match({ prop: sinon.match.typeOf("boolean") });

        assert(match.test({ prop: true }));
    },

    "returns false if test does not match": function () {
        var match = sinon.match({ prop: sinon.match.typeOf("boolean") });

        assert.isFalse(match.test({ prop: "no" }));
    },

    "returns true if deep test matches": function () {
        var match = sinon.match({ deep: { prop: sinon.match.typeOf("boolean") } });

        assert(match.test({ deep: { prop: true } }));
    },

    "returns false if deep test does not match": function () {
        var match = sinon.match({ deep: { prop: sinon.match.typeOf("boolean") } });

        assert.isFalse(match.test({ deep: { prop: "no" } }));
    },

    "returns false if tested value is null or undefined": function () {
        var match = sinon.match({});

        assert.isFalse(match.test(null));
        assert.isFalse(match.test(undefined));
    },

    "returns true if error message matches": function () {
        var match = sinon.match({ message: "evil error" });

        assert(match.test(new Error("evil error")));
    },

    "returns true if string property matches": function () {
        var match = sinon.match({ length: 5 });

        assert(match.test("sinon"));
    },

    "returns true if number property matches": function () {
        var match = sinon.match({ toFixed: sinon.match.func });

        assert(match.test(0));
    },

    "returns true for string match": function () {
        var match = sinon.match("sinon");

        assert(match.test("sinon"));
    },

    "returns true for substring match": function () {
        var match = sinon.match("no");

        assert(match.test("sinon"));
    },

    "returns false for string mismatch": function () {
        var match = sinon.match("Sinon.JS");

        assert.isFalse(match.test(null));
        assert.isFalse(match.test({}));
        assert.isFalse(match.test("sinon"));
        assert.isFalse(match.test("sinon.js"));
    },

    "returns true for regexp match": function () {
        var match = sinon.match(/^[sino]+$/);

        assert(match.test("sinon"));
    },

    "returns false for regexp string mismatch": function () {
        var match = sinon.match(/^[sin]+$/);

        assert.isFalse(match.test("sinon"));
    },

    "returns false for regexp type mismatch": function () {
        var match = sinon.match(/.*/);

        assert.isFalse(match.test());
        assert.isFalse(match.test(null));
        assert.isFalse(match.test(123));
        assert.isFalse(match.test({}));
    },

    "returns true for number match": function () {
        var match = sinon.match(1);

        assert(match.test(1));
        assert(match.test("1"));
        assert(match.test(true));
    },

    "returns false for number mismatch": function () {
        var match = sinon.match(1);

        assert.isFalse(match.test());
        assert.isFalse(match.test(null));
        assert.isFalse(match.test(2));
        assert.isFalse(match.test(false));
        assert.isFalse(match.test({}));
    },

    "returns true if test function in object returns true": function () {
        var match = sinon.match({ test: function () { return true; }});

        assert(match.test());
    },

    "returns false if test function in object returns false": function () {
        var match = sinon.match({ test: function () { return false; }});

        assert.isFalse(match.test());
    },

    "returns false if test function in object returns nothing": function () {
        var match = sinon.match({ test: function () {}});

        assert.isFalse(match.test());
    },

    "passes actual value to test function in object": function () {
        var match = sinon.match({ test: function (arg) { return arg; }});

        assert(match.test(true));
    },

    "uses matcher": function () {
        var match = sinon.match(sinon.match("test"));

        assert(match.test("testing"));
    },

    "toString": {
        "returns message": function () {
            var message = "hello sinon.match";

            var match = sinon.match(function () {}, message);

            assert.same(match.toString(), message);
        },

        "defaults to match(functionName)": function () {
            var match = sinon.match(function custom() {});

            assert.same(match.toString(), "match(custom)");
        }
    },

    "any": {
        "is matcher": function () {
            assert(sinon.match.isMatcher(sinon.match.any));
        },

        "returns true when tested": function () {
            assert(sinon.match.any.test());
        }
    },

    "defined": {
        "is matcher": function () {
            assert(sinon.match.isMatcher(sinon.match.defined));
        },

        "returns false if test is called with null": function () {
            assert.isFalse(sinon.match.defined.test(null));
        },

        "returns false if test is called with undefined": function () {
            assert.isFalse(sinon.match.defined.test(undefined));
        },

        "returns true if test is called with any value": function () {
            assert(sinon.match.defined.test(false));
            assert(sinon.match.defined.test(true));
            assert(sinon.match.defined.test(0));
            assert(sinon.match.defined.test(1));
            assert(sinon.match.defined.test(""));
        },

        "returns true if test is called with any object": function () {
            assert(sinon.match.defined.test({}));
            assert(sinon.match.defined.test(function () {}));
        }
    },

    "truthy": {
        "is matcher": function () {
            assert(sinon.match.isMatcher(sinon.match.truthy));
        },

        "returns true if test is called with trueish value": function () {
            assert(sinon.match.truthy.test(true));
            assert(sinon.match.truthy.test(1));
            assert(sinon.match.truthy.test("yes"));
        },

        "returns false if test is called falsy value": function () {
            assert.isFalse(sinon.match.truthy.test(false));
            assert.isFalse(sinon.match.truthy.test(null));
            assert.isFalse(sinon.match.truthy.test(undefined));
            assert.isFalse(sinon.match.truthy.test(""));
        }
    },

    "falsy": {
        "is matcher": function () {
            assert(sinon.match.isMatcher(sinon.match.falsy));
        },

        "returns true if test is called falsy value": function () {
            assert(sinon.match.falsy.test(false));
            assert(sinon.match.falsy.test(null));
            assert(sinon.match.falsy.test(undefined));
            assert(sinon.match.falsy.test(""));
        },

        "returns false if test is called with trueish value": function () {
            assert.isFalse(sinon.match.falsy.test(true));
            assert.isFalse(sinon.match.falsy.test(1));
            assert.isFalse(sinon.match.falsy.test("yes"));
        }
    },

    "same": {
        "returns matcher": function () {
            var same = sinon.match.same();

            assert(sinon.match.isMatcher(same));
        },

        "returns true if test is called with same argument": function () {
            var object = {};
            var same = sinon.match.same(object);

            assert(same.test(object));
        },

        "returns false if test is not called with same argument": function () {
            var same = sinon.match.same({});

            assert.isFalse(same.test({}));
        }
    },

    "typeOf": {
        "throws if given argument is not a string": function () {
            assert.exception(function () {
                sinon.match.typeOf();
            }, "TypeError");
            assert.exception(function () {
                sinon.match.typeOf(123);
            }, "TypeError");
        },

        "returns matcher": function () {
            var typeOf = sinon.match.typeOf("string");

            assert(sinon.match.isMatcher(typeOf));
        },

        "returns true if test is called with string": function () {
            var typeOf = sinon.match.typeOf("string");

            assert(typeOf.test("Sinon.JS"));
        },

        "returns false if test is not called with string": function () {
            var typeOf = sinon.match.typeOf("string");

            assert.isFalse(typeOf.test(123));
        },

        "returns true if test is called with regexp": function () {
            var typeOf = sinon.match.typeOf("regexp");

            assert(typeOf.test(/.+/));
        },

        "returns false if test is not called with regexp": function () {
            var typeOf = sinon.match.typeOf("regexp");

            assert.isFalse(typeOf.test(true));
        }
    },

    "instanceOf": {
        "throws if given argument is not a function": function () {
            assert.exception(function () {
                sinon.match.instanceOf();
            }, "TypeError");
            assert.exception(function () {
                sinon.match.instanceOf("foo");
            }, "TypeError");
        },

        "returns matcher": function () {
            var instanceOf = sinon.match.instanceOf(function () {});

            assert(sinon.match.isMatcher(instanceOf));
        },

        "returns true if test is called with instance of argument": function () {
            var instanceOf = sinon.match.instanceOf(Array);

            assert(instanceOf.test([]));
        },

        "returns false if test is not called with instance of argument": function () {
            var instanceOf = sinon.match.instanceOf(Array);

            assert.isFalse(instanceOf.test({}));
        }
    },

    "has": propertyMatcherTests(sinon.match.has),
    "hasOwn": propertyMatcherTests(sinon.match.hasOwn),

    "hasSpecial": {
        "returns true if object has inherited property": function () {
            var has = sinon.match.has("toString");

            assert(has.test({}));
        },

        "only includes property in message": function () {
            var has = sinon.match.has("test");

            assert.equals(has.toString(), "has(\"test\")");
        },

        "includes property and value in message": function () {
            var has = sinon.match.has("test", undefined);

            assert.equals(has.toString(), "has(\"test\", undefined)");
        },

        "returns true if string function matches": function () {
            var has = sinon.match.has("toUpperCase", sinon.match.func);

            assert(has.test("sinon"));
        },

        "returns true if number function matches": function () {
            var has = sinon.match.has("toFixed", sinon.match.func);

            assert(has.test(0));
        }
    },

    "hasOwnSpecial": {
        "returns false if object has inherited property": function () {
            var hasOwn = sinon.match.hasOwn("toString");

            assert.isFalse(hasOwn.test({}));
        },

        "only includes property in message": function () {
            var hasOwn = sinon.match.hasOwn("test");

            assert.equals(hasOwn.toString(), "hasOwn(\"test\")");
        },

        "includes property and value in message": function () {
            var hasOwn = sinon.match.hasOwn("test", undefined);

            assert.equals(hasOwn.toString(), "hasOwn(\"test\", undefined)");
        }
    },

    "bool": {
        "is typeOf boolean matcher": function () {
            var bool = sinon.match.bool;

            assert(sinon.match.isMatcher(bool));
            assert.equals(bool.toString(), "typeOf(\"boolean\")");
        }
    },

    "number": {
        "is typeOf number matcher": function () {
            var number = sinon.match.number;

            assert(sinon.match.isMatcher(number));
            assert.equals(number.toString(), "typeOf(\"number\")");
        }
    },

    "string": {
        "is typeOf string matcher": function () {
            var string = sinon.match.string;

            assert(sinon.match.isMatcher(string));
            assert.equals(string.toString(), "typeOf(\"string\")");
        }
    },

    "object": {
        "is typeOf object matcher": function () {
            var object = sinon.match.object;

            assert(sinon.match.isMatcher(object));
            assert.equals(object.toString(), "typeOf(\"object\")");
        }
    },

    "func": {
        "is typeOf function matcher": function () {
            var func = sinon.match.func;

            assert(sinon.match.isMatcher(func));
            assert.equals(func.toString(), "typeOf(\"function\")");
        }
    },

    "array": {
        "is typeOf array matcher": function () {
            var array = sinon.match.array;

            assert(sinon.match.isMatcher(array));
            assert.equals(array.toString(), "typeOf(\"array\")");
        }
    },

    "regexp": {
        "is typeOf regexp matcher": function () {
            var regexp = sinon.match.regexp;

            assert(sinon.match.isMatcher(regexp));
            assert.equals(regexp.toString(), "typeOf(\"regexp\")");
        }
    },

    "date": {
        "is typeOf regexp matcher": function () {
            var date = sinon.match.date;

            assert(sinon.match.isMatcher(date));
            assert.equals(date.toString(), "typeOf(\"date\")");
        }
    },

    "or": {
        "is matcher": function () {
            var numberOrString = sinon.match.number.or(sinon.match.string);

            assert(sinon.match.isMatcher(numberOrString));
            assert.equals(numberOrString.toString(),
                          "typeOf(\"number\").or(typeOf(\"string\"))");
        },

        "requires matcher argument": function () {
            assert.exception(function () {
                sinon.match.instanceOf(Error).or();
            }, "TypeError");
        },

        "will coerce argument to matcher": function () {
            var abcOrDef = sinon.match("abc").or("def");

            assert(sinon.match.isMatcher(abcOrDef));
            assert.equals(abcOrDef.toString(),
                          "match(\"abc\").or(match(\"def\"))");
        },

        "returns true if either matcher matches": function () {
            var numberOrString = sinon.match.number.or(sinon.match.string);

            assert(numberOrString.test(123));
            assert(numberOrString.test("abc"));
        },

        "returns false if neither matcher matches": function () {
            var numberOrAbc = sinon.match.number.or("abc");

            assert.isFalse(numberOrAbc.test(/.+/));
            assert.isFalse(numberOrAbc.test(new Date()));
            assert.isFalse(numberOrAbc.test({}));
        },

        "can be used with undefined": function () {
            var numberOrUndef = sinon.match.number.or(undefined);

            assert(numberOrUndef.test(123));
            assert(numberOrUndef.test(undefined));
        }
    },

    "and": {
        "is matcher": function () {
            var fooAndBar = sinon.match.has("foo").and(sinon.match.has("bar"));

            assert(sinon.match.isMatcher(fooAndBar));
            assert.equals(fooAndBar.toString(), "has(\"foo\").and(has(\"bar\"))");
        },

        "requires matcher argument": function () {
            assert.exception(function () {
                sinon.match.instanceOf(Error).and();
            }, "TypeError");
        },

        "will coerce to matcher": function () {
            var abcOrObj = sinon.match("abc").or({a:1});

            assert(sinon.match.isMatcher(abcOrObj));
            assert.equals(abcOrObj.toString(),
                          "match(\"abc\").or(match(a: 1))");
        },

        "returns true if both matchers match": function () {
            var fooAndBar = sinon.match.has("foo").and({ bar: "bar" });

            assert(fooAndBar.test({ foo: "foo", bar: "bar" }));
        },

        "returns false if either matcher does not match": function () {
            var fooAndBar = sinon.match.has("foo").and(sinon.match.has("bar"));

            assert.isFalse(fooAndBar.test({ foo: "foo" }));
            assert.isFalse(fooAndBar.test({ bar: "bar" }));
        },

        "can be used with undefined": function () {
            var falsyAndUndefined = sinon.match.falsy.and(undefined);

            assert.isFalse(falsyAndUndefined.test(false));
            assert(falsyAndUndefined.test(undefined));
        }
    }
});
