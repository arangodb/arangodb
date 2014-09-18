/*global require, exports
*/

var loadTestRunner = function (tests, options, testMethods) {
  "use strict";
  // declare some useful modules and functions
  var internal = require("internal"), 
      time = internal.time, 
      print = internal.print; 

  // calculate statistical values from series
  var calc = function (values, options) {
    var sum = function (values) {
      return values.reduce(function (previous, current) {
        return previous + current;
      });
    };
    values.sort();
    var n = values.length;
    var result = { 
      min: values[0], 
      max: values[n - 1],
      sum: sum(values),
      avg: sum(values) / n,
      har: sum(values.slice(options.strip, n - options.strip)) / (n - 2 * options.strip)
    };
    return result;
  };

  // execute callback function as many times as we have runs
  // call setup() and teardown() if defined
  // will return series of execution times (not including setup/teardown)
  var measure = function (test, options) {
    var timedExecution = function (testMethodName, testMethodsOptions) { 
      var start = time();
      test.func(test.params, testMethodName, testMethodsOptions); 
      return time() - start; 
    }; 

    var results = [ ], i;
    var oneTestMethod;
    for (oneTestMethod in testMethods) {
      if (testMethods.hasOwnProperty(oneTestMethod)) {
        results[oneTestMethod] = [];
      }
    }
    


    if (typeof test.setup === "function") {

      test.setup(options);
    }

    for (oneTestMethod in testMethods) {
      for (i = 0; i < options.runs; ++i) {
        if (testMethods.hasOwnProperty(oneTestMethod)) {
          results[oneTestMethod][i] = timedExecution(oneTestMethod, testMethods[oneTestMethod]);
        }
      }
    }

    if (typeof test.teardown === "function") {
      test.teardown();
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
    
    print(pad("test name", headLength, "right") + sep +  
          pad("runs", runsLength, "left") + sep +
          pad("total (s)", cellLength, "left") + sep +
          pad("min (s)", cellLength, "left") + sep +
          pad("max (s)", cellLength, "left") + sep +
          pad("avg (s)", cellLength, "left") + sep +
          pad("final (s)", cellLength, "left") + sep +
          pad("%", runsLength, "left"));
 
    print(new Array(lineLength).join("-"));

    var i, baseline;

    for (i = 0; i < tests.length; ++i) {
      var test = tests[i];
      if ((typeof(test.func) !== "function") &&
          (typeof(test.setup) === "function")){
        test.setup(options);
      }
    }

    for (i = 0; i < tests.length; ++i) {
      var test = tests[i];
      if (typeof(test.func) === "function") {
        var resultSet = measure(test, options)
        var oneResult;
        for (oneResult in resultSet) {
          if (resultSet.hasOwnProperty(oneResult)) {
            var stats = calc(resultSet[oneResult], options);

            if (i === 0) {
              stats.rat = 100;
              baseline = stats.har;
            }
            else {
              stats.rat = 100 * stats.har / baseline;
            }
            var name = test.name + ' ' + oneResult;
            print(pad(name , headLength, "right") + sep +  
                  pad(String(options.runs), runsLength, "left") + sep +
                  pad(stats.sum.toFixed(options.digits), cellLength, "left") + sep +
                  pad(stats.min.toFixed(options.digits), cellLength, "left") + sep +
                  pad(stats.max.toFixed(options.digits), cellLength, "left") + sep + 
                  pad(stats.avg.toFixed(options.digits), cellLength, "left") + sep + 
                  pad(stats.har.toFixed(options.digits), cellLength, "left") + sep + 
                  pad(stats.rat.toFixed(2), runsLength, "left")); 
          }
        }
      }
    }
    for (i = 0; i < tests.length; ++i) {
      var test = tests[i];
      if ((typeof(test.func) !== "function") &&
          (typeof(test.teardown) === "function")){
        test.teardown(options);
      }
    }
  };

  run(tests, options);
};


exports.loadTestRunner = loadTestRunner;