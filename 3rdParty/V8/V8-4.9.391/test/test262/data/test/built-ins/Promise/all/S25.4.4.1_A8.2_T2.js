// Copyright 2014 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
info: >
    Promise.all with 2-element array
es6id: S25.4.4.1_A8.2_T2
author: Sam Mikes
description: Promise.all() rejects when second promise in array rejects
includes: [PromiseHelper.js]
---*/

var rejectP2,
    p1 = Promise.resolve(1),
    p2 = new Promise(function (resolve, reject) {
        rejectP2 = reject;
    });

Promise.all([p1, p2]).then(function () {
    $ERROR("Did not expect promise to be fulfilled.");
}, function (rejected) {
    if (rejected !== 2) {
        $ERROR("Expected promise to be rejected with 2, actually " + rejected);
    }
}).then($DONE, $DONE);

rejectP2(2);
