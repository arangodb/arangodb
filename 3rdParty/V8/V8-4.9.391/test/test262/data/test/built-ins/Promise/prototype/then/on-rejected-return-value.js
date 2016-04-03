// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 25.4.5.3
description: The return value of the `onRejected` method
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

var returnVal = {};
var promise = new Promise(function(_, reject) {
  reject();
});

promise.then(null, function() {
  return returnVal;
}).then(function(result) {
  assert.sameValue(result, returnVal);

  $DONE();
}, function() {
  $DONE('The promise should not be rejected');
});
