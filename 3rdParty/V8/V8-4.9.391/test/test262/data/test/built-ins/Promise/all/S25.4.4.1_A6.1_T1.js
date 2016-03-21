// Copyright 2014 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
info: >
    Promise.all with 0-element array
es6id: S25.4.4.1_A5.1_T1
author: Sam Mikes
description: Promise.all([]) produces a promise
---*/

var p = Promise.all([]);

if (!(p instanceof Promise)) {
    $ERROR('Expected Promise.all([]) to be instanceof Promise' + err);
}
