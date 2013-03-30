//<%
/**
 * jsUnity Universal JavaScript Testing Framework v0.6
 * http://jsunity.com/
 *
 * Copyright (c) 2009 Ates Goral
 * Licensed under the MIT license.
 * http://www.opensource.org/licenses/mit-license.php
 */

exports.jsUnity = (function () {
    function fmt(str) {
        var a = Array.prototype.slice.call(arguments, 1);
        return str.replace(/\?/g, function () { return a.shift(); });
    }

    function hash(v) {
        if (v instanceof Object && v != null) {
            var arr = [];
            
            for (var p in v) {
                                if (v.hasOwnProperty(p)) {
                                        arr.push(p);
                                        arr.push(hash(v[p]));    
                                }
            }
            
            return arr.join("#");
        } else {
            return String(v);
        }
    }
    
    var defaultAssertions = {
        assertException: function (fn, message) {
            try {
                fn instanceof Function && fn();
            } catch (e) {
                return;
            }

            throw fmt("?: (?) does not raise an exception or not a function",
                message || "assertException", fn);
        },

        assertTrue: function (actual, message) {
            if (!actual) {
                throw fmt("?: (?) does not evaluate to true",
                    message || "assertTrue", actual);
            }
        },
        
        assertFalse: function (actual, message) {
            if (actual) {
                throw fmt("?: (?) does not evaluate to false",
                    message || "assertFalse", actual);
            }
        },
        
        assertIdentical: function (expected, actual, message) {
            if (expected !== actual) {
                throw fmt("?: (?) is not identical to (?)",
                    message || "assertIdentical", actual, expected);
            }
        },

        assertNotIdentical: function (expected, actual, message) {
            if (expected === actual) {
                throw fmt("?: (?) is identical to (?)",
                    message || "assertNotIdentical", actual, expected);
            }
        },

        assertEqual: function (expected, actual, message) {
            if (hash(expected) != hash(actual)) {
                throw fmt("?: (?) is not equal to (?)",
                    message || "assertEqual", actual, expected);
            }
        },
        
        assertNotEqual: function (expected, actual, message) {
            if (hash(expected) == hash(actual)) {
                throw fmt("?: (?) is equal to (?)",
                    message || "assertNotEqual", actual, expected);
            }
        },
        
        assertMatch: function (re, actual, message) {
            if (!re.test(actual)) {
                throw fmt("?: (?) does not match (?)",
                    message || "assertMatch", actual, re);
            }
        },
        
        assertNotMatch: function (re, actual, message) {
            if (re.test(actual)) {
                throw fmt("?: (?) matches (?)",
                    message || "assertNotMatch", actual, re);
            }
        },
        
        assertTypeOf: function (typ, actual, message) {
            if (typeof actual !== typ) {
                throw fmt("?: (?) is not of type (?)",
                    message || "assertTypeOf", actual, typ);
            }
        },

        assertNotTypeOf: function (typ, actual, message) {
            if (typeof actual === typ) {
                throw fmt("?: (?) is of type (?)",
                    message || "assertNotTypeOf", actual, typ);
            }
        },
        
        assertInstanceOf: function (cls, actual, message) {
            if (!(actual instanceof cls)) {
                throw fmt("?: (?) is not an instance of (?)",
                    message || "assertInstanceOf", actual, cls);
            }
        },

        assertNotInstanceOf: function (cls, actual, message) {
            if (actual instanceof cls) {
                throw fmt("?: (?) is an instance of (?)",
                    message || "assertNotInstanceOf", actual, cls);
            }
        },

        assertNull: function (actual, message) {
            if (actual !== null) {
                throw fmt("?: (?) is not null",
                    message || "assertNull", actual);
            }
        },
        
        assertNotNull: function (actual, message) {
            if (actual === null) {
                throw fmt("?: (?) is null",
                    message || "assertNotNull", actual);
            }
        },
        
        assertUndefined: function (actual, message) {
            if (actual !== undefined) {
                throw fmt("?: (?) is not undefined",
                    message || "assertUndefined", actual);
            }
        },
        
        assertNotUndefined: function (actual, message) {
            if (actual === undefined) {
                throw fmt("?: (?) is undefined",
                    message || "assertNotUndefined", actual);
            }
        },
        
        assertNaN: function (actual, message) {
            if (!isNaN(actual)) {
                throw fmt("?: (?) is not NaN",
                    message || "assertNaN", actual);
            }
        },
        
        assertNotNaN: function (actual, message) {
            if (isNaN(actual)) {
                throw fmt("?: (?) is NaN",
                    message || "assertNotNaN", actual);
            }
        },
        
        fail: function (message) {
            throw message || "fail";
        }
    };
    
    function empty() {}
    
    function plural(cnt, unit) {
        return cnt + " " + unit + (cnt == 1 ? "" : "s");
    }

    function splitFunction(fn) {
        var tokens =
            /^[\s\r\n]*function[\s\r\n]*([^\(\s\r\n]*?)[\s\r\n]*\([^\)\s\r\n]*\)[\s\r\n]*\{((?:[^}]*\}?)+)\}[\s\r\n]*$/
            .exec(fn);
        
        return {
            name: tokens[1].length ? tokens[1] : undefined,
            body: tokens[2]
        };
    }
    
    var probeOutside = function () {
        try {
            return eval(
                [ "typeof ", " === \"function\" && ", "" ].join(arguments[0]));
        } catch (e) {
            return false;
        }
    };

    function parseSuiteString(str) {
        var obj = {};

        var probeInside = new Function(
            splitFunction(probeOutside).body + str);

        var tokenRe = /(\w+)/g; // todo: wiser regex
        var tokens;

        while ((tokens = tokenRe.exec(str))) {
            var token = tokens[1];
            var fn;
    
            if (!obj[token]
                && (fn = probeInside(token))
                && fn != probeOutside(token)) {

                obj[token] = fn;
            }
        }

        return parseSuiteObject(obj);
    }

    function parseSuiteFunction(fn) {
        var fnParts = splitFunction(fn);
        var suite = parseSuiteString(fnParts.body);

        suite.suiteName = fnParts.name;

        return suite;
    }

    function parseSuiteArray(tests) {
        var obj = {};

        for (var i = 0; i < tests.length; i++) {
            var item = tests[i];
            
            if (!obj[item]) {
                switch (typeof item) {
                case "function":
                    var fnParts = splitFunction(item);
                    obj[fnParts.name] = item;
                    break;
                case "string":
                    var fn;
                    
                    if (fn = probeOutside(item)) {
                        obj[item] = fn;
                    }
                }
            }
        }

        return parseSuiteObject(obj);
    }

    function parseSuiteObject(obj) {
        var suite = new jsUnity.TestSuite(obj.suiteName, obj);

        for (var name in obj) {
            if (obj.hasOwnProperty(name)) {
                var fn = obj[name];

                if (typeof fn === "function") {
                    if (/^test/.test(name)) {
                        suite.tests.push({ name: name, fn: fn });
                    } else if (/^(setUp|tearDown)$/.test(name)) {
                        suite[name] = fn;
                    }
                }
            }
        }
        
        return suite;
    }

    var logLevels = {
        error: 1,
        warn: 2,
        info: 3,
        debug: 4
    };
    
    var logStream = {
        write: empty,
        level: "info"
    };

    for (var level in logLevels) {
        logStream[level] = (function () {
            var strLevel = level;
            var numLevel = logLevels[level];

            return function (s) {
                if (numLevel >= logLevels[this.level] ) {
                    this.write(s, strLevel);
                }
            };
        })();
    }

    var tapStream = {
        write: empty
    };

    var resultsStream = {
        begin: function (total, suiteName) {
            jsUnity.tap.write("TAP version 13");
            jsUnity.tap.write("# " + suiteName);
            jsUnity.tap.write("1.." + total);

            jsUnity.log.info("Running "
                + (suiteName || "unnamed test suite"));
            jsUnity.log.info(plural(total, "test") + " found");
        },

        pass: function (index, testName) {
            jsUnity.tap.write(fmt("ok ? - ?", index, testName));
            jsUnity.log.info("[PASSED] " + testName);
        },

        fail: function (index, testName, message) {
            jsUnity.tap.write(fmt("not ok ? - ?", index, testName));
            jsUnity.tap.write("  ---");
            jsUnity.tap.write("  " + message);
            jsUnity.tap.write("  ...");
            jsUnity.log.info(fmt("[FAILED] ?: ?", testName, message));
        },

        end: function (passed, failed, duration) {
            jsUnity.log.info(plural(passed, "test") + " passed");
            jsUnity.log.info(plural(failed, "test") + " failed");
            jsUnity.log.info(plural(duration, "millisecond") + " elapsed");
        }
    };

    return {
        TestSuite: function (suiteName, scope) {
            this.suiteName = suiteName;
            this.scope = scope;
            this.tests = [];
            this.setUp = undefined;
            this.tearDown = undefined;
        },

        TestResults: function () {
            this.suiteName = undefined;
            this.total = 0;
            this.passed = 0;
            this.failed = 0;
            this.duration = 0;
        },

        assertions: defaultAssertions,

        env: {
            defaultScope: this,

            getDate: function () {
                return new Date();
            }
        },
        
        attachAssertions: function (scope) {
            scope = scope || this.env.defaultScope;

            for (var fn in jsUnity.assertions) {
                scope[fn] = jsUnity.assertions[fn];
            }
        },

        results: resultsStream,
        log: logStream,
        tap: tapStream,

        compile: function (v) {
            if (v instanceof jsUnity.TestSuite) {
                return v;
            } else if (v instanceof Function) {
                return parseSuiteFunction(v);
            } else if (v instanceof Array) {
                return parseSuiteArray(v);
            } else if (v instanceof Object) {
                return parseSuiteObject(v);
            } else if (typeof v === "string") {
                return parseSuiteString(v);
            } else {
                throw "Argument must be a function, array, object, string or "
                    + "TestSuite instance.";
            }
        },

        run: function () {
            var results = new jsUnity.TestResults();

            var suiteNames = [];
            var start = jsUnity.env.getDate();

            for (var i = 0; i < arguments.length; i++) {
                try {
                    var suite = jsUnity.compile(arguments[i]);
                } catch (e) {
                    this.log.error("Invalid test suite: " + e);
                    return false;
                }

                var cnt = suite.tests.length;

                this.results.begin(cnt, suite.suiteName);
                // when running multiple suites, report counts at end?
    
                suiteNames.push(suite.suiteName);
                results.total += cnt;
                
                function getFixtureUtil(fnName) {
                    var fn = suite[fnName];
                    
                    return fn
                        ? function (testName) {
                            fn.call(suite.scope, testName);
                        }
                        : empty;
                }
                
                var setUp = getFixtureUtil("setUp");
                var tearDown = getFixtureUtil("tearDown");

                for (var j = 0; j < cnt; j++) {
                    var test = suite.tests[j];
    
                    try {
                        setUp(test.name);
                        test.fn.call(suite.scope, test.name);
                        tearDown(test.name);

                        this.results.pass(j + 1, test.name);

                        results.passed++;
                    } catch (e) {
                        tearDown(test.name); // if tearDown above throws exc, will call again!

                        this.results.fail(j + 1, test.name, e);
                    }
                }
            }

            results.suiteName = suiteNames.join(",");
            results.failed = results.total - results.passed;
            results.duration = jsUnity.env.getDate() - start;

            this.results.end(results.passed, results.failed, results.duration);

            return results;
        }
    };
})();
//%>
