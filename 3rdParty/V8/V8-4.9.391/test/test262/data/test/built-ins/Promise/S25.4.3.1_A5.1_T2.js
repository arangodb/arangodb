// Copyright 2014 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
info: >
    Promise executor has predictable environment
    'this' should be global object in sloppy mode,
    undefined in strict mode
es6id: S25.4.3.1_A5.1_T2
author: Sam Mikes
description: Promise executor gets default handling for 'this'
flags: [onlyStrict]
---*/

var expectedThis = undefined;

var p = new Promise(function (resolve) {
    if (this !== expectedThis) {
        $ERROR("'this' must be undefined, got " + this);
    }

    resolve();
}).then($DONE, $DONE);
