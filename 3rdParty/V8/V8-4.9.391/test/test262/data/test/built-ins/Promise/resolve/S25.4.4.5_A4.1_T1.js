// Copyright 2014 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
info: >
    Section 25.4.1.4.2
es6id: S25.4.4.5_A3.1_T1
author: Sam Mikes
description: self-resolved Promise throws TypeError
---*/

var resolveP,
    p = new Promise(function (resolve) { resolveP = resolve; });

resolveP(p);

p.then(function () {
    $ERROR("Should not fulfill: should reject with TypeError.");
}, function (err) {
    if (!(err instanceof TypeError)) {
        $ERROR("Expected TypeError, got " + err);
    }
}).then($DONE, $DONE);
