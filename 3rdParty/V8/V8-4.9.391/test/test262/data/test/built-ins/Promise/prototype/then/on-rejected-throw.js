// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 25.4.5.3
description: The `onRejected` method throws an error
info: >
    7. Return PerformPromiseThen(promise, onFulfilled, onRejected,
       resultCapability).

    25.4.5.3.1 PerformPromiseThen

    [...]
    9. Else if the value of promise's [[PromiseState]] internal slot is
       "rejected",
       a. Let reason be the value of promise's [[PromiseResult]] internal slot.
       b. Perform EnqueueJob("PromiseJobs", PromiseReactionJob,
          «rejectReaction, reason»).
---*/

var error = new Test262Error();
var promise = new Promise(function(_, reject) {
  reject();
});

promise.then(null, function() {
  throw error;
  }).then(function(result) {
    $DONE('This promise should not be fulfilled');
  }, function(reason) {
    assert.sameValue(reason, error);

    $DONE();
  });
