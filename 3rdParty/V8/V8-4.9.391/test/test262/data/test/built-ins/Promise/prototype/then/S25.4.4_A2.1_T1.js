// Copyright 2014 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
info: >
   Misc sequencing tests
   inspired by https://github.com/promises-aplus/promises-tests/issues/61
   Case "T1"
es6id: S25.4.4_A2.1_T1
author: Sam Mikes
description: Promise onResolved functions are called in predictable sequence
includes: [PromiseHelper.js]
---*/

var resolveP1, rejectP2, sequence = [];

(new Promise(function (resolve, reject) {
    resolveP1 = resolve;
})).then(function (msg) {
    sequence.push(msg);
}).then(function () {
    checkSequence(sequence, "Expected 1,2,3");
}).then($DONE, $DONE);

(new Promise(function (resolve, reject) {
    rejectP2 = reject;
})).catch(function (msg) {
    sequence.push(msg);
});

rejectP2(2);
resolveP1(3);

sequence.push(1);
