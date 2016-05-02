// Copyright 2014 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
info: >
    Promise.all with 1-element array
    should accept an array with settled promise
es6id: S25.4.4.1_A6.1_T2
author: Sam Mikes
description: Promise.all([p1]) is resolved with a promise for a one-element array
---*/

var p1 = Promise.resolve(3);

var pAll = Promise.all([p1]);

pAll.then(function (result) {
    if (!(pAll instanceof Promise)) {
        $ERROR("Expected Promise.all() to be promise, actually " + pAll);
    }
    if (!(result instanceof Array)) {
        $ERROR("Expected Promise.all() to be promise for an Array, actually " + result);
    }
    if (result.length !== 1) {
        $ERROR("Expected Promise.all([p1]) to be a promise for one-element Array, actually " + result);
    }
    if (result[0] !== 3) {
        $ERROR("Expected result[0] to be 3, actually " + result[0]);
    }
}).then($DONE, $DONE);
