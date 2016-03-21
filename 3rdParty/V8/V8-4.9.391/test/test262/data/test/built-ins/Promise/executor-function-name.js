// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 25.4.1.5.1
description: The `name` property of GetCapabilitiesExecutor functions
info: >
  A GetCapabilitiesExecutor function is an anonymous built-in function.

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.
---*/

var executorFunction;
function NotPromise(executor) {
  executorFunction = executor;
  executor(function(){}, function(){});
}
Promise.resolve.call(NotPromise);

assert.sameValue(Object.prototype.hasOwnProperty.call(executorFunction, "name"), false);
