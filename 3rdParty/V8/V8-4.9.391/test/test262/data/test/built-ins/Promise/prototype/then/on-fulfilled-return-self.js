// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 25.4.5.3
description: The return value of the `onFulfilled` method is a "thenable" object
info: >
    7. Return PerformPromiseThen(promise, onFulfilled, onRejected,
       resultCapability).

    25.4.5.3.1 PerformPromiseThen

    [...]
    8. Else if the value of promise's [[PromiseState]] internal slot is
       "fulfilled",
       a. Let value be the value of promise's [[PromiseResult]] internal slot.
       b. Perform EnqueueJob("PromiseJobs", PromiseReactionJob,
          «fulfillReaction, value»).

    25.4.1.3.2 Promise Resolve Functions

    [...]
    6. If SameValue(resolution, promise) is true, then
       a. Let selfResolutionError be a newly created TypeError object.
       b. Return RejectPromise(promise, selfResolutionError).
---*/

var promise1 = new Promise(function(resolve) {
  resolve();
});

var promise2 = promise1.then(function() {
  return promise2;
});

promise2.then(function() {
  $DONE('This promise should not be resolved');
}, function(reason) {
  assert.sameValue(reason.constructor, TypeError);

  $DONE();
});
