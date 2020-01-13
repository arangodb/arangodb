/* jshint evil: true, strict: false */
/* global require, exports */
//<%
/**
 * jsUnity Universal JavaScript Testing Framework v0.6
 * http://jsunity.com/
 *
 * Copyright (c) 2009 Ates Goral
 * Licensed under the MIT license.
 * http://www.opensource.org/licenses/mit-license.php
 */
var counter; // crying
var jsUnity = exports.jsUnity = (function () {
  function fmt(str) {
    var internal = require("internal");
    var a = Array.prototype.slice.call(arguments, 1);
    return "at assertion #" + counter + ": " + str.replace(/\?/g, function () {
      internal.startCaptureMode();
      internal.print(a.shift());

      var outputWithoutNewline = internal.stopCaptureMode();
      return outputWithoutNewline.substr(0, outputWithoutNewline.length - 1);
    });

  }

  function hash(v, seen = []) {
    if (v instanceof Object && v !== null) {
      var arr = [];
      var sorted = Object.keys(v).sort(), n = sorted.length;
      seen.push(v);

      for (var i = 0; i < n; i++) {
        var p = sorted[i];
        if (v.hasOwnProperty(p)) {
          var j = seen.indexOf(v[p]);
          arr.push(p);
          if (j === -1) {
            arr.push(hash(v[p], seen));
          } else {
            arr.push(`&${j}`);
          }
        }
      }

      return arr.join("#");
    } else {
      return String(v);
    }
  }

  var defaultAssertions = {
    assertException: function (fn, message) {
      counter++;
      try {
        if (fn instanceof Function) {
          fn();
        }
      } catch (e) {
        return;
      }
      var err = new Error();

      throw fmt("?: (?) does not raise an exception or not a function\n(?)",
                message || "assertException", fn, err.stack);
    },

    assertTrue: function (actual, message) {
      counter++;
      if (! actual) {
        var err = new Error();
        throw fmt("?: (?) does not evaluate to true\n(?)",
                  message || "assertTrue", actual, err.stack);
      }
    },

    assertFalse: function (actual, message) {
      counter++;
      if (actual) {
        var err = new Error();
        throw fmt("?: (?) does not evaluate to false\n(?)",
                  message || "assertFalse", actual, err.stack);
      }
    },

    assertIdentical: function (expected, actual, message) {
      counter++;
      if (expected !== actual) {
        var err = new Error();
        throw fmt("?: (?) is not identical to (?)\n(?)",
                  message || "assertIdentical", actual,
                  expected, err.stack);
      }
    },

    assertNotIdentical: function (expected, actual, message) {
      counter++;
      if (expected === actual) {
        var err = new Error();
        throw fmt("?: (?) is identical to (?)\n(?)",
                  message || "assertNotIdentical", actual, expected, err.stack);
      }
    },

    assertEqual: function (expected, actual, message) {
      counter++;
      if (hash(expected) !== hash(actual)) {
        var err = new Error();
        throw fmt("?: (?) is not equal to (?)\n(?)",
                  message || "assertEqual", actual, expected, err.stack);
      }
    },

    assertNotEqual: function (expected, actual, message) {
      counter++;
      if (hash(expected) === hash(actual)) {
        var err = new Error();
        throw fmt("?: (?) is equal to (?)\n(?)",
                  message || "assertNotEqual", actual, expected, err.stack);
      }
    },

    assertMatch: function (re, actual, message) {
      counter++;
      if (! re.test(actual)) {
        var err = new Error();
        throw fmt("?: (?) does not match (?)\n(?)",
                  message || "assertMatch", actual, re, err.stack);
      }
    },

    assertNotMatch: function (re, actual, message) {
      counter++;
      if (re.test(actual)) {
        var err = new Error();
        throw fmt("?: (?) matches (?)\n(?)",
                  message || "assertNotMatch", actual, re, err.stack);
      }
    },

    assertTypeOf: function (typ, actual, message) {
      counter++;
      if (typeof actual !== typ) {
        var err = new Error();
        throw fmt("?: (?) is not of type (?)\n(?)",
                  message || "assertTypeOf", actual, typ, err.stack);
      }
    },

    assertNotTypeOf: function (typ, actual, message) {
      counter++;
      if (typeof actual === typ) {
        var err = new Error();
        throw fmt("?: (?) is of type (?)\n(?)",
                  message || "assertNotTypeOf", actual, typ, err.stack);
      }
    },

    assertInstanceOf: function (cls, actual, message) {
      counter++;
      if (!(actual instanceof cls)) {
        var err = new Error();
        throw fmt("?: (?) is not an instance of (?)\n(?)",
                  message || "assertInstanceOf", actual, cls, err.stack);
      }
    },

    assertNotInstanceOf: function (cls, actual, message) {
      counter++;
      if (actual instanceof cls) {
        var err = new Error();
        throw fmt("?: (?) is an instance of (?)\n(?)",
                  message || "assertNotInstanceOf", actual, cls, err.stack);
      }
    },

    assertNull: function (actual, message) {
      counter++;
      if (actual !== null) {
        var err = new Error();
        throw fmt("?: (?) is not null\n(?)",
                  message || "assertNull", actual, err.stack);
      }
    },

    assertNotNull: function (actual, message) {
      counter++;
      if (actual === null) {
        var err = new Error();
        throw fmt("?: (?) is null\n(?)",
                  message || "assertNotNull", actual, err.stack);
      }
    },

    assertUndefined: function (actual, message) {
      counter++;
      if (actual !== undefined) {
        var err = new Error();
        throw fmt("?: (?) is not undefined\n(?)",
                  message || "assertUndefined", actual, err.stack);
      }
    },

    assertNotUndefined: function (actual, message) {
      counter++;
      if (actual === undefined) {
        var err = new Error();
        throw fmt("?: (?) is undefined\n(?)",
                  message || "assertNotUndefined", actual, err.stack);
      }
    },

    assertNaN: function (actual, message) {
      counter++;
      if (!isNaN(actual)) {
        var err = new Error();
        throw fmt("?: (?) is not NaN\n(?)",
                  message || "assertNaN", actual, err.stack);
      }
    },

    assertNotNaN: function (actual, message) {
      counter++;
      if (isNaN(actual)) {
        var err = new Error();
        throw fmt("?: (?) is NaN\n(?)",
                  message || "assertNotNaN", actual, err.stack);
      }
    },

    fail: function (message) {
      throw message || "fail";
    }
  };

  function empty() {}

  function plural(cnt, unit) {
    return cnt + " " + unit + (cnt === 1 ? "" : "s");
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
          && fn !== probeOutside(token)) {

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
          fn = probeOutside(item);
          if (typeof (fn) !== 'undefined') {
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
          } else if (/^(setUp|tearDown|setUpAll|tearDownAll)$/.test(name)) {
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

  var streamLoglevel = function (strLevel, numLevel) {
    return function (s) {
      if (numLevel >= logLevels[this.level] ) {
        this.write(s, strLevel);
      }
    };
  };
  for (var level in logLevels) {
    if (logLevels.hasOwnProperty(level)) {
      logStream[level] = streamLoglevel(level, logLevels[level]);
    }
  }

  var tapStream = {
    write: empty
  };

  var resultsStream = {
    begin: function (total, suiteName) {
      jsUnity.tap.write("TAP version 13");
      jsUnity.tap.write("# " + suiteName);
      jsUnity.tap.write("1.." + total);

      jsUnity.log.info(Date() + " Running "
                       + (suiteName || "unnamed test suite"));
      jsUnity.log.info(plural(total, "test") + " found");
    },

    beginSetUpAll: function(index, testName) {},

    endSetUpAll: function(index, testName) {},

    beginSetUp: function(index, testName) {},

    endSetUp: function(index, testName) {},

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

    beginTeardown: function(index, testName) {},

    endTeardown: function(index, testName) {},

    beginTeardownAll: function(index, testName) {},

    endTeardownAll: function(index, testName) {},

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
      this.setUpAll = undefined;
      this.tearDownAll = undefined;
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
        if (jsUnity.assertions.hasOwnProperty(fn)) {
          scope[fn] = jsUnity.assertions[fn];
        }
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
      var getFixtureUtil = function (fnName, suite) {
        var fn = suite[fnName];

        return fn
          ? function (testName) {
            fn.call(suite.scope, testName);
          }
        : empty;
      };
      var results = new jsUnity.TestResults();

      var suiteNames = [];
      var start = jsUnity.env.getDate();

      for (var i = 0; i < arguments.length; i++) {
        var suite;
        try {
          suite = jsUnity.compile(arguments[i]);
        } catch (e) {
          this.log.error("Invalid test suite: " + e);
          return false;
        }

        var cnt = suite.tests.length;

        this.results.begin(cnt, suite.suiteName);
        // when running multiple suites, report counts at end?

        suiteNames.push(suite.suiteName);
        results.total += cnt;

        var setUp = getFixtureUtil("setUp", suite);
        var tearDown = getFixtureUtil("tearDown", suite);
        var setUpAll = getFixtureUtil("setUpAll", suite);
        var tearDownAll = getFixtureUtil("tearDownAll", suite);
        var runSuite;

        try {
          this.results.beginSetUpAll(suite.scope);
          setUpAll(suite.suiteName);
          this.results.endSetUpAll(suite.scope);
          runSuite = true;
        } catch (setUpAllError) {
          runSuite = false;
          if (setUpAllError.stack !== undefined) {
            this.results.fail(0, suite.suiteName,
                              setUpAllError + " - " + String(setUpAllError.stack) + 
                              " - setUpAll failed");
          }
        }

        if (runSuite) {
          for (var j = 0; j < cnt; j++) {
            var test = suite.tests[j];

            counter = 0;
            let didSetUp = false;
            let didTest = false;
            let skipTest = false;
            let didTearDown = false;
            let messages = [];

            while (1) {
              try {
                if (!didSetUp && !skipTest) {
                  this.results.beginSetUp(suite.scope, test.name);
                  setUp(test.name);
                  this.results.endSetUp(suite.scope, test.name);
                  didSetUp = true;
                }
                if (!didTest && !skipTest) {
                  test.fn.call(suite.scope, test.name);
                  didTest = true;
                }
                if (!didTearDown && !skipTest) {
                  this.results.beginTeardown(suite.scope, test.name);
                  tearDown(test.name);
                  this.results.endTeardown(suite.scope, test.name);
                  didTearDown = true;
                }

                if (messages.length === 0) {
                  this.results.pass(j + 1, test.name);
                  results.passed++;
                } else {
                  this.results.fail(j + 1, test.name, messages.join('\n'));
                }
                break;
              } catch (e) {
                let arangodb = require("@arangodb");
                if ( typeof e === "string" ) {
                  e = new Error(e);
                } else if (e instanceof arangodb.ArangoError && (
                           (e.errorNum === arangodb.errors.ERROR_CLUSTER_TIMEOUT) ||
                           (e.errorNum === arangodb.errors.ERROR_LOCK_TIMEOUT)
                )) {
                  skipTest = true;
                }
                if (!didSetUp) {
                  this.results.endSetUp(suite.scope, test.name);
                  didSetUp = true;
                  messages.push(String(e.stack) + " - setUp failed");
                  skipTest = true;
                  continue;
                }
                if (!didTest && !skipTest) {
                  didTest = true;
                  messages.push(String(e.stack) + " - test failed");
                  continue;
                }
                if (!didTearDown) {
                  this.results.endTeardown(suite.scope, test.name);
                  didTearDown = true;
                  messages.push(String(e.stack) + " - tearDown failed");
                  continue;
                }
              }
            }
          }
        }

        try {
          this.results.beginTeardownAll(suite.scope);
          tearDownAll(suite.suiteName);
          this.results.endTeardownAll(suite.scope);
        } catch (tearDownAllError) {
          if (tearDownAllError.stack !== undefined) {
            this.results.fail(0, suite.suiteName,
                              tearDownAllError + " - " + String(tearDownAllError.stack) + 
                              " - tearDownAll failed");
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
