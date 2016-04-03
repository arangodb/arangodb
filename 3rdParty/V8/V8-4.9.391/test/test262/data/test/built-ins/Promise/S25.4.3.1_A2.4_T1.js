// Copyright 2014 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
info: >
    Promise throws TypeError when 'this' is rejected promise
es6id: S25.4.3.1_A2.4_T1
author: Sam Mikes
description: Promise.call(rejected Promise) throws TypeError
---*/

var p = new Promise(function(resolve, reject) { reject(1) });

p.catch(function () {
    Promise.call(p, function () {});
}).then(function () {
    $ERROR("Unexpected resolution - expected TypeError");
}, function (err) {
    if (!(err instanceof TypeError)) {
        $ERROR("Expected TypeError, got " + err);
    }
}).then($DONE, $DONE);
