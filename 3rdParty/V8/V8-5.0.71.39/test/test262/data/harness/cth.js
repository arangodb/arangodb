/// Copyright (c) 2012 Ecma International.  All rights reserved. 
/// This code is governed by the BSD license found in the LICENSE file.

function testRun(id, path, description, codeString, result, error) {
  if (result!=="pass") {
      throw new Error("Test '" + path + "'failed: " + error);
  }
}

// define a default `print` function for async tests where there is no 
// global `print`
var print;

// in node use console.log
if (typeof console === "object") {
    print = function () {
        var args = Array.prototype.slice.call(arguments);
        console.log(args.join(" "));
    };
}

// in WScript, use WScript.Echo
if (typeof WScript === "object") {
    print = function () {
        var args = Array.prototype.slice.call(arguments);
        WScript.Echo(args.join(" "));
    };

    // also override $ERROR to force a nonzero exit code exit 
    // TODO? report syntax errors
    var oldError = $ERROR;
    $ERROR = function (message) {
        print("Test262 Error: " + message);
        WScript.Quit(1);
    };
}
