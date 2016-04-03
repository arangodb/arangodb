// Copyright 2014 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
es6id: S25.4.4.3_A7.1_T3
author: Sam Mikes
description: Promise.race([p1, p2]) settles when first settles
includes: [PromiseHelper.js]
---*/

var sequence = [];

var p1 = new Promise(function () {}),
    p2 = Promise.resolve(2),
    p = Promise.race([p1, p2]);

sequence.push(1);

p.then(function (arg) {
    if (arg !== 2) {
        $ERROR("Expected promise to be fulfilled with 2, got " + arg);
    }

    sequence.push(4);
    checkSequence(sequence, "This happens second");
}).catch($DONE);

Promise.resolve().then(function () {
    sequence.push(3);
    checkSequence(sequence, "This happens first");
}).then(function () {
    sequence.push(5);
    checkSequence(sequence, "This happens third");
}).then($DONE, $DONE);

sequence.push(2);
