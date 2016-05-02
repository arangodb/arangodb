// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 25.4.1.5.1
description: GetCapabilitiesExecutor functions are not constructors
info: >
  17 ECMAScript Standard Built-in Objects:
    Built-in function objects that are not identified as constructors do not
    implement the [[Construct]] internal method unless otherwise specified
    in the description of a particular function.
---*/

var executorFunction;
function NotPromise(executor) {
  executorFunction = executor;
  executor(function(){}, function(){});
}
Promise.resolve.call(NotPromise);

assert.sameValue(Object.prototype.hasOwnProperty.call(executorFunction, "prototype"), false);
assert.throws(TypeError, function() { new executorFunction(); });
