// Copyright 2014 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
info: >
    PerformPromiseThen
    Ref 25.4.5.3.1
es6id: S25.4.5.3_A4.1_T2
author: Sam Mikes
description: Promise.prototype.then accepts 'undefined' as arg1, arg2
---*/

var obj = {};
var p = Promise.reject(obj);

p.then(undefined, undefined).then(function () {
        $ERROR("Should not be called -- promise was rejected.");
}, function (arg) {
        if (arg !== obj) {
            $ERROR("Expected resolution object to be passed through, got " + arg);
        }
    }).then($DONE, $DONE);
