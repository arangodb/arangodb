// Copyright 2014 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
es6id: S25.4.4.5_A2.3_T1
author: Sam Mikes
description: Promise.resolve passes through an unsettled promise w/ same Constructor
---*/

var rejectP1,
    p1 = new Promise(function (resolve, reject) { rejectP1 = reject; }),
    p2 = Promise.resolve(p1),
    obj = {};

if (p1 !== p2) {
    $ERROR("Expected p1 === Promise.resolve(p1) because they have same constructor");
}

p2.then(function () {
    $ERROR("Expected p2 to be rejected, not fulfilled.");
}, function (arg) {
    if (arg !== obj) {
        $ERROR("Expected promise to be rejected with reason obj, actually " + arg);
    }
}).then($DONE, $DONE);

rejectP1(obj);
