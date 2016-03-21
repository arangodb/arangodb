// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 25.4.5.3
description: The `onFulfilled` method throws an error
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

    25.4.2.2 PromiseResolveThenableJob

    2. Let thenCallResult be Call(then, thenable,
       «resolvingFunctions.[[Resolve]], resolvingFunctions.[[Reject]]»).
    3. If thenCallResult is an abrupt completion,
       a. Let status be Call(resolvingFunctions.[[Reject]], undefined,
          «thenCallResult.[[value]]»).
       b. NextJob Completion(status).
---*/

var error = new Test262Error();
var promiseFulfilled = false;
var promiseRejected = false;
var promise = new Promise(function(resolve) {
    resolve();
  });

promise.then(function() {
    throw error;
  }).then(function() {
    $DONE('This promise should not be fulfilled');
  }, function(reason) {
    assert.sameValue(reason, error);

    $DONE();
  });
