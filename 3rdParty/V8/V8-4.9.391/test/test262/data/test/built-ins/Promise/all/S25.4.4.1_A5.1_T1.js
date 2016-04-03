// Copyright 2014 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
info: >
    Promise.all expects an iterable argument;
    rejects if IteratorStep() throws
es6id: S25.4.4.1_A5.1_T1
author: Sam Mikes
description: iterator.next throws, causing Promise.all to reject
features: [Symbol.iterator]
---*/

var iterThrows = {};
var error = new Test262Error();
iterThrows[Symbol.iterator] = function() {
    return {
        next: function () {
            throw error;
        }
    };
};

Promise.all(iterThrows).then(function () {
    $ERROR('Promise unexpectedly resolved: Promise.all(iterThrows) should throw TypeError');
},function (reason) {
    assert.sameValue(reason, error);
}).then($DONE,$DONE);
