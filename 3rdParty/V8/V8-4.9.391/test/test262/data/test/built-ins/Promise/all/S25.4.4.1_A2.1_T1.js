// Copyright 2014 Cubane Canada, Inc.  All rights reserved.
// See LICENSE for details.

/*---
info: Promise.all([]) is a Promise
es6id: 25.4.4.1_A2.1_T1
author: Sam Mikes
description: Promise.all returns a Promise
---*/

var p = Promise.all([]);
if (!(p instanceof Promise)) {
    $ERROR('Expected p to be a Promise');
}
