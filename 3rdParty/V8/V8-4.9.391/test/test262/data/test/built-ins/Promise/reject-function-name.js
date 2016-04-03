// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 25.4.1.3.1
description: The `name` property of Promise Reject functions
info: >
  A promise reject function is an anonymous built-in function.

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.
---*/

var rejectFunction;
new Promise(function(resolve, reject) {
  rejectFunction = reject;
});

assert.sameValue(Object.prototype.hasOwnProperty.call(rejectFunction, "name"), false);
