// Copyright 2014 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
es6id: S25.4.4.3_A4.1_T1
author: Sam Mikes
description: Promise.race rejects if IteratorStep throws
features: [Symbol.iterator]
---*/

var iterThrows = {};
var error = new Test262Error();
iterThrows[Symbol.iterator] = function () {
    return {
        next: function () {
            throw error;
        }
    };
};

Promise.race(iterThrows).then(function () {
    $ERROR('Promise unexpectedly fulfilled: Promise.race(iterThrows) should throw TypeError');
}, function (reason) {
    assert.sameValue(reason, error);
}).then($DONE,$DONE);

