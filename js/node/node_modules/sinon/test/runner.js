/**
 * More or less copy-pasted from the 'buster' package. The buster
 * "all inclusive" package includes Sinon, which is why we avoid it. 
 */
(function (global, buster, formatio) {
    if (typeof require == "function" && typeof module == "object") {
        buster = require("buster-core");
        formatio = require("formatio");

        module.exports = buster.extend(buster, require("buster-test"), {
            assertions: require("buster-assertions"),
            eventedLogger: require("buster-evented-logger")
        });
    }

    var logFormatter = buster.create(formatio);
    logFormatter.quoteStrings = false;
    var asciiFormat = buster.bind(logFormatter, "ascii");

    buster.console = buster.eventedLogger.create({
        formatter: asciiFormat,
        logFunctions: true
    });

    buster.log = buster.bind(buster.console, "log");

    buster.captureConsole = function () {
        global.console = buster.console;

        if (global.console !== buster.console) {
            global.console.log = buster.bind(buster.console, "log");
        }
    };

    if (asciiFormat) { buster.assertions.format = asciiFormat; }
    global.assert = buster.assertions.assert;
    global.refute = buster.assertions.refute;

    // Assertion counting
    var assertions = 0;
    var count = function () { assertions += 1; };
    buster.assertions.on("pass", count);
    buster.assertions.on("failure", count);

    buster.testRunner.onCreate(function (runner) {
        buster.assertions.bind(runner, { "failure": "assertionFailure" });
        runner.console = buster.console;

        runner.on("test:async", function () {
            buster.assertions.throwOnFailure = false;
        });

        runner.on("test:setUp", function () {
            buster.assertions.throwOnFailure = true;
        });

        runner.on("test:start", function () {
            assertions = 0;
        });

        runner.on("context:start", function (context) {
            if (context.testCase) {
                context.testCase.log = buster.bind(buster.console, "log");
            }
        });
    });

    buster.testRunner.assertionCount = function () { return assertions; };

    var runner = buster.autoRun({
        cwd: typeof process != "undefined" ? process.cwd() : null
    });

    if (buster.testContext.on) {
        buster.testContext.on("create", runner);
    } else {
        buster.testCase.onCreate = runner;
    }

    buster.assertions.add("spy", {
        assert: function (obj) {
            return obj !== null && typeof obj.calledWith === "function" && !obj.returns;
        },
        assertMessage: "Expected object ${0} to be a spy function"
    });

    buster.assertions.add("stub", {
        assert: function (obj) {
            return obj !== null &&
                typeof obj.calledWith === "function" &&
                typeof obj.returns === "function";
        },
        assertMessage: "Expected object ${0} to be a stub function"
    });

    buster.assertions.add("mock", {
        assert: function (obj) {
            return obj !== null &&
                typeof obj.verify === "function" &&
                typeof obj.expects === "function";
        },
        assertMessage: "Expected object ${0} to be a mock"
    });

    buster.assertions.add("fakeServer", {
        assert: function (obj) {
            return obj !== null &&
                Object.prototype.toString.call(obj.requests) == "[object Array]" &&
                typeof obj.respondWith === "function";
        },
        assertMessage: "Expected object ${0} to be a fake server"
    });

    buster.assertions.add("clock", {
        assert: function (obj) {
            return obj !== null &&
                typeof obj.tick === "function" &&
                typeof obj.setTimeout === "function";
        },
        assertMessage: "Expected object ${0} to be a clock"
    });
}(typeof global !== "undefined" ? global : this,
  typeof buster !== "undefined" ? buster : null,
  typeof formatio !== "undefined" ? formatio : null));
