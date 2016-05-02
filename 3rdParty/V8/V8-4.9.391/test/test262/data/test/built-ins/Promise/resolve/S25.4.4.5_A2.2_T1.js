// Copyright 2014 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
es6id: S25.4.4.5_A2.2_T1
author: Sam Mikes
description: Promise.resolve passes through an unsettled promise w/ same Constructor
---*/

var resolveP1,
    p1 = new Promise(function (resolve) { resolveP1 = resolve; }),
    p2 = Promise.resolve(p1),
    obj = {};

if (p1 !== p2) {
    $ERROR("Expected p1 === Promise.resolve(p1) because they have same constructor");
}

p2.then(function (arg) {
    if (arg !== obj) {
        $ERROR("Expected promise to be resolved with obj, actually " + arg);
    }
}).then($DONE, $DONE);

resolveP1(obj);
