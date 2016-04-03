// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    Error thrown when invoking the instance's `then` method
es6id: 25.4.4.1
info: >
    11. Let result be PerformPromiseAll(iteratorRecord, C, promiseCapability).
    12. If result is an abrupt completion,
        a. If iteratorRecord.[[done]] is false, let result be
           IteratorClose(iterator, result).
        b. IfAbruptRejectPromise(result, promiseCapability).

    [...]

    25.4.4.1.1 Runtime Semantics: PerformPromiseAll

    [...]
    6. Repeat
        [...]
        r. Let result be Invoke(nextPromise, "then", «resolveElement,
           resultCapability.[[Reject]]»).
        s. ReturnIfAbrupt(result).
---*/

var promise = new Promise(function() {});
var error = new Test262Error();

promise.then = function() {
  throw error;
};

Promise.all([promise]).then(function() {
  $ERROR('The promise should be rejected');
}, function(reason) {
  assert.sameValue(reason, error);
}).then($DONE, $DONE);
