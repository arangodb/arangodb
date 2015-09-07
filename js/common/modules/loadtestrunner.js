var measurements = ["time",
                    "ast optimization",
                    "plan instantiation",
                    "parsing",
                    "plan optimization",
                    "execution",
                    "initialization"];

var loadTestRunner = function (tests, options, testMethods) {
  'use strict';
  // declare some useful modules and functions
  var internal = require("internal"), 
      time = internal.time, 
      print = internal.print; 

  // calculate statistical values from series
  var calc = function (values, options) {
    var resultList = {};
    var i, j;
    var times = [];
    var sum = function (times) {
      return times.reduce(function (previous, current) {
        return previous + current;
      });
    };
    for (i = 0; i < measurements.length; i++) {
      if (values[0].hasOwnProperty(measurements[i])) {
        times = [];
        for (j = 0; j < values.length; j++) {
          times[j] = values[j][measurements[i]];
        }
        times.sort();
        var n = values.length;
        resultList[measurements[i]] = { 
          min: times[0], 
          max: times[n - 1],
          sum: sum(times),
          avg: sum(times) / n,
          har: sum(times.slice(options.strip, n - options.strip)) / (n - 2 * options.strip)
        };
      }
    }
    return resultList;
  };

  // execute callback function as many times as we have runs
  // call setup() and teardown() if defined
  // will return series of execution times (not including setup/teardown)
  var measure = function (test, options) {
    var timedExecution = function (testMethodName, testMethodsOptions) { 
      var ret = {};
      var start = time();
      ret = test.func(test.params, testMethodName, testMethodsOptions, options); 
      ret.time = time() - start;
      return ret;
    }; 

    var results = {};
    var i;
    var oneTestMethod;
    for (oneTestMethod in testMethods) {
      if (testMethods.hasOwnProperty(oneTestMethod)) {
        results[oneTestMethod] = [];
      }
    }

    if (typeof test.setUp === "function") {

      test.setUp(options);
    }

    for (oneTestMethod in testMethods) {
      if (testMethods.hasOwnProperty(oneTestMethod)) {
        results[oneTestMethod] = [];
        for (i = 0; i < options.runs; ++i) {
          results[oneTestMethod][i] = timedExecution(oneTestMethod, testMethods[oneTestMethod]);
        }
      }
    }

    if (typeof test.tearDown === "function") {
      test.tearDown();
    }
    return results;
  };

  // run all the tests and print the test results
  var run = function (tests, options) {
    var pad = function (s, l, type) {
      if (s.length >= l) {
        return s.substr(0, l);
      }
      if (type === "right") {
        return s + new Array(l - s.length).join(" ");
      }
      return new Array(l - s.length).join(" ") + s;
    };

    var headLength = 25, 
        runsLength = 8, 
        cellLength = 12, 
        sep = " | ",
        lineLength = headLength + 2 * runsLength + 5 * cellLength + 5 * sep.length - 1;

    var results = {};
    var test;

    print(pad("test name", headLength, "right") + sep +  
          pad("runs", runsLength, "left") + sep +
          pad("total (s)", cellLength, "left") + sep +
          pad("min (s)", cellLength, "left") + sep +
          pad("max (s)", cellLength, "left") + sep +
          pad("avg (s)", cellLength, "left") + sep +
          pad("final (s)", cellLength, "left") + sep +
          pad("%", runsLength, "left"));
 
    print(new Array(lineLength).join("-"));

    var i, j, baseline;

    for (i = 0; i < tests.length; ++i) {
      test = tests[i];
      if ((typeof(test.func) !== "function") &&
          (typeof(test.setUp) === "function")){
        test.setUp(options);
      }
    }

    for (i = 0; i < tests.length; ++i) {
      test = tests[i];
      if (typeof(test.func) === "function") {
        var resultSet;
        resultSet  = measure(test, options);

        var oneResult;

        for (oneResult in resultSet) {
          if (resultSet.hasOwnProperty(oneResult) && (oneResult !== 'status')) {
            results[test.name + '/' + oneResult] = {status: true};
            var stats = calc(resultSet[oneResult], options);

            for (j = 0; j < measurements.length;j ++) {
              if (stats.hasOwnProperty(measurements[j])) {
                if (i === 0) {
                  stats[measurements[j]].rat = 100;
                  baseline = stats[measurements[j]].har;
                }
                else {
                  stats[measurements[j]].rat = 100 * stats[measurements[j]].har / baseline;
                }
                var name = test.name + ' ' + oneResult;
                print(pad(name , headLength, "right") + sep +  
                      pad(String(options.runs), runsLength, "left") + sep +
                      pad(stats[measurements[j]].sum.toFixed(options.digits), cellLength, "left") + sep +
                      pad(stats[measurements[j]].min.toFixed(options.digits), cellLength, "left") + sep +
                      pad(stats[measurements[j]].max.toFixed(options.digits), cellLength, "left") + sep + 
                      pad(stats[measurements[j]].avg.toFixed(options.digits), cellLength, "left") + sep + 
                      pad(stats[measurements[j]].har.toFixed(options.digits), cellLength, "left") + sep + 
                      pad(stats[measurements[j]].rat.toFixed(2), runsLength, "left") + sep + measurements[j]);
                for (var calculation in stats[measurements[j]]) {
                  results[test.name + '/' + oneResult][measurements[j] + '/' + calculation] = {
                    status: true,
                    message: '',
                    duration: stats[measurements[j]][calculation]
                  };
                }
              }
            }

            print(new Array(lineLength).join("-"));
          }

        }

      }
    }
    for (i = 0; i < tests.length; ++i) {
      test = tests[i];
      if ((typeof(test.func) !== "function") &&
          (typeof(test.tearDown) === "function")){
        test.tearDown(options);
      }
    }
    results.status = true;
    return results;
  };

  return run(tests, options);
};


exports.loadTestRunner = loadTestRunner;