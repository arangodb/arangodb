// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 25.4.5.3
description: The return value of the `onFulfilled` method is a pending promise
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

var executor1Counter = 0;
var then1Counter = 0;
var executor2Counter = 0;
var then2Counter = 0;
var promise = new Promise(function(resolve) {
    resolve();

    assert.sameValue(executor1Counter, 0);
    assert.sameValue(then1Counter, 0);
    assert.sameValue(executor2Counter, 0);
    assert.sameValue(then2Counter, 0);

    executor1Counter += 1;
  });

promise.then(function() {
    assert.sameValue(executor1Counter, 1);
    assert.sameValue(then1Counter, 0);
    assert.sameValue(executor2Counter, 0);
    assert.sameValue(then2Counter, 0);

    then1Counter += 1;

    return new Promise(function(_, reject) {
      promise.then(function() {
        reject();

        assert.sameValue(executor1Counter, 1);
        assert.sameValue(then1Counter, 1);
        assert.sameValue(executor2Counter, 1);
        assert.sameValue(then2Counter, 0);

        then2Counter += 1;
      });

      assert.sameValue(executor1Counter, 1);
      assert.sameValue(then1Counter, 1);
      assert.sameValue(executor2Counter, 0);
      assert.sameValue(then2Counter, 0);

      executor2Counter += 1;
    });
  }).then(function() {
    $DONE('The promise should not be fulfilled');
  }, function() {
    assert.sameValue(executor1Counter, 1);
    assert.sameValue(then1Counter, 1);
    assert.sameValue(executor2Counter, 1);
    assert.sameValue(then2Counter, 1);

    $DONE();
  });
