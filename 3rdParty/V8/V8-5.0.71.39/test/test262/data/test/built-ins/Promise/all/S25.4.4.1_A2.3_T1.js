// Copyright 2014 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
info: Promise.all([]) returns a promise for a new empty array
es6id: 25.4.4.1_A2.3_T1
author: Sam Mikes
description: Promise.all([]) returns a promise for an array
---*/

var arg = [];

Promise.all(arg).then(function (result) {
    if(!(result instanceof Array)) {
        $ERROR("expected an array from Promise.all, got " + result);
    }
}).then($DONE, $DONE);
