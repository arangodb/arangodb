// Copyright 2014 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
info: >
   Promise.reject
es6id: S25.4.4.4_A2.1_T1
author: Sam Mikes
description: Promise.reject creates a new settled promise
---*/

var p = Promise.reject(3);

if (!(p instanceof Promise)) {
    $ERROR("Expected Promise.reject to return a promise.");
}

p.then(function () {
    $ERROR("Promise should not be fulfilled.");
}, function (arg) {
    if (arg !== 3) {
        $ERROR("Expected promise to be rejected with supplied arg, got " + arg);
    }
}).then($DONE, $DONE);
