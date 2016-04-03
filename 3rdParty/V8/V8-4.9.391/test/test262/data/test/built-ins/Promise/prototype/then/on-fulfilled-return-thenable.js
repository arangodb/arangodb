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
    8. Let then be Get(resolution, "then").
    9. If then is an abrupt completion, then
       a. Return RejectPromise(promise, then.[[value]]).
    10. Let thenAction be then.[[value]].
    11. If IsCallable(thenAction) is false, then
        a. Return FulfillPromise(promise, resolution).
    12. Perform EnqueueJob ("PromiseJobs", PromiseResolveThenableJob, «promise,
        resolution, thenAction»)
    13. Return undefined.
---*/

var callCount = 0;
var promise = new Promise(function(resolve) {
  resolve();
});

var thenable = {
  then: function(resolve, reject) {
    assert.sameValue(
      this, thenable, 'method invoked with correct `this` value'
    );
    assert.sameValue(typeof resolve, 'function', 'type of first argument');
    assert.sameValue(
      resolve.length,
      1,
      'ES6 25.4.1.3.2: The length property of a promise resolve function is 1.'
    );
    assert.sameValue(typeof reject, 'function', 'type of second argument');
    assert.sameValue(
      reject.length,
      1,
      'ES6 25.4.1.3.1: The length property of a promise reject function is 1.'
    );
    assert.sameValue(arguments.length, 2, 'total number of arguments');
    resolve();

    callCount += 1;
  }
};

promise.then(function() {
  return thenable;
}).then(function() {
  assert.sameValue(callCount, 1);

  $DONE();
}, function() {
  $DONE('This promise should not be rejected');
});
