// Copyright 2014 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
info: >
    Promise throws TypeError when 'this' is resolved promise
es6id: S25.4.3.1_A2.3_T1
author: Sam Mikes
description: Promise.call(resolved Promise) throws TypeError
---*/

var p = new Promise(function(resolve) { resolve(1); });

p.then(function () {
    Promise.call(p, function () {});
}).then(function () {
    $ERROR("Unexpected resolution - expected TypeError");
}, function (err) {
    if (!(err instanceof TypeError)) {
        $ERROR("Expected TypeError, got " + err);
    }
}).then($DONE, $DONE);
