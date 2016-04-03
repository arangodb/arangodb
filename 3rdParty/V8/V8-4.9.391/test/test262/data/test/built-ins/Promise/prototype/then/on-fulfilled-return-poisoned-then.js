// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 25.4.5.3
description: >
    Access error for the `then` property of the object returned from the "on fulfilled" handler
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
    8. Let then be Get(resolution, "then").
    9. If then is an abrupt completion, then
       a. Return RejectPromise(promise, then.[[value]]).
---*/

var poisonedThen = {};
var error = new Test262Error();
Object.defineProperty(poisonedThen, 'then', {
  get: function() {
    throw error;
  }
});

var p = new Promise(function(r) { r(); });

p.then(function() {
  return poisonedThen;
}).then(function() {
  $DONE('The promise should not be fulfilled');
}, function(reason) {
  assert.sameValue(reason, error);

  $DONE();
});
