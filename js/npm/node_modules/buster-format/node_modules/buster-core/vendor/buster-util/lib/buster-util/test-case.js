var buster = buster || {};

(function () {
    var bu, silentMode = false, filter;

    if (typeof jstestdriver == "object" && !!jstestdriver.console) {
        return;
    }

    if (typeof require == "function" && typeof module == "object") {
        bu = nodeUtils();
    } else {
        bu = browserUtils();
    }

    var testCases = [], running = false, currentTest, errors = [];

    bu.testCase = function (name, tests) {
        testCases.push({ name: name, tests: tests });
        bu.nextTick(runTests);
    };

    function runTests() {
        if (running) return;
        running = true;
        var tc = testCases.shift();
        if (!tc) {
            if (errors.length == 0) {
                bu.puts("All tests passed!");
            } else {
                bu.puts(errors.length + " failure" + (errors.length > 1 ? "s" : ""), "red");
            }
            return;
        }
        runTestCase(tc.name, tc.tests, runTests);
    }

    function getSetUp(testCase) {
        for (var name in testCase) {
            if (name == "setUp") {
                return testCase[name];
            }
        }
    }

    function getTearDown(testCase) {
        for (var name in testCase) {
            if (name == "tearDown") {
                return testCase[name];
            }
        }
    }

    function getTests(testCase) {
        var tests = [];

        for (var name in testCase) {
            if (!/^setUp|tearDown$/.test(name)) {
                tests.push({ name: name, test: testCase[name] });
            }
        }

        return tests;
    }

    function runTestCase(name, body, next) {
        if (!silentMode) {
            bu.puts(name);
        }
        var tests = getTests(body);
        var setUp = getSetUp(body);
        var tearDown = getTearDown(body);

        function runNext() {
            var test = tests.shift();
            if (!test) {
                running = false;
                return next();
            }
            runTest(test.name, setUp, tearDown, test.test, runNext);
        }

        runNext();
    }

    function runTest(name, setUp, tearDown, test, next) {
        var self = {}, timer, cleared = false;
        currentTest = name;

        function done(err) {
            if (err) {
                bu.puts("  " + name, err.name == "AssertionError" ? "red" : "yellow");
                bu.puts("    " + stack(err));
                errors.push(err);
            } else {
                if (!silentMode) {
                    bu.puts("  " + name, "green");
                }
            }

            next();
        }

        if (filter && !new RegExp(filter).test(name)) {
            return done();
        }

        var ctx = makeContext(function (err) {
            clearTimeout(timer);
            cleared = true;
            var tdErr = captureError(tearDown, self);
            done(err || tdErr);
        });

        var suErr = captureError(setUp, self);
        if (suErr) { return done(suErr); }

        if (test.length == 0) {
            var err = captureError(test, self);
            var tdErr = captureError(tearDown, self);
            done(err || tdErr);
        } else {
            var err = captureError(test, self, ctx);
            if (err) { return done(err); }

            timer = setTimeout(function () {
                // WTF!! Isn't this why we have clearTimeout?
                if (cleared) { return; }
                done({ stack: "Timed out" });
            }, 600);
        }
    }

    function captureError(func, thisp) {
        if (!func) return;
        try {
            func.apply(thisp, [].slice.call(arguments, 2));
        } catch (e) {
            return e;
        }
    }

    function makeContext(next) {
        return {
            end: function (func) {
                if (typeof func == "function") {
                    return function () {
                        var args = [func, this].concat([].slice.call(arguments));
                        next(captureError.apply(this, args));
                    };
                } else {
                    next();
                }
            }
        };
    }

    function stack(err) {
        var lines = (err.stack || "").split("\n");
        var result = [];

        for (var i = 0, l = lines.length; i < l; ++i) {
            if (!/(buster-util\/)|(buster-assertions\/)/.test(lines[i])) {
                result.push("      " + lines[i]);
            }
        }

        return result.join("\n");
    }

    // Environment setup help

    function nodeUtils() {
        var util = require("util");
        var colors = { "red": 1, "green": 2, "yellow": 3 };
        var bu = {};
        silentMode = process.env.SILENT == "true";
        filter = process.env.FILTER;

        bu.puts = function (message, color) {
            if (color) message = "\033[3" + colors[color] + "m" + message + "\033[39m";
            util.print(message + "\n");
        };

        bu.nextTick = process.nextTick;

        process.on("uncaughtException", function (e) {
            if (!bu.testCase.silent) {
                if (currentTest) {
                    bu.puts("  " + currentTest, "red");
                } else {
                    bu.puts("Uncaught error");
                }
                bu.puts(e.message);
                bu.puts(stack(e));
            }
        });

        return bu;
    }

    function browserUtils() {
        buster.util = buster.util || {};
        document.body.innerHTML = "<h1>" + document.title + " test</h1><p id=\"report\"></p><ul>";
        silentMode = /silent=true/.test(window.location.href);

        var report = document.getElementById("report");

        buster.util.puts = function (message, color) {
            if (!message) return;
            message = message.replace(/\n/g, "<br>");

            if (color) {
                message = '<span style="color: ' + color + '">' + message + '</span>';
            }

            document.body.innerHTML += "<li>" + message + "</li>";
        };

        buster.util.nextTick = function (callback) {
            setTimeout(callback, 0);
        };

        window.onerror = function (error) {
            if (!buster.util.testCase.silent) {
                if (currentTest) {
                    buster.util.puts(currentTest, "red");
                } else {
                    buster.util.puts("Uncaught error");
                }

                buster.util.puts([].join.call(arguments));
            }

            return true;
        };

        return buster.util;
    }

    bu.testCase.puts = bu.puts;

    if (typeof require == "function" && typeof module == "object") {
        module.exports = bu.testCase;
    }
}());
